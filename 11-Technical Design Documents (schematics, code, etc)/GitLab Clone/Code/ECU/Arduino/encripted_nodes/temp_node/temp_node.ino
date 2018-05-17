#include <MODChaCha.h>
#include <crypto.h>
#include <mcp_can.h>
#include <SPI.h>

long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128];                        // Array to store serial string
bool receivedmsg;

#define CAN0_INT 2
#define CS 10
#define MUTUAL_KEY 0x11
#define INITIAL_SEED 0x7F
#define MUTUAL_HANDSHAKE 0xDA

#define DEBUG_MODE 1

int sensorPin = A0;

int CAN_ID = 0x5C;

bool RECV_INT_FLAG = false;
bool RESPONSE_REQUESTED = false;


MCP_CAN CAN0(CS);
MODChaCha cipherchachaTemp;

#define MODE_BUTTON 3

void setup()
{
  uint8_t key[] = {0x0D, 0x0A, 0x01, 0x01, 0x01, 0x0E, 0x01, 0x00};
  uint8_t iv[] = {0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x00,0X01};
  uint8_t counter[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
  bool notready = 1;
  
  Serial.begin(115200);
  Serial.println("Tempeture Node Ready.");

  // Initialize MCP2515 running at 16MHz with a baudrate of 500kb/s and the masks and filters disabled.
  while(!(CAN_OK == CAN0.begin(CAN_500KBPS))) {
    if(CAN0.begin(CAN_500KBPS) == CAN_OK) 
      Serial.println("MCP2515 Initialized Successfully!");
    else Serial.println("Error Initializing MCP2515...");
  }
  
  pinMode(CAN0_INT, INPUT);                            // Configuring pin for /INT input
  pinMode(MODE_BUTTON, INPUT_PULLUP);
  
  attachInterrupt(CAN0_INT, MCP2515_ISR, FALLING); // start interrupt

  delay(50); //Give some time for Recv node to be ready

  //Entering sequence
  
  uint8_t rndm;
  uint8_t rndm2;
  randomSeed(analogRead(A0));

  cipherchachaTemp = MODChaCha(20);                   //Initialize the cypher speed

  cipherchachaTemp.setKey(key,8);
  cipherchachaTemp.setIV(iv,8);

  counter[0] = MUTUAL_HANDSHAKE;
  counter[1] = MUTUAL_KEY;
  
  //First wait till receiver node is ready.
  while(notready){
    if(!digitalRead(CAN0_INT)) {           // If CAN0_INT pin is low, read receive buffer
      CAN0.readMsgBufID(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
      if((rxId & 0x1FFFFFFF) == 2)
      {
        Serial.print("Data received: ");
        //PrintArray(rxBuf,len);
        Serial.println(rxBuf[0]);
        
        Serial.print("Random received: ");
        if((rxBuf[0] > 0) && (rxBuf[0] <= INITIAL_SEED))
          rndm = rxBuf[0] + 127;
        else if((rxBuf[0] > INITIAL_SEED) && ((rxBuf[0] < 2*INITIAL_SEED)))
          rndm = rxBuf[0] - 127;

        Serial.println(rndm);

        rxBuf[0] = rndm ^ MUTUAL_HANDSHAKE;
        rndm2 = random(0,255);
        rxBuf[1] = rndm2 ^ rndm;

        Serial.print("Random sent: ");
        Serial.print(rndm2);
        Serial.println();
        sendmsg(rxBuf, 2); 
        

        int starttime = millis();
        int endtime = starttime;

        bool timesup = 0;

        while (!timesup && notready){          //Wait for START from rec_node
          if(!digitalRead(CAN0_INT)) {           // If CAN0_INT pin is low, read receive buffer
            CAN0.readMsgBufID(&rxId, &len, rxBuf); 
            if((rxId & 0x1FFFFFFF) == 2){       
               if((rxBuf[0] ^ rndm2) == MUTUAL_KEY){ //For now is XOR, maybe in the future is spritz
                  notready = !notready;

               }
            }
          }
          endtime = millis();
          if((endtime - starttime) > 5000)
            timesup = 1;
         } 
      } 
    }
  }

  Serial.println("START received");

  counter[2] = rndm;
  counter[3] = rndm2;

  cipherchachaTemp.setCounter(counter,8);

  
  
}

int pModeButton = true;
bool sendMode = true;
bool pSendMode;

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


void loop()
{
  /*
  int modeButton = digitalRead(MODE_BUTTON);
  if(modeButton == LOW && pModeButton){
    if (DEBUG_MODE) Serial.println("Mode changed.");
    sendMode = !sendMode;
  }

  pSendMode = sendMode;
  if(RECV_INT_FLAG) {
    sendMode = false;
    RECV_INT_FLAG = false;
  }

  if(sendMode || RESPONSE_REQUESTED) {
    sendSensorData();
    delay(100);   // send data per 100ms
  } else {
    receivemsg(); // but always listen in receive mode
    if(receivedmsg && pSendMode) {
      sendMode = true;
    }
  }

  pModeButton = modeButton;
  */

  sendSensorData();
  delay(5000);
  
}

int getSensorData() {
  return analogRead(sensorPin)/8 + 50;
}

void MCP2515_ISR()
{
    RECV_INT_FLAG = true;
}

void sendSensorData() {
  uint8_t data[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
  uint8_t cyphertext[8];
  data[0] = getSensorData();  

  cipherchachaTemp.encrypt(cyphertext,data,8);
  bool success = sendmsg(cyphertext, 8);
  if (DEBUG_MODE) {
    if(success) {
      Serial.println("Sent temperature successfully: ");
      PrintArray(cyphertext,8);
      Serial.print("Counter value: ");
      cipherchachaTemp.getCounter(data);
      PrintArray(data,8);
      Serial.print("Posn: ");
      Serial.println( cipherchachaTemp.getPosn());
    } else {
      Serial.println("Error in sending temperature...");
    }
  }
}

bool sendmsg(uint8_t* buf, int len) {
  // send data:  ID, Standard CAN Frame, Data length = 8 bytes, 'data' = array of data bytes to send
  byte sndStat = CAN0.sendMsgBuf(CAN_ID, 0, len, buf);
  
  if(sndStat == CAN_OK){
    return true;
  } 
  return false;
}

void receivemsg() {
  receivedmsg = false;
  
  if(!digitalRead(CAN0_INT)) {           // If CAN0_INT pin is low, read receive buffer
    CAN0.readMsgBufID(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
    
    if((rxId & 0x80000000) == 0x80000000)     // Determine if ID is standard (11 bits) or extended (29 bits)
      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
    else
      sprintf(msgString, "Standard ID: 0x%.3lX  DLC: %1d  Data:", rxId, len);
    
    Serial.print(msgString);
    
    if((rxId & 0x40000000) == 0x40000000) {    // Determine if message is a remote request frame.
      sprintf(msgString, " REMOTE REQUEST FRAME");
      RESPONSE_REQUESTED = true;
      Serial.print(msgString);
    } else {
      for(byte i = 0; i<len; i++) {
        sprintf(msgString, " 0x%.2X", rxBuf[i]);
        Serial.print(msgString);
      }
    }
    Serial.println();

    receivedmsg = true;
  }
}
