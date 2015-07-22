/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2013 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: rryqszq4                                                     |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/basic_functions.h"
#include "ext/standard/php_rand.h"
#include "SAPI.h"
#include "ext/json/php_json.h"
#include "ext/standard/file.h"
#include "php_jsonrpc.h"

#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/multi.h>

#include <sys/select.h>

/* If you declare any globals in php_jsonrpc.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(jsonrpc)
*/

/* True global resources - no need for thread safety here */
static int le_jsonrpc;

static int le_curl;
#define le_curl_name "cURL handle"
static int  le_curl_multi_handle;
#define le_curl_multi_handle_name "cURL Multi Handle"

#define SAVE_CURL_ERROR(__handle, __err) (__handle)->err.no = (int) __err;

/* As of curl 7.11.1 this is no longer defined inside curl.h */
#ifndef HttpPost
#define HttpPost curl_httppost
#endif

#define COUNT_RECURSIVE			1

static zend_class_entry *php_jsonrpc_server_entry;
static zend_class_entry *php_jsonrpc_client_entry;

static void shurrik_dump_zval(zval *data TSRMLS_DC);
static char* shurrik_return_zval_type(zend_uchar type TSRMLS_DC);

/* {{{ jsonrpc_functions[]
 *
 * Every user visible function must have an entry in jsonrpc_functions[].
 */
const zend_function_entry jsonrpc_functions[] = {
	PHP_FE(confirm_jsonrpc_compiled,	NULL)		/* For testing, remove later. */
	PHP_FE_END	/* Must be the last line in jsonrpc_functions[] */
};
/* }}} */

/* {{{ jsonrpc_module_entry
 */
zend_module_entry jsonrpc_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"jsonrpc",
	jsonrpc_functions,
	PHP_MINIT(jsonrpc),
	PHP_MSHUTDOWN(jsonrpc),
	PHP_RINIT(jsonrpc),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(jsonrpc),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(jsonrpc),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_JSONRPC
ZEND_GET_MODULE(jsonrpc)
#endif

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("jsonrpc.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_jsonrpc_globals, jsonrpc_globals)
    STD_PHP_INI_ENTRY("jsonrpc.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_jsonrpc_globals, jsonrpc_globals)
PHP_INI_END()
*/
/* }}} */

/* {{{ php_jsonrpc_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_jsonrpc_init_globals(zend_jsonrpc_globals *jsonrpc_globals)
{
	jsonrpc_globals->global_value = 0;
	jsonrpc_globals->global_string = NULL;
}
*/
/* }}} */

static char* shurrik_return_zval_type(zend_uchar type)
{
	switch (type) {
		case IS_NULL:
			return "IS_NULL";
			break;
		case IS_BOOL:
			return "IS_BOOL";
			break;
		case IS_LONG:
			return "IS_LONG";
			break;
		case IS_DOUBLE:
			return "IS_DOUBLE";
			break;
		case IS_STRING:
			return "IS_STRING";
			break;
		case IS_ARRAY:
			return "IS_ARRAY";
			break;
		case IS_OBJECT:
			return "IS_OBJECT";
			break;
		case IS_RESOURCE:
			return "IS_RESOURCE";
			break;
		default :
			return "unknown";
	}
}

static void shurrik_dump_zval(zval *data)
{
	php_printf("zval<%p> {\n", data);
	php_printf("	refcount__gc -> %d\n", data->refcount__gc);
	php_printf("	is_ref__gc -> %d\n", data->is_ref__gc);
	php_printf("	type -> %s\n", shurrik_return_zval_type(data->type));
	php_printf("	zand_value<%p> {\n", data->value);

	php_printf("		lval -> %d\n", data->value.lval);
	php_printf("		dval -> %e\n", data->value.dval);
	if (Z_TYPE_P(data) == IS_STRING){
		php_printf("		str -> %s\n", Z_STRVAL_P(data));
	}
	php_printf("		*ht<%p> {\n", data->value.ht);
	php_printf("		}\n");
	php_printf("		obj<%p> {\n", data->value.obj);
	php_printf("		}\n");
	php_printf("	}\n");

	php_printf("}\n");
}

static int php_count_recursive(zval *array, long mode TSRMLS_DC) /* {{{ */
{
	long cnt = 0;
	zval **element;

	if (Z_TYPE_P(array) == IS_ARRAY) {
		if (Z_ARRVAL_P(array)->nApplyCount > 1) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "recursion detected");
			return 0;
		}

		cnt = zend_hash_num_elements(Z_ARRVAL_P(array));
		if (mode == COUNT_RECURSIVE) {
			HashPosition pos;

			for (zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(array), &pos);
				zend_hash_get_current_data_ex(Z_ARRVAL_P(array), (void **) &element, &pos) == SUCCESS;
				zend_hash_move_forward_ex(Z_ARRVAL_P(array), &pos)
			) {
				Z_ARRVAL_P(array)->nApplyCount++;
				cnt += php_count_recursive(*element, COUNT_RECURSIVE TSRMLS_CC);
				Z_ARRVAL_P(array)->nApplyCount--;
			}
		}
	}

	return cnt;
}
/* }}} */

static zval* jr_server_get_arguments(zval *request_params, zval *method_params,
	int nb_required_params, int nb_max_params)
{
	zval *retval;
	int nb_params;

	MAKE_STD_ZVAL(retval);

	nb_params = php_count_recursive(request_params, 0 TSRMLS_CC);

	if (nb_params < nb_required_params){
		ZVAL_FALSE(retval);
		return retval;
	}

	if (nb_params > nb_max_params){
		ZVAL_FALSE(retval);
		return retval;
	}

	ZVAL_TRUE(retval);
	return retval;
}

static zval* jr_client_prepare_request(zval *procedure, zval *params)
{
	zval *payload;
	long number;
	int nb_params;

	MAKE_STD_ZVAL(payload);
	array_init(payload);

	add_assoc_string(payload, "jsonrpc", "2.0", 0);
	add_assoc_string(payload, "method", Z_STRVAL_P(procedure), 0);

	if (!BG(mt_rand_is_seeded)) {
		php_mt_srand(GENERATE_SEED() TSRMLS_CC);
	}
	number = (long) (php_mt_rand(TSRMLS_C) >> 1);
	add_assoc_long(payload, "id", number);

	nb_params = php_count_recursive(params, 0 TSRMLS_CC);
	if (nb_params > 0)
	{
		add_assoc_zval(payload, "params", params);
	}

	return payload;
}

static zval* jr_file_get_contents()
{
	zval *payload;

	MAKE_STD_ZVAL(payload);

	zend_bool use_include_path = 0;
	php_stream *stream;
	int len;
	long offset = -1;
	long maxlen = PHP_STREAM_COPY_ALL;
	zval *zcontext = NULL;
	php_stream_context *context = NULL;

	char *contents;

	

	context = php_stream_context_from_zval(zcontext, 0);

	stream = php_stream_open_wrapper_ex("php://input", "rb",
				(use_include_path ? USE_PATH : 0) | ENFORCE_SAFE_MODE | REPORT_ERRORS,
				NULL, context);

	
	if (!stream) {
		ZVAL_NULL(payload);
		php_stream_close(stream);
		return payload;
	}

	if (offset > 0 && php_stream_seek(stream, offset, SEEK_SET) < 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to seek to position %ld in the stream", offset);
		php_stream_close(stream);
		ZVAL_NULL(payload);
		return payload;
	}

	if ((len = php_stream_copy_to_mem(stream, &contents, maxlen, 0)) > 0) {

		if (PG(magic_quotes_runtime)) {
			contents = php_addslashes(contents, len, &len, 1 TSRMLS_CC); 
		}

		ZVAL_STRINGL(payload, contents, len, 1);
		php_stream_close(stream);
		return payload;
	} else if (len == 0) {
		ZVAL_STRING(payload, "", 1);
		php_stream_close(stream);
		return payload;
	} else {
		ZVAL_NULL(payload);
		php_stream_close(stream);
		return payload;
	}
	
}


