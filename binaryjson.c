/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2012 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_binaryjson.h"
#include "binaryjson.h"
#include "id.h"
#include "zend_exceptions.h"

ZEND_DECLARE_MODULE_GLOBALS(binaryjson)

ZEND_BEGIN_ARG_INFO_EX(arginfo_binaryjson_encode, 0, 0, 1)
ZEND_ARG_INFO(0, mixed_var)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_binaryjson_decode, 0, 0, 1)
ZEND_ARG_INFO(0, binary_str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_binaryjson_header_pack, 0, 0, 1)
ZEND_ARG_INFO(0, opcode)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_binaryjson_header_unpack, 0, 0, 1)
ZEND_ARG_INFO(0, binary_str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_binaryjson_msg_pack, 0, 0, 1)
ZEND_ARG_INFO(0, arr_var)
ZEND_ARG_INFO(0, opcode)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_binaryjson_msg_unpack, 0, 0, 1)
ZEND_ARG_INFO(0, binary_str)
ZEND_END_ARG_INFO()


#ifdef WIN32
#  include <memory.h>
#  ifndef int64_t
     typedef __int64 int64_t;
#  endif
#endif

#if PHP_VERSION_ID >= 50300
static int apply_func_args_wrapper(void **data TSRMLS_DC, int num_args, va_list args, zend_hash_key *key);
#else
static int apply_func_args_wrapper(void **data, int num_args, va_list args, zend_hash_key *key);
#endif

static int is_utf8(const char *s, int len);

/* {{{ binaryjson_functions[]
 *
 * Every user visible function must have an entry in binaryjson_functions[].
 */
const zend_function_entry binaryjson_functions[] = {
	PHP_FE(binaryjson_encode,	     arginfo_binaryjson_encode)
	PHP_FE(binaryjson_decode,	     arginfo_binaryjson_decode)
    PHP_FE(binaryjson_header_pack,   arginfo_binaryjson_header_pack)
    PHP_FE(binaryjson_header_unpack, arginfo_binaryjson_header_unpack)
    PHP_FE(binaryjson_msg_pack,      arginfo_binaryjson_msg_pack)
    PHP_FE(binaryjson_msg_unpack,    arginfo_binaryjson_msg_unpack)
	PHP_FE_END	/* Must be the last line in binaryjson_functions[] */
};
/* }}} */

/* {{{ binaryjson_module_entry
 */
zend_module_entry binaryjson_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"binaryjson",
	binaryjson_functions,
	PHP_MINIT(binaryjson),
	PHP_MSHUTDOWN(binaryjson),
	PHP_RINIT(binaryjson),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(binaryjson),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(binaryjson),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_BINARYJSON_VERSION, /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_BINARYJSON
ZEND_GET_MODULE(binaryjson)
#endif

/* }}} */

