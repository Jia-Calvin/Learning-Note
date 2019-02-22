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

#ifndef _NC_LOG_H_
#define _NC_LOG_H_

#define LOG_EMERG   0   /* system in unusable */
#define LOG_ALERT   1   /* action must be taken immediately */
#define LOG_CRIT    2   /* critical conditions */
#define LOG_ERROR   3   /* error conditions */
#define LOG_WARN    4   /* warning conditions */
#define LOG_NOTICE  5   /* normal but significant condition (default) */
#define LOG_INFO    6   /* informational */
#define LOG_DEBUG   7   /* debug messages */
#define LOG_VERB    8   /* verbose messages */
#define LOG_VVERB   9   /* verbose messages on crack */
#define LOG_VVVERB  10  /* verbose messages on ganga */
#define LOG_PVERB   11  /* periodic verbose messages on crack */
#define LOG_N_LEVEL 12  /* max level */

#define LOG_MAX_LEN 512 /* max length of log message */

#define LOG_BUF_MAX_LEN 4194304 /* max length of log message */

typedef struct {
    char        *start;
    char        *pos;
    char        *last;
    char        *end;
    int          size;
} nc_buf_t;

typedef struct {
    pthread_t           thread_id;
    pthread_cond_t      cond;
    pthread_mutex_t     mtx;

    nc_buf_t            lbuf;
    nc_buf_t            sbuf;
} nc_io_thread_pool_t;

struct logger {
    char        *name;              /* log file name */
    char        *full_name;         /* log file name with time */
    size_t      nfull_name;         /* length of full name buf */
    int64_t     full_name_time;     /* time for log file opened */
    int         level;              /* log level */
    int         fd;                 /* log file descriptor */
    int         nerror;             /* # log error */
    int         limit;              /* # limit per 100ms per level */
    int         access_sampling;    /* access sampling every n msg */
    uint64_t    access_counter;     /* access counter */
    int64_t     last_count_time;    /* last log stat time */
    int         count[LOG_N_LEVEL]; /* # log stat per level */


    nc_io_thread_pool_t     *io_thread;
};

/*
 * log_stderr   - log to stderr
 * loga         - log always
 * loga_hexdump - log hexdump always
 * log_panic    - log messages followed by a panic
 * log_emerg    - emerg log messages
 * log_crit     - crit log messages
 * log_error    - error log messages
 * log_warn     - warning log messages
 * log_notice   - notice log messages
 * log_info     - info log messages
 * ...
 * log_debug    - debug log messages based on a log level
 * log_hexdump  - hexadump -C of a log buffer on a log level
 */

#define log_stderr(...) do {                                                \
    _log_stderr(__VA_ARGS__);                                               \
} while (0)

#define loga(...) do {                                                      \
    /* forbidden to call log_loggable to avoid recursively */               \
    _log(-1, __FILE__, __LINE__, 0, __VA_ARGS__);                           \
} while (0)

#define loga_hexdump(_data, _datalen, ...) do {                             \
    _log(-1, __FILE__, __LINE__, 0, __VA_ARGS__);                           \
    _log_hexdump(-1, __FILE__, __LINE__, (char *)(_data), (int)(_datalen)); \
} while (0)                                                                 \

#define log_panic(...) do {                                                 \
    if (log_loggable(LOG_EMERG) != 0) {                                     \
        _log(LOG_EMERG, __FILE__, __LINE__, 1, __VA_ARGS__);                \
    }                                                                       \
} while (0)

#define log_emerg(...) do {                                                 \
    if (log_loggable(LOG_EMERG) != 0) {                                     \
        _log(LOG_EMERG, __FILE__, __LINE__, 0, __VA_ARGS__);                \
    }                                                                       \
} while (0)

#define log_alert(...) do {                                                 \
    if (log_loggable(LOG_ALERT) != 0) {                                     \
        _log(LOG_ALERT, __FILE__, __LINE__, 0, __VA_ARGS__);                \
    }                                                                       \
} while (0)

#define log_crit(...) do {                                                  \
    if (log_loggable(LOG_CRIT) != 0) {                                      \
        _log(LOG_CRIT, __FILE__, __LINE__, 0, __VA_ARGS__);                 \
    }                                                                       \
} while (0)

#define log_error(...) do {                                                 \
    if (log_loggable(LOG_ERROR) != 0) {                                     \
        _log(LOG_ERROR, __FILE__, __LINE__, 0, __VA_ARGS__);                \
    }                                                                       \
} while (0)

#define log_warn(...) do {                                                  \
    if (log_loggable(LOG_WARN) != 0) {                                      \
        _log(LOG_WARN, __FILE__, __LINE__, 0, __VA_ARGS__);                 \
    }                                                                       \
} while (0)

#define log_notice(...) do {                                                \
    if (log_loggable(LOG_NOTICE) != 0) {                                    \
        _log(LOG_NOTICE, __FILE__, __LINE__, 0, __VA_ARGS__);               \
    }                                                                       \
} while (0)

#define log_info(...) do {                                                  \
    if (log_loggable(LOG_INFO) != 0) {                                      \
        _log(LOG_INFO, __FILE__, __LINE__, 0, __VA_ARGS__);                 \
    }                                                                       \
} while (0)

#ifdef NC_DEBUG_LOG

#define log_debug(_level, ...) do {                                         \
    if (log_loggable(_level) != 0) {                                        \
        _log(_level, __FILE__, __LINE__, 0, __VA_ARGS__);                   \
    }                                                                       \
} while (0)

#else

#define log_debug(_level, ...)

#endif

#define log_hexdump(_level, _data, _datalen, ...) do {                      \
    if (log_loggable(_level) != 0) {                                        \
        _log(_level, __FILE__, __LINE__, 0, __VA_ARGS__);                   \
        _log_hexdump(_level, __FILE__, __LINE__, (char *)(_data),           \
                     (int)(_datalen));                                      \
    }                                                                       \
} while (0)

#define log_access(...) do {                                                \
    if (log_access_loggable(LOG_NOTICE) != 0) {                             \
        _log(LOG_NOTICE, __FILE__, __LINE__, 0, __VA_ARGS__);               \
    }                                                                       \
} while (0)

int log_init(int level, char *filename, int limit, int access_sampling);
void log_deinit(void);
void log_level_up(void);
void log_level_down(void);
void log_level_set(int level);
void log_reopen(void);
int log_loggable(int level);
int log_access_loggable(int level);
void _log(int level, const char *file, int line, int panic,
          const char *fmt, ...);
void _log_stderr(const char *fmt, ...);
void _log_hexdump(int level, const char *file, int line,
                  char *data, int datalen);

#endif