static int php_curl_option_url(php_curl *ch, const char *url, const int len) /* {{{ */
{
	CURLcode error = CURLE_OK;
#if LIBCURL_VERSION_NUM < 0x071100
	char *copystr = NULL;
#endif
	TSRMLS_FETCH();

	/* Disable file:// if open_basedir or safe_mode are used */
	if ((PG(open_basedir) && *PG(open_basedir)) || PG(safe_mode)) {
#if LIBCURL_VERSION_NUM >= 0x071304
		error = curl_easy_setopt(ch->cp, CURLOPT_PROTOCOLS, CURLPROTO_ALL & ~CURLPROTO_FILE);
#else
		php_url *uri;

		if (!(uri = php_url_parse_ex(url, len))) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid URL '%s'", url);
			return 0;
		}

		if (uri->scheme && !strncasecmp("file", uri->scheme, sizeof("file"))) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Protocol 'file' disabled in cURL");
			php_url_free(uri);
			return 0;
		}
		php_url_free(uri);
#endif
	}
	/* Strings passed to libcurl as 'char *' arguments, are copied by the library... NOTE: before 7.17.0 strings were not copied. */
#if LIBCURL_VERSION_NUM >= 0x071100
	error = curl_easy_setopt(ch->cp, CURLOPT_URL, url);
#else
	copystr = estrndup(url, len);
	error = curl_easy_setopt(ch->cp, CURLOPT_URL, copystr);
	zend_llist_add_element(&ch->to_free->str, &copystr);
#endif

	return (error == CURLE_OK ? 1 : 0);
}
/* }}} */

/* {{{ curl_read
 */
static size_t curl_read(char *data, size_t size, size_t nmemb, void *ctx)
{
	php_curl       *ch = (php_curl *) ctx;
	php_curl_read  *t  = ch->handlers->read;
	int             length = 0;

	switch (t->method) {
		case PHP_CURL_DIRECT:
			if (t->fp) {
				length = fread(data, size, nmemb, t->fp);
			}
			break;
		case PHP_CURL_USER: {
			zval **argv[3];
			zval  *handle = NULL;
			zval  *zfd = NULL;
			zval  *zlength = NULL;
			zval  *retval_ptr;
			int   error;
			zend_fcall_info fci;
			TSRMLS_FETCH_FROM_CTX(ch->thread_ctx);

			MAKE_STD_ZVAL(handle);
			MAKE_STD_ZVAL(zfd);
			MAKE_STD_ZVAL(zlength);

			ZVAL_RESOURCE(handle, ch->id);
			zend_list_addref(ch->id);
			ZVAL_RESOURCE(zfd, t->fd);
			zend_list_addref(t->fd);
			ZVAL_LONG(zlength, (int) size * nmemb);

			argv[0] = &handle;
			argv[1] = &zfd;
			argv[2] = &zlength;

			fci.size = sizeof(fci);
			fci.function_table = EG(function_table);
			fci.function_name = t->func_name;
			fci.object_ptr = NULL;
			fci.retval_ptr_ptr = &retval_ptr;
			fci.param_count = 3;
			fci.params = argv;
			fci.no_separation = 0;
			fci.symbol_table = NULL;

			ch->in_callback = 1;
			error = zend_call_function(&fci, &t->fci_cache TSRMLS_CC);
			ch->in_callback = 0;
			if (error == FAILURE) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Cannot call the CURLOPT_READFUNCTION");
#if LIBCURL_VERSION_NUM >= 0x070c01 /* 7.12.1 */
				length = CURL_READFUNC_ABORT;
#endif
			} else if (retval_ptr) {
				if (Z_TYPE_P(retval_ptr) == IS_STRING) {
					length = MIN((int) (size * nmemb), Z_STRLEN_P(retval_ptr));
					memcpy(data, Z_STRVAL_P(retval_ptr), length);
				}
				zval_ptr_dtor(&retval_ptr);
			}

			zval_ptr_dtor(argv[0]);
			zval_ptr_dtor(argv[1]);
			zval_ptr_dtor(argv[2]);
			break;
		}
	}

	return length;
}
/* }}} */

/* {{{ curl_write_nothing
 * Used as a work around. See _php_curl_close_ex
 */
static size_t curl_write_nothing(char *data, size_t size, size_t nmemb, void *ctx) 
{
	return size * nmemb;
}
/* }}} */

/* {{{ curl_write
 */
static size_t curl_write(char *data, size_t size, size_t nmemb, void *ctx)
{
	php_curl       *ch     = (php_curl *) ctx;
	php_curl_write *t      = ch->handlers->write;
	size_t          length = size * nmemb;
	TSRMLS_FETCH_FROM_CTX(ch->thread_ctx);

#if PHP_CURL_DEBUG
	fprintf(stderr, "curl_write() called\n");
	fprintf(stderr, "data = %s, size = %d, nmemb = %d, ctx = %x\n", data, size, nmemb, ctx);
#endif

	switch (t->method) {
		case PHP_CURL_STDOUT:
			PHPWRITE(data, length);
			break;
		case PHP_CURL_FILE:
			return fwrite(data, size, nmemb, t->fp);
		case PHP_CURL_RETURN:
			if (length > 0) {
				smart_str_appendl(&t->buf, data, (int) length);
			}
			break;
		case PHP_CURL_USER: {
			zval **argv[2];
			zval *retval_ptr = NULL;
			zval *handle = NULL;
			zval *zdata = NULL;
			int   error;
			zend_fcall_info fci;

			MAKE_STD_ZVAL(handle);
			ZVAL_RESOURCE(handle, ch->id);
			zend_list_addref(ch->id);
			argv[0] = &handle;

			MAKE_STD_ZVAL(zdata);
			ZVAL_STRINGL(zdata, data, length, 1);
			argv[1] = &zdata;

			fci.size = sizeof(fci);
			fci.function_table = EG(function_table);
			fci.object_ptr = NULL;
			fci.function_name = t->func_name;
			fci.retval_ptr_ptr = &retval_ptr;
			fci.param_count = 2;
			fci.params = argv;
			fci.no_separation = 0;
			fci.symbol_table = NULL;

			ch->in_callback = 1;
			error = zend_call_function(&fci, &t->fci_cache TSRMLS_CC);
			ch->in_callback = 0;
			if (error == FAILURE) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not call the CURLOPT_WRITEFUNCTION");
				length = -1;
			} else if (retval_ptr) {
				if (Z_TYPE_P(retval_ptr) != IS_LONG) {
					convert_to_long_ex(&retval_ptr);
				}
				length = Z_LVAL_P(retval_ptr);
				zval_ptr_dtor(&retval_ptr);
			}

			zval_ptr_dtor(argv[0]);
			zval_ptr_dtor(argv[1]);
			break;
		}
	}

	return length;
}
/* }}} */