static void php_binaryjson_init_globals(zend_binaryjson_globals *binaryjson_globals)
{
    binaryjson_globals->request_id = 3;
    
	/* On windows, the max length is 256. Linux doesn't have a limit, but it
	 * will fill in the first 256 chars of hostname even if the actual
	 * hostname is longer. If you can't get a unique character in the first
	 * 256 chars of your hostname, you're doing it wrong. */
	int len, win_max = 256;
	char *hostname, host_start[256];
	register ulong hash;

	hostname = host_start;
	/* from the gnu manual:
	 *     gethostname stores the beginning of the host name in name even if the
	 *     host name won't entirely fit. For some purposes, a truncated host name
	 *     is good enough. If it is, you can ignore the error code.
	 * So we'll ignore the error code.
	 * Returns 0-terminated hostname. */
	gethostname(hostname, win_max);
	len = strlen(hostname);

	hash = 5381;

	/* from zend_hash.h */
	/* variant with the hash unrolled eight times */
	for (; len >= 8; len -= 8) {
		hash = ((hash << 5) + hash) + *hostname++;
		hash = ((hash << 5) + hash) + *hostname++;
		hash = ((hash << 5) + hash) + *hostname++;
		hash = ((hash << 5) + hash) + *hostname++;
		hash = ((hash << 5) + hash) + *hostname++;
		hash = ((hash << 5) + hash) + *hostname++;
		hash = ((hash << 5) + hash) + *hostname++;
		hash = ((hash << 5) + hash) + *hostname++;
	}

	switch (len) {
		case 7: hash = ((hash << 5) + hash) + *hostname++; /* fallthrough... */
		case 6: hash = ((hash << 5) + hash) + *hostname++; /* fallthrough... */
		case 5: hash = ((hash << 5) + hash) + *hostname++; /* fallthrough... */
		case 4: hash = ((hash << 5) + hash) + *hostname++; /* fallthrough... */
		case 3: hash = ((hash << 5) + hash) + *hostname++; /* fallthrough... */
		case 2: hash = ((hash << 5) + hash) + *hostname++; /* fallthrough... */
		case 1: hash = ((hash << 5) + hash) + *hostname++; break;
		case 0: break;
	}

	binaryjson_globals->machine = hash;

	binaryjson_globals->inc = rand() & 0xFFFFFF;
}

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(binaryjson)
{
    REGISTER_LONG_CONSTANT("BINARYJSON_MSG_HEADER_SIZE"      , MSG_HEADER_SIZE  , CONST_CS | CONST_PERSISTENT);
    
    REGISTER_LONG_CONSTANT("BINARYJSON_HEADER_OP_REPLY"      , OP_REPLY         , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINARYJSON_HEADER_OP_MSG"        , OP_MSG           , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINARYJSON_HEADER_OP_UPDATE"     , OP_UPDATE        , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINARYJSON_HEADER_OP_INSERT"     , OP_INSERT        , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINARYJSON_HEADER_OP_GET_BY_OID" , OP_GET_BY_OID    , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINARYJSON_HEADER_OP_QUERY"      , OP_QUERY         , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINARYJSON_HEADER_OP_GET_MORE"   , OP_GET_MORE      , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINARYJSON_HEADER_OP_DELETE"     , OP_DELETE        , CONST_CS | CONST_PERSISTENT);

    ZEND_INIT_MODULE_GLOBALS(binaryjson, php_binaryjson_init_globals, NULL);
    
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(binaryjson)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(binaryjson)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(binaryjson)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(binaryjson)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "binaryjson support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */

static int php_binaryjson_serialize_size(char *start, buffer *buf, int max_size TSRMLS_DC)
{
	int total = MONGO_32((buf->pos - start));

	if (buf->pos - start > max_size) {
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 3 TSRMLS_CC, "document fragment is too large: %d, max: %d", buf->pos - start, max_size);
		return FAILURE;
	}
	memcpy(start, &total, INT_32);
	return SUCCESS;
}

/* serialize a zval */
int zval_to_binaryjson(buffer *buf, HashTable *hash, int prep, int max_document_size TSRMLS_DC)
{
	uint start;
	int num = 0;

	/* check buf size */
	if (BUF_REMAINING <= 5) {
		binaryjson_resize_buf(buf, 5);
	}

	/* keep a record of the starting position as an offset, in case the memory
	 * is resized */
	start = buf->pos-buf->start;

	/* skip first 4 bytes to leave room for size */
	buf->pos += INT_32;

	if (zend_hash_num_elements(hash) > 0) {
    	if (prep) {
			prep_id_for_binaryjson(buf, hash TSRMLS_CC);
			num++;
		}
#if PHP_VERSION_ID >= 50300
		zend_hash_apply_with_arguments(hash TSRMLS_CC, (apply_func_args_t)apply_func_args_wrapper, 3, buf, prep, &num);
#else
		zend_hash_apply_with_arguments(hash, (apply_func_args_t)apply_func_args_wrapper, 4, buf, prep, &num TSRMLS_CC);
#endif
	}
	php_binaryjson_serialize_null(buf);
	php_binaryjson_serialize_size(buf->start + start, buf, max_document_size TSRMLS_CC);
	return EG(exception) ? FAILURE : num;
}

#if defined(_MSC_VER)
# define strtoll(s, f, b) _atoi64(s)
#elif !defined(HAVE_STRTOLL)
# if defined(HAVE_ATOLL)
#  define strtoll(s, f, b) atoll(s)
# else
#  define strtoll(s, f, b) strtol(s, f, b)
# endif
#endif

void php_binaryjson_serialize_byte(buffer *buf, char b)
{
	if (BUF_REMAINING <= 1) {
		binaryjson_resize_buf(buf, 1);
	}
	*(buf->pos) = b;
	buf->pos += 1;
}

void php_binaryjson_serialize_bytes(buffer *buf, char *str, int str_len)
{
	if (BUF_REMAINING <= str_len) {
		binaryjson_resize_buf(buf, str_len);
	}
	memcpy(buf->pos, str, str_len);
	buf->pos += str_len;
}

void php_binaryjson_serialize_string(buffer *buf, char *str, int str_len)
{
	if (BUF_REMAINING <= str_len + 1) {
		binaryjson_resize_buf(buf, str_len + 1);
	}

	memcpy(buf->pos, str, str_len);
	/* add \0 at the end of the string */
	buf->pos[str_len] = 0;
	buf->pos += str_len + 1;
}

void php_binaryjson_serialize_int(buffer *buf, int num)
{
	int i = MONGO_32(num);

	if (BUF_REMAINING <= INT_32) {
		binaryjson_resize_buf(buf, INT_32);
	}
	memcpy(buf->pos, &i, INT_32);
	buf->pos += INT_32;
}

void php_binaryjson_serialize_long(buffer *buf, int64_t num)
{
	int64_t i = MONGO_64(num);

	if (BUF_REMAINING <= INT_64) {
		binaryjson_resize_buf(buf, INT_64);
	}

	memcpy(buf->pos, &i, INT_64);
	buf->pos += INT_64;
}

void php_binaryjson_serialize_double(buffer *buf, double num)
{
	int64_t dest, *dest_p;
	dest_p = &dest;
	memcpy(dest_p, &num, 8);
	dest = MONGO_64(dest);

	if (BUF_REMAINING <= INT_64) {
		binaryjson_resize_buf(buf, INT_64);
	}
	memcpy(buf->pos, dest_p, DOUBLE_64);
	buf->pos += DOUBLE_64;
}

/*
 * prep == true
 * we are inserting, so keys can't have .'s in them
 */
void php_binaryjson_serialize_key(buffer *buf, const char *str, int str_len, int prep TSRMLS_DC)
{
	if (str[0] == '\0') {
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 1 TSRMLS_CC, "zero-length keys are not allowed, did you use $ with double quotes?");
		return;
	}

	if (BUF_REMAINING <= str_len + 1) {
		binaryjson_resize_buf(buf, str_len + 1);
	}

	if (memchr(str, '\0', str_len) != NULL) {
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 2 TSRMLS_CC, "'\\0' not allowed in key: %s\\0...", str);
		return;
	}

	if (prep && (strchr(str, '.') != 0)) {
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 2 TSRMLS_CC, "'.' not allowed in key: %s", str);
		return;
	}

    memcpy(buf->pos, str, str_len);

	/* add \0 at the end of the string */
	buf->pos[str_len] = 0;
	buf->pos += str_len + 1;
}

