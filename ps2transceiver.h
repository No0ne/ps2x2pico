#ifndef PS2TRANSCEIVER_H
#define PS2TRANSCEIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/pio.h"
#include "pico/util/queue.h"

typedef struct {
  uint8_t bytes[5];
  int count;
  int sentCount;
  int id;
} MessageToSend;

typedef void (*MessageReceived)(uint8_t message, bool parityIsCorrect);

typedef struct {  
  PIO assignedPio;
  uint stateMachineID;
  uint programHandle;
  MessageReceived messageReceivedCallback;
  queue_t outputBuffer;
  queue_t freeMessages;
  MessageToSend outputMessageStore[16];
  bool messageInFlight;
  bool pauseSending;
} Ps2Transceiver;

void initializePs2Transceiver(Ps2Transceiver* context, PIO pio, uint dataPin, MessageReceived msgRecvCB);

bool sendBytes(Ps2Transceiver* context, uint8_t* toSend, uint count);

bool sendByte(Ps2Transceiver* context, uint8_t toSend);

void runLoopIteration(Ps2Transceiver* context);

void resumeSending(Ps2Transceiver* context);

/*
 Enesure that sending is paused and no messages are in flight prior to calling.
 */
void clearOutputBuffer(Ps2Transceiver* context);

/*
 Enesure that sending is paused and no messages are in flight prior to calling.
 */
void prioritySendBytes(Ps2Transceiver* context, uint8_t* toSend, uint count);

#endif
