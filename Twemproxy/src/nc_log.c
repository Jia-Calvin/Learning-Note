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
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include <nc_core.h>
#include <nc_util.h>

static struct logger logger;

static int
nc_io_thread_mutex_create(pthread_mutex_t *mtx)
{
    int     err;

    pthread_mutexattr_t attr;

    err = pthread_mutexattr_init(&attr);
    if (err != 0) {
        _log_stderr("pthread_mutexattr_init failed: %d", err);
        goto error1;
    }

    err = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    if (err != 0) {
        _log_stderr("pthread_mutexattr_init failed: %d", err);
        goto error2;
    }

    err = pthread_mutex_init(mtx, &attr);
    if (err != 0) {
        _log_stderr("pthread_mutex_init failed: %d", err);
        goto error2;
    }

    return 0;

error1:
    return -1;

error2:
    err = pthread_mutexattr_destroy(&attr);
    if (err != 0) {
        _log_stderr("pthread_mutex_destroy failed: %d", err);
    }

    return -1;
}

static int
nc_io_thread_cond_create(pthread_cond_t *cond)
{
    int     err;

    err = pthread_cond_init(cond, NULL);
    if (err != 0) {
        _log_stderr("pthread_cond_init failed: %d", err);
        return -1;
    }

    return 0;
}


static void *
nc_io_thread_pool_cycle(void *data)
{
    ssize_t n;

    nc_buf_t  *lbuf, *sbuf, tbuf;

    struct logger *l = &logger;
    nc_io_thread_pool_t *io_thread;

    _log_stderr("nc_io_thread_pool_cycle, start running");
    io_thread = l->io_thread;
    if (io_thread == NULL) {
        _log_stderr("logger->io_thread is empty exit now");
        goto end;
    }

    lbuf = &io_thread->lbuf;
    sbuf = &io_thread->sbuf;

    for (;;) {
        //Still have data left in local buf;
        if (lbuf->last - lbuf->pos > 0) {
            n = nc_write(l->fd, lbuf->pos, (ssize_t)(lbuf->last - lbuf->pos));
            if (n < 0 && (errno == EAGAIN || errno == EINTR)) {
                continue;
            } else if (n > 0) {
                if (lbuf->pos + n == lbuf->last) {
                    //Reset pos and last to start
                    lbuf->pos = lbuf->start;
                    lbuf->last = lbuf->start;
                } else {
                    lbuf->pos += n;
                }
            } else {
                //Logging to Consol
                _log_stderr("io_thread write to log file failed");
            }
        } else {
            pthread_mutex_lock(&io_thread->mtx);
            while (sbuf->start == sbuf->last) pthread_cond_wait(&io_thread->cond, &io_thread->mtx);
            tbuf = *lbuf;
            /* Exchange the buffer pointer*/

            *lbuf = *sbuf;
            *sbuf = tbuf;

            pthread_mutex_unlock(&io_thread->mtx);
        }
    }
end:
    nc_write(l->fd, "crit: io_thread exit\n", 21);
    //nc_io_thread_pool_deinit();
    abort();
    return NULL;
}

static int
nc_io_buffer_init(nc_buf_t *buf, int size)
{
    if (buf == NULL) {
        return -1;
    }

    buf->start = nc_zalloc(size);
    if (buf->start == NULL) {
        return -1;
    }

    buf->size = size;
    buf->pos = buf->start;
    buf->last = buf->start;
    buf->end = buf->start + size;

    return 0;
}

