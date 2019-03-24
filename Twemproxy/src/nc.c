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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include <nc_core.h>
#include <nc_conf.h>
#include <nc_signal.h>
#include <nc_blacklist.h>
#include <nc_metric.h>
#include <extern/crc32.h>

#include "buildinfo.h"

#define NC_CONF_PATH        "conf/nutcracker.yml"

#define NC_LOG_DEFAULT      LOG_NOTICE
#define NC_LOG_MIN          LOG_EMERG
#define NC_LOG_MAX          LOG_PVERB
#define NC_LOG_PATH         NULL
#define NC_LOG_LIMIT        0
#define NC_LOG_ACCESS_SAMPLING 0

#define NC_STATS_PORT       STATS_PORT
#define NC_STATS_ADDR       STATS_ADDR
#define NC_STATS_INTERVAL   STATS_INTERVAL

#define NC_PID_FILE         NULL
#define NC_MANAGE_ADDR      NULL

#define NC_MBUF_SIZE        MBUF_SIZE
#define NC_MBUF_MIN_SIZE    MBUF_MIN_SIZE
#define NC_MBUF_MAX_SIZE    MBUF_MAX_SIZE
#define NC_MBUF_FREE_LIMIT  MBUF_FREE_LIMIT

#define NC_MAX_CLUSTER_NAME 256
#define NC_DOUBLE_CLUSTER_MODE MODE_MASTER

static int show_help;
static int show_version;
static int test_conf;
static int daemonize;
static int describe_stats;

static struct option long_options[] = {
    { "help",           no_argument,        NULL,   'h' },
    { "version",        no_argument,        NULL,   'V' },
    { "test-conf",      no_argument,        NULL,   't' },
    { "daemonize",      no_argument,        NULL,   'd' },
    { "describe-stats", no_argument,        NULL,   'D' },
    { "reuse-port",     no_argument,        NULL,   'R' },
    { "no-async",       no_argument,        NULL,   'N' },
    { "verbose",        required_argument,  NULL,   'v' },
    { "output",         required_argument,  NULL,   'o' },
    { "output-limit",   required_argument,  NULL,   'l' },
    { "access-sampling",required_argument,  NULL,   'r' },
    { "conf-file",      required_argument,  NULL,   'c' },
    { "stats-port",     required_argument,  NULL,   's' },
    { "stats-interval", required_argument,  NULL,   'i' },
    { "stats-addr",     required_argument,  NULL,   'a' },
    { "pid-file",       required_argument,  NULL,   'p' },
    { "mbuf-size",      required_argument,  NULL,   'm' },
    { "mbuf-nfree",     required_argument,  NULL,   'f' },
    { "lpm",            required_argument,  NULL,   'L' },
    { "blacklist",      required_argument,  NULL,   'b' },
    { "cluster",        required_argument,  NULL,   'C' },
    { "route",          required_argument,  NULL,   'g' },
    { "manage-addr",    required_argument,  NULL,   'M' },
    { "double-mode",    required_argument,  NULL,   'T' },
    { NULL,             0,                  NULL,    0  }
};

static char short_options[] = "hVtdDRNv:o:l:r:c:s:i:a:p:m:f:L:b:C:g:M:T:";

