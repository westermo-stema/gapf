#ifndef _RECORD_H_
#define _RECORD_H_

#include "node.h"
#include "packet.h"


typedef enum {
    PACKET_UNKNOWN = 0,
    PACKET_READY,
    PACKET_MISSING,
    PACKET_GOOD,
    PACKET_DELAYED,
    PACKET_LOST,
    PACKET_ERROR
} PacketState;

typedef struct {
    Node *tx;
    Node *rx;
    Packet packet;
    PacketState packet_state;
    int rx_time;
    int delay;
    bool reported;
} Record;


void record_init(Record *self, Node *tx);
void record_destroy(Record *self);

bool record_send_packet(Record *self);


bool record_packet_received(Record *self);


const char *packet_state_to_cstr(PacketState state);

#endif /* _RECORD_H_ */
