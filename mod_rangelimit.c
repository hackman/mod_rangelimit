#include "mod_rangelimit.h"
#define DEBUG_FIXRANGE

module AP_MODULE_DECLARE_DATA rangelimit_module;

typedef struct {
	int max_ranges;
	int max_overlaps;
} ranges_server_conf;

static void *create_rangelim_config(apr_pool_t *p, server_rec *s) {
    ranges_server_conf *cfg = (ranges_server_conf *) apr_pcalloc(p, sizeof(ranges_server_conf));
    cfg->max_ranges = 20;
	cfg->max_overlaps = 5;
    return cfg;
}

static int check_range(int *arr, int *val) {
	int i = 0;
	for (i=0;i<=16;i++) {
		if (*arr == 0)
			return 0;
		if (*arr == *val)
			return i;
		arr++;
	}
	return -1;
}

static int range_handler(request_rec *r) {
	const char *range_header = apr_table_get(r->headers_in, "Range");
    ranges_server_conf *cfg = ap_get_module_config(r->server->module_config, &rangelimit_module);
	int range_num = 0;			// counter of the number of checked ranges
	int range_overlaps = 0;		// counter of the number of overlapping ranges
	char *ranges = NULL;		// pointer to keep the ranges location
	int start = -1, end = -1;	// start and end of a single range
	int n = 0;					// number of characters collected by sscanf()
	int start_count = 0;		// counter for the new unique starts
	int end_count = 0;			// counter for the new unique ends
	int begins[60];
	int ends[60];

    if (!range_header || strncasecmp(range_header, "bytes=", 6) || r->status != HTTP_OK ) {
#ifdef DEBUG_FIXRANGE
		ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "mod_rangelimit: no range found" );
#endif
        return OK;
	}
#ifdef DEBUG_FIXRANGE
//	ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "mod_rangelimit: Range header: %s", range_header);
#endif
    if (!ap_strchr_c(range_header, ',')) {
#ifdef DEBUG_FIXRANGE
		ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "mod_rangelimit: single range, nothing to do here");
#endif
		return OK;
	}
    // multiple ranges
	ranges = ap_strchr_c(range_header, '=');
	ranges++;			// move the pointer to the begining of the first range
	// start walking over the ranges
	while ( sscanf(ranges, "%11d-%11d%n", &start, &end, &n) >= 1 ) {
#ifdef DEBUG_FIXRANGE
		ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r,
			"mod_rangelimit: range_num: %d overpalpping: %d start: %d end: %d n: %d",
			range_num, range_overlaps, start, end, n);
#endif
		if (start < 0) {
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
				"mod_rangelimit: requested range(%d-%d) not satisfiable", start, end);
			return HTTP_RANGE_NOT_SATISFIABLE;
		}

		if (end < 0 || end < start) {
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
				"mod_rangelimit: requested range(%d-%d) not satisfiable", start, end);
			return HTTP_RANGE_NOT_SATISFIABLE;
		}
		// code for handling overlaps
		if (check_range(begins, &start) >= 0) {
			range_overlaps++;
		} else {
			begins[start_count] = start;
			start_count++;
		}
		if (check_range(ends, &end) >= 0) {
			range_overlaps++;
		} else {
			ends[end_count] = end;
			end_count++;
		}
		// end of the overlaps code

		range_num++;	// increase the number of found ranges
		if (range_num > cfg->max_ranges) {
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
				"mod_rangelimit: too many ranges - %d", range_num);
			return HTTP_RANGE_NOT_SATISFIABLE;
		}
		if (range_overlaps > cfg->max_overlaps) {
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
				"mod_rangelimit: too many overlapping ranges - %d", range_overlaps);
			return HTTP_RANGE_NOT_SATISFIABLE;
		}
		if (end == -1) {
			// since n is not set when end is not read
			// find the end of this range using strchr
			ranges = ap_strchr_c(ranges, '-') + 1;
		} else {
			ranges += n;	// advance the pointer by the number of characters read
		}
		if ( *ranges != ',' ) {
			break;			// didn't find an expected delimiter, done?
		}
		++ranges;			// skip the delimiter
		start = end = -1;	// clear the start and end points
		n = 0;				// clear the number of found chars
	}
	return OK;
}

static const char *set_max_ranges(cmd_parms *cmd, void *mconfig, const char *arg) {
    ranges_server_conf *cfg = ap_get_module_config(cmd->server->module_config, &rangelimit_module);
    cfg->max_ranges = atoi(arg);
    return NULL;
}

static const char *set_max_overlaps(cmd_parms *cmd, void *mconfig, const char *arg) {
    ranges_server_conf *cfg = ap_get_module_config(cmd->server->module_config, &rangelimit_module);
    cfg->max_overlaps = atoi(arg);
    return NULL;
}

static const command_rec rangelim_cmds[] = {
    AP_INIT_TAKE1("MaxRanges", set_max_ranges, NULL, RSRC_CONF,
        "Define the maximum number of allowed range definitions. Default 20"),
    AP_INIT_TAKE1("MaxOverlappingRanges", set_max_overlaps, NULL, RSRC_CONF,
        "Define the maximum number of allowed ranges that overlap each other. Default 5"),
    {NULL}
};

static void register_hooks(apr_pool_t *p) {
	static const char * const aszPost[] = { "mod_setenvif.c", NULL };
    ap_hook_header_parser(range_handler, NULL, aszPost, APR_HOOK_MIDDLE);
}

module AP_MODULE_DECLARE_DATA rangelimit_module = {
    STANDARD20_MODULE_STUFF,
    NULL,					/* per-directory config creator */
    NULL,					/* dir config merger */
    create_rangelim_config,	/* server config creator */
    NULL,					/* server config merger */
    rangelim_cmds,			/* command table */
    register_hooks,			/* set up other request processing hooks */
};
