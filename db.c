#include <masc.h>

#include "db.h"
#include "cfg.h"

// Nodes and their links
static List nodes;
static List links;
static int next_link;
// Record database
static Record *records;
static int rec_count;
static int rec_in, rec_out;
static List reports;


void db_init(int num_records)
{
    // Nodes and 
    nodes = init(List);
    links = init(List);
    next_link = 0;
    // Ongoing gap reports
    reports = init(List);
    // Packet records
    rec_count = num_records;
    records = malloc(sizeof(Record) * rec_count);
    rec_in = 0, rec_out = 0;
    for(int i = 0; i < rec_count; i++) {
        records[i].packet_state = PACKET_UNKNOWN;
    }
}

void db_destroy(void)
{
    destroy(&reports);
    destroy(&links);
    destroy(&nodes);
}

void db_add_node(Node *node)
{
    log_info("db: add node %O", node);
    list_append(&nodes, node);
}

Node *db_get_node_by_id(int id)
{
    Node *node = NULL;
    Iter itr = init(Iter, &nodes);
    for (Node *n = next(&itr); n != NULL; n = next(&itr)) {
        if (n->id == id) {
            node = n;
            break;
        }
    }
    destroy(&itr);
    return node;
}

Node *db_get_node_by_name(const char *name)
{
    Node *node = NULL;
    Iter itr = init(Iter, &nodes);
    for (Node *n = next(&itr); n != NULL; n = next(&itr)) {
        if (cstr_eq(n->name, name)) {
            node = n;
            break;
        }
    }
    destroy(&itr);
    return node;
}

void db_add_link(Link *link)
{
    log_info("db: add link %O", link);
    list_append(&links, link);
}

Link *db_get_next_link(void)
{
    Link *link = list_get_at(&links, next_link);
    next_link = (next_link + 1 < list_len(&links)) ? next_link + 1 : 0;
    return link;
}

Link *db_get_link_by_id(int id)
{
    Link *link = NULL;
    Iter itr = init(Iter, &links);
    for (Link *l = next(&itr); l != NULL; l = next(&itr)) {
        if (l->id == id) {
            link = l;
            break;
        }
    }
    destroy(&itr);
    return link;
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

Record *db_new_record(Link *link)
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
    record_init(r, link);
    rec_in = next_in;
    return r;
}

Record *db_get_record(int link_id, int seq_num)
{
    if (rec_emtpy())
        return NULL;
    // Start by searching the most recent records (should be faster).
    Record *record = NULL;
    int idx = rec_in;
    while (idx != rec_out) {
        idx = rec_index_decr(idx);
        Record *r = records + idx;
        if (r->link == NULL) {
            continue;
        }
        // Match the record with the link and sequence number
        if (r->link->id == link_id && r->packet.seq_num == seq_num) {
            record = r;
            break;
        }
    }
    return record;
}

static Report *start_report(ReportType type, Record *rec)
{
    Report *report = new(Report, type, rec->link);
    // Add new report to database and ...
    list_append(&reports, report);
    // ... add the record to the report.
    report_add_record(report, rec);
    return report;
}

static Report *get_open_report(Link *link)
{
    Report *report = NULL;
    Iter i = init(Iter, &reports);
    for (Report *r = next(&i); r != NULL; r = next(&i)) {
        if (!r->finished && r->link->id == link->id) {
            report = r;
            break;
        }
    }
    destroy(&i);
    return report;
}

static void report_lost_packet(Record *rec)
{
    log_info("%s -> %s: lost packet (seq: %i)!",
            rec->link->tx->name, rec->link->rx->name, rec->packet.seq_num);
    // Look for open report
    Report *report = get_open_report(rec->link);
    if (report != NULL) {
        if (report->type == REPORT_TYPE_LOST) {
            // If the type matches, just add the record to the open report ...
            report_add_record(report, rec);
        } else {
            // ... otherwise finish it and ...
            report_finish(report, rec);
            // ... start a new report for lost packets.
            start_report(REPORT_TYPE_LOST, rec);
        }
    } else {
        start_report(REPORT_TYPE_LOST, rec);
    }
}

static void report_delayed_packet(Record *rec)
{
    log_info("%s -> %s: delayed packet (seq: %i, delay: %i ms)!",
            rec->link->tx->name, rec->link->rx->name,
            rec->packet.seq_num, rec->delay);
    // Look for open report
    Report *report = get_open_report(rec->link);
    if (report != NULL) {
        // If the report is of type rest, ...
        if (report->type == REPORT_TYPE_RELIEVE) {
            // ... finish it and ...
            report_finish(report, rec);
            // ... start a new report for delayed packets.
            start_report(REPORT_TYPE_DELAY, rec);
        } else {
            // ... otherwise the type does not matter, just add the record.
            report_add_record(report, rec);
        }
    } else {
        start_report(REPORT_TYPE_DELAY, rec);
    }
}

static void report_good_packet(Record *rec)
{
    Report *report = get_open_report(rec->link);
    if (report != NULL) {
        // If the open report is of type relieve ...
        if (report->type == REPORT_TYPE_RELIEVE) {
            // ... check that the relieve time is still in range ...
            if (rec->rx_time - report->start < cfg.report_relieve_threshold) {
                report_add_record(report, rec);
            } else {
                // ... otherwise the relieve report becomes a link ok report.
                report->type = REPORT_TYPE_LINK_OK;
                report_finish(report, rec);
            }
        } else {
            // ... otherwise finish it and ...
            report_finish(report, rec);
            // ... start a new report for relieve packets.
            start_report(REPORT_TYPE_RELIEVE, rec);
        }
    }
    rec->reported = true;
}

static List *gather_finished_reports(void)
{
    List *finished_reports = NULL;
    Iter i = init(Iter, &reports);
    for (Report *r = next(&i); r != NULL; r = next(&i)) {
        if (r->finished) {
            if (!list_remove(&reports, r)) {
                continue;
            }
            // If this is the first report ...
            if (finished_reports == NULL) {
                // ... create a list for the reports.
                finished_reports = new(List);
            }
            // Append the found report to the list.
            list_append(finished_reports, r);
        }
    }
    destroy(&i);
    return finished_reports;
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
        // If a packet is missing ...
        if (r->packet_state == PACKET_MISSING) {
            if ((time - r->packet.tx_time) > cfg.packet_lost_threshold) {
                // ... more than its lost threshold ago, mark it as lost and ...
                r->packet_state = PACKET_LOST;
                // ... report it.
                report_lost_packet(r);
                continue;
            } else {
                // ... but not received yet, do no further analysis.
                break;
            }
        } else if (r->packet_state == PACKET_GOOD) {
            report_good_packet(r);
        } else if (r->packet_state == PACKET_DELAYED) {
            report_delayed_packet(r);
        }
    }
    return gather_finished_reports();
}