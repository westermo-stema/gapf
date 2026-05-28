#include <masc.h>

#include "cfg.h"
#include "db.h"


static MlTimer *packet_timer = NULL;


static void stdin_line_cb(MlIoPkg *self, void *data, size_t size, void *arg)
{
}

static void packet_timer_cb(MlTimer *timer, void *arg)
{

    // Prepare packet record
    Node *node = db_get_next_node();
    Record *record = db_new_record(node);
    // Send record
    record_send(record);
    // Analyse incoming packets
    db_analyse_records();
    ml_timer_add(timer, cfg.packet_interval);
}

static void packet_rx_cb(Node *receiver, Packet *p, int rx_time)
{
    // Calculate delay
    int delay = rx_time - p->tx_time;
    log_debug("received: %i -> %i, seq: %i, delay: %i ms",
            receiver->id, p->sender_id, p->seq_num, delay);
    // Lookup correspondig record
    Record *r = db_get_record(p->sender_id, p->seq_num);
    if (r == NULL) {
        // Handle unexpected packet
        Node *sender = db_get_node_by_id(p->sender_id);
        if (sender != NULL) {
            log_warn("%s -> %s: unknown packet (seq: %i, delay: %i ms)!",
                    sender->name, receiver->name, p->seq_num, delay);
        } else {
            log_error("? -> %s: packet from unknown sender (id: %i, seq: %i)!",
                    receiver->name, p->sender_id, p->seq_num);
        }
        return;
    }
    if (r->state == PACKET_SENT) {
        // Mark packet as received
        r->receiver = receiver;
        r->rx_time = rx_time;
        r->delay = delay;
        r->state = PACKET_RECEIVED;
    } else if (r->state == PACKET_LOST) {
        log_warn("%s -> %s: too late -> marked lost (seq: %i, delay: %i ms)!",
                r->sender->name, receiver->name, p->seq_num, delay);
    } else {
        log_error("? -> %s: unexpected packet (id: %i, seq: %i, state: %s)!",
                receiver->name, p->sender_id, p->seq_num,
                record_state_to_cstr(r));
    }
}

int main(int argc, char *argv[])
{
    // Read configuration
    cfg_init(argc, argv);
    // Setup logging
    log_init(cfg.log_level);
    log_add_stdout();
    // Init main loop
    mloop_init();
    // Setup node A and B
    Node *node_a = new(Node, "A", cfg.ip_a, cfg.port);
    Node *node_b = new(Node, "B", cfg.ip_b, cfg.port, packet_rx_cb);
    node_connect(node_a, node_b, packet_rx_cb);
    node_connect(node_b, node_a, packet_rx_cb);
    // Setup test database
    db_init(cfg.packet_records);
    db_add_node(node_a);
    db_add_node(node_b);
    // Setup standard input
    Io input = init(Io, STDIN_FILENO);
    mloop_io_pkg_new(&input, '\n', stdin_line_cb, NULL, NULL);
    // Setup packet timer
    packet_timer = mloop_timer_new(cfg.packet_interval, packet_timer_cb, NULL);
    // Start main loop
    mloop_run();
    // Cleanup everthing
    destroy(&input);
    db_destroy();
    mloop_destroy();
    log_destroy();
    cfg_destroy();
    return 0;
}