/* {{{ curl_write_header
 */
static size_t curl_write_header(char *data, size_t size, size_t nmemb, void *ctx)
{
	php_curl       *ch  = (php_curl *) ctx;
	php_curl_write *t   = ch->handlers->write_header;
	size_t          length = size * nmemb;
	TSRMLS_FETCH_FROM_CTX(ch->thread_ctx);

	switch (t->method) {
		case PHP_CURL_STDOUT:
			/* Handle special case write when we're returning the entire transfer
			 */
			if (ch->handlers->write->method == PHP_CURL_RETURN && length > 0) {
				smart_str_appendl(&ch->handlers->write->buf, data, (int) length);
			} else {
				PHPWRITE(data, length);
			}
			break;
		case PHP_CURL_FILE:
			return fwrite(data, size, nmemb, t->fp);
		case PHP_CURL_USER: {
			zval **argv[2];
			zval  *handle = NULL;
			zval  *zdata = NULL;
			zval  *retval_ptr;
			int   error;
			zend_fcall_info fci;

			MAKE_STD_ZVAL(handle);
			MAKE_STD_ZVAL(zdata);

			ZVAL_RESOURCE(handle, ch->id);
			zend_list_addref(ch->id);
			ZVAL_STRINGL(zdata, data, length, 1);

			argv[0] = &handle;
			argv[1] = &zdata;

			fci.size = sizeof(fci);
			fci.function_table = EG(function_table);
			fci.function_name = t->func_name;
			fci.symbol_table = NULL;
			fci.object_ptr = NULL;
			fci.retval_ptr_ptr = &retval_ptr;
			fci.param_count = 2;
			fci.params = argv;
			fci.no_separation = 0;

			ch->in_callback = 1;
			error = zend_call_function(&fci, &t->fci_cache TSRMLS_CC);
			ch->in_callback = 0;
			if (error == FAILURE) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not call the CURLOPT_HEADERFUNCTION");
				length = -1;
			} else if (retval_ptr) {
				if (Z_TYPE_P(retval_ptr) != IS_LONG) {
					convert_to_long_ex(&retval_ptr);
				}
				length = Z_LVAL_P(retval_ptr);
				zval_ptr_dtor(&retval_ptr);
			}
			zval_ptr_dtor(argv[0]);
			zval_ptr_dtor(argv[1]);
			break;
		}

		case PHP_CURL_IGNORE:
			return length;

		default:
			return -1;
	}

	return length;
}
/* }}} */

int _php_curl_verify_handlers(php_curl *ch, int reporterror TSRMLS_DC) /* {{{ */
{
	php_stream *stream;
	if (!ch || !ch->handlers) {
		return 0;
	}

	if (ch->handlers->std_err) {
		stream = (php_stream *) zend_fetch_resource(&ch->handlers->std_err TSRMLS_CC, -1, NULL, NULL, 2, php_file_le_stream(), php_file_le_pstream());
		if (stream == NULL) {
			if (reporterror) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "CURLOPT_STDERR resource has gone away, resetting to stderr");
			}
			zval_ptr_dtor(&ch->handlers->std_err);
			ch->handlers->std_err = NULL;

			curl_easy_setopt(ch->cp, CURLOPT_STDERR, stderr);
		}
	}
	if (ch->handlers->read && ch->handlers->read->stream) {
		stream = (php_stream *) zend_fetch_resource(&ch->handlers->read->stream TSRMLS_CC, -1, NULL, NULL, 2, php_file_le_stream(), php_file_le_pstream());
		if (stream == NULL) {
			if (reporterror) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "CURLOPT_INFILE resource has gone away, resetting to default");
			}
			zval_ptr_dtor(&ch->handlers->read->stream);
			ch->handlers->read->fd = 0;
			ch->handlers->read->fp = 0;
			ch->handlers->read->stream = NULL;

			curl_easy_setopt(ch->cp, CURLOPT_INFILE, (void *) ch);
		}
	}
	if (ch->handlers->write_header && ch->handlers->write_header->stream) {
		stream = (php_stream *) zend_fetch_resource(&ch->handlers->write_header->stream TSRMLS_CC, -1, NULL, NULL, 2, php_file_le_stream(), php_file_le_pstream());
		if (stream == NULL) {
			if (reporterror) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "CURLOPT_WRITEHEADER resource has gone away, resetting to default");
			}
			zval_ptr_dtor(&ch->handlers->write_header->stream);
			ch->handlers->write_header->fp = 0;
			ch->handlers->write_header->stream = NULL;

			ch->handlers->write_header->method = PHP_CURL_IGNORE;
			curl_easy_setopt(ch->cp, CURLOPT_WRITEHEADER, (void *) ch);
		}
	}
	if (ch->handlers->write && ch->handlers->write->stream) {
		stream = (php_stream *) zend_fetch_resource(&ch->handlers->write->stream TSRMLS_CC, -1, NULL, NULL, 2, php_file_le_stream(), php_file_le_pstream());
		if (stream == NULL) {
			if (reporterror) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "CURLOPT_FILE resource has gone away, resetting to default");
			}
			zval_ptr_dtor(&ch->handlers->write->stream);
			ch->handlers->write->fp = 0;
			ch->handlers->write->stream = NULL;

			ch->handlers->write->method = PHP_CURL_STDOUT;
			ch->handlers->write->type   = PHP_CURL_ASCII;
			curl_easy_setopt(ch->cp, CURLOPT_FILE, (void *) ch);
		}
	}
	return 1;
}
/* }}} */

/* {{{ _php_curl_cleanup_handle(ch)
   Cleanup an execution phase */
void _php_curl_cleanup_handle(php_curl *ch)
{
	if (ch->handlers->write->buf.len > 0) {
		smart_str_free(&ch->handlers->write->buf);
	}
	if (ch->header.str_len) {
		efree(ch->header.str);
		ch->header.str_len = 0;
	}

	memset(ch->err.str, 0, CURL_ERROR_SIZE + 1);
	ch->err.no = 0;
}
/* }}} */

/* {{{ _php_curl_close()
   List destructor for curl handles */
