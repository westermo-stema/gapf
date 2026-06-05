#ifndef _LINK_H_
#define _LINK_H_

#include <masc/object.h>

#include "node.h"


typedef struct Link {
    Object;
    uint16_t id;
    char *name;
    Node *tx;
    Node *rx;
    int next_seq_num;
} Link;


extern const class *LinkCls;


void link_init(Link *self, const char *name, Node *tx, Node *rx);
void link_destroy(Link *self);

bool link_send_packet(Link *self, Packet *packet);

int link_cmp(const Link *self, const Link *other);

size_t link_to_cstr(Link *self, char *cstr, size_t size);

#endif /* _LINK_H_ */
