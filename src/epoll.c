
/*
 * Copyright (C) Zhu Jiashun
 * Copyright (C) Zaver
 */

#include "epoll.h"
#include "dbg.h"

struct epoll_event *events;

int zv_epoll_create(int flags) {
    int fd = epoll_create1(flags);
// linux 2.6.8开始 create（size）过时了
// size是忽略的，但要大于等于零
/* epoll_create1()
 *      If  flags  is 0, then, other than the fact that the obsolete size argu‐
 *      ment is dropped, epoll_create1() is the same  as  epoll_create().   The
 *      following value can be included in flags to obtain different behavior:
 *
 *      EPOLL_CLOEXEC
 *             Set the close-on-exec (FD_CLOEXEC) flag on the new file descrip‐
 *             tor.  See the description of the O_CLOEXEC flag in  open(2)  for
 *             reasons why this may be useful.
*/
    check(fd > 0, "zv_epoll_create: epoll_create1");

    events = (struct epoll_event *)malloc(sizeof(struct epoll_event) * MAXEVENTS);
    check(events != NULL, "zv_epoll_create: malloc");
    return fd;
}

void zv_epoll_add(int epfd, int fd, struct epoll_event *event) {
    int rc = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, event);
    check(rc == 0, "zv_epoll_add: epoll_ctl");
    return;
}

void zv_epoll_mod(int epfd, int fd, struct epoll_event *event) {
    int rc = epoll_ctl(epfd, EPOLL_CTL_MOD, fd, event);
    check(rc == 0, "zv_epoll_mod: epoll_ctl");
    return;
}

void zv_epoll_del(int epfd, int fd, struct epoll_event *event) {
    int rc = epoll_ctl(epfd, EPOLL_CTL_DEL, fd, event);
    check(rc == 0, "zv_epoll_del: epoll_ctl");
    return;
}

// timeout = -1 Block; timeout = 0 return immediately
int zv_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
    int n = epoll_wait(epfd, events, maxevents, timeout);
    check(n >= 0, "zv_epoll_wait: epoll_wait");
    return n;
}
