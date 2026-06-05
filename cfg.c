#include <stdlib.h>
#include <string.h>

#include "cfg.h"


Config cfg = {
    .file_path = NULL,
    .log_level = CFG_DEF_LOG_LEVEL,
    .nodes = {
        { .name = "AP", .ip = "192.168.3.2", .port = CFG_DEF_PORT },
        { .name = "STA", .ip = "192.168.2.2", .port = CFG_DEF_PORT }
    },
    .num_links = 0,
    .packet_records = CFG_DEF_PACKET_RECORDS,
    .packet_interval = CFG_DEF_PACKET_INTERVAL,
    .packet_delay_threshold = CFG_DEF_PACKET_DELAY_THRESHOLD,
    .packet_lost_threshold = CFG_DEF_PACKET_LOST_THRESHOLD,
    .report_relieve_threshold = CFG_DEF_REPORT_RELIEVE_THRESHOLD,
    .report_grouping = CFG_DEF_REPORT_GROUPING
};

static const char *cfg_file_paths[] = {
    ".gapf.json",
    "~/.gapf.json",
    "/etc/gapf.json"
};
static const char *prog_name = NULL;
static Map *cmdline_args = NULL;


static void *log_level_check(Str *log_level_str, Str **err_msg)
{
    Int *log_level = argparse_int(log_level_str, err_msg);
    if (log_level != NULL) {
        if (!int_in_range(log_level, 0, 7)) {
            *err_msg = str_new("invalid log level: %O!", log_level_str);
            delete(log_level);
            log_level = NULL;
        }
    }
    return log_level;
}

static void *config_file_check(Str *path, Str **err_msg)
{
    Json *js = NULL;
    File *js_file = argparse_file(path, err_msg);
    if (!is_none(js_file)) {
        Str *js_str = readstr(js_file, -1);
        js = json_new_cstr(str_cstr(js_str));
        delete(js_str);
        if (json_is_valid(js)) {
            // Save config file path
            cfg.file_path = strdup(js_file->path);
        } else {
            *err_msg = str_new("config file '%O' is invalid!", path);
            delete(js);
            js = NULL;
        }
        delete(js_file);
    }
    return js;
}

static void *host_check(Str *hostname, Str **err_msg)
{
    Str *ip = net_gethostbyname(str_cstr(hostname));
    if (ip == NULL) {
        *err_msg = str_new("unknown host: %O!", hostname);
    }
    return ip;
}

static bool port_range_check(long port, Str **err_msg)
{
    if (port < 0 || port > 65535) {
        *err_msg = str_new("invalid port number: %i!", port);
        return false;
    }
    return true;
}

static void *port_check(Str *port_str, Str **err_msg)
{
    Int *port = argparse_int(port_str, err_msg);
    if (port != NULL) {
        if (!port_range_check(int_get(port), err_msg)) {
            delete(port);
            port = NULL;
        }
    }
    return port;
}

static Map *parse_cmdline_args(int argc, char *argv[])
{
    Map *args;
    prog_name = path_basename(argv[0]);
    /* Setup argument parser */
    Argparse *ap = new(Argparse, prog_name, PROJECT_TITLE);
    // * Config file
    argparse_add_opt(ap, 'c', "cfg-file", "FILE", "1", config_file_check,
            "Configuration file");
    // * Log level
    argparse_add_opt(ap, 'l', "log-level", "LEVEL", "1", log_level_check,
            "Log level (0 - 7)");
    // * Network port of the nodes
    argparse_add_opt(ap, 'p', "port", "PORT", "1", port_check,
            "Network port (0 - 65535)");
    // * IP addresses of AP and STA
    argparse_add_arg(ap, "ip_ap", "AP", "?", host_check, "Local IP of AP");
    argparse_add_arg(ap, "ip_sta", "STA", "?", host_check, "Local IP of STA");
    // Parse command line arguments
    args = argparse_parse(ap, argc, argv);
    delete(ap);
    return args;
}

static void merge_cmdline_args(Map *args)
{
    bool ap = false, sta = false;
    // IP addresses of AP and STA
    Str *ip_ap = map_get(args, "ip_ap");
    if (!is_none(ip_ap)) {
        cfg.nodes[0].ip = str_cstr(ip_ap);
        ap = true;
    }
    Str *ip_sta = map_get(args, "ip_sta");
    if (!is_none(ip_sta)) {
        cfg.nodes[1].ip = str_cstr(ip_sta);
        sta = true;
    }
    // If both ap and sta are defined define down- and uplink
    if (cfg.num_links == 0 && ap && sta) {
        cfg.links[0].name = "downlink";
        cfg.links[0].tx_name = cfg.nodes[0].name;
        cfg.links[0].rx_name = cfg.nodes[1].name;
        cfg.links[1].name = "uplink";
        cfg.links[1].tx_name = cfg.nodes[1].name;
        cfg.links[1].rx_name = cfg.nodes[0].name;
        cfg.num_links = 2;
    }
    // Log level
    Int *log_level = map_get(args, "log-level");
    if (!is_none(log_level)) {
        cfg.log_level = int_get(log_level);
    }
    // Network port of the nodes
    Int *port = map_get(args, "port");
    if (!is_none(port)) {
           cfg.nodes[0].port = cfg.nodes[1].port = int_get(port);
    }
}

