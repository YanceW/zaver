
/*
 * Copyright (C) Zhu Jiashun
 * Copyright (C) Zaver
 */

#ifndef _GNU_SOURCE
/* why define _GNU_SOURCE? http:// stackoverflow.com/questions/15334558/compiler-gets-warnings-when-using-strptime-function-ci */
#define _GNU_SOURCE
#endif

#include <math.h>
#include <time.h>
#include <unistd.h>
#include "http.h"
#include "http_request.h"
#include "error.h"

static int zv_http_process_ignore(zv_http_request_t *r, zv_http_out_t *out, char *data, int len);
static int zv_http_process_connection(zv_http_request_t *r, zv_http_out_t *out, char *data, int len);
static int zv_http_process_if_modified_since(zv_http_request_t *r, zv_http_out_t *out, char *data, int len);

zv_http_header_handle_t zv_http_headers_in[] = {
    {"Host", zv_http_process_ignore},
    {"Connection", zv_http_process_connection},
    {"If-Modified-Since", zv_http_process_if_modified_since},
    {"", zv_http_process_ignore}
};

int zv_init_request_t(zv_http_request_t *r, int fd, int epfd, zv_conf_t *cf) {
    r->fd = fd;
    r->epfd = epfd;
    r->pos = r->last = 0;
    r->state = 0;
    r->root = cf->root;
    INIT_LIST_HEAD(&(r->list));

    return ZV_OK;
}

int zv_free_request_t(zv_http_request_t *r) {
    // TODO
    (void) r;
    return ZV_OK;
}

int zv_init_out_t(zv_http_out_t *o, int fd) {
    o->fd = fd;
    o->keep_alive = 0;
    o->modified = 1;
    o->status = 0;

    return ZV_OK;
}

int zv_free_out_t(zv_http_out_t *o) {
    // TODO
    (void) o;
    return ZV_OK;
}

void zv_http_handle_header(zv_http_request_t *r, zv_http_out_t *o) {
    list_head *pos;
   /*
   首部字段的key:val
typedef struct zv_http_header_s {
    void *key_start, *key_end;          // not include end 
    void *value_start, *value_end;
    list_head list;
} zv_http_header_t;
*/

 zv_http_header_t *hd;
// 处理的字段与函数
    zv_http_header_handle_t *header_in;
    int len;

	// 遍历字段（每次处理结束删除一个链表节点）
    list_for_each(pos, &(r->list)) {

        hd = list_entry(pos, zv_http_header_t, list);
        /* handle */

		// 寻找字段对应的处理函数
        for (header_in = zv_http_headers_in; // zv全局的字段➕方法的数组
            strlen(header_in->name) > 0;// 非ignore方法
            header_in++) {
            if (strncmp(hd->key_start, header_in->name, hd->key_end - hd->key_start) == 0) {
            
                // debug("key = %.*s, value = %.*s", hd->key_end-hd->key_start, hd->key_start, hd->value_end-hd->value_start, hd->value_start);
                len = hd->value_end - hd->value_start;
                
                // 处理
                (*(header_in->handler))(r, o, hd->value_start, len);
                break;
            }    
        }

        /* delete it from the original list */
        list_del(pos);
        free(hd);
    }
}

int zv_http_close_conn(zv_http_request_t *r) {
    // NOTICE: closing a file descriptor will cause it to be removed from all epoll sets automatically
    // http:// stackoverflow.com/questions/8707601/is-it-necessary-to-deregister-a-socket-from-epoll-before-closing-it
    close(r->fd);
    free(r);

    return ZV_OK;
}


// 关于（void），其实是#define Q_UNUSED(X) (void)X;
// 定义或声明却没使用的变量会warning，有的工程-Werror，视warning为错误
// 所以要用（void）忽略这种无关紧要的错误，这种“无用”变量是为了方便以后拓展的预留
// 所以这个函数有啥用？？？^_^
static int zv_http_process_ignore(zv_http_request_t *r, zv_http_out_t *out, char *data, int len) {
    (void) r;
    (void) out;
    (void) data;
    (void) len;
    
    return ZV_OK;
}

// 监测长短连接
static int zv_http_process_connection(zv_http_request_t *r, zv_http_out_t *out, char *data, int len) {
    (void) r;
    if (strncasecmp("keep-alive", data, len) == 0) {
        out->keep_alive = 1;
    }

    return ZV_OK;
}

// 检测
static int zv_http_process_if_modified_since(zv_http_request_t *r, zv_http_out_t *out, char *data, int len) {
    (void) r;
    (void) len;

    struct tm tm;
    if (strptime(data, "%a, %d %b %Y %H:%M:%S GMT", &tm) == (char *)NULL) {
        return ZV_OK;
    }
    time_t client_time = mktime(&tm);

    double time_diff = difftime(out->mtime, client_time);
    if (fabs(time_diff) < 1e-6) {
        // log_info("content not modified clienttime = %d, mtime = %d\n", client_time, out->mtime);
        /* Not modified */
        out->modified = 0;
        out->status = ZV_HTTP_NOT_MODIFIED;
    }
    
    return ZV_OK;
}

// 状态码到状态短语的转换
const char *get_shortmsg_from_status_code(int status_code) {
    /*  for code to msg mapping, please check: 
    *
 http:// users.polytech.unice.fr/~buffa/cours/internet/POLYS/servlets/Servlet-Tutorial-Response-Status-Line.html
    */
    if (status_code == ZV_HTTP_OK) {
        return "OK";
    }

    if (status_code == ZV_HTTP_NOT_MODIFIED) {
        return "Not Modified";
    }

    if (status_code == ZV_HTTP_NOT_FOUND) {
        return "Not Found";
    }
    

    return "Unknown";
}