static void _php_curl_close_ex(php_curl *ch TSRMLS_DC)
{
#if PHP_CURL_DEBUG
	fprintf(stderr, "DTOR CALLED, ch = %x\n", ch);
#endif

	_php_curl_verify_handlers(ch, 0 TSRMLS_CC);

	/* 
	 * Libcurl is doing connection caching. When easy handle is cleaned up,
	 * if the handle was previously used by the curl_multi_api, the connection 
	 * remains open un the curl multi handle is cleaned up. Some protocols are
	 * sending content like the FTP one, and libcurl try to use the 
	 * WRITEFUNCTION or the HEADERFUNCTION. Since structures used in those
	 * callback are freed, we need to use an other callback to which avoid
	 * segfaults.
	 *
	 * Libcurl commit d021f2e8a00 fix this issue and should be part of 7.28.2 
	 */
	curl_easy_setopt(ch->cp, CURLOPT_HEADERFUNCTION, curl_write_nothing);
	curl_easy_setopt(ch->cp, CURLOPT_WRITEFUNCTION, curl_write_nothing);

	curl_easy_cleanup(ch->cp);

	/* cURL destructors should be invoked only by last curl handle */
	if (Z_REFCOUNT_P(ch->clone) <= 1) {
		zend_llist_clean(&ch->to_free->str);
		zend_llist_clean(&ch->to_free->slist);
		zend_llist_clean(&ch->to_free->post);
		efree(ch->to_free);
		FREE_ZVAL(ch->clone);
	} else {
		Z_DELREF_P(ch->clone);
	}

	if (ch->handlers->write->buf.len > 0) {
		smart_str_free(&ch->handlers->write->buf);
	}
	if (ch->handlers->write->func_name) {
		zval_ptr_dtor(&ch->handlers->write->func_name);
	}
	if (ch->handlers->read->func_name) {
		zval_ptr_dtor(&ch->handlers->read->func_name);
	}
	if (ch->handlers->write_header->func_name) {
		zval_ptr_dtor(&ch->handlers->write_header->func_name);
	}
	if (ch->handlers->progress->func_name) {
		zval_ptr_dtor(&ch->handlers->progress->func_name);
	}
	if (ch->handlers->passwd) {
		zval_ptr_dtor(&ch->handlers->passwd);
	}
	if (ch->handlers->std_err) {
		zval_ptr_dtor(&ch->handlers->std_err);
	}
	if (ch->header.str_len > 0) {
		efree(ch->header.str);
	}

	if (ch->handlers->write_header->stream) {
		zval_ptr_dtor(&ch->handlers->write_header->stream);
	}
	if (ch->handlers->write->stream) {
		zval_ptr_dtor(&ch->handlers->write->stream);
	}
	if (ch->handlers->read->stream) {
		zval_ptr_dtor(&ch->handlers->read->stream);
	}

	efree(ch->handlers->write);
	efree(ch->handlers->write_header);
	efree(ch->handlers->read);
	efree(ch->handlers->progress);
	efree(ch->handlers);
	efree(ch);
}
/* }}} */

/* {{{ _php_curl_close()
   List destructor for curl handles */
static void _php_curl_close(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	php_curl *ch = (php_curl *) rsrc->ptr;
	_php_curl_close_ex(ch TSRMLS_CC);
}
/* }}} */

/* {{{ curl_free_string
 */
static void curl_free_string(void **string)
{
	efree(*string);
}
/* }}} */

/* {{{ curl_free_post
 */
static void curl_free_post(void **post)
{
	curl_formfree((struct HttpPost *) *post);
}
/* }}} */

/* {{{ curl_free_slist
 */
static void curl_free_slist(void **slist)
{
	curl_slist_free_all((struct curl_slist *) *slist);
}
/* }}} */

/* {{{ alloc_curl_handle
 */
static void alloc_curl_handle(php_curl **ch)
{
	*ch                           = emalloc(sizeof(php_curl));
	(*ch)->to_free                = ecalloc(1, sizeof(struct _php_curl_free));
	(*ch)->handlers               = ecalloc(1, sizeof(php_curl_handlers));
	(*ch)->handlers->write        = ecalloc(1, sizeof(php_curl_write));
	(*ch)->handlers->write_header = ecalloc(1, sizeof(php_curl_write));
	(*ch)->handlers->read         = ecalloc(1, sizeof(php_curl_read));
	(*ch)->handlers->progress     = ecalloc(1, sizeof(php_curl_progress));

	(*ch)->in_callback = 0;
	(*ch)->header.str_len = 0;

	memset(&(*ch)->err, 0, sizeof((*ch)->err));
	(*ch)->handlers->write->stream = NULL;
	(*ch)->handlers->write_header->stream = NULL;
	(*ch)->handlers->read->stream = NULL;

	zend_llist_init(&(*ch)->to_free->str,   sizeof(char *),            (llist_dtor_func_t) curl_free_string, 0);
	zend_llist_init(&(*ch)->to_free->slist, sizeof(struct curl_slist), (llist_dtor_func_t) curl_free_slist,  0);
	zend_llist_init(&(*ch)->to_free->post,  sizeof(struct HttpPost),   (llist_dtor_func_t) curl_free_post,   0);
}
/* }}} */

static void _php_curlm_close_persistent(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	php_curlm *mh = (php_curlm *) rsrc->ptr;
	if (mh) {
		zend_llist_position pos;
		php_curl *ch;
		zval	*pz_ch;

		for(pz_ch = (zval *)zend_llist_get_first_ex(&mh->easyh, &pos); pz_ch;
			pz_ch = (zval *)zend_llist_get_next_ex(&mh->easyh, &pos)) {

			ch = (php_curl *) zend_fetch_resource(&pz_ch TSRMLS_CC, -1, le_curl_name, NULL, 1, le_curl);
			_php_curl_verify_handlers(ch, 0 TSRMLS_CC);
		}

		curl_multi_cleanup(mh->multi);
		zend_llist_clean(&mh->easyh);
		efree(mh);
		rsrc->ptr = NULL;
	}
}

static _php_curl_multi_init()
{

}


PHP_FUNCTION(jsonrpc_server_new)
{
	zval *payload, *callbacks, *classes;
	zval *object = getThis();

	ALLOC_INIT_ZVAL(payload);
	MAKE_STD_ZVAL(callbacks);
	MAKE_STD_ZVAL(classes);

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|zzz", 
		&payload, &callbacks, &classes) == FAILURE)
	{
		RETURN_NULL();
	}

	if (Z_TYPE_P(payload) == IS_STRING){
		add_property_zval(object, "payload", payload);
	}else {
		ZVAL_NULL(payload);
		add_property_zval(object, "payload", payload);
	}

	if (Z_TYPE_P(callbacks) == IS_ARRAY){
		add_property_zval(object, "callbacks", callbacks);
	}else {
		array_init(callbacks);
		add_property_zval(object, "callbacks", callbacks);
	}
	
	if (Z_TYPE_P(classes) == IS_ARRAY){
		add_property_zval(object, "classes", classes);
	}else {
		array_init(classes);
		add_property_zval(object, "classes", classes);
	}

	//payload = NULL;
	payload = zend_read_property(
			php_jsonrpc_server_entry, getThis(), "payload", sizeof("payload")-1, 0 TSRMLS_CC
		);
	//php_var_dump(&payload, 1 TSRMLS_CC);

	//php_printf("jsonrpc_server new\n");
}

