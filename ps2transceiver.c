#include "ps2transceiver.h"

#include "ps2device.pio.h"
#include <string.h>
#include "logger.h"

uint16_t ps2_frame(uint8_t data);
void handleSendComplete(Ps2Transceiver* context);
void handleSendFailed(Ps2Transceiver* context);
bool checkParity(uint32_t message);

void initializePs2Transceiver(Ps2Transceiver* context, PIO pio, uint dataPin, MessageReceived msgRecvCB) {
  context->assignedPio = pio;
  context->stateMachineID = pio_claim_unused_sm(pio, true);
  context->programHandle = pio_add_program(pio, &ps2dev_program);
  context->messageReceivedCallback = msgRecvCB;
  memset(&context->outputBuffer, 0, sizeof(queue_t));
  queue_init(&context->outputBuffer, sizeof(int), 16);
  memset(&context->freeMessages, 0, sizeof(queue_t));
  queue_init(&context->freeMessages, sizeof(int), 16);
  memset(context->outputMessageStore, 0, 16 * sizeof(MessageToSend));
  for (int i=0; i < 16; i++) {
    queue_add_blocking(&context->freeMessages, &i);
  }

  for (int i=0; i < 16; i++) {
    context->outputMessageStore[i].id = i;
  }

  context->messageInFlight = false;
  context->pauseSending = false;
  
  ps2dev_program_init(pio, context->stateMachineID, context->programHandle, dataPin);
  debug_print("Transceiver initialized clock pin %d, data pin %d\n", dataPin + 1, dataPin);
}

bool sendBytes(Ps2Transceiver* context, uint8_t* toSend, uint count) {
  int freeId;
  if (!queue_try_remove(&context->freeMessages, &freeId)) {
    return false;
  }

  MessageToSend *cur = &context->outputMessageStore[freeId];
  for (int i=0; i < count; i++) {
    cur->bytes[i] = toSend[i];
  }

  cur->count = count;
  cur->sentCount = 0;

  queue_add_blocking(&context->outputBuffer, &freeId);
}

bool sendByte(Ps2Transceiver* context, uint8_t toSend) {
  int freeId;
  if (!queue_try_remove(&context->freeMessages, &freeId)) {
    return false;
  }

  MessageToSend *cur = &context->outputMessageStore[freeId];
  cur->bytes[0] = toSend;

  cur->count = 1;
  cur->sentCount = 0;
  
  queue_add_blocking(&context->outputBuffer, &freeId);

  debug_print("Enqueued for send %01x to buffer %d\n", toSend, freeId);
}

void runLoopIteration(Ps2Transceiver* context) {
  int curId;
  if (!context->pauseSending && !context->messageInFlight && queue_try_peek(&context->outputBuffer, &curId)) {
    debug_print("Found work to send in buffer %d.\n", curId);
    context->messageInFlight = true;
    
    // We have data to send, but nothing in flight.
    MessageToSend *cur = &context->outputMessageStore[curId];
    for(int i=0; i < cur->count; i++) {
      debug_print("Sending %01x\n", cur->bytes[i]);
      pio_sm_put(context->assignedPio, context->stateMachineID, ps2_frame(cur->bytes[i]));
    }    
  }

  if(!pio_sm_is_rx_fifo_empty(context->assignedPio, context->stateMachineID)) {
    uint32_t received = pio_sm_get(context->assignedPio, context->stateMachineID);    
    debug_print("Raw received %08x\n", received);
    received = received >> 22;    
    if (received == 0x3ff) {
      debug_print("Received %08x, notification that send completed.\n", received);
      // We received a send completed message.
      handleSendComplete(context);
    } else if (received == 0x3C0) {
      debug_print("Received %08x, notification that send failed.\n", received);
      // We received a failure to send.
      handleSendFailed(context);
    } else if (received != 0 && context->messageReceivedCallback != NULL) {
      // Process as a normal message.
      debug_print("Received messge %08x, parity is %s\n", received, checkParity(received) ? "good" : "bad");
      (*context->messageReceivedCallback)(received, checkParity(received));
    }    
  }
}

void resumeSending(Ps2Transceiver* context) {
  context->pauseSending = false;
}

void clearOutputBuffer(Ps2Transceiver* context) {
  int curId;
  while (queue_try_remove(&context->outputBuffer, &curId)) {
    queue_add_blocking(&context->freeMessages, &curId);
  }
}

void prioritySendBytes(Ps2Transceiver* context, uint8_t* toSend, uint count) {
  int freeId;
  if (!queue_try_remove(&context->freeMessages, &freeId)) {
    return;
  }

  MessageToSend *cur = &context->outputMessageStore[freeId];
  for (int i=0; i < count; i++) {
    cur->bytes[i] = toSend[i];
  }

  cur->count = count;
  cur->sentCount = 0;

  queue_add_blocking(&context->outputBuffer, &freeId);

  // Rotate queue until freeId is at front.
  int topId;
  while (queue_try_peek(&context->outputBuffer, &topId) && topId != freeId) {
    queue_remove_blocking(&context->outputBuffer, &topId);
    queue_add_blocking(&context->outputBuffer, &topId);
  }
}

bool checkParity(uint32_t message) {
  uint8_t parity = 1;
  for(uint8_t i = 0; i < 8; i++) {
    parity = parity ^ (message >> i & 1);
  }

  return parity == (message & 0x100) >> 8;
}

uint16_t ps2_frame(uint8_t data) {
  uint8_t parity = 1;
  for(uint8_t i = 0; i < 8; i++) {
    parity = parity ^ (data >> i & 1);
  }
  
  return ((1 << 10) | (parity << 9) | (data << 1)) ^ 0x7ff;
}

void handleSendComplete(Ps2Transceiver* context) {
  int curId;
  if (queue_try_peek(&context->outputBuffer, &curId)) {
    MessageToSend *cur = &context->outputMessageStore[curId];
    cur->sentCount++;
    if (cur->sentCount == cur->count) {
      queue_remove_blocking(&context->outputBuffer, &curId);
      queue_add_blocking(&context->freeMessages, &curId);
      context->messageInFlight = false;
    }
  }
}

void handleSendFailed(Ps2Transceiver* context) {
  int curId;
  if (queue_try_peek(&context->outputBuffer, &curId)) {
    MessageToSend *cur = &context->outputMessageStore[curId];
    // We failed to send the message.    
    cur->sentCount = 0;
    context->messageInFlight = false;
    // Pause sending so in comming command can be processed first.
    context->pauseSending = true;
  }
}
