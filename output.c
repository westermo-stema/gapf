#include "output.h"
#include "cfg.h"


static void print_text_report(Report *r)
{
    print("%s -> %s, %O\n", r->link->tx->name, r->link->rx->name, r);
}

static void print_text_group(GroupReport *group)
{
    print("Group #%i: %i ms, duration: %i ms\n", group->number,
            group->start_time,
            group->end_time - group->start_time);
    for (int i = 0; i < cfg.num_links; i++) {
        Iter itr = init(Iter, &group->links[i].reports);
        for (Report *r = next(&itr); r != NULL; r = next(&itr)) {
            if (iter_get_idx(&itr) == 0) {
                Link *l = r->link;
                print(" * %s (%s -> %s)\n", l->name, l->tx->name, l->rx->name);
            }
            print("   * %O\n", r);
        }
        destroy(&itr);
    }
}

static void print_json_report(Report *report)
{
    Map *out = report_to_json(report);
    put(out);
    delete(out);
}

static void print_json_group(GroupReport *group)
{
    Map *out = group_report_to_json(group);
    put(out);
    delete(out);
}

void print_report(Report *report)
{
    if (cfg.output_json) {
        print_json_report(report);
    } else {
        print_text_report(report);
    }
}

void print_group(GroupReport *group)
{
    if (cfg.output_json) {
        print_json_group(group);
    } else {
        print_text_group(group);
    }
}
