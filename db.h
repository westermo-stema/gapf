#ifndef _GAPF_DB_H
#define _GAPF_DB_H

#include "node.h"
#include "record.h"


void db_init(int num_records);
void db_destroy(void);

void db_add_node(Node *node);
Node *db_get_node_by_id(int id);
Node *db_get_next_node(void);

Record *db_new_record(Node *node);
Record *db_get_record(int sender_id, int seq_num);

List *db_analyse_records(void);

#endif /* _GAPF_DB_H */
