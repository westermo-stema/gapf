#include "packet.h"


void packet_init(Packet *self)
{
    self->tx_id = -1;
    self->seq_num = -1;
    self->tx_time = -1;
}

void packet_destroy(Packet *self)
{
}