static rstatus_t
nc_daemonize(int dump_core)
{
    rstatus_t status;
    pid_t pid, sid;
    int fd;

    // 当创建子进程时，实则有两个进程从这里再发出执行代码
    // 父进程返回的是，创建的子进程的pid
    // 子进程返回的是，0
    pid = fork();
    switch (pid) {
    case -1:
        log_error("fork() failed: %s", strerror(errno));
        return NC_ERROR;

    case 0:
        // 子进程进入这里，跳出switch
        break;

    default:
        // 父进程会进这里，整个程序中断
        /* parent terminates */
        _exit(0);
    }

    /* 1st child continues and becomes the session leader */
    // 启动新的会话，并且其进程成为这个会话的组长进程（首进程）
    sid = setsid();
    if (sid < 0) {
        log_error("setsid() failed: %s", strerror(errno));
        return NC_ERROR;
    }

    // 忽略终端的 SIGHUP， 终端挂起退出发起的信号
    if (signal(SIGHUP, SIG_IGN) == SIG_ERR) {
        log_error("signal(SIGHUP, SIG_IGN) failed: %s", strerror(errno));
        return NC_ERROR;
    }

    // 停掉会话里的组长进程，启动一个新的子进程继续。why?
    pid = fork();
    switch (pid) {
    case -1:
        log_error("fork() failed: %s", strerror(errno));
        return NC_ERROR;

    case 0:
        break;

    default:
        /* 1st child terminates */
        _exit(0);
    }

    /* 2nd child continues */

    /* change working directory */
    if (dump_core == 0) {
        status = chdir("/");
        if (status < 0) {
            log_error("chdir(\"/\") failed: %s", strerror(errno));
            return NC_ERROR;
        }
    }

    /* clear file mode creation mask */
    umask(0);

    /* redirect stdin, stdout and stderr to "/dev/null" */
    // 讲stdin， stdout， stderr 重定向到 /dev/null
    fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        log_error("open(\"/dev/null\") failed: %s", strerror(errno));
        return NC_ERROR;
    }
    // dup2函数成功返回时，目标描述符（dup2函数的第二个参数）
    // 将变成源描述符（dup2函数的第一个参数）的复制品
    status = dup2(fd, STDIN_FILENO);
    if (status < 0) {
        log_error("dup2(%d, STDIN) failed: %s", fd, strerror(errno));
        close(fd);
        return NC_ERROR;
    }

    status = dup2(fd, STDOUT_FILENO);
    if (status < 0) {
        log_error("dup2(%d, STDOUT) failed: %s", fd, strerror(errno));
        close(fd);
        return NC_ERROR;
    }

    status = dup2(fd, STDERR_FILENO);
    if (status < 0) {
        log_error("dup2(%d, STDERR) failed: %s", fd, strerror(errno));
        close(fd);
        return NC_ERROR;
    }

    if (fd > STDERR_FILENO) {
        status = close(fd);
        if (status < 0) {
            log_error("close(%d) failed: %s", fd, strerror(errno));
            return NC_ERROR;
        }
    }

    return NC_OK;
}

static void
nc_print_run(struct instance *nci)
{
    int status;
    struct utsname name;

    status = uname(&name);
    if (status < 0) {
        loga("nutcracker-%s %s started on pid %d", NC_VERSION_STRING,
             get_buildinfo(), nci->pid);
    } else {
        loga("nutcracker-%s %s built for %s %s %s started on pid %d",
             NC_VERSION_STRING, get_buildinfo(),
             name.sysname, name.release, name.machine,
             nci->pid);
    }

    loga("run, rabbit run / dig that hole, forget the sun / "
         "and when at last the work is done / don't sit down / "
         "it's time to dig another one");
}

static void
nc_print_done(void)
{
    loga("done, rabbit done");
}

static void
nc_show_usage(void)
{
    log_stderr(
        "Usage: nutcracker [-?hVdDt] [-v verbosity level] [-o output file]" CRLF
        "                  [-c conf file] [-s stats port] [-a stats addr]" CRLF
        "                  [-i stats interval] [-p pid file] [-m mbuf size]" CRLF
        "                  [-l logging limit per 100ms per level]" CRLF
        "                  [-r logging access sampling] [-M manage addr]" CRLF
        "");
    log_stderr(
        "Options:" CRLF
        "  -h, --help             : this help" CRLF
        "  -V, --version          : show version and exit" CRLF
        "  -t, --test-conf        : test configuration for syntax errors and exit" CRLF
        "  -d, --daemonize        : run as a daemon" CRLF
        "  -D, --describe-stats   : print stats description and exit" CRLF
        "  -R, --reuse-port       : reuse the listen port" CRLF
        "  -N, --no-async         : no async request");
    log_stderr(
        "  -v, --verbosity=N      : set logging level (default: %d, min: %d, max: %d)" CRLF
        "  -o, --output=S         : set logging file (default: %s)" CRLF
        "  -l, --output-limit=S   : set logging limit per 100ms per level (default: %d)" CRLF
        "  -r, --access-sampling=S: set logging access sampling every n msg " CRLF
        "                           0 to disable (default: %d)" CRLF
        "  -c, --conf-file=S      : set configuration file (default: %s)" CRLF
        "  -s, --stats-port=N     : set stats monitoring port (default: %d)" CRLF
        "  -a, --stats-addr=S     : set stats monitoring ip (default: %s)" CRLF
        "  -i, --stats-interval=N : set stats aggregation interval in msec (default: %d msec)" CRLF
        "  -p, --pid-file=S       : set pid file (default: %s)" CRLF
        "  -m, --mbuf-size=N      : set size of mbuf chunk in bytes (default: %d bytes)" CRLF
        "  -f, --mbuf-free-limit=N: set mbuf free limit (default: %d)" CRLF
        "  -L, --lpm=N            : set LPM bits (default: 0, min: 0, max: 32)" CRLF
        "  -b, --blacklist=S      : set blacklist file (default: '')" CRLF
        "  -C, --cluster=S        : set cluster name (default: '')" CRLF
        "  -g, --route=S          : set route file (default: '')" CRLF
        "  -M, --manage-addr=S    : set proxy manage unix socket path" CRLF
        "  -T, --double-mode=N    : set double cluster mode, master0/slave1/redirect2/readother3" CRLF
        "",
        NC_LOG_DEFAULT, NC_LOG_MIN, NC_LOG_MAX,
        NC_LOG_PATH != NULL ? NC_LOG_PATH : "stderr",
        NC_LOG_LIMIT, NC_LOG_ACCESS_SAMPLING,
        NC_CONF_PATH,
        NC_STATS_PORT, NC_STATS_ADDR, NC_STATS_INTERVAL,
        NC_PID_FILE != NULL ? NC_PID_FILE : "off",
        NC_MBUF_SIZE, NC_MBUF_FREE_LIMIT);
}