static int
nc_io_thread_pool_init(void)
{
    int             err;
    nc_buf_t        *lbuf = NULL, *sbuf = NULL;
    struct logger   *l = &logger;

    nc_io_thread_pool_t *io_thread;

    pthread_t       tid;
    pthread_attr_t  attr;

    /*Alloc for async logging thread*/
    l->io_thread = nc_zalloc(sizeof(nc_io_thread_pool_t));
    if (l->io_thread == NULL) {
        goto error1;
    }

    io_thread = l->io_thread;
    lbuf = &io_thread->lbuf;
    sbuf = &io_thread->sbuf;

    /*Step1 init io_thread private log buffer*/
    if (nc_io_buffer_init(lbuf, LOG_BUF_MAX_LEN) == -1) {
        goto error1;
    }

    /*Step2 init global log buffer*/
    if (nc_io_buffer_init(sbuf, LOG_BUF_MAX_LEN) == -1) {
        goto error1;
    }

    err = pthread_attr_init(&attr);
    if (err == -1) {
        _log_stderr("pthread_attr_init failed");
        goto error1;
    }

    if (nc_io_thread_mutex_create(&io_thread->mtx) == -1) {
        goto error2;
    }

    if (nc_io_thread_cond_create(&io_thread->cond) == -1) {
        err = pthread_mutex_destroy(&io_thread->mtx);
        if (err != 0) {
            _log_stderr("pthread_mutex_destroy mtx failed");
        }
        goto error2;
    }

    err = pthread_create(&tid, &attr, nc_io_thread_pool_cycle, io_thread);
    if (err) {
        _log_stderr("pthread_create failed");
        goto error2;
    }

    io_thread->thread_id = tid;

    return 0;


error2:
    pthread_attr_destroy(&attr);

error1:
    if (lbuf != NULL && lbuf->start != NULL) {
        nc_free(lbuf->start);
    }

    if (sbuf != NULL && sbuf->start != NULL) {
        nc_free(sbuf->start);
    }
    if (l->io_thread == NULL) {
        nc_free(l->io_thread);
        l->io_thread = NULL;
    }
    return -1;
}

int
log_init(int level, char *name, int limit, int access_sampling)
{
    struct logger *l = &logger;

    l->level = MAX(LOG_EMERG, MIN(level, LOG_PVERB));
    l->name = name;
    l->full_name = NULL;
    l->nfull_name = 0;
    l->full_name_time = 0;
    l->fd = -1;
    l->limit = limit;
    l->access_sampling = access_sampling;
    l->access_counter = 0;

    memset(l->count, 0, sizeof(l->count));
    if (name == NULL || !strlen(name)) {
        l->fd = STDERR_FILENO;
    } else {
        l->nfull_name = strlen(l->name) + 64;
        l->full_name = nc_zalloc(l->nfull_name);
        if (l->full_name == NULL) {
            return -1;
        }

        log_reopen();
    }

    /*Step3 init thread pool*/
    if (nc_io_thread_pool_init() == -1) {
        return -1;
    }

    // 第一句 log 出现在这
    log_notice("io_thread_pool successfully initilized ");
    return 0;
}

void
log_deinit(void)
{
    struct logger *l = &logger;

    if (l->fd < 0 || l->fd == STDERR_FILENO) {
        return;
    }

    close(l->fd);
}

static void
log_full_name(void)
{
    struct logger *l = &logger;
    char timestr[64];
    int64_t now;
    time_t now_t;
    struct tm *local;

    now = nc_usec_now();
    now_t = now / 1000000;
    local = localtime(&now_t);
    strftime(timestr, sizeof(timestr), "%Y-%m-%d", local);

    nc_scnprintf(l->full_name, l->nfull_name, "%s.%s", l->name, timestr);
    l->full_name_time = now;
}

static const char *xbasename(const char *filename)
{
    const char *ptr = strrchr(filename, '/');

    return ptr ? (ptr + 1) : filename;
}

static void
log_symlink(void)
{
    struct logger *l = &logger;
    unlink(l->name);
    if (symlink(xbasename(l->full_name), l->name)) {
        log_stderr("symlink log file '%s' failed: %s", l->full_name,
                   strerror(errno));
    }
}

