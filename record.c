#include "record.h"
#include "cfg.h"


static const char *state_to_cstr[] = {
    [PACKET_UNKNOWN] = "UNKNOWN",
    [PACKET_READY] = "READY",
    [PACKET_MISSING] = "MISSING",
    [PACKET_GOOD] = "GOOD",
    [PACKET_DELAYED] = "DELAYED",
    [PACKET_LOST] = "LOST",
    [PACKET_ERROR] = "ERROR"
};


void record_init(Record *self, Link *link)
{
    self->link = link;
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
    if (!link_send_packet(self->link, &self->packet)) {
        self->packet_state = PACKET_ERROR;
        return false;
    }
    self->packet_state = PACKET_MISSING;
    return true;
}

bool record_receive_packet(Record *self, Node *rx, Packet *packet, int time)
{
    if (self->packet_state == PACKET_MISSING) {
        // Mark packet as received
        self->rx_time = time;
        self->delay = time - packet->tx_time;
        if (self->delay < cfg.packet_delay_threshold) {
            self->packet_state = PACKET_GOOD;
        } else {
            self->packet_state = PACKET_DELAYED;
        }
    } else if (self->packet_state == PACKET_LOST) {
        log_warn("rx: %s -> %s, too late -> already marked as lost (seq: %i, delay: %i ms)!",
                self->link->tx->name, rx->name, packet->seq_num, self->delay);
    } else {
        log_error("rx: %s -> %s, unexpected packet (seq: %i, state: %s)!",
                self->link->tx->name, rx->name, packet->seq_num,
                packet_state_to_cstr(self->packet_state));
        return false;
    }
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