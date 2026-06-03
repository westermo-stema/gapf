#include "record.h"


static const char *state_to_cstr[] = {
    [PACKET_UNKNOWN] = "UNKNOWN",
    [PACKET_READY] = "READY",
    [PACKET_MISSING] = "MISSING",
    [PACKET_GOOD] = "GOOD",
    [PACKET_DELAYED] = "DELAYED",
    [PACKET_LOST] = "LOST",
    [PACKET_ERROR] = "ERROR"
};


void record_init(Record *self, Node *tx)
{
    self->tx = tx;
    self->rx = NULL;
    packet_init(&self->packet);
    self->packet_state = PACKET_READY;
    self->rx_time = -1;
    self->delay = -1;
    self->reported = false;
}

void record_destroy(Record *self)
{
    packet_destroy(&self->packet);
    self->packet_state = PACKET_UNKNOWN;
}

bool record_send_packet(Record *self)
{
    if (self->packet_state != PACKET_READY) {
        return false;
    }
    if (!node_send_packet(self->tx, &self->packet)) {
        self->packet_state = PACKET_ERROR;
        return false;
    }
    self->packet_state = PACKET_MISSING;
    return true;
}



bool record_packet_received(Record *self)
{
    return self->rx_time >= 0;
}

const char *packet_state_to_cstr(PacketState state)
{
    if (state < 0 || state > PACKET_ERROR) {
        state = PACKET_UNKNOWN;
    }
    return state_to_cstr[state];
}