PHP_FUNCTION(jsonrpc_server_register)
{
	zval *closure=NULL;
	zval *val;
	zval *name;
	zval *callbacks;
	char *lcname;
	zend_function *fptr;
	char *name_str;
	int name_len;

	zval *object = getThis();


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zs", 
		&name, &name_str, &name_len) == FAILURE)
	{
		
	}

	//if (Z_TYPE_P(closure) == IS_OBJECT){
	//	fptr = (zend_function*)zend_get_closure_method_def(closure TSRMLS_CC);
	//	Z_ADDREF_P(closure);
	//}else if (Z_TYPE_P(closure) == IS_STRING){
		char *nsname;
		//name_str = Z_STRVAL_P(closure);
		//name_len = Z_STRLEN_p(closure);


		lcname = zend_str_tolower_dup(name_str, name_len);

		/* Ignore leading "\" */
		nsname = lcname;
		if (lcname[0] == '\\') {
			nsname = &lcname[1];
			name_len--;
		}
		
		if (zend_hash_find(EG(function_table), nsname, name_len + 1, (void **)&fptr) == FAILURE) {
			efree(lcname);
			php_error_docref(NULL TSRMLS_CC, E_WARNING, 
				"Function %s() does not exist", name_str);
			return;
		}
		efree(lcname);
	//}

	callbacks = zend_read_property(
		php_jsonrpc_server_entry, getThis(), "callbacks", sizeof("callbacks")-1, 0 TSRMLS_CC
	);

	MAKE_STD_ZVAL(val);

	zend_create_closure(val, fptr TSRMLS_CC);
	//shurrik_dump_zval(val);
	add_assoc_zval(callbacks, Z_STRVAL_P(name), val);


	RETURN_ZVAL(object,1,0);

}

PHP_FUNCTION(jsonrpc_server_execute)
{
	zval *object;
	zval *func;
	zval retval;
	zval **func_params, **exec_params;
	zval *data, *payload, *error, *id, *response;
	zval **payload_method, **payload_params;
	zval *return_val;

	object = getThis();
	
	MAKE_STD_ZVAL(error);
	MAKE_STD_ZVAL(id);
	MAKE_STD_ZVAL(data);
	array_init(error);
	array_init(id);
	array_init(data);
	
	//add_assoc_zval(response, "id", id);

	INIT_ZVAL(retval);
	MAKE_STD_ZVAL(func);
	ZVAL_STRINGL(func, "jsonformat", sizeof("jsonformat") - 1, 0);
	if (call_user_function(NULL, &object, func, &retval, 0, NULL TSRMLS_CC) == FAILURE){

	}
	if (!Z_BVAL(retval)){
		add_assoc_long(error, "code", -32700);
		add_assoc_string(error, "message", "Parse error", 0);
		add_assoc_null(id,"id");
		add_assoc_zval(data, "error", error);
		goto getresponse;
	}


	ZVAL_STRINGL(func, "rpcformat", sizeof("rpcformat") - 1, 0);
	if (call_user_function(NULL, &object, func, &retval, 0, NULL TSRMLS_CC) == FAILURE){

	}
	if (!Z_BVAL(retval)){
		add_assoc_long(error, "code", -32600);
		add_assoc_string(error, "message", "Invalid Request", 0);
		add_assoc_null(id,"id");
		add_assoc_zval(data, "error", error);
		goto getresponse;
	}

	exec_params = emalloc(sizeof(zval *) * 2);
	payload = zend_read_property(
			php_jsonrpc_server_entry, getThis(), "payload", sizeof("payload")-1, 0 TSRMLS_CC
		);
	if (zend_hash_find(Z_ARRVAL_P(payload), "method", strlen("method")+1, (void **)&payload_method) == FAILURE)
	{
	}
	//shurrik_dump_zval(*payload_method);
	exec_params[0] = *payload_method;

	if (zend_hash_find(Z_ARRVAL_P(payload), "params", strlen("params")+1, (void **)&payload_params) == FAILURE)
	{
	}
	//shurrik_dump_zval(*payload_params);
	exec_params[1] = *payload_params;

	MAKE_STD_ZVAL(return_val);
	ZVAL_STRINGL(func, "executeprocedure", sizeof("executeprocedure") - 1, 0);
	if (call_user_function(NULL, &object, func, return_val, 2, exec_params TSRMLS_CC) == FAILURE){

	}

	RETVAL_ZVAL(return_val, 1, 0);
	add_assoc_zval(data, "result", return_val);
	id = payload;
	efree(exec_params);
	goto getresponse;

	return ;

getresponse:
	func_params = emalloc(sizeof(zval *) * 2);
	func_params[0] = data;
	func_params[1] = id;
	
	ZVAL_STRINGL(func, "getresponse", sizeof("getresponse") - 1, 0);
	if (call_user_function(NULL, &object, func, &retval, 2, func_params TSRMLS_CC) == FAILURE)
	{

	}

	efree(func_params);
	ZVAL_STRINGL(return_value, Z_STRVAL(retval), Z_STRLEN(retval), 1);
	return ;

}

PHP_FUNCTION(jsonrpc_server_jformat)
{
	zval *payload, *val;
	zval *object = getThis();

	payload = zend_read_property(
			php_jsonrpc_server_entry, object, "payload", sizeof("payload")-1, 0 TSRMLS_CC
		);

	//shurrik_dump_zval(payload);


	if (Z_TYPE_P(payload) == IS_NULL){
		/*context = php_stream_context_alloc(TSRMLS_CC);
		stream = php_stream_open_wrapper_ex("php://input", "rb",
				(use_include_path ? USE_PATH : 0) | ENFORCE_SAFE_MODE | REPORT_ERRORS,
				NULL, context);

		if ((len = php_stream_copy_to_mem(stream, &contents, maxlen, 0)) > 0) {

			if (PG(magic_quotes_runtime)) {
				contents = php_addslashes(contents, len, &len, 1 TSRMLS_CC); 
			}

			ZVAL_STRINGL(payload, contents, len, 1);
		} else if (len == 0) {
			ZVAL_STRING(payload, "", 1);
		} else {
			ZVAL_NULL(payload);
		}
		php_stream_close(stream);
		zend_update_property(php_jsonrpc_server_entry, object, "payload", sizeof(payload)-1, payload TSRMLS_CC);
		*/
		
		payload = jr_file_get_contents();
		
		zend_update_property(php_jsonrpc_server_entry, object, "payload", sizeof(payload)-1, payload TSRMLS_CC);
	}
	if (Z_TYPE_P(payload) == IS_STRING){
		php_json_decode(payload, Z_STRVAL_P(payload), Z_STRLEN_P(payload), 1, 512 TSRMLS_CC);
		zend_update_property(php_jsonrpc_server_entry, object, "payload", sizeof(payload)-1, payload TSRMLS_CC);
	}
	if (Z_TYPE_P(payload) != IS_ARRAY){
		RETVAL_FALSE;
	}else {
		RETVAL_TRUE;
	}
}

PHP_FUNCTION(jsonrpc_server_rformat)
{
	zval *payload;
	zval *object;
	zval **method = NULL;
	zval **jsonrpc = NULL;
	zval **params = NULL;

	object = getThis();

	payload = zend_read_property(
			php_jsonrpc_server_entry, object, "payload", sizeof("payload")-1, 0 TSRMLS_CC
		);

	if (Z_TYPE_P(payload) != IS_ARRAY)
	{
		RETVAL_FALSE;
		return ;
	}

	if (!zend_symtable_exists(Z_ARRVAL_P(payload), "jsonrpc", strlen("jsonrpc")+1))
	{
		RETVAL_FALSE;
		return ;
	}

	if (!zend_symtable_exists(Z_ARRVAL_P(payload), "method", strlen("method")+1))
	{
		RETVAL_FALSE;
		return ;
	}

	//MAKE_STD_ZVAL(&method);
	if (zend_hash_find(Z_ARRVAL_P(payload), "method", strlen("method")+1, (void **)&method) == FAILURE)
	{
		RETVAL_FALSE;
		return ;
	}

	if (Z_TYPE_PP(method) != IS_STRING)
	{
		RETVAL_FALSE;
		return ;
	}

	//MAKE_STD_ZVAL(&jsonrpc);
	if (zend_hash_find(Z_ARRVAL_P(payload), "jsonrpc", strlen("jsonrpc")+1, (void **)&jsonrpc) == FAILURE)
	{
		RETVAL_FALSE;
		return ;
	}

	if (strcmp(Z_STRVAL_PP(jsonrpc),"2.0") != 0)
	{
		RETVAL_FALSE;
		return ;
	}

	
	//MAKE_STD_ZVAL(&params);
	if (zend_hash_find(Z_ARRVAL_P(payload), "params", strlen("params")+1, (void **)&params) == FAILURE)
	{
		RETVAL_FALSE;
		return ;
	}
	if (Z_TYPE_PP(params) != IS_ARRAY)
	{
		RETVAL_FALSE;
		return ;
	}

	RETVAL_TRUE;
	return ;

}

