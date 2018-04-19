#include <MODChaCha.h>
#include <crypto.h>
#include <mcp_can.h>
#include <SPI.h>

long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128];                        // Array to store serial string
bool receivedmsg;
MODChaCha cipherchacha;                        // Cypher

#define CAN0_INT 2
#define CS 10

#define DEBUG_MODE 1

int sensorPin = A0;
int brakePin = A1;

int speedID = 0x100;
int brakeID = 0x101;

bool RECV_INT_FLAG = false;
bool RESPONSE_REQUESTED = false;

MCP_CAN CAN0(CS);

#define MODE_BUTTON 3

void setup()
{
  uint8_t key[] = {0x0D, 0x0A, 0x01, 0x01, 0x01, 0x0E, 0x01, 0x00};
  uint8_t iv[] = {0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x00,0X01};
  uint8_t counter[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
  
  Serial.begin(115200);
  Serial.println("Ready.");

  // Initialize MCP2515 running at 16MHz with a baudrate of 500kb/s and the masks and filters disabled.
  while(!(CAN_OK == CAN0.begin(CAN_500KBPS))) {
    if(CAN0.begin(CAN_500KBPS) == CAN_OK) 
      Serial.println("MCP2515 Initialized Successfully!");
    else Serial.println("Error Initializing MCP2515...");
  }
  
  pinMode(CAN0_INT, INPUT);                            // Configuring pin for /INT input
  pinMode(MODE_BUTTON, INPUT_PULLUP);
  
  attachInterrupt(CAN0_INT, MCP2515_ISR, FALLING); // start interrupt

  cipherchacha = MODChaCha(20);                   //Initialize the cypher

  cipherchacha.setKey(key,8);
  cipherchacha.setIV(iv,8);
  cipherchacha.setCounter(counter,8);
  
}

byte data[8] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
int pModeButton = true;
bool sendMode = true;
bool pSendMode;

void loop()
{
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
    Serial.println("Sent data!");
    delay(100);   // send data per 100ms
  } else {
    receivemsg(); // but always listen in receive mode
    if(receivedmsg && pSendMode) {
      sendMode = true;
    }
  }

  pModeButton = modeButton;
}

int getSpeedData() {
  return analogRead(sensorPin) / 4;
}

int getBrakeData() {
  return digitalRead(brakePin);
}

void MCP2515_ISR()
{
    RECV_INT_FLAG = true;
}

void sendSensorData() {
  char data[8] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}; // Modified this to be 8 bytes long.
  char ciphermsg[8];
  
  data[0] = getSpeedData();
  cipherchacha.encrypt(ciphermsg,data,8);
  
  bool success = sendmsg(speedID, data, 1);
  
  data[0] = getBrakeData();
  cipherchacha.encrypt(ciphermsg,data,8);

  success = success && sendmsg(brakeID, data, 1);
  if (true) {
    if(success) {
      Serial.println("Sent temperature successfully");
    } else {
      Serial.println("Error in sending temperature...");
    }
  }
}

void sendSpeedData() {
  char data[8] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}; // Modified this to be 8 bytes long.
  data[0] = getSpeedData();
  char ciphermsg[8];

  cipherchacha.encrypt(ciphermsg,data,8);
  
  bool success = sendmsg(speedID, data, 1);
  if (DEBUG_MODE) {
    if(success) {
      Serial.println("Sent speed successfully");
    } else {
      Serial.println("Error in sending speed...");
    }
  }
}

void sendBrakeData() {
  char data[8] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}; // Modified this to be 8 bytes long.
  char ciphermsg[8];
  data[0] = (char)getBrakeData();

  cipherchacha.encrypt(ciphermsg,data,8);
  
  bool success = sendmsg(brakeID, data, 1);
  if (DEBUG_MODE) {
    if(success) {
      Serial.println("Sent brake info successfully");
    } else {
      Serial.println("Error in sending brake info...");
    }
  }
}

bool sendmsg(int CAN_ID, char* buf, int len) {
  // send data:  ID = 0x100, Standard CAN Frame, Data length = 8 bytes, 'data' = array of data bytes to send

  cipherchacha.encrypt(ciphermsg,data,8);
  
  byte sndStat = CAN0.sendMsgBuf(CAN_ID, 0, len, data);
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
