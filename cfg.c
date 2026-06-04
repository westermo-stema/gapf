#include <stdlib.h>
#include <string.h>

#include "cfg.h"


Config cfg = {
    .file_path = NULL,
    .log_level = CFG_DEF_LOG_LEVEL,
    .ip_a = NULL,
    .ip_b = NULL,
    .port = CFG_DEF_PORT,
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

static void *port_check(Str *port_str, Str **err_msg)
{
    Int *port = argparse_int(port_str, err_msg);
    if (port != NULL) {
        if (!int_in_range(port, 0, 65535)) {
            *err_msg = str_new("invalid port number: %O!", port_str);
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
    // * IP addresses of node A and B
    argparse_add_arg(ap, "ip_a", "IP", NULL, host_check, "Local IP of node A");
    argparse_add_arg(ap, "ip_b", "IP", NULL, host_check, "Local IP of node B");
    // Parse command line arguments
    args = argparse_parse(ap, argc, argv);
    delete(ap);
    return args;
}

static void merge_cmdline_args(Map *args)
{
    /* Assign mandatory arguments to the configuration */
    // IP addresses of node A and B
    Str *ip_a = map_get(args, "ip_a");
    cfg.ip_a = str_cstr(ip_a);
    Str *ip_b = map_get(args, "ip_b");
    cfg.ip_b = str_cstr(ip_b);
    /* Merge optional arguments */
    // * Log level
    Int *log_level = map_get(args, "log-level");
    if (!is_none(log_level)) {
        cfg.log_level = int_get(log_level);
    }
    // * Network port of the nodes
    Int *port = map_get(args, "port");
    if (!is_none(port)) {
           cfg.port = int_get(port);
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

static Json *check_default_config_files(void)
{
    Json *js = NULL;
    Str *err_msg = NULL;
    for (int i = 0; i < ARRAY_LEN(cfg_file_paths); i++) {
        const char *path_cstr = cfg_file_paths[i];
        Str *path = path_expanduser(path_cstr);
        if (path_is_file(path->cstr)) {
            js = config_file_check(path, &err_msg);
        }
        delete(path);
        if (js != NULL) {
            return js;
        } else if (err_msg != NULL) {
            fprint(stderr, "%s: error: %O\n", prog_name, err_msg);
            delete(err_msg);
            exit(1);
        }
    }
    return js;
}

static void parse_config_file(Json *js)
{
    Str *err_msg = NULL;
    // Logging settings
    Object *log_level_obj = json_get_node(js, "log_level");
    if (!is_none(log_level_obj)) {
        Str *log_level_str = to_str(log_level_obj);
        Int *log_level = log_level_check(log_level_str, &err_msg);
        if (!is_none(log_level)) {
            cfg.log_level = int_get(log_level);
            delete(log_level);
        } else {
            goto out;
        }
    }
    // Packet Records
    Object *p_recs_obj = json_get_node(js, "packet_records");
    if (!is_none(p_recs_obj)) {
        if (!isinstance(p_recs_obj, Int)) {
            err_msg = str_new("Expecting type Int for 'packet_records'!");
            goto out;
        }
        int p_recs = to_int((Int *)p_recs_obj);
        // Check that packet records is greater than zero
        if (p_recs < CFG_PACKET_RECORDS_MIN) {
            err_msg = str_new("Value of 'packet_records' is out of range!");
            goto out;
        }
        cfg.packet_records = p_recs;
    }
    // Packet Interval
    Object *p_ival_obj = json_get_node(js, "packet_interval");
    if (!is_none(p_ival_obj)) {
        if (!isinstance(p_ival_obj, Int)) {
            err_msg = str_new("Expecting type Int for 'packet_interval'!");
            goto out;
        }
        int p_ival = to_int((Int *)p_ival_obj);
        // Check that packet interval is greater than zero
        if (p_ival <= 0) {
            err_msg = str_new("Value of 'packet_interval' must be greater than 0!");
            goto out;
        }
        cfg.packet_interval = p_ival;
    }
    // Minimal packet delay
    Object *p_delay_obj = json_get_node(js, "packet_delay_threshold");
    if (!is_none(p_delay_obj)) {
        if (!isinstance(p_delay_obj, Int)) {
            err_msg = str_new("Expecting type Int for 'packet_delay_threshold'!");
            goto out;
        }
        int p_delay = to_int((Int *)p_delay_obj);
        // Check that the minimal packet delay is a positive number
        if (p_delay < 0) {
            err_msg = str_new("Value of 'packet_delay_threshold' must be positive!");
            goto out;
        }
        cfg.packet_delay_threshold = p_delay;
    }
    // Threshold of the packet delay to mark a packet as lost
    Object *p_lost_obj = json_get_node(js, "packet_lost_threshold");
    if (!is_none(p_lost_obj)) {
        if (!isinstance(p_lost_obj, Int)) {
            err_msg = str_new("Expecting type Int for 'packet_lost_threshold'!");
            goto out;
        }
        int p_lost = to_int((Int *)p_lost_obj);
        // Check that the minimal packet lost threshold
        if (p_lost < CFG_PACKET_LOST_THRESHOLD_MIN) {
            err_msg = str_new("Value of 'packet_lost_threshold' is out of range!");
            goto out;
        }
        cfg.packet_lost_threshold = p_lost;
    }
    // Relieve time between gaps
    Object *r_relieve_obj = json_get_node(js, "report_relieve_threshold");
    if (!is_none(r_relieve_obj)) {
        if (!isinstance(r_relieve_obj, Int)) {
            err_msg = str_new("Expecting type Int for 'report_relieve_threshold'!");
            goto out;
        }
        int r_relieve = to_int((Int *)r_relieve_obj);
        // Check that the minimal packet lost threshold
        if (r_relieve  <= 0) {
            err_msg = str_new("Value of 'report_relieve_threshold' must be greater than 0!");
            goto out;
        }
        cfg.report_relieve_threshold = r_relieve;
    }
    // Report grouping
    Object *r_grouping_obj = json_get_node(js, "report_grouping");
    if (!is_none(r_grouping_obj)) {
        if (!isinstance(r_grouping_obj, Bool)) {
            err_msg = str_new("Expecting type Bool for 'report_grouping'!");
            goto out;
        }
        cfg.report_grouping  = bool_get((Bool *)r_grouping_obj);
    }
    return;
out:
    fprint(stderr, "%s: error: %O\n", prog_name, err_msg);
    delete(err_msg);
    exit(1);
}

void cfg_init(int argc, char *argv[])
{
    // Initialise default configuration

    // First parse command line arguments to get config file path
    cmdline_args = parse_cmdline_args(argc, argv);
    Json *cfg_js = map_get(cmdline_args, "cfg-file");
    // If no config file is defined ...
    if (is_none(cfg_js)) {
        // ... check the default config file locations.
        cfg_js = check_default_config_files();
    }
    // If a config file was found ...
    if (!is_none(cfg_js)) {
        // ... parse the config file.
        parse_config_file(cfg_js);
    }
    // Finally, merge the configuration with command line arguments.
    merge_cmdline_args(cmdline_args);
}

void cfg_destroy(void)
{
    free(cfg.file_path);
    delete(cmdline_args);
}