void
log_reopen(void)
{
    struct logger *l = &logger;

    if (l->fd != STDERR_FILENO) {
        if (l->fd > 0) {
            close(l->fd);
        }
        log_full_name();
        l->fd = open(l->full_name, O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (l->fd < 0) {
            log_stderr("opening log file '%s' failed: %s", l->full_name,
                       strerror(errno));
        }

        log_symlink();
    }
}

void
log_level_up(void)
{
    struct logger *l = &logger;

    if (l->level < LOG_PVERB) {
        l->level++;
        loga("up log level to %d", l->level);
    }
}

void
log_level_down(void)
{
    struct logger *l = &logger;

    if (l->level > LOG_EMERG) {
        l->level--;
        loga("down log level to %d", l->level);
    }
}

void
log_level_set(int level)
{
    struct logger *l = &logger;

    l->level = MAX(LOG_EMERG, MIN(level, LOG_PVERB));
    loga("set log level to %d", l->level);
}

static const char *
log_level_str(int level)
{
    static const char * _level_str[] = {
        "0 EMERG",
        "1 ALERT",
        "2 CRIT",
        "3 ERROR",
        "4 WARN",
        "5 NOTICE",
        "6 INFO",
        "7 DEBUG",
        "8 VERB",
        "9 VVERB",
        "10 VVVERB",
        "11 PVERB"
    };
    if (level < 0 || level > LOG_PVERB) {
        return "0 SPECIAL";
    } else {
        return _level_str[level];
    }
}

#define POSITIVE(n) ((n) > 0 ? (n) : 0)
static bool
log_reach_limit(int level, int64_t now)
{
    struct logger *l = &logger;
    int suppressed;
    int i;

    /* the limit is controled by every 100ms, so we check the count and
     * clear it every 100ms */
    if (now / 100000 > l->last_count_time / 100000) {
        suppressed = 0;
        i = 0;
        for (i = 0; i < LOG_N_LEVEL; ++i) {
            suppressed += POSITIVE(l->count[i] - l->limit);
        }
        if (suppressed > 0) {
            /* loga use -1 as level, so it will not be recursively */
            loga("LOG SUPPRESSED %d : EMERG %d ALERT %d CRIT %d ERROR %d "
                 "WARN %d NOTICE %d INFO %d DEBUG %d VERB %d VVERB %d "
                 "VVVERB %d PVERB %d",
                 suppressed,
                 POSITIVE(l->count[0] - l->limit),
                 POSITIVE(l->count[1] - l->limit),
                 POSITIVE(l->count[2] - l->limit),
                 POSITIVE(l->count[3] - l->limit),
                 POSITIVE(l->count[4] - l->limit),
                 POSITIVE(l->count[5] - l->limit),
                 POSITIVE(l->count[6] - l->limit),
                 POSITIVE(l->count[7] - l->limit),
                 POSITIVE(l->count[8] - l->limit),
                 POSITIVE(l->count[9] - l->limit),
                 POSITIVE(l->count[10] - l->limit),
                 POSITIVE(l->count[11] - l->limit));
        }
        memset(&l->count, 0, sizeof(l->count));
        l->last_count_time = now;
    }

    l->count[level]++;
    if (l->count[level] == l->limit + 1) {
        loga("LOG LEVEL %d REACHING LIMIT %d", level, l->limit);
        return true;
    } else if (l->count[level] > l->limit + 1) {
        return true; /* suppress these logs */
    } else {
        return false;
    }
}

static void
log_rename_check(int64_t now)
{
    struct logger *l = &logger;
    int64_t rename_interval = 86400L;  /* seconds */

    if (l->fd != STDERR_FILENO) {
        /* i.e. timezone is -28800 in Beijing */
        if ((now / 1000000 - timezone) / rename_interval
                > (l->full_name_time / 1000000 - timezone) / rename_interval) {
            loga("log will be renamed");
            log_reopen();
        }
    }
}

int
log_loggable(int level)
{
    struct logger *l = &logger;
    int64_t now;

    if (l->fd < 0) { /* maybe a bad fd if reopen failed */
        return 0;
    }

    if (level > l->level) {
        return 0;
    }

    now = nc_usec_now();
    if (level > 0 && level < LOG_N_LEVEL
            && l->limit > 0 && log_reach_limit(level, now)) {
        return 0;
    }

    /* log_rename_check cannot be called in _log, because it use _log inside */
    log_rename_check(now);

    return 1;
}

int
log_access_loggable(int level)
{
    struct logger *l = &logger;

    if (level > l->level || l->access_sampling <= 0
            || (l->access_counter++ % (uint64_t)l->access_sampling != 0)) {
        return 0;
    }

    return 1;
}


static int
_async_log(char *buf, int len)
{
    int         rc;
    size_t      dlen;

    nc_buf_t   *sbuf;

    nc_io_thread_pool_t *io_thread = NULL;
    struct logger *l = &logger;


    io_thread = l->io_thread;
    //If io_thread disable, just return;
    if (io_thread == NULL) {
        return -1;
    }

    pthread_mutex_lock(&io_thread->mtx);
    sbuf = &(io_thread->sbuf);

    if (sbuf->end - sbuf->last > len) {
        dlen = (size_t)len;
        memcpy(sbuf->last, buf, dlen);
        sbuf->last += len;
        rc = len;
    } else {
        rc = -1;
    }
    pthread_mutex_unlock(&io_thread->mtx);

    pthread_cond_signal(&io_thread->cond);
    return rc;
}


void
_log(int level, const char *file, int line, int panic, const char *fmt, ...)
{
    struct logger *l = &logger;
    int len, size, errno_save;
    char buf[LOG_MAX_LEN];
    char timestr[64];
    va_list args;
    int64_t now;
    time_t now_t;
    struct tm *local;
    ssize_t n;

    if (l->fd < 0) {
        return;
    }

    errno_save = errno;
    len = 0;            /* length of output buffer */
    size = LOG_MAX_LEN; /* size of output buffer */

    now = nc_usec_now();
    now_t = now / 1000000;
    local = localtime(&now_t);
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", local);

    len += nc_scnprintf(buf + len, size - len, "%.*s.%06d %s %s:%d ",
                        strlen(timestr), timestr, now % 1000000,
                        log_level_str(level), file, line);

    va_start(args, fmt);
    len += nc_vscnprintf(buf + len, size - len, fmt, args);
    va_end(args);

    buf[len++] = '\n';

    n = _async_log(buf, len);
    if (n == -1) {
        l->nerror++;
    }

    errno = errno_save;

    if (panic) {
        abort();
    }
}

void
_log_stderr(const char *fmt, ...)
{
    struct logger *l = &logger;
    int len, size, errno_save;
    char buf[4 * LOG_MAX_LEN];
    va_list args;
    ssize_t n;

    errno_save = errno;
    len = 0;                /* length of output buffer */
    size = 4 * LOG_MAX_LEN; /* size of output buffer */

    va_start(args, fmt);
    len += nc_vscnprintf(buf, size, fmt, args);
    va_end(args);

    buf[len++] = '\n';

    n = nc_write(STDERR_FILENO, buf, len);
    if (n < 0) {
        l->nerror++;
    }

    errno = errno_save;
}

/*
 * Hexadecimal dump in the canonical hex + ascii display
 * See -C option in man hexdump
 */
void
_log_hexdump(int level, const char *file, int line, char *data, int datalen)
{
    struct logger *l = &logger;
    char buf[8 * LOG_MAX_LEN];
    int i, off, len, size, errno_save;

    if (l->fd < 0) {
        return;
    }

    /* log hexdump */
    errno_save = errno;
    off = 0;                  /* data offset */
    len = 0;                  /* length of output buffer */
    size = 8 * LOG_MAX_LEN;   /* size of output buffer */

    while (datalen != 0 && (len < size - 1)) {
        char *save, *str;
        unsigned char c;
        int savelen;

        len += nc_scnprintf(buf + len, size - len, "DUMP %08x  ", off);

        save = data;
        savelen = datalen;

        for (i = 0; datalen != 0 && i < 16; data++, datalen--, i++) {
            c = (unsigned char)(*data);
            str = (i == 7) ? "  " : " ";
            len += nc_scnprintf(buf + len, size - len, "%02x%s", c, str);
        }
        for ( ; i < 16; i++) {
            str = (i == 7) ? "  " : " ";
            len += nc_scnprintf(buf + len, size - len, "  %s", str);
        }

        data = save;
        datalen = savelen;

        len += nc_scnprintf(buf + len, size - len, "  |");

        for (i = 0; datalen != 0 && i < 16; data++, datalen--, i++) {
            c = (unsigned char)(isprint(*data) ? *data : '.');
            len += nc_scnprintf(buf + len, size - len, "%c", c);
        }
        len += nc_scnprintf(buf + len, size - len, "|");

        off += 16;

        _log(level, file, line, 0, "%s", buf);
        len = 0;
    }

    errno = errno_save;
}