void binaryjson_buf_init(char *dest)
{
	dest[0] = '\0';
}

void binaryjson_buf_append(char *dest, char *piece)
{
	int pos = strlen(dest);
	memcpy(dest + pos, piece, strlen(piece) + 1);
}

int binaryjson_resize_buf(buffer *buf, int size)
{
	int total = buf->end - buf->start;
	int used = buf->pos - buf->start;

	total = total < GROW_SLOWLY ? total*2 : total + INITIAL_BUF_SIZE;
	while (total-used < size) {
		total += size;
	}

	buf->start = (char*)erealloc(buf->start, total);
	buf->pos = buf->start + used;
	buf->end = buf->start + total;
	return total;
}

#if PHP_VERSION_ID >= 50300
static int apply_func_args_wrapper(void **data TSRMLS_DC, int num_args, va_list args, zend_hash_key *key)
#else
static int apply_func_args_wrapper(void **data, int num_args, va_list args, zend_hash_key *key)
#endif
{
	buffer *buf = va_arg(args, buffer*);
	int prep = va_arg(args, int);
	int *num = va_arg(args, int*);
#if ZTS && PHP_VERSION_ID < 50300
	void ***tsrm_ls = va_arg(args, void***);
#endif

	if (key->nKeyLength) {
		return php_binaryjson_serialize_element(key->arKey, key->nKeyLength - 1, (zval**)data, buf, prep TSRMLS_CC);
	} else {
		long current = key->h;
		int pos = 29, negative = 0;
		char name[30];

		/* If the key is a number in ascending order, we're still dealing with
		 * an array, not an object, so increase the count */
		if (key->h == (unsigned int)*num) {
			(*num)++;
		}

		name[pos--] = '\0';

		/* take care of negative indexes */
		if (current < 0) {
			current *= -1;
			negative = 1;
		}

		do {
			int digit = current % 10;
			digit += (int)'0';
			name[pos--] = (char)digit;
			current = current / 10;
		} while (current > 0);

		if (negative) {
			name[pos--] = '-';
		}
		return php_binaryjson_serialize_element(name + pos + 1, strlen(name + pos + 1), (zval**)data, buf, prep TSRMLS_CC);
	}
}