static Str *path_expanduser(const char *path)
{
    if (!cstr_startswith(path, "~/")) {
        goto out;
    }
    char *home = getenv("HOME");
    if (home == NULL) {
        goto out;
    }
    return path_join(home, path + 2);
out:
    // Expanding home directory failed, return the original path.
    return str_new_cstr(path);
}

static Json *check_default_config_files(Str **err_msg)
{
    Json *js = NULL;
    for (int i = 0; i < ARRAY_LEN(cfg_file_paths); i++) {
        const char *path_cstr = cfg_file_paths[i];
        Str *path = path_expanduser(path_cstr);
        if (path_is_file(path->cstr)) {
            js = config_file_check(path, err_msg);
        }
        delete(path);
        if (js != NULL) {
            return js;
        }
    }
    return js;
}

static bool parse_node_config(Map *node, int idx, Str **err_msg)
{
    // Name
    Object *name_obj = map_get(node, "name");
    if (!is_none(name_obj)) {
        if (!isinstance(name_obj, Str)) {
            *err_msg = str_new("Expecting type Str for node 'name'!");
            return false;
        }
        cfg.nodes[idx].name = str_cstr((Str *)name_obj);
    }
    // IP Address
    Object *ip_obj = map_get(node, "ip");
    if (!is_none(ip_obj)) {
        if (!isinstance(ip_obj, Str)) {
            *err_msg = str_new("Expecting type Str for node 'ip'!");
            return false;
        }
        Str *only_for_check = host_check((Str *)ip_obj, err_msg);
        if (only_for_check == NULL) {
            return false;
        }
        cfg.nodes[idx].ip = str_cstr((Str *)ip_obj);
        delete(only_for_check);
    }
    // Network Port
    Object *port_obj = map_get(node, "port");
    if (!is_none(port_obj)) {
        if (!isinstance(port_obj, Int)) {
            *err_msg = str_new("Expecting type Int for node 'port'!");
            return false;
        }
        int port = int_get((Int *)port_obj);
        if (!port_range_check(port, err_msg)) {
            return false;
        }
        cfg.nodes[idx].port = port;
    }
    return true;
}

static bool parse_link_config(Map *link, int idx, Str **err_msg)
{
    // Name
    Object *name_obj = map_get(link, "name");
    if (!is_none(name_obj)) {
        if (!isinstance(name_obj, Str)) {
            *err_msg = str_new("Expecting type Str for link 'name'!");
            return false;
        }
        cfg.links[idx].name = str_cstr((Str *)name_obj);
    }
    // Name of Sender (tx) Node
    Object *tx_obj = map_get(link, "tx");
    if (!is_none(tx_obj)) {
        if (!isinstance(tx_obj, Str)) {
            *err_msg = str_new("Expecting type Str for link 'tx'!");
            return false;
        }
        cfg.links[idx].tx_name = str_cstr((Str *)tx_obj);
    }
    // Name of Receiver (rx) Node
    Object *rx_obj = map_get(link, "rx");
    if (!is_none(rx_obj)) {
        if (!isinstance(rx_obj, Str)) {
            *err_msg = str_new("Expecting type Str for link 'rx'!");
            return false;
        }
        cfg.links[idx].rx_name = str_cstr((Str *)rx_obj);
    }
    return true;
}

