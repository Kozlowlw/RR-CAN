/*
 *Author:Daniel Moreno
 *Single NODE/ECU demo
*/

//Libraries
#include "Crypto/ChaCha.h"
#include "Crypto/ChaCha.cpp"
#include "Crypto/Cipher.h"
#include "Crypto/Cipher.cpp"
#include "Crypto/Crypto.h"
#include "Crypto/Crypto.cpp"
#include "Hash/BLAKE2s/BLAKE2s.h"
#include "Hash/BLAKE2s/BLAKE2s.cpp"
#include "Hash/BLAKE2s/Hash.h"
#include "Hash/BLAKE2s/Hash.cpp"
#include "Hash/SipHashC/siphash.c"
#include <cstdlib>
#include <random>
#include <cmath>
#include <chrono>
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

//Constants
#define ID_RECON 0x02
#define ID_CHALLENGE 0x03
#define ID_WINNER 0x04
#define ID_BEGIN 0x05
#define TIMEOUT_CHALLENGER_micros 10000
#define SOCKET_NAME "can1"


//Global variables
std::random_device rd;
std::mt19937_64 e2(rd());
std::uniform_int_distribution<uint64_t> dist(std::llround(0), std::llround(std::pow(2,63)));
uint8_t rndm[8];

typedef void *(*StateFunc)();
StateFunc statefunc; 
uint8_t const sharedKey[16] = "Senior Projectz";
uint8_t s1[8];
uint8_t s2[8];
ChaCha cipher;//Single node only
BLAKE2s blake;
int canSocket;
struct can_frame rxFrame;

//states
void *sendChallenge();
void *waitChallenger();
void *verifyChallenger();
void *sendAck();
void *recieveCiphertext();

//timing utils
auto startProtocol = std::chrono::high_resolution_clock::now();
auto beginSeq = std::chrono::high_resolution_clock::now();
auto endAuth = std::chrono::high_resolution_clock::now();
auto endKeyAgree = std::chrono::high_resolution_clock::now();
long int rndmTime;