void php_binaryjson_serialize_ns(buffer *buf, char *str TSRMLS_DC)
{
	if (BUF_REMAINING <= (int)strlen(str) + 1) {
		binaryjson_resize_buf(buf, strlen(str) + 1);
	}
    memcpy(buf->pos, str, strlen(str));
    buf->pos[strlen(str)] = 0;
    buf->pos += strlen(str) + 1;
}

int php_binaryjson_serialize_element(const char *name, int name_len, zval **data, buffer *buf, int prep TSRMLS_DC)
{
	if (prep && strcmp(name, "_id") == 0) {
		return ZEND_HASH_APPLY_KEEP;
	}

	switch (Z_TYPE_PP(data)) {
		case IS_NULL:
			PHP_BINARYJSON_SERIALIZE_KEY(BSON_NULL);
			break;

		case IS_LONG:
            PHP_BINARYJSON_SERIALIZE_KEY(BSON_INT);
            php_binaryjson_serialize_int(buf, Z_LVAL_PP(data));
			break;

		case IS_DOUBLE:
			PHP_BINARYJSON_SERIALIZE_KEY(BSON_DOUBLE);
			php_binaryjson_serialize_double(buf, Z_DVAL_PP(data));
			break;

		case IS_BOOL:
			PHP_BINARYJSON_SERIALIZE_KEY(BSON_BOOL);
			php_binaryjson_serialize_bool(buf, Z_BVAL_PP(data));
			break;

		case IS_STRING: {
			PHP_BINARYJSON_SERIALIZE_KEY(BSON_STRING);

			/* if this is not a valid string, stop */
			if (!is_utf8(Z_STRVAL_PP(data), Z_STRLEN_PP(data))) {
				zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 12 TSRMLS_CC, "non-utf8 string: %s", Z_STRVAL_PP(data));
				return ZEND_HASH_APPLY_STOP;
			}
			php_binaryjson_serialize_int(buf, Z_STRLEN_PP(data) + 1);
			php_binaryjson_serialize_string(buf, Z_STRVAL_PP(data), Z_STRLEN_PP(data));
			break;
		}

		case IS_ARRAY: {
			int num;
			/* if we realloc, we need an offset, not an abs pos (phew) */
			int type_offset = buf->pos-buf->start;

			/* serialize */
			PHP_BINARYJSON_SERIALIZE_KEY(BSON_ARRAY);
			num = zval_to_binaryjson(buf, Z_ARRVAL_PP(data), NO_PREP, DEFAULT_MAX_DOCUMENT_SIZE TSRMLS_CC);
			if (EG(exception)) {
				return ZEND_HASH_APPLY_STOP;
			}

			/* now go back and set the type bit */
			if (num == zend_hash_num_elements(Z_ARRVAL_PP(data))) {
				buf->start[type_offset] = BSON_ARRAY;
			} else {
				buf->start[type_offset] = BSON_OBJECT;
			}
			break;
		}

        case IS_OBJECT: {
            HashTable *hash = Z_OBJPROP_PP(data);
            /* go through the k/v pairs and serialize them */
            PHP_BINARYJSON_SERIALIZE_KEY(BSON_OBJECT);

            zval_to_binaryjson(buf, hash, NO_PREP, DEFAULT_MAX_MESSAGE_SIZE TSRMLS_CC);
            if (EG(exception)) {
                return ZEND_HASH_APPLY_STOP;
            }
            break;
        }
	}
	return ZEND_HASH_APPLY_KEEP;
}