static bool parse_config_file(Json *js, Str **err_msg)
{
    // Logging settings
    Object *log_level_obj = json_get_node(js, "log_level");
    if (!is_none(log_level_obj)) {
        Str *log_level_str = to_str(log_level_obj);
        Int *log_level = log_level_check(log_level_str, err_msg);
        if (!is_none(log_level)) {
            cfg.log_level = int_get(log_level);
            delete(log_level);
        } else {
            return false;
        }
    }
    // Nodes
    Object *nodes_obj = json_get_node(js, "nodes");
    if (isinstance(nodes_obj, List)) {
        List *nodes = (List *)nodes_obj;
        if (len(nodes) > CFG_NUM_NODES) {
            *err_msg = str_new("More than two nodes are not supported!");
            return false;
        }
        Iter itr = init(Iter, nodes);
        for (Map *node = next(&itr); node != NULL; node = next(&itr)) {
            if (!isinstance(node, Map)) {
                log_warn("cfg: Invalid node '%O', skip it!", node);
                continue;
            }
            if (!parse_node_config(node, iter_get_idx(&itr), err_msg)) {
                return false;
            }
        }
        destroy(&itr);
    }
    // Links
    Object *links_obj = json_get_node(js, "links");
    if (isinstance(links_obj, List)) {
        List *links = (List *)links_obj;
        if (len(links) > CFG_LINKS_MAX) {
            *err_msg = str_new("More than two links are not supported!");
            return false;
        }
        Iter itr = init(Iter, links);
        for (Map *link = next(&itr); link != NULL; link = next(&itr)) {
            if (!isinstance(link, Map)) {
                log_warn("cfg: Invalid link '%O', skip it!", link);
                continue;
            }
            if (!parse_link_config(link, iter_get_idx(&itr), err_msg)) {
                return false;
            }
        }
        destroy(&itr);
        // If all links could be parsed, set the number of links
        cfg.num_links = len(links);
    }
    // Packet Records
    Object *p_recs_obj = json_get_node(js, "packet_records");
    if (!is_none(p_recs_obj)) {
        if (!isinstance(p_recs_obj, Int)) {
            *err_msg = str_new("Expecting type Int for 'packet_records'!");
            return false;
        }
        int p_recs = to_int((Int *)p_recs_obj);
        // Check that packet records is greater than zero
        if (p_recs < CFG_PACKET_RECORDS_MIN) {
            *err_msg = str_new("Value of 'packet_records' is out of range!");
            return false;
        }
        cfg.packet_records = p_recs;
    }
    // Packet Interval
    Object *p_ival_obj = json_get_node(js, "packet_interval");
    if (!is_none(p_ival_obj)) {
        if (!isinstance(p_ival_obj, Int)) {
            *err_msg = str_new("Expecting type Int for 'packet_interval'!");
            return false;
        }
        int p_ival = to_int((Int *)p_ival_obj);
        // Check that packet interval is greater than zero
        if (p_ival <= 0) {
            *err_msg = str_new("Value of 'packet_interval' must be greater than 0!");
            return false;
        }
        cfg.packet_interval = p_ival;
    }
    // Minimal packet delay
    Object *p_delay_obj = json_get_node(js, "packet_delay_threshold");
    if (!is_none(p_delay_obj)) {
        if (!isinstance(p_delay_obj, Int)) {
            *err_msg = str_new("Expecting type Int for 'packet_delay_threshold'!");
            return false;
        }
        int p_delay = to_int((Int *)p_delay_obj);
        // Check that the minimal packet delay is a positive number
        if (p_delay < 0) {
            *err_msg = str_new("Value of 'packet_delay_threshold' must be positive!");
            return false;
        }
        cfg.packet_delay_threshold = p_delay;
    }
    // Threshold of the packet delay to mark a packet as lost
    Object *p_lost_obj = json_get_node(js, "packet_lost_threshold");
    if (!is_none(p_lost_obj)) {
        if (!isinstance(p_lost_obj, Int)) {
            *err_msg = str_new("Expecting type Int for 'packet_lost_threshold'!");
            return false;
        }
        int p_lost = to_int((Int *)p_lost_obj);
        // Check that the minimal packet lost threshold
        if (p_lost < CFG_PACKET_LOST_THRESHOLD_MIN) {
            *err_msg = str_new("Value of 'packet_lost_threshold' is out of range!");
            return false;
        }
        cfg.packet_lost_threshold = p_lost;
    }
    // Relieve time between gaps
    Object *r_relieve_obj = json_get_node(js, "report_relieve_threshold");
    if (!is_none(r_relieve_obj)) {
        if (!isinstance(r_relieve_obj, Int)) {
            *err_msg = str_new("Expecting type Int for 'report_relieve_threshold'!");
            return false;
        }
        int r_relieve = to_int((Int *)r_relieve_obj);
        // Check that the minimal packet lost threshold
        if (r_relieve  <= 0) {
            *err_msg = str_new("Value of 'report_relieve_threshold' must be greater than 0!");
            return false;
        }
        cfg.report_relieve_threshold = r_relieve;
    }
    // Report grouping
    Object *r_grouping_obj = json_get_node(js, "report_grouping");
    if (!is_none(r_grouping_obj)) {
        if (!isinstance(r_grouping_obj, Bool)) {
            *err_msg = str_new("Expecting type Bool for 'report_grouping'!");
            return false;
        }
        cfg.report_grouping  = bool_get((Bool *)r_grouping_obj);
    }
    return true;
}

static bool validate(Str **err_msg) {
    // Check the number of links
    if (cfg.num_links < 1) {
        *err_msg = str_new("Define at least 1 link!");
        return false;
    }
    return true;
}

void cfg_init(int argc, char *argv[])
{
    Str *err_msg = NULL;
    // First parse command line arguments to get config file path
    cmdline_args = parse_cmdline_args(argc, argv);
    Json *cfg_js = map_get(cmdline_args, "cfg-file");
    // If no config file is defined ...
    if (is_none(cfg_js)) {
        // ... check the default config file locations.
        cfg_js = check_default_config_files(&err_msg);
        if (err_msg != NULL) {
            goto out;
        }
    }
    // If a config file was found ...
    if (!is_none(cfg_js)) {
        // ... parse the config file.
        if (!parse_config_file(cfg_js, &err_msg))
            goto out;
    }
    // Merge the configuration with command line arguments.
    merge_cmdline_args(cmdline_args);
    // Finally, validate the configration
    if (validate(&err_msg)){
        return;
    }
out:
    fprint(stderr, "%s: error: %O\n", prog_name, err_msg);
    delete(err_msg);
    delete(cmdline_args);
    exit(1);

}

void cfg_destroy(void)
{
    free(cfg.file_path);
    delete(cmdline_args);
}
