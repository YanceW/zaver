
/*
 * Copyright (C) Zhu Jiashun
 * Copyright (C) Zaver
 */

#include <strings.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "http.h"
#include "http_parse.h"
#include "http_request.h"
#include "epoll.h"
#include "error.h"
#include "timer.h"

// 文件后缀->MIME（zaver_mime)
// 如果类型不存在，默认映射NULL->text/plain
static const char* get_file_type(const char *type);

// @breif	uri提取filename = ROOT +　filename（空：设为/; /：设为index.html)
// @param	IN uri length; OUT filename; UNUSED querystring
static void parse_uri(char *uri, int length, char *filename, char *querystring);

// @breif	构建错误信息响应，并且rio_writen到fd
// @param	fd通信的socket； cause？？？；longmsg详细信息
static void do_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

// @breif	设置响应静态资源响应信息（未修改只返回头部），修改了便添加文件到body，已用rio_writen写入
// @param	fd——sockfd; filename-可以来自parse_uri; filesize-; out 头部状态码和字段值
static void serve_static(int fd, char *filename, size_t filesize, zv_http_out_t *out);
static char *ROOT = NULL;

mime_type_t zaver_mime[] = 
{
    {".html", "text/html"},
    {".xml", "text/xml"},
    {".xhtml", "application/xhtml+xml"},
    {".txt", "text/plain"},
    {".rtf", "application/rtf"},
    {".pdf", "application/pdf"},
    {".word", "application/msword"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".au", "audio/basic"},
    {".mpeg", "video/mpeg"},
    {".mpg", "video/mpeg"},
    {".avi", "video/x-msvideo"},
    {".gz", "application/x-gzip"},
    {".tar", "application/x-tar"},
    {".css", "text/css"},
    {NULL ,"text/plain"}
};

void do_request(void *ptr) {
//	zv_http_request_t含有各种东西
    zv_http_request_t *r = (zv_http_request_t *)ptr;
    int fd = r->fd;
    int rc, n;
    char filename[SHORTLINE];//512
    struct stat sbuf;
    ROOT = r->root;
    char *plast = NULL;
    size_t remain_size;
    
    zv_del_timer(r);
    
    
    for(;;) {
        plast = &r->buf[r->last % MAX_BUF];
        remain_size = MIN(MAX_BUF - (r->last - r->pos) - 1, MAX_BUF - r->last % MAX_BUF);

        n = read(fd, plast, remain_size);
        /*
        A：判断条件
        M：错误信息
        @breif	A false -> do
        都是在stderr输出信息
        check(A, M, ...) 		ERROR	
        check_exit(A, M, ...)	ERROR
        check_debug(A, M, ...)	DEBUG
        */
        check(r->last - r->pos < MAX_BUF, "request buffer overflow!");

        if (n == 0) {   
            // EOF
            log_info("read return 0, ready to close fd %d, remain_size = %zu", fd, remain_size);
            goto err;
        }

        if (n < 0) {
            if (errno != EAGAIN) {
                log_err("read err, and errno = %d", errno);
                goto err;
            }
            break;
        }

        r->last += n;
        check(r->last - r->pos < MAX_BUF, "request buffer overflow!");
        
        log_info("ready to parse request line"); 
        rc = zv_http_parse_request_line(r);
        if (rc == ZV_AGAIN) {
            continue;
        } else if (rc != ZV_OK) {
            log_err("rc != ZV_OK");
            goto err;
        }

        log_info("method == %.*s", (int)(r->method_end - r->request_start), (char *)r->request_start);
        log_info("uri == %.*s", (int)(r->uri_end - r->uri_start), (char *)r->uri_start);

        debug("ready to parse request body");
        rc = zv_http_parse_request_body(r);
        if (rc == ZV_AGAIN) {
            continue;
        } else if (rc != ZV_OK) {
            log_err("rc != ZV_OK");
            goto err;
        }
        
        /*
        *   handle http header
        */
        zv_http_out_t *out = (zv_http_out_t *)malloc(sizeof(zv_http_out_t));
        if (out == NULL) {
            log_err("no enough space for zv_http_out_t");
            exit(1);
        }

        rc = zv_init_out_t(out, fd);
        check(rc == ZV_OK, "zv_init_out_t");

        // 取得filename
        parse_uri(r->uri_start, r->uri_end - r->uri_start, filename, NULL);

		// stat检查
        if(stat(filename, &sbuf) < 0) {
            do_error(fd, filename, "404", "Not Found", "zaver can't find the file");
            continue;
        }

        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
        {
            do_error(fd, filename, "403", "Forbidden",
                    "zaver can't read the file");
            continue;
        }
        
        out->mtime = sbuf.st_mtime;

		// 根据字段-handler处理请求首部的字段，可去http-request拓展功能
        zv_http_handle_header(r, out);
        // zv_http_request_t的list保存的是struct（key:val的起止指针，应该是rbuf里的位置）的链表
        // 上一个函数遍历list，每处理一个移除一个节点，最后应该为空
        check(list_empty(&(r->list)) == 1, "header list should be empty");
        
        // 前面的错误都没有
        if (out->status == 0) {
            out->status = ZV_HTTP_OK;
        }

		// 构造静态资源响应写入rbuf
        serve_static(fd, filename, sbuf.st_size, out);

        if (!out->keep_alive) {
            log_info("no keep_alive! ready to close");
            free(out);
            goto close;
        }
        free(out);

    }
    
    struct epoll_event event;
    event.data.ptr = ptr;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

    zv_epoll_mod(r->epfd, r->fd, &event);
    zv_add_timer(r, TIMEOUT_DEFAULT, zv_http_close_conn);
    return;

err:
close:
    rc = zv_http_close_conn(r);
    check(rc == 0, "do_request: zv_http_close_conn");
}

