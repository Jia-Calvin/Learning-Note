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

#include <stdlib.h>
#include <signal.h>

#include <nc_core.h>
#include <nc_signal.h>

// 需要处理的信号集合
const int signals[] = {
    SIGUSR1, SIGUSR2, SIGTTIN, SIGTTOU, SIGINT,
};
const int num_signals = 5;

int reload_config;
int double_conf;
int double_read;

void
update_reload_conf(void)
{
    reload_config = 1;
}

void
update_double_read(void)
{
    double_conf = 1;
}

rstatus_t
signal_init(void)
{
    struct signal *sig;
    sigset_t mask;
    int i, error;

    /* init reload_config */
    reload_config = 0;
    double_conf = 0;
    double_read = 0;

    // 忽略管道破裂的信号，这个信号被直接忽略，why ? 
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        log_error("Failed to ignore SIGPIPE: %s", strerror(errno));
        return NC_ERROR;
    }

    // 初始化由 mask 指定的信号集
    // 信号集里面的所有信号被清空
    sigemptyset(&mask);
    for (i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i) {
        // 将signals中所有信号添加到mask中，需要处理的信号
        sigaddset(&mask, signals[i]);
    }
    // 可以使用pthread_sigmask函数来屏蔽某个线程对某些信号的
    // 响应处理，仅留下需要处理该信号的线程来处理指定的信号
    error = pthread_sigmask(SIG_BLOCK, &mask, NULL);
    if (error) {
        log_error("failed to pthread_sigmask: %s", strerror(errno));
        return NC_ERROR;
    }

    return NC_OK;
}

void
signal_deinit(void)
{
}


// 信号处理函数，对每种信号的发生都需要处理，如果不处理则会中断整个进程
void
signal_handler(int signo)
{
    void (*action)(void);
    char *actionstr;
    bool done;

    actionstr = "";
    action = NULL;
    done = false;

    switch (signo) {
    case SIGUSR1:
        actionstr = ", reload conf file";
        action = update_reload_conf;
        break;

    case SIGUSR2:
        actionstr = ", reload conf file, begin double read";
        action = update_double_read;
        break;

    case SIGTTIN:
        actionstr = ", up logging level";
        action = log_level_up;
        break;

    case SIGTTOU:
        actionstr = ", down logging level";
        action = log_level_down;
        break;

    case SIGINT:
        done = true;
        actionstr = ", exiting";
        break;

    default:
        NOT_REACHED();
    }

    loga("signal %d (%s) received%s", signo, strsignal(signo), actionstr);

    if (action != NULL) {
        action();
    }

    if (done) {
        exit(1);
    }
}