static int prep_id_for_binaryjson(buffer *buf, HashTable *array TSRMLS_DC)
{
	zval **data, *newid;

	/* if _id field doesn't exist, add it */
	if (zend_hash_find(array, "_id", 4, (void**)&data) == FAILURE) {
		/* create new MongoId */
		MAKE_STD_ZVAL(newid);
		php_binaryjson_id_populate(newid, NULL TSRMLS_CC);
		/* add to obj */
		zend_hash_add(array, "_id", 4, &newid, sizeof(zval*), NULL);
		/* set to data */
		data = &newid;
	}
	php_binaryjson_serialize_element("_id", 3, data, buf, 0 TSRMLS_CC);
	if (EG(exception)) {
		return FAILURE;
	}
	return SUCCESS;
}

char* binaryjson_to_zval(char *buf, HashTable *result TSRMLS_DC)
{
	/* buf_start is used for debugging
	 *
	 * If the deserializer runs into bson it can't parse, it will dump the
	 * bytes to that point.
	 *
	 * We lose buf's position as we iterate, so we need buf_start to save it. */
	char *buf_start = buf, *buf_end;
	unsigned char type;

	if (buf == 0) {
		return 0;
	}

	buf_end = buf + MONGO_32(*((int*)buf));

	/* for size */
	buf += INT_32;

	while ((type = *buf++) != 0) {
		char *name;
		zval *value;

		name = buf;
		/* get past field name */
		buf += strlen(buf) + 1;

		MAKE_STD_ZVAL(value);
		ZVAL_NULL(value);

		/* get value */
		switch(type) {
			case BSON_OID: {
				char *id;
				char *tmp_id;

				CHECK_BUFFER_LEN(OID_SIZE);
				id = estrndup(buf, OID_SIZE);
				tmp_id = php_binaryjson_id_to_hex(id);
				ZVAL_STRING(value, tmp_id, 0);

				buf += OID_SIZE;
				break;
			}

			case BSON_DOUBLE: {
				double d;
				int64_t i, *i_p;

				CHECK_BUFFER_LEN(DOUBLE_64);

				d = *(double*)buf;
				i_p = &i;

				memcpy(i_p, &d, DOUBLE_64);
				i = MONGO_64(i);
				memcpy(&d, i_p, DOUBLE_64);

				ZVAL_DOUBLE(value, d);
				buf += DOUBLE_64;
				break;
			}

			case BSON_SYMBOL:
			case BSON_STRING: {
				int len;

				CHECK_BUFFER_LEN(INT_32);

				len = MONGO_32(*((int*)buf));
				buf += INT_32;

				/* len includes \0 */
				if (len < 1) {
					zval_ptr_dtor(&value);
					zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 21 TSRMLS_CC, "invalid string length for key \"%s\": %d", name, len);
					return 0;
				}

				CHECK_BUFFER_LEN(len);

				ZVAL_STRINGL(value, buf, len-1, 1);
				buf += len;
				break;
			}

			case BSON_OBJECT:
			case BSON_ARRAY: {
				array_init(value);
				buf = binaryjson_to_zval(buf, Z_ARRVAL_P(value) TSRMLS_CC);
				if (EG(exception)) {
					zval_ptr_dtor(&value);
					return 0;
				}
				break;
			}

			case BSON_BOOL: {
				CHECK_BUFFER_LEN(BYTE_8);
				ZVAL_BOOL(value, *buf++);
				break;
			}

			case BSON_UNDEF:
			case BSON_NULL: {
				ZVAL_NULL(value);
				break;
			}

			case BSON_INT: {
				CHECK_BUFFER_LEN(INT_32);
				ZVAL_LONG(value, MONGO_32(*((int*)buf)));
				buf += INT_32;
				break;
			}

			default: {
				/* If we run into a type we don't recognize, there's either been
				 * some corruption or we've messed up on the parsing.  Either way,
				 * it's helpful to know the situation that led us here, so this
				 * dumps the buffer up to this point to stdout and returns.
				 *
				 * We can't dump any more of the buffer, unfortunately, because we
				 * don't keep track of the size.  Besides, if it is corrupt, the
				 * size might be messed up, too. */
				char *msg, *pos, *template;
				int i, width, len;
				unsigned char t = type;

				template = "type 0x00 not supported:";

				/* each byte is " xx" (3 chars) */
				width = 3;
				len = (buf - buf_start) * width;

				msg = (char*)emalloc(strlen(template) + len + 1);
				memcpy(msg, template, strlen(template));
				pos = msg + 7;

				sprintf(pos++, "%x", t / 16);
				t = t % 16;
				sprintf(pos++, "%x", t);
				/* remove '\0' added by sprintf */
				*(pos) = ' ';

				/* jump to end of template */
				pos = msg + strlen(template);
				for (i=0; i<buf-buf_start; i++) {
					sprintf(pos, " %02x", (unsigned char)buf_start[i]);
					pos += width;
				}
				/* sprintf 0-terminates the string */

				zend_throw_exception(zend_exception_get_default(TSRMLS_C), msg, 17 TSRMLS_CC);
				efree(msg);
				return 0;
			}
		}
		zend_symtable_update(result, name, strlen(name) + 1, &value, sizeof(zval*), NULL);
	}

	return buf;
}