static void parse_uri(char *uri, int uri_length, char *filename, char *querystring) {
    check(uri != NULL, "parse_uri: uri is NULL");
    uri[uri_length] = '\0';

    char *question_mark = strchr(uri, '?');// 返回uri第一个？的指针，不存在NULL
    
    // 分割？前面为文件名，并取得文件名长度
    int file_length;
    if (question_mark) {
        file_length = (int)(question_mark - uri);
        debug("file_length = (question_mark - uri) = %d", file_length);
    } else {
        file_length = uri_length;
        debug("file_length = uri_length = %d", file_length);
    }

    if (querystring) {
        //TODO
    }
    
    // 复制（网页）根路径到filename；实现不同网站部署
    strcpy(filename, ROOT);

    // uri_length can not be too long
    // SHORTLINE 512
    //　linux文件名长度有限制，ext3貌似255？ 绝对路径最长4096
    if (uri_length > (SHORTLINE >> 1)) {
        log_err("uri too long: %.*s", uri_length, uri);
        return;
    }

    debug("before strncat, filename = %s, uri = %.*s, file_len = %d", filename, file_length, uri, file_length);
    // uri的前file_length字节接入
    // filename = ROOT + 请求的file
    strncat(filename, uri, file_length);

	// strchr的反向查找——最后一个
    char *last_comp = strrchr(filename, '/');
    char *last_dot = strrchr(last_comp, '.');
    
    // 啥都妹请求
    if (last_dot == NULL && filename[strlen(filename)-1] != '/') {
        strcat(filename, "/");
    }
    
    // GET / HTTP/xx ------> GET /index.html HTTP/xx
    if(filename[strlen(filename)-1] == '/') {
        strcat(filename, "index.html");
    }

    log_info("filename = %s", filename);
    return;
}

static void do_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
	// MAXLINE 8192
    char header[MAXLINE], body[MAXLINE];

    sprintf(body, "<html><title>Zaver Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\n", body);
    sprintf(body, "%s%s: %s\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\n</p>", body, longmsg, cause);
    sprintf(body, "%s<hr><em>Zaver web server</em>\n</body></html>", body);

    sprintf(header, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
    sprintf(header, "%sServer: Zaver\r\n", header);
    sprintf(header, "%sContent-type: text/html\r\n", header);
    sprintf(header, "%sConnection: close\r\n", header);
    sprintf(header, "%sContent-length: %d\r\n\r\n", header, (int)strlen(body));
    //log_info("header  = \n %s\n", header);
    rio_writen(fd, header, strlen(header));
    rio_writen(fd, body, strlen(body));
    //log_info("leave clienterror\n");
    return;
}




