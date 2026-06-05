#include <string.h>
#include <masc.h>

#include "link.h"


void link_init(Link *self, const char *name, Node *tx, Node *rx)
{
    object_init(self, LinkCls);
    if (tx != NULL && rx != NULL) {
        self->id = tx->id + (rx->id << sizeof(self->id) * 4);
    }
    self->name = strdup(name);
    self->tx = tx;
    self->rx = rx;
    self->next_seq_num = 1;
}

void link_destroy(Link *self)
{
    free(self->name);
    object_destroy(self);
}

bool link_send_packet(Link *self, Packet *packet)
{
    packet->link_id = self->id;
    packet->seq_num = self->next_seq_num++;
    packet->tx_time = mloop_run_time();
    size_t size = sizeof(Packet);
    return write(self->tx, packet, size) == size;
}

int link_cmp(const Link *self, const Link *other)
{
    return strcmp(self->name, other->name);
}

size_t link_to_cstr(Link *self, char *cstr, size_t size)
{
    return format(cstr, size, "%s (%O -> %O)", self->name, self->tx, self->rx);
}

static void _vinit(Link *self, va_list va)
{
    char *name = va_arg(va, char *);
    Node *tx = va_arg(va, Node *);
    Node *rx = va_arg(va, Node *);
    link_init(self, name, tx, rx);
}

static void _init_class(class *cls)
{
    cls->super = ObjectCls;
}


static class _LinkCls = {
    .name = "Link",
    .size = sizeof(Link),
    .super = NULL,
    .init_class = _init_class,
    .vinit = (vinit_cb)_vinit,
    .init_copy = (init_copy_cb)object_init_copy,
    .destroy = (destroy_cb)link_destroy,
    .cmp = (cmp_cb)link_cmp,
    .repr = (repr_cb)link_to_cstr,
    .to_cstr = (to_cstr_cb)link_to_cstr,
};

const class *LinkCls = &_LinkCls;