static int is_utf8(const char *s, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (i + 3 < len && (s[i] & 248) == 240 && (s[i + 1] & 192) == 128 && (s[i + 2] & 192) == 128 && (s[i + 3] & 192) == 128) {
			i += 3;
		} else if (i + 2 < len && (s[i] & 240) == 224 && (s[i + 1] & 192) == 128 && (s[i + 2] & 192) == 128) {
			i += 2;
		} else if (i + 1 < len && (s[i] & 224) == 192 && (s[i + 1] & 192) == 128) {
			i += 1;
		} else if ((s[i] & 128) != 0) {
			return 0;
		}
	}
	return 1;
}


/* {{{ proto string bson_encode(mixed document)
   Takes any type of PHP var and turns it into BSON */
PHP_FUNCTION(binaryjson_encode)
{
	zval *z;
	buffer buf;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &z) == FAILURE) {
		return;
	}
	switch (Z_TYPE_P(z)) {
		case IS_NULL: {
			RETURN_STRING("", 1);
			break;
		}
		case IS_LONG: {
			CREATE_BUF_STATIC(9);
#if SIZEOF_LONG == 4
			php_binaryjson_serialize_int(&buf, Z_LVAL_P(z));
			RETURN_STRINGL(buf.start, 4, 1);
#else
			php_binaryjson_serialize_long(&buf, Z_LVAL_P(z));
			RETURN_STRINGL(buf.start, 8, 1);
#endif
			break;
		}
		case IS_DOUBLE: {
			CREATE_BUF_STATIC(9);
			php_binaryjson_serialize_double(&buf, Z_DVAL_P(z));
			RETURN_STRINGL(b, 8, 1);
			break;
		}
		case IS_BOOL: {
			if (Z_BVAL_P(z)) {
				RETURN_STRINGL("\x01", 1, 1);
			} else {
				RETURN_STRINGL("\x00", 1, 1);
			}
			break;
		}
		case IS_STRING: {
			RETURN_STRINGL(Z_STRVAL_P(z), Z_STRLEN_P(z), 1);
			break;
		}
		/* fallthrough for a normal obj */
        case IS_OBJECT:
		case IS_ARRAY: {
			CREATE_BUF(buf, INITIAL_BUF_SIZE);
			zval_to_binaryjson(&buf, HASH_P(z), 0, DEFAULT_MAX_MESSAGE_SIZE TSRMLS_CC);

			RETVAL_STRINGL(buf.start, buf.pos-buf.start, 1);
			efree(buf.start);
			break;
		}
		default:
			zend_throw_exception(zend_exception_get_default(TSRMLS_C), "couldn't serialize element", 0 TSRMLS_CC);
			return;
	}
}

PHP_FUNCTION(binaryjson_decode)
{
	char *str;
	int str_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &str_len) == FAILURE) {
		return;
	}
	array_init(return_value);
	binaryjson_to_zval(str, HASH_P(return_value) TSRMLS_CC);
}

PHP_FUNCTION(binaryjson_header_pack)
{
    int opcode;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &opcode) == FAILURE) {
		return;
	}
    buffer buf;    
	CREATE_BUF(buf, INITIAL_BUF_SIZE);

    binaryjson_msg_header header;
    CREATE_MSG_HEADER(BINARYJSON_G(request_id)++, 0, opcode);    

    buf.pos += INT_32;    
	php_binaryjson_serialize_int((buffer *)(&buf), header.request_id); 
	php_binaryjson_serialize_int((buffer *)(&buf), header.response_to); 
	php_binaryjson_serialize_int((buffer *)(&buf), header.op); 
	php_binaryjson_serialize_int((buffer *)(&buf), 0);
    
    RETVAL_STRINGL(buf.start, buf.pos - buf.start, 1);
}

