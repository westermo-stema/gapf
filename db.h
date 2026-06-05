#ifndef _GAPF_DB_H
#define _GAPF_DB_H

#include "node.h"
#include "link.h"
#include "record.h"
#include "report.h"


void db_init(int num_records);
void db_destroy(void);

void db_add_node(Node *node);
Node *db_get_node_by_id(int id);
Node *db_get_node_by_name(const char *name);

void db_add_link(Link *link);
Link *db_get_next_link(void);
Link *db_get_link_by_id(int id);

Record *db_new_record(Link *link);
Record *db_get_record(int link_id, int seq_num);

List *db_analyse_records(void);

#endif /* _GAPF_DB_H */
