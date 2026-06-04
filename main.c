#include <masc.h>

#include "cfg.h"
#include "db.h"


static MlTimer *packet_timer = NULL;


static void stdin_line_cb(MlIoPkg *self, void *data, size_t size, void *arg)
{
}

static void print_reports(List *reports)
{
    Iter i = init(Iter, reports);
    for (Report *r = next(&i); r != NULL; r = next(&i)) {
        print(" * %O\n", r);
    }
    destroy(&i);
}

static void packet_timer_cb(MlTimer *timer, void *arg)
{

    // Prepare packet record
    Node *node = db_get_next_node();
    Record *record = db_new_record(node);
    // Send record
    record_send_packet(record);
    // Analyse incoming packets
    List *reports = db_analyse_records();
    // Print reports
    if (reports != NULL) {
        print_reports(reports);
    }
    ml_timer_add(timer, cfg.packet_interval);
}

static void packet_rx_cb(Node *rx, Packet *packet, int rx_time)
{
    // Calculate delay
    int delay = rx_time - packet->tx_time;
    log_debug("rx: %i -> %i, seq: %i, delay: %i ms",
            rx->id, packet->tx_id, packet->seq_num, delay);
    // Lookup correspondig record
    Record *record = db_get_record(packet->tx_id, packet->seq_num);
    if (record == NULL) {
        // Handle unexpected packet
        Node *tx = db_get_node_by_id(packet->tx_id);
        if (tx != NULL) {
            log_warn("rx: %s -> %s, unknown packet (seq: %i, delay: %i ms)!",
                    tx->name, rx->name, packet->seq_num, delay);
        } else {
            log_error("rx: ? -> %s, packet from unknown sender (id: %i, seq: %i)!",
                    rx->name, packet->tx_id, packet->seq_num);
        }
        return;
    }
    record_receive_packet(record, rx, packet, rx_time);
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
    Node *node_b = new(Node, "B", cfg.ip_b, cfg.port);
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