//utils
bool open_port(const char *port){ //returns the socket
    struct ifreq ifr;
    struct sockaddr_can addr;

    /* open socket */
    canSocket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if(canSocket < 0)
    {
        return 0;
    }
    /* convert interface string port into interface index */
    addr.can_family = AF_CAN;
    strcpy(ifr.ifr_name, port);

    if (ioctl(canSocket, SIOCGIFINDEX, &ifr) < 0)
    {
        return 0;
    }
    
    /* setup address for bind */
    addr.can_ifindex = ifr.ifr_ifindex;

    fcntl(canSocket, F_SETFL, O_NONBLOCK);

    if (bind(canSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        return 0;
    }
    
    return 1;

}

bool writeOnCAN(unsigned long id, uint8_t* data, size_t dlc){
    struct can_frame frame;
    int nbytes; 

        
    /* first fill, then send the CAN frame */
    frame.can_id = id;
    frame.can_dlc = dlc; 
    
    memcpy(frame.data, data, frame.can_dlc);

    nbytes = write(canSocket, &frame, sizeof(frame));
    
    if(nbytes < 0)
    {  
       std::cout << "Error sending frame"; 
       return 0;
    }
    
    return 1;
}
bool read_port(){ //This will get ia a thread that is
                  //always reading the CAN-BUS and a flag set if 
                  //ID is contained in reconIDList
  int nbytes = read(canSocket, &rxFrame, sizeof(struct can_frame));

  if (nbytes < 0) {
    //std::cout << "nothing to read" << std::endl;
    return 0;
  }

  /* paranoid check ... */
  if (nbytes < sizeof(struct can_frame)){
    //std::cout << "what I read, was wrong" << std::endl;
    return 0;

  }
  
  return 1;
            
       
}

void getRndm(uint8_t* data, size_t len) {
    uint64_t random = dist(e2);
    memcpy(data, &random, len);
}

void printHex(uint8_t Data) {
  if (Data < 0x10) 
    printf("0");
  printf("%x ", Data);  
}

void printArray(uint8_t Source[], uint8_t LEN) {
  for (int Idx = 0; Idx < LEN; Idx++)
    printHex(Source[Idx]);
  printf("\n");
}

bool array_cmp(uint8_t *a, uint8_t *b, uint8_t len_a, uint8_t len_b){
     uint8_t n;
     
     if (len_a != len_b) 
      return 0;

     for (n=0;n<len_a;n++)
      if (a[n]!=b[n])  
        return 0;
        
     return 1;
}

//define states

void *sendChallenge(){
  //Time utils
  startProtocol = std::chrono::high_resolution_clock::now();
  getRndm(rndm,sizeof(rndm));
  //Time utils
  auto endRndm = std::chrono::high_resolution_clock::now();
  rndmTime = std::chrono::duration_cast<std::chrono::microseconds>(endRndm-startProtocol).count();
  
  writeOnCAN(ID_CHALLENGE,rndm,sizeof(rndm));
  beginSeq = std::chrono::high_resolution_clock::now();


  
  return (void *) waitChallenger;
}

void *waitChallenger(){
  bool arrived = false;
  bool timeout = false;
  unsigned long int timeoutmicros = TIMEOUT_CHALLENGER_micros;
  auto start = std::chrono::high_resolution_clock::now();

  
  while(!(arrived||timeout)){
    auto end = std::chrono::high_resolution_clock::now();
    arrived = read_port();
    timeout = (std::chrono::duration_cast<std::chrono::microseconds>(end-start).count() >= TIMEOUT_CHALLENGER_micros);
  }
  
  
  if(arrived){
    /*debug stuff
    printf("id=0x%x dlc = %d data= ",rxFrame.can_id, rxFrame.can_dlc);
    printArray(rxFrame.data,rxFrame.can_dlc);
    */
    return (void *)verifyChallenger;
  }
  
  //debug stuff
  std::cout << "timeout!" << std::endl;
  
  return (void *)sendChallenge; //just in case something weird happens
}

void *verifyChallenger(){
  uint8_t s1Data[12];
  uint8_t s2Data[12];
  uint32_t IDbuf = ID_WINNER;
  
  //Load s1Data
  memcpy(s1Data,rndm,sizeof(rndm));
  memcpy(s1Data+sizeof(rndm), &rxFrame.can_id, sizeof(rxFrame.can_id));
  
  //calculate s1
  siphash(s1Data, sizeof(s1Data), sharedKey, s1, sizeof(s1));
  
  //verify challenger
  if(!array_cmp(s1,rxFrame.data,sizeof(s1),sizeof(rxFrame.data)))
    return (void *)sendChallenge;
    
  //Load s2Data
  memcpy(s2Data,s1,sizeof(s1));
  memcpy(s2Data+sizeof(s1), &IDbuf, sizeof(IDbuf));
  
  //calculate s2
  siphash(s2Data, sizeof(s2Data), sharedKey, s2, sizeof(s2));

  /*debug stuff
  std::cout << "S1 DATA: ";
  printArray(s1Data,sizeof(s1Data));
  std::cout << "S1: ";
  printArray(s1,sizeof(s1));
  std::cout << "S2 DATA: ";
  printArray(s2Data,sizeof(s2Data));
  std::cout << "S2: ";
  printArray(s2,sizeof(s2));
  */
  return (void*) sendAck;
  
}

void *sendAck(){ //Now is a bit barebones, in multinode he will check if list
                 //is empty, then decide if send another challenge or start 
                 //with deciphering
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

  writeOnCAN(ID_WINNER,s2,sizeof(s2));
  endAuth = std::chrono::high_resolution_clock::now();
  
  return (void *) recieveCiphertext;
}

void *recieveCiphertext(){
  //send 8 ciphered messages and go back to waitChallenge
  uint8_t plaintext[8];
  
  for(uint8_t n = 0; n < 8; n++)
  {
    while(!read_port());
    cipher.decrypt(plaintext,rxFrame.data,sizeof(rxFrame.data));
    
    //debug stuff 
    if(n==0)
      endKeyAgree = std::chrono::high_resolution_clock::now();
    std::cout << "ciphertext: ";
    printArray(rxFrame.data,sizeof(rxFrame.data));
    std::cout << "Plaintext: ";
    printArray(plaintext,sizeof(plaintext));
  }
  
  //Just to do the sequence
  cipher.clear(); 
  
  std::cout << "Coming up with a random took " << rndmTime << " microseconds" << std::endl;
  std::cout << "Authentication took " << std::chrono::duration_cast<std::chrono::microseconds>(endAuth-beginSeq).count() << " microseconds" << std::endl;
  std::cout << "The first message received in " << std::chrono::duration_cast<std::chrono::microseconds>(endKeyAgree-endAuth).count() << " microseconds after authentication" << std::endl;
  std::cout << "The protocol took " << std::chrono::duration_cast<std::chrono::microseconds>(endKeyAgree-startProtocol).count() << " microseconds to initialize encrypted communication" << std::endl;

  std::cout << std::endl;
  std::cout << std::endl;
  sleep(4);



  
  
  return (void *) sendChallenge;
}

//main 



int main(){
  
  //setup
  char socketName[] = SOCKET_NAME; 
  
  if(!open_port(socketName)){
    std::cout << "Error opening socket, closing program" << std::endl;
    return 1;
  }
  
  //state machine
  
  statefunc = sendChallenge; //Beginning state
  
  while(1){statefunc = (StateFunc)(*statefunc)();}
  
  return 0;
}

