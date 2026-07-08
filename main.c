#include <masc.h>

#include "cfg.h"
#include "db.h"
#include "output.h"
#include "group_report.h"


static MlTimer *packet_timer = NULL;
static GroupReport group_report;


static void process_reports(List *reports)
{
    Iter i = init(Iter, reports);
    for (Report *r = next(&i); r != NULL; r = next(&i)) {
        if (cfg.report_grouping) {
            group_report_add(&group_report, r);
            // If all links in the current group are ok ...
            if (group_report_links_ok(&group_report)) {
                // ... print the group and ...
                print_group(&group_report);
                // ... start a new group report.
                group_report_reset(&group_report);
            }
        } else {
            // Skip link ok reports
            if (r->type == REPORT_TYPE_LINK_OK)
                continue;
            print_report(r);
        }
    }
    destroy(&i);
}

static void packet_timer_cb(MlTimer *timer, void *arg)
{
    // Prepare packet record
    Link *link = db_get_next_link();
    if (link == NULL) {
        log_error("no link defined to send data!");
        mloop_stop();
        return;
    }
    Record *record = db_new_record(link);
    // Send record
    if(!record_send_packet(record)) {
        log_warn("could not send packet (seq: %i)!", record->packet.seq_num);
    }
    // Analyse incoming packets
    List *reports = db_analyse_records();
    if (reports != NULL) {
        process_reports(reports);
    }
    ml_timer_add(timer, cfg.packet_interval);
}

static void packet_rx_cb(Node *rx, Packet *packet, int rx_time)
{
    // Calculate delay
    int delay = rx_time - packet->tx_time;
    log_debug("rx: link: 0x%04x, seq: %i, delay: %i ms",
            packet->link_id, packet->seq_num, delay);
    // Lookup correspondig record
    Record *record = db_get_record(packet->link_id, packet->seq_num);
    if (record == NULL) {
        // Handle unexpected packet
        Link *link = db_get_link_by_id(packet->link_id);
        if (link != NULL) {
            log_warn("rx: %s -> %s, unknown packet (seq: %i, delay: %i ms)!",
                    link->tx->name, link->rx->name, packet->seq_num, delay);
        } else {
            log_error("rx: ? -> %s, packet from unknown sender (link: 0x%04x, seq: %i)!",
                    rx->name, packet->link_id, packet->seq_num);
        }
        return;
    }
    record_receive_packet(record, rx, packet, rx_time);
}

static void stdin_line_cb(MlIoPkg *self, void *data, size_t size, void *arg)
{
}

static void setup_nodes(void)
{
    for (int i = 0; i < CFG_NUM_NODES; i++) {
        CfgNode *n = &cfg.nodes[i];
        Node *node = new(Node, n->name, n->ip, n->port);
        // Register packet callback
        if (node_receive_packets(node, packet_rx_cb)) {
            db_add_node(node);
        } else {
            log_error("unable to create %O!", node);
            delete(node);
        }
    }
}

static void setup_links(void)
{
    for (int i = 0; i < cfg.num_links; i++) {
        const char *name = cfg.links[i].name;
        Node *_get_node(const char *node_name) {
            Node *n = db_get_node_by_name(node_name);
            if (n == NULL) {
                log_error("unable to find node '%s' to create link '%s'!",
                        node_name, name);
            }
            return n;
        }
        Node *tx = _get_node(cfg.links[i].tx_name);
        Node *rx = _get_node(cfg.links[i].rx_name);
        if (tx == NULL || rx == NULL) {
            continue;
        }
        Link *link = new(Link, name, tx, rx);
        if (node_connect(tx, rx)) {
            db_add_link(link);
            group_report_add_link(&group_report, link->id);
        } else {
            log_error("unable to create %O!", link);
            delete(link);
        }
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
    // Setup test database
    db_init(cfg.packet_records);
    // Setup group reporting
    group_report_init(&group_report);
    // Setup nodes and their links
    setup_nodes();
    setup_links();
    // Setup standard input
    Io input = init(Io, STDIN_FILENO);
    mloop_io_pkg_new(&input, '\n', stdin_line_cb, NULL, NULL);
    // Setup packet timer
    packet_timer = mloop_timer_new(cfg.packet_interval, packet_timer_cb, NULL);
    // Print environment
    print_env();
    // Start main loop
    mloop_run();
    // Cleanup everthing
    group_report_destroy(&group_report);
    destroy(&input);
    db_destroy();
    mloop_destroy();
    log_destroy();
    cfg_destroy();
    return 0;
}
