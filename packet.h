#ifndef _PACKET_H_
#define _PACKET_H_


typedef struct {
    int sender_id;
    int seq_num;
    int tx_time;
} Packet;


void packet_init(Packet *self);
void packet_destroy(Packet *self);

#endif /* _PACKET_H_ */
