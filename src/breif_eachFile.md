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

## http

- HTTP的PARSER
- 对外提供一个外部接口void do_request(void *)解析请求
  - 形参为一个reuqest（@http_request.h/c）
  - 先从rbuf读取(判断==0输出信息跳到close，<0&&!= EAGAIN输出错误跳到close，)
  - 调用parse_request_line(缩写函数名，在http_parse，解析的是整个头部)，定位之后需要的在上一步获取的buf内的各种指针（反正就是可分析的单独的字段的指针（@http_request.h/c）在这里被设置完成）
  - 根据uri指针处理获取函数名，检查，有误就do_error
  - 根据处理产生的头部信息，查找函数映射表，执行处理
  - serve_static
  - 判断keep-alive选择close_conn或丢回ep池
- 内部有多个函数
  - get_file_type
  - parse_uri
  - do_error //构建错误信息写入rbuf
  - serve_static //构建正确请求写入buf