#ifndef _NODE_H_
#define _NODE_H_

#include <masc/socket.h>

#include "packet.h"


typedef enum {
    NODE_CONNECTED,
    NODE_DISCONNECTED,
    NODE_CLOSED,
    NODE_BIND_ERROR,
    NODE_CONNECT_ERROR,
    NODE_ERROR
} NodeState;

typedef struct Node Node;

typedef void (*packet_cb)(Node *rx, Packet *packet, int rx_time);

struct Node {
    Socket;
    int id;
    char *name;
    char *ip;
    int port;
    Node *peer;
    NodeState state;
    int next_seq_num;
    packet_cb cb;
};


extern const class *NodeCls;


void node_init(Node *self, const char *name, const char *ip, int port);
void node_destroy(Node *self);

bool node_connect(Node *self, Node *peer, packet_cb cb);
bool node_send_packet(Node *self, Packet *packet);

int node_cmp(const Node *self, const Node *other);

size_t node_to_cstr(Node *self, char *cstr, size_t size);

#endif /* _NODE_H_ */
