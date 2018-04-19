/*
 * This node simply de-encrypts everything it receives, it is the only way to obtain synchronization between messages.
 */

#include <MODChaCha.h>
#include <crypto.h>
#include <mcp_can.h>
#include <SPI.h>

long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128];                        // Array to store serial string
bool receivedmsg;
//MODChaCha cipherchachaSpeed;                        // Cypher for speed messages
//MODChaCha cipherchachaBrakes;
MODChaCha cipherchachaTemp;                        // Cypher for temp messages

                                                //Maybe I should keep two cyphers for now

#define CAN0_INT 2
#define CS 10
#define SPEED_ID 0x101
#define TEMP_ID 0x100
#define OK_ID 0x02
#define MUTUAL_KEY 0x11
#define INITIAL_SEED 0x7F
#define HANDSHAKE 0xDA

#define DEBUG_MODE 1

bool RECV_INT_FLAG = true;

MCP_CAN CAN0(CS);

void setup()
{
  bool firstmessagereceived = 0;
  uint8_t key[] = {0x0D, 0x0A, 0x01, 0x01, 0x01, 0x0E, 0x01, 0x00};
  uint8_t iv[] = {0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x00,0X01};
  uint8_t counter[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
  
  Serial.begin(115200);
  Serial.println("Receiver Node Ready.");

  // Initialize MCP2515 running at 16MHz with a baudrate of 500kb/s and the masks and filters disabled.
  while(!(CAN_OK == CAN0.begin(CAN_500KBPS))) {
    if(CAN0.begin(CAN_500KBPS) == CAN_OK) 
      Serial.println("MCP2515 Initialized Successfully!");
    else Serial.println("Error Initializing MCP2515...");
  }
  
  pinMode(CAN0_INT, INPUT);                            // Configuring pin for /INT input

  attachInterrupt(CAN0_INT, MCP2515_ISR, FALLING); // start interrupt

  randomSeed(analogRead(A0));

  cipherchachaTemp = MODChaCha(20);                   //Initialize the cypher speed

  cipherchachaTemp.setKey(key,8);
  cipherchachaTemp.setIV(iv,8);


  //Entering sequence 

  uint8_t rndm;
  uint8_t rndm2;
  uint8_t data[1];
  
  while(!firstmessagereceived){   
    rndm =  random(0,255); //With this I will deencrypt message sent by node.
    if(rndm > INITIAL_SEED)
      data[0] = rndm - INITIAL_SEED;
    else
      data[0] = rndm + INITIAL_SEED;

    
    Serial.print("Message sent:");
    Serial.println(data[0]);
    Serial.print("Random: ");
    Serial.println(rndm);
    
    byte sndStat = CAN0.sendMsgBuf(OK_ID, 0, sizeof(data), data);
    if (DEBUG_MODE) {
      if(sndStat == CAN_OK) {
        Serial.println("Sent ok");
      } else {
        Serial.println("Error in sending ok...");
      }
    }
    
    Serial.println();

    //Listen for 50 milis to see if the other node has sent the ok.
    int starttime = millis();
    int endtime = starttime;

    bool timesup = 0;

    
    while (!timesup && !firstmessagereceived){
      if(!digitalRead(CAN0_INT)) {           // If CAN0_INT pin is low, read receive buffer
        CAN0.readMsgBufID(&rxId, &len, rxBuf);
        
        if((rxBuf[0] ^ rndm) == HANDSHAKE){ //For now is XOR, maybe in the future is spritz
          firstmessagereceived = 1; 
        }
      }
      endtime = millis();
      if((endtime - starttime) > 5000)
        timesup = 1;
    }
  } 

  Serial.println("Received the handshake");

  //Send START and get out of loop

  Serial.println("Second random: ");
  rndm2 = rxBuf[1] ^ rndm;
  Serial.print(rndm2);
  Serial.println();

  data[0] = MUTUAL_KEY ^ rndm2;

  counter[0] = HANDSHAKE;
  counter[1] = MUTUAL_KEY;
  counter[2] = rndm;
  counter[3] = rndm2;
  
  
  cipherchachaTemp.setCounter(counter,8);
 
  
  byte sndStat = CAN0.sendMsgBuf(OK_ID, 0, sizeof(data), data);
  if (DEBUG_MODE) {
    if(sndStat == CAN_OK) {
      Serial.println("Sent ok");
      } else {
      Serial.println("Error in sending ok...");
      }
  }

   //Ready to receive

   Serial.println("Ready to start receiving!");
}


void MCP2515_ISR()
{
    RECV_INT_FLAG = true;
}

void PrintArray (byte Source[], byte LEN)
{
  for (int Idx = 0; Idx < LEN; Idx++)
    SerialPrintHex (Source[Idx]);
  Serial.println();
}

void SerialPrintHex ( byte Data ) 
{
  if (Data < 0x10) Serial.print('0');
  Serial.print(Data, HEX);  
}

void loop() {
  // put your main code here, to run repeatedly:

  //This will only work with TEMP node for now, I have to add descrimination by ID later
  receivemsg();
}

void receivemsg() {
  if(RECV_INT_FLAG){
    uint8_t plaintext[8] = {0,0,0,0,0,0,0,0};
    if(!digitalRead(CAN0_INT)) {           // If CAN0_INT pin is low, read receive buffer
      CAN0.readMsgBufID(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)

      cipherchachaTemp.decrypt(plaintext,rxBuf,8);
      
      
      if((rxId & 0x80000000) == 0x80000000)     // Determine if ID is standard (11 bits) or extended (29 bits)
        sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data: ", (rxId & 0x1FFFFFFF), len);
      else
        sprintf(msgString, "Standard ID: 0x%.3lX  DLC: %1d  Data: ", rxId, len);
      
      Serial.print(msgString);
      
      if((rxId & 0x40000000) == 0x40000000) {    // Determine if message is a remote request frame.
        sprintf(msgString, " REMOTE REQUEST FRAME");
        Serial.print(msgString);
      } else {
        
        for(byte i = 0; i<len; i++) {
          sprintf(msgString, " 0x%.2X", rxBuf[i]);
          Serial.print(msgString);
        }
        
      }
      Serial.println();
      
      Serial.println("Decrypted message: ");
      PrintArray(plaintext, 8);
       Serial.print("Counter value: ");
      cipherchachaTemp.getCounter(plaintext);
      PrintArray(plaintext,8);
      Serial.print("Posn: ");
      Serial.println( cipherchachaTemp.getPosn());
    }
  }
}

