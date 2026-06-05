#ifndef _CFG_H_
#define _CFG_H_

#include <masc.h>

/* Default values of the configuration */
#define CFG_DEF_LOG_LEVEL LOG_WARNING
#define CFG_DEF_PORT 5678
#define CFG_DEF_PACKET_RECORDS 1024
#define CFG_DEF_PACKET_INTERVAL 10
#define CFG_DEF_PACKET_DELAY_THRESHOLD 10
#define CFG_DEF_PACKET_LOST_THRESHOLD 500
#define CFG_DEF_REPORT_RELIEVE_THRESHOLD 200
#define CFG_DEF_REPORT_GROUPING true
/* Ranges of configuration values */
#define CFG_NUM_NODES 2
#define CFG_LINKS_MIN 1
#define CFG_LINKS_MAX 2
#define CFG_PACKET_RECORDS_MIN 16
#define CFG_PACKET_LOST_THRESHOLD_MIN 100


typedef struct {
    const char *name;
    const char *ip;
    int port;
} CfgNode;

typedef struct {
    const char *name;
    const char *tx_name;
    const char *rx_name;
} CfgLink;

typedef struct {
    char *file_path;
    int log_level;
    CfgNode nodes[CFG_NUM_NODES];
    int num_links;
    CfgLink links[CFG_LINKS_MAX];
    int packet_records;
    int packet_interval;
    int packet_delay_threshold;
    int packet_lost_threshold;
    int report_relieve_threshold;
    bool report_grouping;
} Config;


extern Config cfg;


void cfg_init(int argc, char *argv[]);
void cfg_destroy(void);

#endif /* _CFG_H_ */
