#include "mod_rangelimit.h"

// Module version: 1.0

#ifdef APACHE2
module AP_MODULE_DECLARE_DATA rangelimit_module;
#else
module MODULE_VAR_EXPORT rangelimit_module;
#endif

typedef struct {
	int max_ranges;
	int max_overlaps;
} ranges_server_conf;

static void *create_rangelim_config(apr_pool_t *p, server_rec *s) {
#ifdef APACHE2
    ranges_server_conf *cfg = (ranges_server_conf *) apr_pcalloc(p, sizeof(ranges_server_conf));
#else
    ranges_server_conf *cfg = (ranges_server_conf *) ap_pcalloc(p, sizeof(ranges_server_conf));
#endif
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
#ifdef APACHE2
	const char *range_header = apr_table_get(r->headers_in, "Range");
#else
	const char *range_header = apr_table_get(r->headers_in, "Range");
#endif
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
		if (r->server->loglevel == APLOG_DEBUG)
#ifdef APACHE2
			ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "mod_rangelimit: no range found" );
#else
			ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, r, "mod_rangelimit: no range found" );
#endif
        return OK;
	}
//	if (r->server->loglevel == APLOG_DEBUG)
//#ifdef APACHE2
//		ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "mod_rangelimit: Range header: %s", range_header);
//#else
//		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, r, "mod_rangelimit: Range header: %s", range_header);
//#endif
    if (!ap_strchr_c(range_header, ',')) {
		if (r->server->loglevel == APLOG_DEBUG)
#ifdef APACHE2
			ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "mod_rangelimit: single range, nothing to do here");
#else
			ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, r, "mod_rangelimit: single range, nothing to do here");
#endif
		return OK;
	}
    // multiple ranges
#ifdef APACHE
	ranges = ap_strchr_c(range_header, '=');
#else
	ranges = strchr(range_header, '=');
#endif
	ranges++;			// move the pointer to the begining of the first range
	// start walking over the ranges
	while ( sscanf(ranges, "%11d-%11d%n", &start, &end, &n) >= 1 ) {
		if (start < 0) {
#ifdef APACHE2
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
				"mod_rangelimit: requested range(%d-%d) not satisfiable", start, end);
#else
			ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, r,
				"mod_rangelimit: requested range(%d-%d) not satisfiable", start, end);
#endif
			return HTTP_RANGE_NOT_SATISFIABLE;
		}

		if (end >= 0 && end < start) {
#ifdef APACHE2
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
				"mod_rangelimit: requested range(%d-%d) not satisfiable", start, end);
#else
			ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, r,
				"mod_rangelimit: requested range(%d-%d) not satisfiable", start, end);
#endif
			return HTTP_RANGE_NOT_SATISFIABLE;
		}
		// code for handling overlaps
		if (check_range(begins, &start) > 0) {
			range_overlaps++;
		} else {
			begins[start_count] = start;
			start_count++;
		}
		if (check_range(ends, &end) > 0) {
			range_overlaps++;
		} else {
			ends[end_count] = end;
			end_count++;
		}
		// end of the overlaps code

		range_num++;	// increase the number of found ranges

		if (r->server->loglevel == APLOG_DEBUG)
#ifdef APACHE2
			ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r,
				"mod_rangelimit: range_num: %d overpalpping: %d start: %d end: %d n: %d",
				range_num, range_overlaps, start, end, n);
#else
			ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, r,
				"mod_rangelimit: range_num: %d overpalpping: %d start: %d end: %d n: %d",
				range_num, range_overlaps, start, end, n);
#endif

		if (range_num > cfg->max_ranges) {
#ifdef APACHE2
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
				"mod_rangelimit: too many ranges - %d", range_num);
#else
			ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, r,
				"mod_rangelimit: too many ranges - %d", range_num);
#endif
			return HTTP_RANGE_NOT_SATISFIABLE;
		}
		if (range_overlaps > cfg->max_overlaps) {
#ifdef APACHE2
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
				"mod_rangelimit: too many overlapping ranges - %d", range_overlaps);
#else
			ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, r,
				"mod_rangelimit: too many overlapping ranges - %d", range_overlaps);
#endif
			return HTTP_RANGE_NOT_SATISFIABLE;
		}
		if (end == -1) {
			// since n is not set when end is not read
			// find the end of this range using strchr
#ifdef APACHE2
			ranges = ap_strchr_c(ranges, '-') + 1;
#else
			ranges = strchr(ranges, '-') + 1;
#endif
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
#ifdef APACHE2
    AP_INIT_TAKE1("MaxRanges", set_max_ranges, NULL, RSRC_CONF,
        "Define the maximum number of allowed range definitions. Default 20"),
    AP_INIT_TAKE1("MaxOverlappingRanges", set_max_overlaps, NULL, RSRC_CONF,
        "Define the maximum number of allowed ranges that overlap each other. Default 5"),
#else
	{ "MaxRanges", set_max_ranges, NULL, RSRC_CONF, TAKE1, 
		"Define the maximum number of allowed range definitions. Default 20" },
	{ "MaxOverlappingRanges", set_max_overlaps, NULL, RSRC_CONF, TAKE1, 
		"Define the maximum number of allowed ranges that overlap each other. Default 5" },
#endif
    {NULL}
};

#ifdef APACHE2
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
#else
module MODULE_VAR_EXPORT rangelimit_module = { 
    STANDARD_MODULE_STUFF,
    NULL,                       /* module initializer */
    NULL,                       /* per-directory config creator */
    NULL,                       /* dir config merger */
    create_rangelim_config,     /* server config creator */
    NULL,                       /* server config merger */
    rangelim_cmds,				/* command table */
    NULL,                       /* [9] list of handlers */
    NULL,                       /* [2] filename-to-URI translation */
    NULL,                       /* [5] check/validate user_id */
    NULL,                       /* [6] check user_id is valid *here* */
    NULL,                       /* [4] check access by host address */
    NULL,                       /* [7] MIME type checker/setter */
    NULL,                       /* [8] fixups */
    NULL,                       /* [10] logger */
#if MODULE_MAGIC_NUMBER >= 19970103
    range_handler,  /* [3] header parser */
#endif
#if MODULE_MAGIC_NUMBER >= 19970719
    NULL,           /* process initializer */
#endif
#if MODULE_MAGIC_NUMBER >= 19970728
    NULL,           /* process exit/cleanup */
#endif
#if MODULE_MAGIC_NUMBER >= 19970902
    NULL            /* [1] post read_request handling */
#endif
};
#endif // APACHE2
