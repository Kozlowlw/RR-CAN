/*
 * Author: Daniel Moreno
 * Demo of CAN-CHAP, single node network
 */

//Libraries
#include <ChaCha.h>
#include <Crypto.h>
#include <BLAKE2s.h>
#include <SPI.h>
#include <mcp_can.h>
#include "SipHashC/siphash.c"

//Constants
#define ID_NODE 0x102 //Different for every node
#define ID_RECON 0x02
#define ID_CHALLENGE 0x03
#define ID_WINNER 0x04
#define ID_BEGIN 0x05
#define CS 10
#define CAN0_INT 2 //Pin for input
#define TIMEOUT_ACK_micros 10000

//Global variables 
typedef void *(*StateFunc)();
StateFunc statefunc; 
uint8_t const sharedKey[16] = "Senior Projectz";
uint8_t rxBuf[8];
uint8_t rxLen;
uint8_t s1[8];
uint8_t s2[8];
ChaCha cipher;
BLAKE2s blake;
MCP_CAN CAN0(CS);
bool RECV_INT_FLAG = false;

//States
void *waitChallenge();
void *challengeAccepted();
void *waitAck();
void *whatAmI();
void *winner();
void *sendCipher(); //For now, no checking if cipher is out of sync

//Set flag function
void MCP2515_ISR()
{
    RECV_INT_FLAG = true;
}

//time utils
long int beginning;
long int auth;
long int ciphercreate;
long int daend;


//utils
void PrintArray (uint8_t Source[], uint8_t LEN)
{
  for (int Idx = 0; Idx < LEN; Idx++)
    SerialPrintHex (Source[Idx]);
  Serial.println();
}

void SerialPrintHex (uint8_t Data ) 
{
  if (Data < 0x10) Serial.print('0');
  Serial.print(Data, HEX);
  Serial.print(" ");  
}

bool sendmsg(uint8_t* buf, int len) {
  // send data:  ID = ID_NODE, Standard CAN Frame, Data length = 8 bytes, 'buf' = array of data bytes to send
  byte sndStat = CAN0.sendMsgBuf(ID_NODE, 0, len, (const byte*) buf);
  if(sndStat == CAN_OK){
    return true;
  } 
  return false;
}

boolean array_cmp(uint8_t *a, uint8_t *b, uint8_t len_a, uint8_t len_b){
     uint8_t n;
     
     if (len_a != len_b) 
      return false;

     for (n=0;n<len_a;n++)
      if (a[n]!=b[n])  
        return false;
        
     return true;
}

//Define States
void *waitChallenge(){
  bool challenge = 0;
  long unsigned int rxId = 0;
 
  while(!challenge){
    if(!digitalRead(CAN0_INT)) {
        CAN0.readMsgBufID(&rxId, &rxLen, rxBuf);

        if(rxId == ID_CHALLENGE && rxLen == 8){
          challenge = 1;
          beginning = micros();
        }
    }
  }

  return (void *) challengeAccepted; //next state
}

void *challengeAccepted(){
   uint8_t s1Data[12];
   uint8_t s2Data[12];
   uint32_t IDbuf = ID_NODE;

   //Load s1Data
   memcpy(s1Data,rxBuf,sizeof(rxBuf));
   memcpy(s1Data+sizeof(rxBuf), &IDbuf, sizeof(IDbuf));

   //Calculate S1
   siphash(s1Data, sizeof(s1Data), sharedKey, s1, sizeof(s1));

   //Load s2Data
   IDbuf = ID_WINNER;
   memcpy(s2Data,s1,sizeof(s1));
   memcpy(s2Data+sizeof(s1), &IDbuf, sizeof(IDbuf));
   
   //Calculate s2
   siphash(s2Data, sizeof(s2Data), sharedKey, s2, sizeof(s2));
   
   /*DebugStuff
   Serial.print("S1 DATA: ");
   PrintArray(s1Data,sizeof(s1Data));
   Serial.print("S1: ");
   PrintArray(s1,sizeof(s1));
   Serial.print("S2 DATA: ");
   PrintArray(s2Data,sizeof(s2Data));
   Serial.print("S2: ");
   PrintArray(s2,sizeof(s2));
   //DebugStuff*/
    
   //send s1
   while(!sendmsg(s1, sizeof(s1)));
   
   return (void *) waitAck;
}

