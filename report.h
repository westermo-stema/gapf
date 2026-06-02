#ifndef _REPORT_H_
#define _REPORT_H_

#include "record.h"


typedef enum {
    REPORT_TYPE_LOST,
    REPORT_TYPE_DELAY,
    REPORT_TYPE_UNKNOWN
} ReportType;

typedef struct {
    Object;
    ReportType type;
    Node *tx;
    Node *rx;
    int gap_start;
    int gap_end;
    int lost_packets;
    int delayed_packets;
    bool finished;
} Report;


extern const class *ReportCls;


void report_init(Report *self, ReportType type, Node *tx, Node *rx);
void report_destroy(Report *self);

const char *report_type_to_cstr(Report *self);

void report_add_record(Report *self, Record *record);
bool report_finish(Report *self, Record *record);

int report_cmp(const Report *self, const Report *other);

size_t report_to_cstr(Report *self, char *cstr, size_t size);

#endif /* _REPORT_H_ */
