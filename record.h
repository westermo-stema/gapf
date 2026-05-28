#ifndef _RECORD_H_
#define _RECORD_H_

#include "node.h"
#include "packet.h"


typedef enum {
    PACKET_UNKNOWN = 0,
    PACKET_READY,
    PACKET_SENT,
    PACKET_RECEIVED,
    PACKET_LOST,
    PACKET_ERROR
} RecordState;

typedef struct {
    RecordState state;
    Node *sender;
    Node *receiver;
    Packet packet;
    int rx_time;
    int delay;
    bool reported;
} Record;


void record_init(Record *self, Node *sender);
void record_destroy(Record *self);

const char *record_state_to_cstr(Record *self);

bool record_send(Record *self);

#endif /* _RECORD_H_ */