void *waitAck(){
  long start = micros();
  long end = 0;
  long unsigned int rxId = 0;
  
  do{
    end = micros();
    if(!digitalRead(CAN0_INT)) {
        CAN0.readMsgBufID(&rxId, &rxLen, rxBuf);

        if(rxId == ID_WINNER){
          return (void *) whatAmI;
        }
        
        if(rxId == ID_CHALLENGE){
          return (void *) challengeAccepted;
        }
    }
    
  }while(end-start <= TIMEOUT_ACK_micros);
  //}while(1);//Erase this
}

void *whatAmI(){

   if(array_cmp(s2,rxBuf, sizeof(s2),sizeof(rxBuf)))
    return (void *) winner;

   return (void *) waitChallenge;
    
}

void *winner(){
  //time utils
  auth = micros();
  
  //In multinode, wait for ID begin, for now, send as soon as Blake2s is done and cipher set
  cipher = ChaCha(20);
  uint8_t hash[32];

  //Calculate new key
  blake.reset();
  blake.update(sharedKey,sizeof(sharedKey));
  blake.update(s1,sizeof(s1));
  blake.update(s2,sizeof(s2));
  blake.finalize(hash, sizeof(hash));

  //initialize cipher
  cipher.setKey(hash,sizeof(hash));
  cipher.setIV(s1,sizeof(s1));
  cipher.setCounter(s2,sizeof(s2));

  //time utils
  ciphercreate = micros();
  
  return (void *) sendCipher;
}

void *sendCipher(){
  //send 8 ciphered messages and go back to waitChallenge
  uint8_t plaintext[]  = {0x0D, 0x0A, 0x01, 0x01, 0x01, 0x0E, 0x07, 0x00};
  uint8_t ciphertext[8];
  
  for(uint8_t n = 0; n < 8; n++)
  {
     cipher.encrypt(ciphertext,plaintext,sizeof(plaintext));
     while(!sendmsg(ciphertext,sizeof(ciphertext)));
     if(n == 0)
      daend = micros();
     delay(10);
  }

  //Just to do the sequence
  cipher.clear(); 

  
  Serial.println();
  Serial.println();
  Serial.print("Mutual authentication took: ");
  Serial.print(auth - beginning);
  Serial.print(" microseconds after recieving challenge");
  Serial.println(" microseconds");
  Serial.print("Cipher creation took ");
  Serial.print(ciphercreate - auth);
  Serial.println(" microseconds");
  Serial.print("First cipher sent after authentication took: ");
  Serial.print(daend - ciphercreate);
  Serial.println(" microseconds");

  Serial.print("The protocol took ");
  Serial.print(daend - beginning);
  Serial.println(" microseconds to initialize communication with ECU after recieving challenge");

  

  
  
  return (void *) waitChallenge;
}


// Normal arduino stuff

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Ready.");

  // Initialize MCP2515 running at 16MHz with a baudrate of 500kb/s and the masks and filters disabled.
  while(!(CAN_OK == CAN0.begin(CAN_500KBPS))) {
    if(CAN0.begin(CAN_500KBPS) == CAN_OK) 
      Serial.println("MCP2515 Initialized Successfully!");
    else Serial.println("Error Initializing MCP2515...");
  }
  
  pinMode(CAN0_INT, INPUT);                            // Configuring pin for /INT input
   
  attachInterrupt(CAN0_INT, MCP2515_ISR, FALLING); // start interrupt

  statefunc = waitChallenge; //Beginning state
  
}



void loop() {
  // put your main code here, to run repeatedly:
  statefunc = (StateFunc)(*statefunc)();
}



