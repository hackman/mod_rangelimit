#include <stdio.h>
#include <string.h>

#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_main.h"
#include "http_protocol.h"
#include "util_script.h"

#ifndef APACHE_RELEASE
#define APACHE2
#endif

#ifdef APACHE2
#include <strings.h>

#include "http_request.h"
#include "http_connection.h"
#include "apr_strings.h"
#endif
