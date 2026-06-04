#include <stdio.h>
#include <masc.h>

#include "report.h"


static const char *state_to_cstr[] = {
    [REPORT_TYPE_LOST] = "LOST",
    [REPORT_TYPE_DELAY] = "DELAYED",
    [REPORT_TYPE_RELIEVE] = "RELIEVE",
    [REPORT_TYPE_LINK_OK] = "LINK OK",
    [REPORT_TYPE_UNKNOWN] = "UNKNOWN"
};


void report_init(Report *self, ReportType type, Node *tx, Node *rx)
{
    object_init(self, ReportCls);
    self->type = type;
    self->rx = rx;
    self->tx = tx;
    self->start = -1;
    self->end = -1;
    self->good_packets = 0;
    self->delayed_packets = 0;
    self->lost_packets = 0;
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
    // Calculate start time of the report.
    if (self->start < 0) {
        self->start = record->packet.tx_time;
    }
    // Update packet counts and calculate end time of the report.
    if (record->packet_state == PACKET_LOST) {
        self->lost_packets++;
    } else if (record->packet_state == PACKET_DELAYED) {
        if (self->end < 0 || record->rx_time < self->end) {
            self->end = record->rx_time;
        }
        self->delayed_packets++;
    } else if (record->packet_state == PACKET_GOOD) {
        self->end = record->rx_time;
        self->good_packets++;
    }
    record->reported = true;
}

bool report_finish(Report *self, Record *record)
{
    if (self->start < 0) {
        log_error("report: cannot finish report that did not start!");
        return false;
    }
    if (self->type != REPORT_TYPE_LINK_OK) {
        if (self->end < 0) {
            if (self->start < record->rx_time) {
                self->end = record->rx_time;
            } else {
                log_error("report: ends before it starts!");
                return false;
            }
        }
    } else {
        // The link ok report has no end
        self->end = -1;
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
    const char *tx_name = "?", *rx_name = "?", *tx_rx_fmt = "%s -> %s: %s";
    if (self->tx != NULL) {
        tx_name = self->tx->name;
        if (self->rx != NULL) {
            rx_name = self->rx->name;
        } else {
            rx_name = self->tx->peer->name;
            tx_rx_fmt = "%s -> (%s): %s";
        }
    }
    long l = snprintf(cstr, size, tx_rx_fmt, tx_name, rx_name,
            report_type_to_cstr(self));
    if (self->finished) {
        if (self->type != REPORT_TYPE_LINK_OK) {
            l += snprintf(cstr + l, max(0, size - l), ", duration: %i ms",
                    self->end - self->start);
            if (self->lost_packets > 0) {
                l += snprintf(cstr + l, max(0, size - l),
                        ", lost: %i", self->lost_packets);
            }
            if (self->delayed_packets > 0) {
                l += snprintf(cstr + l, max(0, size - l),
                        ", delayed: %i", self->delayed_packets);
            }
            if (self->good_packets > 0) {
                l += snprintf(cstr + l, max(0, size - l),
                        ", good: %i", self->good_packets);
            }
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
