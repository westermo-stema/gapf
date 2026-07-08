#include "group_report.h"


void group_report_init(GroupReport *self)
{
    self->number = 1;
    self->start_time = -1;
    self->n_links = 0;
}

void group_report_destroy(GroupReport *self)
{
    for (int i = 0; i < self->n_links; i++) {
        list_destroy(&self->links[i].reports);
    }
}

bool group_report_add_link(GroupReport *self, uint16_t id)
{
    if (self->n_links >= CFG_LINKS_MAX) {
        return false;
    }
    self->links[self->n_links].link_id = id;
    self->links[self->n_links].reports = init(List);
    self->links[self->n_links].ok = false;
    self->n_links++;
    return true;
}

bool group_report_add(GroupReport *self, Report *report)
{
    bool added = false;
    // Set start time
    if (self->start_time < 0) {
        self->start_time = report->start;
    }
    // Add the report to the corresponding link
    for (int i = 0; i < self->n_links; i++) {
        if (self->links[i].link_id != report->link->id) {
            continue;
        }
        // If report type is link ok ...
        if (report->type == REPORT_TYPE_LINK_OK) {
            // ... set the current link status as ok.
            self->links[i].ok = true;
        } else {
            // ... otherwise append the report to the proper link and ...
            list_append(&self->links[i].reports, report);
            // ... update the end time of the group record.
            self->end_time = report->end;
            added = true;
        }
        break;
    }
    return added;
}

bool group_report_links_ok(GroupReport *self)
{
    for (int i = 0; i < self->n_links; i++) {
        if (!self->links[i].ok) {
            return false;
        }
    }
    return true;
}

void group_report_reset(GroupReport *self)
{
    for (int i = 0; i < self->n_links; i++) {
        list_delete_all(&self->links[i].reports);
        self->links[i].ok = false;
    }
    self->start_time = -1;
    self->number++;
}
