#include <mcp_can.h>
#include <SPI.h>

long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128];                        // Array to store serial string
bool receivedmsg;

#define CAN0_INT 2
#define CS 10

#define DEBUG_MODE 1

int sensorPin = A0;

int errorID = 0x001;

int CAN_ID = 0x00C;

bool RECV_INT_FLAG = false;
bool RESPONSE_REQUESTED = false;
bool ID_CONFLICT = false;

MCP_CAN CAN0(CS);

#define MODE_BUTTON 3

void setup()
{
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
    delay(100);   // send data per 100ms
  } else {
    receivemsg(); // but always listen in receive mode
    if(receivedmsg && pSendMode) {
      sendMode = true;
    }

    if(ID_CONFLICT) {
      Serial.println("Our ID was detected!! Error!!");
      sendError();
      Serial.println("Sent an error message");
    }
  }

  pModeButton = modeButton;
}

int getSensorData() {
  return analogRead(sensorPin)*12;
}

void MCP2515_ISR()
{
    RECV_INT_FLAG = true;
}

void sendSensorData() {
  char data[2];
  int rpm = getSensorData();
  data[1] = (rpm & 0xFF00) >> 8;
  data[0] = (rpm & 0x00FF);
  bool success = sendmsg(CAN_ID, data, 2);
  if (DEBUG_MODE) {
    if(success) {
      Serial.println("Sent rpm successfully");
    } else {
      Serial.println("Error in sending rpm...");
    }
  }
}

void sendError() {
  char data[2];
  data[1] = 0x01;
  data[0] = 0x01;

  bool success = sendmsg(errorID, data, 2);
  if (true) {
    if(success) {
      Serial.println("Sent error successfully");
    } else {
      Serial.println("Error in sending error...");
    }
  }
}

bool sendmsg(int CAN_ID, char* buf, int len) {
  // send data:  ID = 0x100, Standard CAN Frame, Data length = 8 bytes, 'data' = array of data bytes to send
  byte sndStat = CAN0.sendMsgBuf(CAN_ID, 0, len, (const byte*) buf);
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