PHP_FUNCTION(jsonrpc_server_execute_procedure)
{
	zval *procedure, *params;
	zval *callbacks;
	zval *object;
	zval **func_params;
	zval *func;
	zval *retval;
	zval **procedure_params;

	object = getThis();


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "za", 
		&procedure, &params) == FAILURE)
	{	
		
	}

	callbacks = zend_read_property(
			php_jsonrpc_server_entry, object, "callbacks", sizeof("callbacks")-1, 0 TSRMLS_CC
		);

	if (zend_symtable_exists(Z_ARRVAL_P(callbacks), Z_STRVAL_P(procedure), Z_STRLEN_P(procedure) + 1))
	{
		if (zend_hash_find(Z_ARRVAL_P(callbacks), Z_STRVAL_P(procedure), Z_STRLEN_P(procedure)+1, (void **)&procedure_params) == FAILURE)
		{

		}

		MAKE_STD_ZVAL(retval);
		MAKE_STD_ZVAL(func);
		ZVAL_STRINGL(func, "executecallback", sizeof("executecallback") - 1, 0);
		func_params = emalloc(sizeof(zval *) * 2);
		//shurrik_dump_zval(*procedure_params);
		func_params[0] = *procedure_params;
		func_params[1] = params;
		if (call_user_function(NULL, &object, func, retval, 2, func_params TSRMLS_CC) == FAILURE){
		}

		efree(func_params);

		RETVAL_ZVAL(retval, 1, 0);
		/*if (Z_TYPE_P(retval) == IS_BOOL){
			RETVAL_FALSE;
		}else {
			RETVAL_NULL();
		}*/
		return ;
	}
	
}

PHP_FUNCTION(jsonrpc_server_execute_callback)
{
	zval *closure = NULL;
	zval *params;
	zval *arguments;
	zval *robj;
	zval *func;
	zval *reflection;
	
	int func_params_num;
	zval **func_params;

	zval *method_params;
	zval *nb_required_params;
	zval *nb_max_params;

	zval *closure_result_ptr = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", 
		&closure, &params) == SUCCESS)
	{
		//fptr = (zend_function*)zend_get_closure_method_def(closure TSRMLS_CC);
		//Z_ADDREF_P(closure);
	}

	if (!closure || !zend_is_callable(closure, 0, NULL TSRMLS_CC))
	{
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "closeure is not expected to be a valid callback");
		RETVAL_FALSE;
		return;
	}

	MAKE_STD_ZVAL(closure_result_ptr);

	func_params_num = php_count_recursive(params, 0 TSRMLS_CC);
	func_params = emalloc(sizeof(zval *) * func_params_num);

	zval              **current;
	HashTable          *ph;
	int i = 0;

	ph = HASH_OF(params);
	if (!ph) {
	}

	for (zend_hash_internal_pointer_reset(ph);
		 zend_hash_get_current_data(ph, (void **) &current) == SUCCESS;
		 zend_hash_move_forward(ph)
	) {
		SEPARATE_ZVAL(current);
		func_params[i] = *current;

		i++;
	}

	//shurrik_dump_zval(closure);
	if (call_user_function(EG(function_table), NULL, closure, closure_result_ptr, func_params_num, func_params TSRMLS_CC) == FAILURE)
	{
	}

	RETVAL_ZVAL(closure_result_ptr, 1, 0);


	/*MAKE_STD_ZVAL(robj)
	object_init_ex(robj,reflection_function_ptr);

	MAKE_STD_ZVAL(func);
	ZVAL_STRINGL(func, "__construct", sizeof("__construct") - 1, 0);

	func_params = emalloc(sizeof(zval *) * 2);
	func_params[0] = closure;

	if (call_user_function(NULL, &robj, func, reflection, 1, func_params TSRMLS_CC) == FAILURE){

	}

	ZVAL_STRINGL(func, "getParameters", sizeof("getParameters") - 1, 0);
	if (call_user_function(NULL, &reflection, func, method_params, 0, NULL TSRMLS_CC) == FAILURE)
	{

	}

	ZVAL_STRINGL(func, "getNumberOfRequiredParameters", sizeof("getNumberOfRequiredParameters") - 1, 0);
	if (call_user_function(NULL, &reflection, func, nb_required_params, 0, NULL TSRMLS_CC) == FAILURE)
	{

	}

	ZVAL_STRINGL(func, "getNumberOfParameters", sizeof("getNumberOfParameters") - 1, 0);
	if (call_user_function(NULL, &reflection, func, nb_max_params, 0, NULL TSRMLS_CC) == FAILURE)
	{

	}

	MAKE_STD_ZVAL(arguments);
	arguments = jr_server_get_arguments(
			params,
			method_params,
			Z_LVAL_p(nb_required_params),
			Z_LVAL_p(nb_max_params)
		);

	if (Z_TYPE_P(arguments) == IS_BOOL)
	{
		RETVAL_FALSE;
	}

	efree(func_params);
	zval_ptr_dtor(&robj);
	*/
}

PHP_FUNCTION(jsonrpc_server_getresponse)
{
	zval *data;
	zval *key;
	HashTable *payload;
	zval *response;
	zval **id = NULL;
	smart_str buf = {0};
	sapi_header_line ctr = {0};

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "aH", 
		&data, &payload) == FAILURE)
	{	
		return ;
	}

	MAKE_STD_ZVAL(key);
	ZVAL_STRING(key, "id", 0);
	if (!zend_symtable_exists(payload, Z_STRVAL_P(key), Z_STRLEN_P(key) + 1))
	{
		RETVAL_STRING("", 0);
		return ;
	}

	MAKE_STD_ZVAL(response);
	array_init(response);
	add_assoc_string(response, "jsonrpc", "2.0", 0);
	
	if (zend_hash_find(payload, "id", 3, (void **)&id ) == FAILURE)
	{
		RETVAL_STRING("", 0);
		return ;
	}

	if (Z_TYPE_PP(id) == IS_NULL){
		add_assoc_null(response, "id");
	}else if(Z_TYPE_PP(id) == IS_STRING){
		convert_to_string(*id);
		add_assoc_string(response, "id", Z_STRVAL_PP(id), 0);
	}

	zend_hash_merge(Z_ARRVAL_P(response), Z_ARRVAL_P(data), NULL, NULL, sizeof(zval *), 1);

	ctr.line = "Content-Type: application/json";
	ctr.line_len = strlen(ctr.line);
	sapi_header_op(SAPI_HEADER_REPLACE, &ctr TSRMLS_CC);

	php_json_encode(&buf, response, 0 TSRMLS_CC);
	ZVAL_STRINGL(return_value, buf.c, buf.len, 1);
	smart_str_free(&buf);

}

