#ifndef _PACKET_H_
#define _PACKET_H_

#include <stdint.h>


typedef struct {
    uint16_t link_id;
    int seq_num;
    int tx_time;
} Packet;


void packet_init(Packet *self);
void packet_destroy(Packet *self);

#endif /* _PACKET_H_ */
