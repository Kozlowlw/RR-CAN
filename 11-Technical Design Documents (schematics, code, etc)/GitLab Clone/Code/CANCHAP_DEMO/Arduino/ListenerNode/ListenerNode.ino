#include <SPI.h>
#include <MODChaCha.h>
#include <crypto.h>
#include <mcp_can.h>

#define CS 10
#define CAN0_INT 2


MCP_CAN CAN0(CS);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  // Initialize MCP2515 running at 16MHz with a baudrate of 500kb/s and the masks and filters disabled.
  while(!(CAN_OK == CAN0.begin(CAN_500KBPS))) {
    if(CAN0.begin(CAN_500KBPS) == CAN_OK) 
      Serial.println("MCP2515 Initialized Successfully!");
    else Serial.println("Error Initializing MCP2515...");
  }
  Serial.println();
  pinMode(CAN0_INT, INPUT);                            // Configuring pin for /INT input
  
  Serial.println("Listening...");

}

void loop() {
  // put your main code here, to run repeatedly:
  receivemsg();
}

void receivemsg() {
  long unsigned int rxId;
  unsigned char len = 0;
  unsigned char rxBuf[8];
  char msgString[128];                        // Array to store serial string

  
  if(!digitalRead(CAN0_INT)) {
    CAN0.readMsgBufID(&rxId, &len, rxBuf);


    // Determine if ID is standard (11 bits) or extended (29 bits)
     if((rxId & 0x80000000) == 0x80000000)     
      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
    else
      sprintf(msgString, "Standard ID: 0x%.3lX  DLC: %1d  Data:", rxId, len);
    
    Serial.print(msgString);


    // Determine if message is a remote request frame.
    if((rxId & 0x40000000) == 0x40000000) {    
      sprintf(msgString, " REMOTE REQUEST FRAME");
      Serial.print(msgString);
    } else {
      for(byte i = 0; i<len; i++) {
        sprintf(msgString, " 0x%.2X", rxBuf[i]);
        Serial.print(msgString);
      }
    }
    Serial.println();
  }
}

