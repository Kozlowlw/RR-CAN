#include <mcp_can.h>
#include <SPI.h>

#define CAN0_INT 2
#define CS 10

#define SEND_DELAY  50
#define CYCLE_DELAY 2

#define DEBUG_MODE  1
#define HARDENED    1

MCP_CAN CAN0(CS);
long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128];                        // Array to store serial string
bool receivedmsg;

bool ID_CONFLICT = false;

int sendtimer = SEND_DELAY;

int sensorPin = A0;
int brakePin = A1;

int errorID = 0x001;

int speedID = 0x00D;
int brakeID = 0x0FC;


char getSpeedData() {
  return analogRead(sensorPin) / 4;
}

char getBrakeData() {
  return digitalRead(brakePin);
}


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
  pinMode(brakePin, INPUT);
  //attachInterrupt(CAN0_INT, MCP2515_ISR, FALLING); // start interrupt
}

void loop()
{
  if (CAN0.checkReceive()) {
    receivemsg();
    if (ID_CONFLICT && HARDENED) {
      Serial.println("Someone else is using our id...?");
      senderror();
    }
  }
  if (sendtimer == 0) {
    sendData();
    sendtimer = SEND_DELAY;
  }
  sendtimer--;
  delay(CYCLE_DELAY);
}

void receivemsg() {
  receivedmsg = false;
  ID_CONFLICT = false;
  if(!digitalRead(CAN0_INT)) {           // If CAN0_INT pin is low, read receive buffer
    CAN0.readMsgBufID(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
    
    if((rxId & 0x80000000) == 0x80000000)     // Determine if ID is standard (11 bits) or extended (29 bits)
      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
    else
      sprintf(msgString, "Standard ID: 0x%.3lX  DLC: %1d  Data:", rxId, len);
    
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

    if (rxId == speedID || rxId == brakeID)
      ID_CONFLICT = true;

    receivedmsg = true;
  }
}
void senderror() {
  char data[2];
  data[1] = 0x00;
  data[0] = 0x0d;

  bool success = sendmsg(errorID, data, 2);
  if (true) {
    if(success) {
      Serial.println("Sent error successfully");
    } else {
      Serial.println("Error in sending error...");
    }
  }
}

void sendData() {
  char data[1];
  bool success;

  data[0] = getSpeedData();
  success = sendmsg(speedID, data, 1);

  data[0] = getBrakeData();
  success = success && sendmsg(brakeID, data, 1);

  if(success) {
    Serial.println("Sent data successfully");
  } else {
    Serial.println("Error in sending data...");
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
