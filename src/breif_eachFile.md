## list

## priority_queue

## Util

- typedef zv_conf_s zv_conf_t: root, port, thread_num 

- open_listenfd:开监听端口

- make_socket_non_blocking

- read_conf:用strstr分割（delim为=）配置文件的各项

## error

- ZV_OK 0, ZV_ERROR -1

## epoll

- epoll三函数的封装

- MAXEVENTS 1024

- 用的epoll_create1

## dbg

- 一堆用于debug的宏

## rio

- ring buffer
- rio_t: fd; cnt; ptr; buf[8192]
- readn：fd----->usrbuf
- readnb: rio_t.buf ----->usrbuf(使用read)
- readlineb: rio_t.buf -----> usrbuf(使用read一个一个)
- read: rio_t.buf----->usrbuf（memcpy）(rio_t空时，先从rio_t.fd -----> rio_t.buf)
- writen: usrbuf---->fd
- readinitb: set rio_t

## threadpoll

- **struct** task_t: *func, *arg, *next_task
- **struct** threadpoll_t: mutex, cond, *threads, *task_head, thread_cnt, queue_size, **shutdown, started**

## http_request

- **struct** request_t：root，sockfd，epfd，buf，state（解析所处状态），在buf上整个报文分割指针（含header_t），timer
- **struct** out_t：响应报文相关，fd,  keep-alive, mtime, modified, status
- **struct** header_t：key：value分割指针
- **struct** header_handle_t：key：handler
- header_handler()
- shortmsg_from_status_code()

## http_parse

- 固定request_t(@http_request.h/c)的指针到buf上
- parse_request_line(request_t)	// 解析请求行
- parse_request_body(request_t)  // 解析首部字段

## http

- HTTP的PARSER

- 对外
  
  `void do_request(void *request_t)`
  
  - reuqest_t（@http_request.h/c）
  - 先从rbuf读取(判断==0输出信息跳到close，<0&&!= EAGAIN输出错误跳到close，)
  - 调用parse_request_line(@http_parse.h/c)
  - 调用parse_request_body(@http_parse.h/c)
  - (根据以上两步结果选择继续read/向下/解析err)
  - 根据uri指针处理获取文件名，检查，有误就do_error
  - 调用handle_header(@http_request.h/c)处理各个首部字段
  - 调用serve_static
  - 判断keep-alive选择close_conn（@http_request）或丢回ep池
  
- 对内
  - get_file_type
  - parse_uri
  - do_error //构建错误响应信息写入rbuf
  - serve_static //构建正确响应信息写入buf