static rstatus_t
nc_create_pidfile(struct instance *nci)
{
    char pid[NC_UINTMAX_MAXLEN];
    int fd, pid_len;
    ssize_t n;

    fd = open(nci->pid_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        log_error("opening pid file '%s' failed: %s", nci->pid_filename,
                  strerror(errno));
        return NC_ERROR;
    }
    nci->pidfile = 1;

    pid_len = nc_snprintf(pid, NC_UINTMAX_MAXLEN, "%d", nci->pid);

    n = nc_write(fd, pid, pid_len);
    if (n < 0) {
        log_error("write to pid file '%s' failed: %s", nci->pid_filename,
                  strerror(errno));
        return NC_ERROR;
    }

    close(fd);

    return NC_OK;
}

static void
nc_remove_pidfile(struct instance *nci)
{
    int status;

    status = unlink(nci->pid_filename);
    if (status < 0) {
        log_error("unlink of pid file '%s' failed, ignored: %s",
                  nci->pid_filename, strerror(errno));
    }
}

static void
nc_set_default_options(struct instance *nci)
{
    int status;

    nci->ctx = NULL;

    nci->log_level = NC_LOG_DEFAULT;
    nci->log_filename = NC_LOG_PATH;
    nci->log_limit = NC_LOG_LIMIT;
    nci->log_access_sampling = NC_LOG_ACCESS_SAMPLING;

    nci->conf_filename = NC_CONF_PATH;

    nci->stats_port = NC_STATS_PORT;
    nci->stats_addr = NC_STATS_ADDR;
    nci->stats_interval = NC_STATS_INTERVAL;

    nci->manage_addr = NC_MANAGE_ADDR;
    status = nc_gethostname(nci->hostname, NC_MAXHOSTNAMELEN);
    if (status < 0) {
        log_warn("gethostname failed, ignored: %s", strerror(errno));
        nc_snprintf(nci->hostname, NC_MAXHOSTNAMELEN, "unknown");
    }
    nci->hostname[NC_MAXHOSTNAMELEN - 1] = '\0';

    nci->mbuf_chunk_size = NC_MBUF_SIZE;
    nci->mbuf_free_limit = NC_MBUF_FREE_LIMIT;

    nci->pid = (pid_t)-1;
    nci->pid_filename = NULL;
    nci->pidfile = 0;

    nci->reuse_port = 0;
    nci->no_async = 0;
    nci->lpm_mask = 0;
    nci->blacklist_filename = NULL;
    nci->cluster_name = NULL;
    nci->route = NULL;

    nci->double_cluster_mode = NC_DOUBLE_CLUSTER_MODE;
}