PHP_FUNCTION(jsonrpc_client_new)
{
	CURL *cp;
	php_curl	*ch;
	zval		*clone;
	char		*url = NULL;
	int		url_len = 0;
	char *cainfo;

	zval *_url;
	zval *timeout_c;
	zval *headers;
	zval *object;
	zval *_headers;
	zval *_ch;

	object = getThis();

	MAKE_STD_ZVAL(timeout_c);
	ZVAL_LONG(timeout_c, 5);

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|zz",
		&_url, &timeout_c, &headers) == FAILURE)
	{

	}

	MAKE_STD_ZVAL(_headers);
	array_init(_headers);
	add_index_string(_headers, 0, estrdup("Content-Type: application/json"), 0);
	add_next_index_string(_headers, estrdup("Accept: application/json"), 0);

	add_property_zval(object, "url", _url);
	
	if (Z_TYPE_P(timeout_c) == IS_LONG){
		add_property_zval(object, "timeout", timeout_c);
	}else {
		ZVAL_LONG(timeout_c, 5);
		add_property_zval(object, "timeout", timeout_c);
	}

	if (Z_TYPE_P(headers) == IS_ARRAY){
		zend_hash_merge(Z_ARRVAL_P(_headers), Z_ARRVAL_P(headers), NULL, NULL, sizeof(zval *), 1);
		add_property_zval(object, "headers", _headers);
	}else {
		add_property_zval(object, "headers", _headers);
	}
	

	cp = curl_easy_init();
	if (!cp){
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not initialize a new cURL handle");
		RETURN_FALSE;
	}

	alloc_curl_handle(&ch);
	TSRMLS_SET_CTX(ch->thread_ctx);

	ch->cp = cp;

	ch->handlers->write->method = PHP_CURL_STDOUT;
	ch->handlers->write->type   = PHP_CURL_ASCII;
	ch->handlers->read->method  = PHP_CURL_DIRECT;
	ch->handlers->write_header->method = PHP_CURL_IGNORE;

	ch->uses = 0;

	MAKE_STD_ZVAL(clone);
	ch->clone = clone;

	curl_easy_setopt(ch->cp, CURLOPT_NOPROGRESS,        1);
	curl_easy_setopt(ch->cp, CURLOPT_VERBOSE,           0);
	curl_easy_setopt(ch->cp, CURLOPT_ERRORBUFFER,       ch->err.str);
	curl_easy_setopt(ch->cp, CURLOPT_WRITEFUNCTION,     curl_write);
	curl_easy_setopt(ch->cp, CURLOPT_FILE,              (void *) ch);
	curl_easy_setopt(ch->cp, CURLOPT_READFUNCTION,      curl_read);
	curl_easy_setopt(ch->cp, CURLOPT_INFILE,            (void *) ch);
	curl_easy_setopt(ch->cp, CURLOPT_HEADERFUNCTION,    curl_write_header);
	curl_easy_setopt(ch->cp, CURLOPT_WRITEHEADER,       (void *) ch);
	curl_easy_setopt(ch->cp, CURLOPT_DNS_USE_GLOBAL_CACHE, 1);
	curl_easy_setopt(ch->cp, CURLOPT_DNS_CACHE_TIMEOUT, 120);
	curl_easy_setopt(ch->cp, CURLOPT_MAXREDIRS, 20); /* prevent infinite redirects */

	cainfo = INI_STR("curl.cainfo");
	if (cainfo && strlen(cainfo) > 0) {
		curl_easy_setopt(ch->cp, CURLOPT_CAINFO, cainfo);
	}

#if defined(ZTS)
	curl_easy_setopt(ch->cp, CURLOPT_NOSIGNAL, 1);
#endif

	if (url) {
		if (!php_curl_option_url(ch, url, url_len)) {
			_php_curl_close_ex(ch TSRMLS_CC);
			RETURN_FALSE;
		}
	}

	MAKE_STD_ZVAL(_ch);
	ZEND_REGISTER_RESOURCE(_ch, ch, le_curl);
	ch->id = Z_LVAL_P(_ch);
	add_property_resource(object, "ch", ch->id);

}

PHP_FUNCTION(jsonrpc_client_del)
{
	zval		*zid;
	php_curl	*ch;
	zval *object;

	object = getThis();

	zid = zend_read_property(
			php_jsonrpc_server_entry, object, "ch", sizeof("ch")-1, 0 TSRMLS_CC
		);

	ZEND_FETCH_RESOURCE(ch, php_curl *, &zid, -1, le_curl_name, le_curl);

	if (ch->in_callback) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Attempt to close cURL handle from a callback");
		return;
	}

	if (ch->uses) {
		ch->uses--;
	} else {
		zend_list_delete(Z_LVAL_P(zid));
	}
}

PHP_FUNCTION(jsonrpc_client_execute)
{
	zval *procedure;
	zval *params;
	zval *request;
	zval *response;

	zval *object;
	zval *func;
	zval **func_params;

	object = getThis();

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz",
		&procedure, &params) == FAILURE)
	{

	}

	MAKE_STD_ZVAL(response);
	MAKE_STD_ZVAL(request);
	request = jr_client_prepare_request(procedure, params);

	func_params = emalloc(sizeof(zval *) * 1);
	func_params[0] = request;

	MAKE_STD_ZVAL(func);
	ZVAL_STRINGL(func, "dorequest", sizeof("dorequest") - 1, 0);

	if (call_user_function(NULL, &object, func, response, 1, func_params TSRMLS_CC) == FAILURE)
	{

	}
	efree(func_params);

	RETVAL_ZVAL(response, 1, 0);

}

