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
/* Ranges of configuration values */
#define CFG_PACKET_RECORDS_MIN 16
#define CFG_PACKET_LOST_THRESHOLD_MIN 100


typedef struct {
    char *file_path;
    int log_level;
    const char *ip_a;
    const char *ip_b;
    int port;
    int packet_records;
    int packet_interval;
    int packet_delay_threshold;
    int packet_lost_threshold;
    int report_relieve_threshold;
} Config;


extern Config cfg;


void cfg_init(int argc, char *argv[]);
void cfg_destroy(void);

#endif /* _CFG_H_ */
