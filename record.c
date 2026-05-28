#include "record.h"


static const char *state_to_cstr[] = {
    [PACKET_UNKNOWN] = "UNKNOWN",
    [PACKET_READY] = "READY",
    [PACKET_SENT] = "SENT",
    [PACKET_RECEIVED] = "RECEIVED",
    [PACKET_LOST] = "LOST",
    [PACKET_ERROR] = "ERROR"
};


void record_init(Record *self, Node *sender)
{
    self->state = PACKET_READY;
    self->sender = sender;
    self->receiver = NULL;
    packet_init(&self->packet);
    self->rx_time = -1;
    self->delay = -1;
    self->reported = false;
}

void record_destroy(Record *self)
{
    packet_destroy(&self->packet);
    self->state = PACKET_UNKNOWN;
}

const char *record_state_to_cstr(Record *self)
{
    if (self->state < 0 || self->state > PACKET_ERROR) {
        self->state = PACKET_UNKNOWN;
    }
    return state_to_cstr[self->state];
}

bool record_send(Record *self)
{
    if (self->state != PACKET_READY) {
        return false;
    }
    if (!node_send_packet(self->sender, &self->packet)) {
        self->state = PACKET_ERROR;
        return false;
    }
    self->state = PACKET_SENT;
    return true;
}