PHP_FUNCTION(jsonrpc_client_dorequest)
{
	zval *payload;
	zval		*zid;
	php_curl	*ch;

	zval *url, *timeout, *headers;

	zval *object = getThis();

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", 
		&payload) == FAILURE)
	{
	}

	zid = zend_read_property(
			php_jsonrpc_server_entry, object, "ch", sizeof("ch")-1, 0 TSRMLS_CC
		);
	url = zend_read_property(
			php_jsonrpc_server_entry, object, "url", sizeof("url")-1, 0 TSRMLS_CC
		);
	timeout = zend_read_property(
			php_jsonrpc_server_entry, object, "timeout", sizeof("timeout")-1, 0 TSRMLS_CC
		);
	headers = zend_read_property(
			php_jsonrpc_server_entry, object, "headers", sizeof("headers")-1, 0 TSRMLS_CC
		);

	zval              **current;
	HashTable          *ph;
	struct curl_slist  *slist = NULL;

	ph = HASH_OF(headers);
	if (!ph) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "You must pass either an object or an array with headers");
		RETVAL_FALSE;
		return ;
	}
	
	for (zend_hash_internal_pointer_reset(ph);
		 zend_hash_get_current_data(ph, (void **) &current) == SUCCESS;
		 zend_hash_move_forward(ph)
	) {
		SEPARATE_ZVAL(current);
		convert_to_string_ex(current);

		slist = curl_slist_append(slist, Z_STRVAL_PP(current));
		if (!slist) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not build curl_slist");
			RETVAL_FALSE;
			return ;
		}
	}
	//zend_llist_add_element(&ch->to_free->slist, &slist);

	smart_str buf = {0};

	php_json_encode(&buf, payload, 0 TSRMLS_CC);
	ZVAL_STRINGL(payload, buf.c, buf.len, 1);
	smart_str_free(&buf);


	ZEND_FETCH_RESOURCE(ch, php_curl *, &zid, -1, le_curl_name, le_curl);

	//curl_easy_setopt(ch->cp, CURLOPT_URL, Z_STRVAL_P(url));
	if (!php_curl_option_url(ch, Z_STRVAL_P(url), Z_STRLEN_P(url)) ) {
			_php_curl_close_ex(ch TSRMLS_CC);
			RETVAL_FALSE;
			return ;
	}
	curl_easy_setopt(ch->cp, CURLOPT_HEADER, 0);
	//curl_easy_setopt(ch->cp, CURLOPT_RETURNTRANSFER, 1);
	ch->handlers->write->method = PHP_CURL_RETURN;

	convert_to_long_ex(&timeout);
	curl_easy_setopt(ch->cp, CURLOPT_CONNECTTIMEOUT, Z_LVAL_P(timeout));
	curl_easy_setopt(ch->cp, CURLOPT_USERAGENT, "JSON-RPC PHP Client");
	curl_easy_setopt(ch->cp, CURLOPT_HTTPHEADER, slist);
	curl_easy_setopt(ch->cp, CURLOPT_FOLLOWLOCATION, 0);
	curl_easy_setopt(ch->cp, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(ch->cp, CURLOPT_SSL_VERIFYPEER, 1);

	convert_to_string_ex(&payload);
	curl_easy_setopt(ch->cp, CURLOPT_POSTFIELDS, Z_STRVAL_P(payload));
	curl_easy_setopt(ch->cp, CURLOPT_POSTFIELDSIZE, Z_STRLEN_P(payload));

	/* curl_exec */
	zval *http_body;
	zval *response;
	CURLcode	error;

	MAKE_STD_ZVAL(http_body);
	MAKE_STD_ZVAL(response);

	_php_curl_verify_handlers(ch, 1 TSRMLS_CC);

	_php_curl_cleanup_handle(ch);

	error = curl_easy_perform(ch->cp);
	SAVE_CURL_ERROR(ch, error);
	/* CURLE_PARTIAL_FILE is returned by HEAD requests */
	if (error != CURLE_OK && error != CURLE_PARTIAL_FILE) {
		if (ch->handlers->write->buf.len > 0) {
			smart_str_free(&ch->handlers->write->buf);
		}
		//RETURN_FALSE;
		RETVAL_FALSE;
		return ;
	}

	if (ch->handlers->std_err) {
		php_stream  *stream;
		stream = (php_stream*)zend_fetch_resource(&ch->handlers->std_err TSRMLS_CC, -1, NULL, NULL, 2, php_file_le_stream(), php_file_le_pstream());
		if (stream) {
			php_stream_flush(stream);
		}
	}

	if (ch->handlers->write->method == PHP_CURL_RETURN && ch->handlers->write->buf.len > 0) {
		smart_str_0(&ch->handlers->write->buf);
		ZVAL_STRINGL(http_body, ch->handlers->write->buf.c, ch->handlers->write->buf.len, 1);
		goto doresponse;
	}

	/* flush the file handle, so any remaining data is synched to disk */
	if (ch->handlers->write->method == PHP_CURL_FILE && ch->handlers->write->fp) {
		fflush(ch->handlers->write->fp);
	}
	if (ch->handlers->write_header->method == PHP_CURL_FILE && ch->handlers->write_header->fp) {
		fflush(ch->handlers->write_header->fp);
	}

	if (ch->handlers->write->method == PHP_CURL_RETURN) {
		ZVAL_STRING(http_body, "", 1);
		goto doresponse;
	} else {
		//RETURN_TRUE;
		RETVAL_TRUE;
		return ;
	}

doresponse:
	array_init(response);
	php_json_decode(response, Z_STRVAL_P(http_body), Z_STRLEN_P(http_body), 1, 512 TSRMLS_CC);

	if (Z_TYPE_P(response) == IS_ARRAY)
	{
		RETVAL_ZVAL(response, 1, 0);
		return ;
	} else {
		RETVAL_ZVAL(response, 1, 0);
		return ;
	}

}

static zend_function_entry jsonrpc_server_class_functions[] = {
	PHP_FALIAS(__construct, jsonrpc_server_new, NULL)
	PHP_FALIAS(register, jsonrpc_server_register, NULL)
	PHP_FALIAS(execute, jsonrpc_server_execute, NULL)
	PHP_FALIAS(jsonformat, jsonrpc_server_jformat, NULL)
	PHP_FALIAS(rpcformat, jsonrpc_server_rformat, NULL)
	PHP_FALIAS(executeprocedure, jsonrpc_server_execute_procedure, NULL)
	PHP_FALIAS(executecallback, jsonrpc_server_execute_callback, NULL)
	PHP_FALIAS(getresponse, jsonrpc_server_getresponse, NULL)
	{NULL, NULL, NULL}
};

static zend_function_entry jsonrpc_client_class_functions[] = {
	PHP_FALIAS(__construct, jsonrpc_client_new, NULL)
	PHP_FALIAS(__destruct, jsonrpc_client_del, NULL)
	PHP_FALIAS(execute, jsonrpc_client_execute, NULL)
	PHP_FALIAS(dorequest, jsonrpc_client_dorequest, NULL)
	{NULL, NULL, NULL}
};

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(jsonrpc)
{
	le_curl = zend_register_list_destructors_ex(_php_curl_close, NULL, "curl", module_number);
	//le_curl_multi_handle = zend_register_list_destructors_ex(NULL, _php_curlm_close_persistent, "curl_multi", module_number);
	/* If you have INI entries, uncomment these lines 
	REGISTER_INI_ENTRIES();
	*/

	zend_class_entry jsonrpc_server_class_entry;
	INIT_CLASS_ENTRY(jsonrpc_server_class_entry, "Jsonrpc_Server", jsonrpc_server_class_functions);
	php_jsonrpc_server_entry = zend_register_internal_class(&jsonrpc_server_class_entry TSRMLS_CC);

	zend_class_entry jsonrpc_client_class_entry;
	INIT_CLASS_ENTRY(jsonrpc_client_class_entry, "Jsonrpc_Client", jsonrpc_client_class_functions);
	php_jsonrpc_client_entry = zend_register_internal_class(&jsonrpc_client_class_entry TSRMLS_CC);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(jsonrpc)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(jsonrpc)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(jsonrpc)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(jsonrpc)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "jsonrpc support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */


/* Remove the following function when you have succesfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_jsonrpc_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(confirm_jsonrpc_compiled)
{
	char *arg = NULL;
	int arg_len, len;
	char *strg;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &arg_len) == FAILURE) {
		return;
	}

	len = spprintf(&strg, 0, "Congratulations! You have successfully modified ext/%.78s/config.m4. Module %.78s is now compiled into PHP.", "jsonrpc", arg);
	RETURN_STRINGL(strg, len, 0);
}
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
