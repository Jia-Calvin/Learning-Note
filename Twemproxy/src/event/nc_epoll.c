/*
 * twemproxy - A fast and lightweight proxy for memcached protocol.
 * Copyright (C) 2011 Twitter, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <nc_core.h>
#include <signal.h>


#include <sys/epoll.h>
#include <sys/signalfd.h>

static int signal_data;

static int create_signal_fd(int ep, int num_signals, const int *signals)
{
    sigset_t mask;
    int i, fd;
    // epoll_event 结构体包含了 events 和 data
    struct epoll_event event;

    // 将maks里面的信号集合设置成signals
    sigemptyset(&mask);
    for (i = 0; i < num_signals; ++i) {
        sigaddset(&mask, signals[i]);
    }

    // signalfd可以将信号抽象为一个文件描述符，当有信号发生时可以对其read，这样可以将信号的监听放到 select、poll、epoll 等监听队列中。
    fd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
    if (fd < 0) {
        log_error("failed to signalfd: %s", strerror(errno));
        goto err;
    }

    // EPOLLET： 将EPOLL设为边缘触发(Edge Triggered)模式，这是相对于水平触发(Level Triggered)来说的。
    // EPOLLIN ：表示对应的文件描述符可以读（包括对端SOCKET正常关闭）
    event.events = EPOLLIN | EPOLLET;
    event.data.ptr = &signal_data;
    // epoll_ctl第二个参数，表示op操作，用三个宏来表示：添加EPOLL_CTL_ADD，删除EPOLL_CTL_DEL，修改EPOLL_CTL_MOD。分别添加、删除和修改对fd的监听事件。
    // 实则是将信号事情放入epoll队列里，也就是要监听这些信号的发生
    if (epoll_ctl(ep, EPOLL_CTL_ADD, fd, &event)) {
        log_error("failed to epoll_ctl: %s", strerror(errno));
        goto err2;
    }

    return fd;
err2:
    close(fd);
err:
    return -1;
}

struct event_base *
event_base_create(int nevent, event_cb_t cb, int num_signals,
                  const int *signals, void (*signal_handler)(int))
{
    struct event_base *evb;
    int status, ep;
    struct epoll_event *event;

    ASSERT(nevent > 0);

    // 调用系统函数epoll_create创建epoll的描述符
    ep = epoll_create(nevent);
    if (ep < 0) {
        log_error("epoll create of size %d failed: %s", nevent, strerror(errno));
        return NULL;
    }
    // 申请event的空间, nevent = 1024，多成员，1024个event指针
    // 实则是数据指针？
    event = nc_calloc(nevent, sizeof(*event));
    if (event == NULL) {
        status = close(ep);
        if (status < 0) {
            log_error("close e %d failed, ignored: %s", ep, strerror(errno));
        }
        return NULL;
    }

    // 申请evb的空间
    evb = nc_alloc(sizeof(*evb));
    if (evb == NULL) {
        nc_free(event);
        status = close(ep);
        if (status < 0) {
            log_error("close e %d failed, ignored: %s", ep, strerror(errno));
        }
        return NULL;
    }

    evb->ep = ep;
    evb->event = event;
    evb->nevent = nevent;

    // 这里还没有调用，只是将函数core_core地址给了它
    evb->cb = cb;

    // 创建信号描述符，放入了epoll->ep队列中，意味着需要监听这些信号
    // 当然在发生时也需要处理这些信号
    evb->signal_fd = create_signal_fd(ep, num_signals, signals);
    if (evb->signal_fd < 0) {
        nc_free(evb);
        nc_free(event);
        status = close(ep);
        if (status < 0) {
            log_error("close e %d failed, ignored: %s", ep, strerror(errno));
        }
        return NULL;
    }
    evb->signal_handler = signal_handler;

    log_info("e %d with nevent %d", evb->ep, evb->nevent);

    return evb;
}

void
event_base_destroy(struct event_base *evb)
{
    int status;

    if (evb == NULL) {
        return;
    }

    ASSERT(evb->ep > 0);

    nc_free(evb->event);

    status = close(evb->ep);
    if (status < 0) {
        log_error("close e %d failed, ignored: %s", evb->ep, strerror(errno));
    }
    evb->ep = -1;
    close(evb->signal_fd);

    nc_free(evb);
}

int
event_add_in(struct event_base *evb, struct conn *c)
{
    int status;
    struct epoll_event event;
    int ep = evb->ep;

    ASSERT(ep > 0);
    ASSERT(c != NULL);
    ASSERT(c->sd > 0);

    if (c->recv_active) {
        return 0;
    }

    event.events = (uint32_t)(EPOLLIN | EPOLLET);
    event.data.ptr = c;

    status = epoll_ctl(ep, EPOLL_CTL_MOD, c->sd, &event);
    if (status < 0) {
        log_error("epoll ctl on e %d sd %d failed: %s", ep, c->sd,
                  strerror(errno));
    } else {
        c->recv_active = 1;
    }

    return status;
}

int
event_del_in(struct event_base *evb, struct conn *c)
{
    return 0;
}

int
event_add_out(struct event_base *evb, struct conn *c)
{
    int status;
    struct epoll_event event;
    int ep = evb->ep;

    ASSERT(ep > 0);
    ASSERT(c != NULL);
    ASSERT(c->sd > 0);
    ASSERT(c->recv_active);

    if (c->send_active) {
        return 0;
    }

    event.events = (uint32_t)(EPOLLIN | EPOLLOUT | EPOLLET);
    event.data.ptr = c;

    status = epoll_ctl(ep, EPOLL_CTL_MOD, c->sd, &event);
    if (status < 0) {
        log_error("epoll ctl on e %d sd %d failed: %s", ep, c->sd,
                  strerror(errno));
    } else {
        c->send_active = 1;
    }

    return status;
}

int
event_del_out(struct event_base *evb, struct conn *c)
{
    int status;
    struct epoll_event event;
    int ep = evb->ep;

    ASSERT(ep > 0);
    ASSERT(c != NULL);
    ASSERT(c->sd > 0);
    ASSERT(c->recv_active);

    if (!c->send_active) {
        return 0;
    }

    event.events = (uint32_t)(EPOLLIN | EPOLLET);
    // 事件 返回的数据 指向 conn
    event.data.ptr = c;

    // EPOLL_CTL_MOD, modify 修改 sd 的监听事件（修改为监听in以及ET模式）
    status = epoll_ctl(ep, EPOLL_CTL_MOD, c->sd, &event);
    if (status < 0) {
        log_error("epoll ctl on e %d sd %d failed: %s", ep, c->sd,
                  strerror(errno));
    } else {
        // 将其发送修改为不活跃，此时在监听in事件
        c->send_active = 0;
    }

    return status;
}

int
event_add_conn(struct event_base *evb, struct conn *c)
{
    int status;
    struct epoll_event event;
    int ep = evb->ep;

    ASSERT(ep > 0);
    ASSERT(c != NULL);
    ASSERT(c->sd > 0);

    event.events = (uint32_t)(EPOLLIN | EPOLLOUT | EPOLLET);
    event.data.ptr = c;

    status = epoll_ctl(ep, EPOLL_CTL_ADD, c->sd, &event);
    if (status < 0) {
        log_error("epoll ctl on e %d sd %d failed: %s", ep, c->sd,
                  strerror(errno));
    } else {
        c->send_active = 1;
        c->recv_active = 1;
    }

    return status;
}

int
event_del_conn(struct event_base *evb, struct conn *c)
{
    int status;
    int ep = evb->ep;

    ASSERT(ep > 0);
    ASSERT(c != NULL);
    ASSERT(c->sd > 0);

    status = epoll_ctl(ep, EPOLL_CTL_DEL, c->sd, NULL);
    if (status < 0) {
        log_error("epoll ctl on e %d sd %d failed: %s", ep, c->sd,
                  strerror(errno));
    } else {
        c->recv_active = 0;
        c->send_active = 0;
    }

    return status;
}

int
event_wait(struct event_base *evb, int timeout)
{
    int ep = evb->ep;
    struct epoll_event *event = evb->event;
    int nevent = evb->nevent;

    ASSERT(ep > 0);
    ASSERT(event != NULL);
    ASSERT(nevent > 0);

    for (;;) {
        int i, nsd;

        // 等待epfd上的io事件，参数events用来从内核得到事件的集合
        nsd = epoll_wait(ep, event, nevent, timeout);
        if (nsd > 0) {
            for (i = 0; i < nsd; i++) {
                struct epoll_event *ev = &evb->event[i];
                uint32_t events = 0;

                log_debug(LOG_VVERB, "epoll %04"PRIX32" triggered on conn %p",
                          ev->events, ev->data.ptr);

                if (ev->events & EPOLLERR) {
                    events |= EVENT_ERR;
                }

                if (ev->events & (EPOLLIN | EPOLLHUP)) {
                    events |= EVENT_READ;
                }

                //  ev->events = 0004, events = 65280
                if (ev->events & EPOLLOUT) {
                    events |= EVENT_WRITE;
                }

                // 判断数据是否是signals里面的信号集合
                // 在赋予signals的event时，其data是用静态变量signal_data给予地址的
                if (ev->data.ptr == &signal_data) {
                    struct signalfd_siginfo fdsi;
                    ssize_t s;

                    for (;;) {
                        s = read(evb->signal_fd, &fdsi, sizeof(fdsi));
                        if (s < 0) {
                            if (errno != EAGAIN) {
                                log_error("read signalfd: %s", strerror(errno));
                            }
                            break;
                        }
                        ASSERT(s == sizeof(fdsi));
                        evb->signal_handler(fdsi.ssi_signo);
                    }
                } else if (evb->cb != NULL) {
                    // 调用core_core函数，一开始赋予evb的
                    // 传入epoll的 数据返回参数 以及 事件返回参数
                    evb->cb(ev->data.ptr, events);
                }
            }
            return nsd;
        }

        if (nsd == 0) {
            if (timeout == -1) {
               log_error("epoll wait on e %d with %d events and %d timeout "
                         "returned no events", ep, nevent, timeout);
                return -1;
            }

            return 0;
        }

        if (errno == EINTR) {
            continue;
        }

        log_error("epoll wait on e %d with %d events failed: %s", ep, nevent,
                  strerror(errno));
        return -1;
    }

    NOT_REACHED();
}

void
event_loop_stats(event_stats_cb_t cb, void *arg)
{
    struct stats *st = arg;
    int status, ep;
    struct epoll_event ev;

    ep = epoll_create(1);
    if (ep < 0) {
        log_error("epoll create failed: %s", strerror(errno));
        return;
    }

    ev.data.fd = st->sd;
    ev.events = EPOLLIN;

    status = epoll_ctl(ep, EPOLL_CTL_ADD, st->sd, &ev);
    if (status < 0) {
        log_error("epoll ctl on e %d sd %d failed: %s", ep, st->sd,
                  strerror(errno));
        goto error;
    }

    for (;;) {
        int n;

        n = epoll_wait(ep, &ev, 1, st->interval);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            log_error("epoll wait on e %d with m %d failed: %s", ep,
                      st->sd, strerror(errno));
            break;
        }

        cb(st, &n);
    }

error:
    status = close(ep);
    if (status < 0) {
        log_error("close e %d failed, ignored: %s", ep, strerror(errno));
    }
    ep = -1;
}

