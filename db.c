#include <masc.h>

#include "db.h"
#include "cfg.h"


static List nodes;
static int cur_node;
static Record *records;
static int rec_count;
static int rec_in, rec_out;
static List reports;


void db_init(int num_records)
{
    // Nodes
    nodes = init(List);
    cur_node = 0;
    // Ongoing gap reports
    reports = init(List);
    // Packet records
    rec_count = num_records;
    records = malloc(sizeof(Record) * rec_count);
    rec_in = 0, rec_out = 0;
    for(int i = 0; i < rec_count; i++) {
        records[i].state = PACKET_UNKNOWN;
    }
}

void db_destroy(void)
{
    destroy(&reports);
    destroy(&nodes);
}

void db_add_node(Node *node)
{
    list_append(&nodes, node);
}

Node *db_get_node_by_id(int id)
{
    Node *node = NULL;
    Iter itr = init(Iter, nodes);
    for (Node *n = next(&itr); n != NULL; n = next(&itr)) {
        if (n->id == id) {
            node = n;
            break;
        }
    }
    destroy(&itr);
    return node;
}

Node *db_get_next_node(void)
{
    Node *node = list_get_at(&nodes, cur_node);
    cur_node = (cur_node + 1 < list_len(&nodes)) ? cur_node + 1 : 0;
    return node;
}

static int rec_index_incr(int index)
{
    return (index + 1 < rec_count) ? index + 1 : 0;
}

static int rec_index_decr(int index)
{
    return ((index - 1 >= 0) ? index : rec_count) - 1;
}

static bool rec_emtpy(void)
{
    return rec_in == rec_out;
}

Record *db_new_record(Node *node)
{
    // Calculate next index for incoming record
    int next_in = rec_index_incr(rec_in);
    // Check if ring buffer is full
    if (next_in == rec_out) {
        log_error("db: records ring buffer is full, overwriting records!");
        // Delete last record in the ring buffer.
        record_destroy(records + rec_out);
        rec_out = rec_index_incr(rec_out);
    }
    Record *r = records + rec_in;
    record_init(r, node);
    rec_in = next_in;
    return r;
}

Record *db_get_record(int sender_id, int seq_num)
{
    Record *record = NULL;
    if (rec_emtpy())
        return record;
    // Start by searching the most recent records (should be faster).
    int idx = rec_in;
    while (idx != rec_out) {
        idx = rec_index_decr(idx);
        Record *r = records + idx;
        if (r->sender == NULL) {
            continue;
        }
        // Match the record with the sender and sequence number
        if (r->sender->id == sender_id && r->packet.seq_num == seq_num) {
            record = r;
            break;
        }
    }
    return record;
}

static void report_lost_packet(Record *r)
{
    r->state = PACKET_LOST;
    log_info("%s -> ?: lost packet (seq: %i)!",
            r->sender->name, r->packet.seq_num);
    //TODO: Currently, we report the lost packet (no gap analysis).
    r->reported = true;
}

static void report_delayed_packet(Record *r)
{
    log_info("%s -> %s: delayed packet (seq: %i, delay: %i ms)!",
            r->sender->name, r->receiver->name, r->packet.seq_num, r->delay);
    r->reported = true;
}

static void report_good_packet(Record *r)
{
    r->reported = true;
}

static List *gather_finished_reports(void)
{
    return NULL;
}

List *db_analyse_records(void)
{
    if (rec_emtpy())
        return NULL;
    // Start analysing with the oldest records
    int time = mloop_run_time();
    bool purge_reported = true;
    for (int idx = rec_out; idx != rec_in; idx = rec_index_incr(idx)) {
        Record *r = records + idx;
        // Purge reported records
        if (purge_reported) {
            if (r->reported) {
                continue;
            } else {
                // Do not purge any records after this record ...
                purge_reported = false;
                // .. and advance the out pointer to current index
                rec_out = idx;
            }
        }
        // If a packet was sent more than its lost threshold ago, ...
        if (r->state == PACKET_SENT
                && (time - r->packet.tx_time) > cfg.packet_lost_threshold) {
            // ... report packet as lost.
            report_lost_packet(r);
            continue;
        }
        if (r->state == PACKET_RECEIVED && !r->reported) {
            if (r->delay > cfg.packet_delay_threshold) {
                report_delayed_packet(r);
            } else {
                report_good_packet(r);
            }
        }
    }
    return gather_finished_reports();
}