static rstatus_t
nc_get_options(int argc, char **argv, struct instance *nci)
{
    int c, value;
    size_t tmp;

    opterr = 0;

    for (;;) {
        // argc, argv 是从 main 函数传递过来的，getopt_long 支持长选项的命令解析
        // 所有选项的参数都被设置为需要的或是无参数的，因为在一开始就设置了默认参数，
        // getopt_long能用过冒号决定需要参数与否
        c = getopt_long(argc, argv, short_options, long_options, NULL);
        if (c == -1) {
            /* no more options */
            break;
        }

        switch (c) {
        case 'h':
            show_version = 1;
            show_help = 1;
            break;

        case 'V':
            show_version = 1;
            break;

        case 't':
            test_conf = 1;
            break;

        case 'd':
            daemonize = 1;
            break;

        case 'D':
            describe_stats = 1;
            show_version = 1;
            break;

        case 'R':
            nci->reuse_port = 1;
            break;

        case 'N':
            nci->no_async = 1;
            break;

        case 'v':
            value = nc_atoi(optarg, strlen(optarg));
            if (value < 0) {
                log_stderr("nutcracker: option -v requires a number");
                return NC_ERROR;
            }
            nci->log_level = value;
            break;

        case 'o':
            nci->log_filename = optarg;
            break;

        case 'l':
            nci->log_limit = nc_atoi(optarg, strlen(optarg));
            break;

        case 'r':
            nci->log_access_sampling = nc_atoi(optarg, strlen(optarg));
            break;

        case 'c':
            nci->conf_filename = optarg;
            break;

        case 's':
            value = nc_atoi(optarg, strlen(optarg));
            if (value < 0) {
                log_stderr("nutcracker: option -s requires a number");
                return NC_ERROR;
            }
            if (!nc_valid_port(value)) {
                log_stderr("nutcracker: option -s value %d is not a valid "
                           "port", value);
                return NC_ERROR;
            }

            nci->stats_port = (uint16_t)value;
            break;

        case 'i':
            value = nc_atoi(optarg, strlen(optarg));
            if (value < 0) {
                log_stderr("nutcracker: option -i requires a number");
                return NC_ERROR;
            }

            nci->stats_interval = value;
            break;

        case 'a':
            nci->stats_addr = optarg;
            break;

        case 'M':
            nci->manage_addr = optarg;
            break;

        case 'p':
            nci->pid_filename = optarg;
            break;

        case 'm':
            value = nc_atoi(optarg, strlen(optarg));
            if (value <= 0) {
                log_stderr("nutcracker: option -m requires a non-zero number");
                return NC_ERROR;
            }

            if (value < NC_MBUF_MIN_SIZE || value > NC_MBUF_MAX_SIZE) {
                log_stderr("nutcracker: mbuf chunk size must be between %zu and"
                           " %zu bytes", NC_MBUF_MIN_SIZE, NC_MBUF_MAX_SIZE);
                return NC_ERROR;
            }

            nci->mbuf_chunk_size = (size_t)value;
            break;

        case 'f':
            value = nc_atoi(optarg, strlen(optarg));
            if (value < 0) {
                log_stderr("nutcracker: option -f requires a non-zero number");
                return NC_ERROR;
            }
            nci->mbuf_free_limit = (size_t)value;
            break;

        case 'L':
            value = nc_atoi(optarg, strlen(optarg));
            if (value < 0 || value > 32) {
                log_stderr("nutcracker: option -L requires a number in [0,32]");
                return NC_ERROR;
            }
            nci->lpm_mask = ~((1ull << (32 - value)) - 1);
            break;

        case 'b':
            nci->blacklist_filename = optarg;
            break;
        case 'C':
            tmp = strlen(optarg);
            if (tmp > 0 && tmp < NC_MAX_CLUSTER_NAME) {
                nci->cluster_name = optarg;
            } else {
                log_stderr("nutcracker: option -n requires string length in [1, 255].");
                return NC_ERROR;
            }
            break;
        case 'T':
            value = nc_atoi(optarg, strlen(optarg));
            if (value < 0 || value > 4) {
                log_stderr("nutcracker: option -T requires a number in 0/1/2/3");
                return NC_ERROR;
            }
            nci->double_cluster_mode = (size_t)value;
            break;
        case 'g':
            nci->route = route_graph_load(optarg);
            if (!nci->route) {
                log_stderr("Failed to load %s", optarg);
                return NC_ERROR;
            }
            route_graph_dump(nci->route);
            break;
        case '?':
            switch (optopt) {
            case 'o':
            case 'c':
            case 'p':
                log_stderr("nutcracker: option -%c requires a file name",
                           optopt);
                break;

            case 'm':
            case 'v':
            case 's':
            case 'i':
                log_stderr("nutcracker: option -%c requires a number", optopt);
                break;

            case 'a':
                log_stderr("nutcracker: option -%c requires a string", optopt);
                break;

            default:
                log_stderr("nutcracker: invalid option -- '%c'", optopt);
                break;
            }
            return NC_ERROR;

        default:
            log_stderr("nutcracker: invalid option -- '%c'", optopt);
            return NC_ERROR;

        }
    }

    return NC_OK;
}