/*

typedef struct {
    int fd;
    int keep_alive;
    time_t mtime;      // Unix时间戳 // 文件的修改时间
    int modified;       
    int status;	//状态码
} zv_http_out_t;

*/

static void serve_static(int fd, char *filename, size_t filesize, zv_http_out_t *out) {
    char header[MAXLINE];	// 8192
    char buf[SHORTLINE];	// 512
    size_t n;

    //           struct tm {
    //           int tm_sec;    /* Seconds (0-60) */
    //          int tm_min;    /* Minutes (0-59) */
    //           int tm_hour;   /* Hours (0-23) */
    //          int tm_mday;   /* Day of the month (1-31) */
    //           int tm_mon;    /* Month (0-11) */
    //           int tm_year;   /* Year - 1900 */
    //           int tm_wday;   /* Day of the week (0-6, Sunday = 0) */
    //           int tm_yday;   /* Day in the year (0-365, 1 Jan = 0) */
    //           int tm_isdst;  /* Daylight saving time */
    //      };

    struct tm tm;
    
    const char *file_type;	// 顶层const
    const char *dot_pos = strrchr(filename, '.');
    // 取mime
    file_type = get_file_type(dot_pos);

// 在http_request.h
    sprintf(header, "HTTP/1.1 %d %s\r\n", out->status, get_shortmsg_from_status_code(out->status));

// where is TIMEOUT_DEFAULT???
    if (out->keep_alive) {
        sprintf(header, "%sConnection: keep-alive\r\n", header);
        sprintf(header, "%sKeep-Alive: timeout=%d\r\n", header, TIMEOUT_DEFAULT);
    }

    if (out->modified) {
        sprintf(header, "%sContent-type: %s\r\n", header, file_type);
        sprintf(header, "%sContent-length: %zu\r\n", header, filesize);
        localtime_r(&(out->mtime), &tm);	// 文件上次修改时间戳转换为正常日期
        // 提取时间到buf
        strftime(buf, SHORTLINE,  "%a, %d %b %Y %H:%M:%S GMT", &tm);
        sprintf(header, "%sLast-Modified: %s\r\n", header, buf);
    }

    sprintf(header, "%sServer: Zaver\r\n", header);
    sprintf(header, "%s\r\n", header);

    n = (size_t)rio_writen(fd, header, strlen(header));
    check(n == strlen(header), "rio_writen error, errno = %d", errno);
    if (n != strlen(header)) {
        log_err("n != strlen(header)");
        goto out; 
    }

	// 未修改返回首部
    if (!out->modified) {
        goto out;
    }

    int srcfd = open(filename, O_RDONLY, 0);
    check(srcfd > 2, "open error");
    // can use sendfile
    // mmap, munmap <sys/mman.h>
    // 映射, 取消映射 file/device 到 mem
    // mmap在进程的虚拟地址空间创建一个映射，（addr, length, prot, flags, fd, offset）
    // addr NULL时kernel自己找，！NULL，作为一个hint（提示信息），在最近的一个页的边界创建映射；实际映射的地址作为函数的返回
    // 映射fd文件 从 offset（必须为页size的倍数）开始的length
    // prot（protection），映射空间需要的保护等级，不能与fd的打开方式矛盾，有PROT_EXEC页可能执行 PROT_READ可能读 PROT_WRITE可能写 PROT_NONE可能不可访问
    // flags，定义对映射空间的操作对映射同一个地方的其他进程是否可见；或是否把更新应用到实际文件；MAP_SHARED操作对其他进程可见并且修改实际文件（用到msync，刷新底层文件） MAP_PRIVATE与SHARED相反，并且底层文件的修改对映射区是否可见是未定义 ...还有很多
    char *srcaddr = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    check(srcaddr != (void *) -1, "mmap error");
    close(srcfd);

    n = rio_writen(fd, srcaddr, filesize);
    // check(n == filesize, "rio_writen error");

    munmap(srcaddr, filesize);

out:
    return;
}

static const char* get_file_type(const char *type)
{
    if (type == NULL) {
        return "text/plain";
    }

    int i;
    for (i = 0; zaver_mime[i].type != NULL; ++i) {
        if (strcmp(type, zaver_mime[i].type) == 0)
            return zaver_mime[i].value;
    }
    return zaver_mime[i].value;
}
