#include <stdio.h>
#include <masc.h>

#include "report.h"


static const char *state_to_cstr[] = {
    [REPORT_TYPE_LOST] = "LOST",
    [REPORT_TYPE_DELAY] = "DELAYED",
    [REPORT_TYPE_UNKNOWN] = "UNKNOWN"
};


void report_init(Report *self, ReportType type, Node *tx, Node *rx)
{
    object_init(self, ReportCls);
    self->type = type;
    self->rx = rx;
    self->tx = tx;
    self->gap_start = -1;
    self->gap_end = -1;
    self->lost_packets = 0;
    self->delayed_packets = 0;
    self->finished = false;
}

void report_destroy(Report *self)
{
}

const char *report_type_to_cstr(Report *self)
{
    if (self->type < 0 || self->type > REPORT_TYPE_UNKNOWN) {
        self->type = REPORT_TYPE_UNKNOWN;
    }
    return state_to_cstr[self->type];
}

void report_add_record(Report *self, Record *record)
{
    // Calculate start time of the gap.
    if (self->gap_start < 0 || record->packet.tx_time < self->gap_start) {
        self->gap_start = record->packet.tx_time;
    }
    // Update packet counts and calculate end time of the gap.
    if (record->packet_state == PACKET_LOST) {
        self->lost_packets++;
    } else if (record->packet_state == PACKET_DELAYED) {
        self->delayed_packets++;
        if (self->gap_end < 0 || record->rx_time < self->gap_end) {
            self->gap_end = record->rx_time;
        }
    }
    record->reported = true;
}

bool report_finish(Report *self, Record *record)
{
    if (self->gap_start < 0) {
        log_error("report: cannot finish report that did not start!");
        return false;
    }
    if (self->gap_end < 0) {
        if (self->gap_start < record->rx_time) {
            self->gap_end = record->rx_time;
        } else {
            log_error("report: gap ends before it starts!");
            return false;
        }
    }
    self->finished = true;
    return true;
}

int report_cmp(const Report *self, const Report *other)
{
    if (self->tx->id > other->tx->id) {
        return 1;
    } else if (self->tx->id < other->tx->id) {
        return -1;
    } else {
        return 0;
    }
}

size_t report_to_cstr(Report *self, char *cstr, size_t size)
{
    const char *tx_name = "?", *rx_name = "?", *tx_rx_fmt = "%s -> %s:";
    if (self->tx != NULL) {
        tx_name = self->tx->name;
        if (self->rx != NULL) {
            rx_name = self->rx->name;
        } else {
            rx_name = self->tx->peer->name;
            tx_rx_fmt = "%s -> (%s):";
        }
    }
    long l = snprintf(cstr, size, tx_rx_fmt, tx_name, rx_name);
    if (self->finished) {
        l += snprintf(cstr + l, max(0, size - l), " %s, gap: %i ms",
                report_type_to_cstr(self), self->gap_end - self->gap_start);
        if (self->lost_packets > 0) {
            l += snprintf(cstr + l, max(0, size - l),
                    ", lost: %i", self->lost_packets);
        }
        if (self->delayed_packets > 0) {
            l += snprintf(cstr + l, max(0, size - l),
                    ", delayed: %i", self->delayed_packets);
        }
    } else {
        l += cstr_ncopy(cstr + l, " ongoing ...", max(0, size - l));
    }
    return l;
}

static void _vinit(Report *self, va_list va)
{
    ReportType type = va_arg(va, ReportType);
    Node *tx = va_arg(va, Node *);
    Node *rx = va_arg(va, Node *);
    report_init(self, type, tx, rx);
}

static void _init_class(class *cls)
{
    cls->super = ObjectCls;
}


static class _ReportCls = {
    .name = "Report",
    .size = sizeof(Report),
    .super = NULL,
    .init_class = _init_class,
    .vinit = (vinit_cb)_vinit,
    .init_copy = (init_copy_cb)object_init_copy,
    .destroy = (destroy_cb)report_destroy,
    .cmp = (cmp_cb)report_cmp,
    .repr = (repr_cb)report_to_cstr,
    .to_cstr = (to_cstr_cb)report_to_cstr,
};

const class *ReportCls = &_ReportCls;