PHP_FUNCTION(binaryjson_header_unpack)
{
    char *buf;
    int buf_len;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &buf, &buf_len) == FAILURE) {
		return;
	}
    array_init(return_value);
    add_assoc_long(return_value, "length", MONGO_32(*((int*)buf)));
    buf += INT_32;
    add_assoc_long(return_value, "request_id", MONGO_32(*((int*)buf)));
    buf += INT_32;
    add_assoc_long(return_value, "response_to", MONGO_32(*((int*)buf)));
    buf += INT_32;
    add_assoc_long(return_value, "opcode", MONGO_32(*((int*)buf)));
    buf += INT_32;    
    add_assoc_long(return_value, "opts", MONGO_32(*((int*)buf)));    
}

PHP_FUNCTION(binaryjson_msg_pack)
{
    zval *doc;
    int opcode = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|l", &doc, &opcode) == FAILURE) {
		return;
	}
    buffer buf;
	CREATE_BUF(buf, INITIAL_BUF_SIZE);

    int head_offset = buf.pos - buf.start;

    binaryjson_msg_header header;
    CREATE_MSG_HEADER(BINARYJSON_G(request_id)++, 0, opcode);

    buf.pos += INT_32;
	php_binaryjson_serialize_int((buffer *)(&buf), header.request_id);
	php_binaryjson_serialize_int((buffer *)(&buf), header.response_to);
	php_binaryjson_serialize_int((buffer *)(&buf), header.op);
	php_binaryjson_serialize_int((buffer *)(&buf), 0);
    
    int body_offset = buf.pos - buf.start;
    // printf("head_offset: %d\tbody_offset: %d\tstrlen: %d\n", head_offset, body_offset, strlen(ns)+1);
    
    int result = zval_to_binaryjson(&buf, HASH_P(doc), PREP, DEFAULT_MAX_DOCUMENT_SIZE TSRMLS_CC);

	/* throw exception if serialization crapped out */
	if (EG(exception) || FAILURE == result) {
        RETVAL_LONG(-1);
	} else if (0 == result) {
		/* return if there were 0 elements */
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 4 TSRMLS_CC, "no elements in doc");
        RETVAL_LONG(-2);
	}

	if (buf.pos - (buf.start + body_offset) > DEFAULT_MAX_DOCUMENT_SIZE) {
		zend_throw_exception_ex(zend_exception_get_default(TSRMLS_C), 5 TSRMLS_CC, "size of BSON doc is %d bytes, max is %d", buf.pos - (buf.start + body_offset), DEFAULT_MAX_DOCUMENT_SIZE);
        RETVAL_LONG(-3);        
	}

	if(php_binaryjson_serialize_size(buf.start + head_offset, &buf, DEFAULT_MAX_DOCUMENT_SIZE TSRMLS_CC) == SUCCESS) {
        RETVAL_STRINGL(buf.start, buf.pos - buf.start, 1);
    } else {
        RETVAL_LONG(-3);
    }
}

PHP_FUNCTION(binaryjson_msg_unpack)
{
    char *buf;
    int buf_len;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &buf, &buf_len) == FAILURE) {
		return;
	}
    
    zval *head;
    MAKE_STD_ZVAL(head);
    array_init(head);
    
    add_assoc_long(head, "length", MONGO_32(*((int*)buf)));
    buf += INT_32;
    add_assoc_long(head, "request_id", MONGO_32(*((int*)buf)));
    buf += INT_32;
    add_assoc_long(head, "response_to", MONGO_32(*((int*)buf)));
    buf += INT_32;
    add_assoc_long(head, "opcode", MONGO_32(*((int*)buf)));
    buf += INT_32;    
    add_assoc_long(head, "opts", MONGO_32(*((int*)buf)));
    buf += INT_32;    

    zval *body;
    MAKE_STD_ZVAL(body);
    array_init(body);
	binaryjson_to_zval(buf, HASH_P(body) TSRMLS_CC);

    array_init(return_value);
    add_assoc_zval(return_value, "head", head);
    add_assoc_zval(return_value, "body", body);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
