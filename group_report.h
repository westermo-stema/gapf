#ifndef _GROUP_REPORT_H_
#define _GROUP_REPORT_H_

#include "cfg.h"
#include "report.h"


typedef struct {
    int number;
    int start_time;
    int end_time;
    int n_links;
    struct {
        uint16_t link_id;
        List reports;
        bool ok;
    } links[CFG_LINKS_MAX];
} GroupReport;


void group_report_init(GroupReport *self);
void group_report_destroy(GroupReport *self);

bool group_report_add_link(GroupReport *self, uint16_t id);
bool group_report_add(GroupReport *self, Report *report);
bool group_report_links_ok(GroupReport *self);
void group_report_reset(GroupReport *self);

Map *group_report_to_json(GroupReport *self);

#endif /* _GROUP_REPORT_H_ */