#include <string.h>
#include <stdio.h>
#include <masc.h>

#include "node.h"
#include "packet.h"


static int next_node_id = 1;


void node_init(Node *self, const char *name, const char *ip, int port)
{
    socket_init(self, AF_INET, SOCK_DGRAM, 0);
    self->id = next_node_id++;
    self->name = strdup(name);
    self->ip = strdup(ip);
    self->port = port;
    self->peer = NULL;
    self->state = NODE_DISCONNECTED;
    self->next_seq_num = 1;
    self->cb = NULL;
}

void node_destroy(Node *self)
{
    self->state = NODE_CLOSED;
    free(self->ip);
    free(self->name);
    socket_destroy(self);
}

static void data_rx_cb(MlIo *ml_io, int fd, ml_io_flag_t events, void *arg)
{
    if (!(events & ML_IO_READ))
        return;
    // Measure rx time
    int rx_time = mloop_run_time();
    Node *receiver = (Node *)ml_io->io;
    // Read packet data
    Packet packet;
    while(true) {
        ssize_t len = read(fd, &packet, sizeof(packet));
        if (len == sizeof(packet)) {
            if (receiver->cb != NULL) {
                receiver->cb(receiver, &packet, rx_time);
            }
        } else if (len <= 0) {
            break;
        } else {
            log_warn("node: %s received %i bytes of garbage!",
                    receiver->name, len);
        }
    }
}

bool node_connect(Node *self, Node *peer, packet_cb cb)
{
    if (peer == NULL)
        return false;
    // Register data callback
    if (mloop_io_new(self, ML_IO_READ, data_rx_cb, NULL) == NULL) {
        self->state = NODE_ERROR;
        return false;
    }
    self->cb = cb;
    // Bind socket to local address
    if (!socket_bind(self, self->ip, self->port)) {
        self->state = NODE_BIND_ERROR;
        return false;
    }
    // Connect to peer node
    if (!socket_connect(self, peer->ip, peer->port)) {
        self->state = NODE_CONNECT_ERROR;
    }
    self->peer = peer;
    self->state = NODE_CONNECTED;
    return true;
}

bool node_send_packet(Node *self, Packet *packet)
{
    packet->sender_id = self->id;
    packet->seq_num = self->next_seq_num++;
    packet->tx_time = mloop_run_time();
    size_t size = sizeof(Packet);
    return write(self, packet, size) == size;
}

int node_cmp(const Node *self, const Node *other)
{
    return strcmp(self->name, other->name);
}

size_t node_to_cstr(Node *self, char *cstr, size_t size)
{
    return snprintf(cstr, size, "%s (#%i, %s:%i)",
            self->name, self->id, self->ip, self->port);
}

static void _vinit(Node *self, va_list va)
{
    char *name = va_arg(va, char *);
    char *ip = va_arg(va, char *);
    int port = va_arg(va, int);
    node_init(self, name, ip, port);
}

static void _init_class(class *cls)
{
    cls->super = SocketCls;
}


static io_class _NodeCls = {
    .name = "Node",
    .size = sizeof(Node),
    .super = NULL,
    .init_class = _init_class,
    .vinit = (vinit_cb)_vinit,
    .init_copy = (init_copy_cb)object_init_copy,
    .destroy = (destroy_cb)node_destroy,
    .cmp = (cmp_cb)node_cmp,
    .repr = (repr_cb)node_to_cstr,
    .to_cstr = (to_cstr_cb)node_to_cstr,
    // Io Class
    .get_fd = (get_fd_cb)io_get_fd,
    .__read__ = (read_cb)io_read,
    .readstr = (readstr_cb)io_readstr,
    .readline = (readline_cb)io_readline,
    .__write__ = (write_cb)io_write,
    .__close__ = (close_cb)io_close,
};

const class *NodeCls = &_NodeCls;