/*
 * Returns true if configuration file has a valid syntax, otherwise
 * returns false
 */
static bool
nc_test_conf(struct instance *nci)
{
    struct conf *cf;

    cf = conf_create(nci->conf_filename);
    if (cf == NULL) {
        log_stderr("nutcracker: configuration file '%s' syntax is invalid",
                   nci->conf_filename);
        return false;
    }

    conf_destroy(cf);

    log_stderr("nutcracker: configuration file '%s' syntax is ok",
               nci->conf_filename);
    return true;
}

static rstatus_t
nc_pre_run(struct instance *nci)
{
    rstatus_t status;

    // 信号初始化，这里指定了对什么信号要进行处理
    // 信号列表放在了signals[]里
    status = signal_init();
    if (status != NC_OK) {
        return status;
    }

    // log的初始化
    status = log_init(nci->log_level, nci->log_filename, nci->log_limit,
                      nci->log_access_sampling);
    if (status != NC_OK) {
        return status;
    }

    // 判断是否是守护进程启动
    if (daemonize) {
        status = nc_daemonize(1);
        if (status != NC_OK) {
            return status;
        }
    }

    nci->pid = getpid();

    // 创建进程pid的文件
    if (nci->pid_filename) {
        status = nc_create_pidfile(nci);
        if (status != NC_OK) {
            return status;
        }
    }

    // 黑名单不知道是什么鬼
    if (nci->blacklist_filename) {
        if (!blacklist_init(nci->blacklist_filename)) {
            log_error("Failed to load blacklist: %s", nci->blacklist_filename);
        }
    }

    // 初始化集群
    if (nci->cluster_name) {
        if (NC_OK != nc_init_metric(nci)) {
            log_error("Failed to init metric for cluster %s", nci->cluster_name);
            return NC_ERROR;
        }
    }

    crc32_init();
    // 这里面打印的run, rabbit run / dig that hole
    nc_print_run(nci);  

    return NC_OK;
}

static void
nc_post_run(struct instance *nci)
{
    if (nci->pidfile) {
        nc_remove_pidfile(nci);
    }

    signal_deinit();

    nc_print_done();

    nc_destroy_metric();

    log_deinit();
}

static void
nc_run(struct instance *nci)
{
    rstatus_t status;
    // 申明对应的上下文
    struct context *ctx;

    // 创建上下文ctx，包括里面的所有服务池的初始化
    ctx = core_start(nci);
    if (ctx == NULL) {
        return;
    }

    /* run rabbit run */
    for (;;) {
        // ok 则继续运行这个loop，如果有问题则报错退出，停止core
        status = core_loop(ctx);
        if (status != NC_OK) {
            break;
        }
    }

    core_stop(ctx);
}

int
main(int argc, char **argv)
{
    rstatus_t status;
    // move to global
    //struct instance nci;

    // 设置默认的启动参数
    nc_set_default_options(&global_nci);

    // 这里主要是将所有的启动参数抓下来，替换掉默认的启动参数
    // 若是没有对应项的启动参数，则选择使用默认的启动参数
    status = nc_get_options(argc, argv, &global_nci);
    if (status != NC_OK) {
        nc_show_usage();
        exit(1);
    }

    // 是否只需要展现版本  --version
    if (show_version) {
        log_stderr("This is nutcracker-%s %s" CRLF, NC_VERSION_STRING, get_buildinfo());
        if (show_help) {
            nc_show_usage();
        }

        if (describe_stats) {
            stats_describe();
        }

        exit(0);
    }

    // 是否只是想要测试conf配置文件(他们的配置文件格式是yaml的)
    // 在这里 -t 只是想要测试这个文件是否符合预期的格式以及标准 
    if (test_conf) {
        if (!nc_test_conf(&global_nci)) {
            exit(1);
        }
        exit(0);
    }

    // 叫做预处理吧，对log，signal，黑名单，集群metric(规则)，守护进程等进行初始化
    status = nc_pre_run(&global_nci);
    if (status != NC_OK) {
        nc_post_run(&global_nci);
        exit(1);
    }

    // 正式开始运行proxy，里面包括了对上下文的初始化以及对死循环epoll等待事件并进行处理
    nc_run(&global_nci);

    nc_post_run(&global_nci);

    exit(1);
}
