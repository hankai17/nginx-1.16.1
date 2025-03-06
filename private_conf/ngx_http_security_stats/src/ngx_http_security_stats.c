#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_md5.h>
#include <sys/time.h>

#include "ngx_http_security_stats.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#define OUT_FILE "/home/debug.txt"
#define WAF_LOG_DEBUG(fmt,...)\
do{ \
    char cmd[512+100] = {0}; \
    char tmp[512] = {0}; \
    static time_t ngx_now;\
    time(&ngx_now);\
    struct tm * ngx_curTime;\
    ngx_curTime = localtime(&ngx_now); \
    snprintf(tmp, 512,"%d-%d-%d %2d:%2d:%2d   Func:%s Len:%d ------  "fmt,\
            ngx_curTime->tm_year+1900,ngx_curTime->tm_mon+1,ngx_curTime->tm_mday, \
            ngx_curTime->tm_hour,ngx_curTime->tm_min, ngx_curTime->tm_sec, \
            __FUNCTION__,__LINE__ ,##__VA_ARGS__);\
    snprintf(cmd, 612, "echo %s >> %s", tmp, OUT_FILE); \
    system(cmd);\
}while(0)


int stats_share_fd = -1;
static void *stats_share_ptr = NULL;
ngx_stat_head_t *ngx_stat_head = NULL;

static ngx_event_t stats_ev;
static ngx_connection_t  dumb;

typedef struct {
    long        enable;
    ngx_stat   *stat;
} ngx_http_security_stats_conf_t;

static ngx_int_t ngx_http_stats_init(ngx_conf_t *cf);
static void * ngx_http_security_stats_create_loc_conf(ngx_conf_t *cf);
static char * ngx_http_security_stats_merge_loc_conf(ngx_conf_t *cf, 
        void *parent, void *child);

static ngx_command_t  ngx_http_security_stats_commands[] = {
      {
      ngx_string("security_stat"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_security_stats_conf_t, enable),
      NULL },
      ngx_null_command
};

static ngx_http_module_t  ngx_http_security_stats_module_ctx = {
    NULL,                                   /* preconfiguration */
    ngx_http_stats_init,                    /* postconfiguration */
    NULL,                                   /* create main configuration */
    NULL,                                   /* init main configuration */
    NULL,                                   /* create server configuration */
    NULL,                                   /* merge server configuration */
    ngx_http_security_stats_create_loc_conf,/* create location configuration */
    ngx_http_security_stats_merge_loc_conf  /* merge location configuration */
};

ngx_module_t  ngx_http_security_stats_module = {
    NGX_MODULE_V1,
    &ngx_http_security_stats_module_ctx,   /* module context */
    ngx_http_security_stats_commands,      /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

static void ngx_http_stats_timer_do(ngx_event_t *stats_ev)
{
    void                       *ptr = NULL;
    ngx_stat_head_t            *head;
    unsigned int                i;
    ngx_int_t                   fd = -1;
    static uint64_t             sleep_time = 0;

    sleep_time++;

    fd = shm_open(NGX_STATS_SHARE_MEM_PATH, O_RDWR, 0666);
    if (fd == -1) {
        ngx_add_timer(stats_ev, 5000);
        return ;
    }

    ptr = mmap(NULL, NGX_STATS_SHARE_MEMORY_LEN,
            PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        ngx_add_timer(stats_ev, 5000);
        close (fd);
        return ;
    }

    head = ptr;
    if (head == NULL) {
        munmap(ptr, NGX_STATS_SHARE_MEMORY_LEN);
        ngx_add_timer(stats_ev, 5000);
        close (fd);
        return ;
    }

    if (sleep_time % 2 == 0) {
        for (i = 0; i < head->total; i++) {
            if (head->stat[i].used == 1) {
                head->stat[i].last_requests = ngx_atomic_fetch_add(&head->stat[i].requests, 0) / 2;                
                head->stat[i].last_req_cps = ngx_atomic_fetch_add(&head->stat[i].req_cps, 0) / 2;
                head->stat[i].last_bandwidth = ngx_atomic_fetch_add(&head->stat[i].bandwidth, 0) / 2;
                ngx_atomic_cmp_set(&head->stat[i].requests, head->stat[i].requests, 0);
                ngx_atomic_cmp_set(&head->stat[i].req_cps, head->stat[i].req_cps, 0);
                ngx_atomic_cmp_set(&head->stat[i].bandwidth, head->stat[i].bandwidth, 0);
                /*
                WAF_LOG_DEBUG("connections is %ld, requests %ld, bandwidth %ld",
                        head->stat[i].connections, 
                        head->stat[i].last_requests, 
                        head->stat[i].last_bandwidth);
                */
            }
        }
        sleep_time = 0;
    }

    munmap(ptr, NGX_STATS_SHARE_MEMORY_LEN);
    ngx_add_timer(stats_ev, 1000);
    close (fd);
    return ;
}

void ngx_http_stats_timer(ngx_cycle_t *cycle)
{
    ngx_uint_t                  time = 1000;

    stats_ev.handler= ngx_http_stats_timer_do;
    stats_ev.log = cycle->log;
    stats_ev.data = &dumb;
    dumb.fd = (ngx_socket_t) -1;

    ngx_add_timer(&stats_ev, time);

    return;
}

void ngx_http_security_stats_bandwidth(ngx_http_request_t *r, __u64 len)
{
    ngx_stat                        *stat;
    ngx_http_security_stats_conf_t  *sscf = NULL;

    sscf = ngx_http_get_module_loc_conf(r, ngx_http_security_stats_module);
    if (sscf == NULL 
            || sscf->enable == 0 
            || sscf->stat == NULL) {
        return;
    }

    stat = (ngx_stat *)sscf->stat;
    ngx_atomic_fetch_add(&stat->bandwidth, len);

    return;
}

ngx_int_t
ngx_http_security_stats_write_filter(ngx_http_request_t *r, off_t bsent)
{
    off_t                           bytes;
    ngx_stat                       *stat;
    ngx_http_security_stats_conf_t *sscf = NULL;

    sscf = ngx_http_get_module_loc_conf(r, ngx_http_security_stats_module);
    if (sscf == NULL 
            || sscf->enable == 0 
            || sscf->stat == NULL) {
        return NGX_DECLINED;
    }

    stat = (ngx_stat *)sscf->stat;
    bytes = r->connection->sent - bsent;
    ngx_atomic_fetch_add(&stat->bandwidth, bytes);

    return NGX_DECLINED;
}

ngx_int_t
ngx_http_security_stats_handler(ngx_http_request_t *r)
{
    /*
    ngx_stat                        *stat;
    ngx_http_security_stats_conf_t  *sscf = NULL;

    sscf = ngx_http_get_module_loc_conf(r, ngx_http_security_stats_module);
    if (sscf == NULL 
            || sscf->enable == 0 
            || sscf->stat == NULL) {
        return NGX_DECLINED;
    }

    stat = (ngx_stat *)sscf->stat;
    if (stat) {
        printf("policy name: %s\n", stat->name);
    }
    */
    return NGX_DECLINED;
}

static ngx_int_t
ngx_http_stats_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    h = ngx_array_push(&cmcf->phases[NGX_HTTP_PREACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_security_stats_handler;

    ngx_http_top_write_filter = ngx_http_security_stats_write_filter;

    return NGX_OK;
}

ngx_stat *ngx_http_stats_add_server()
{
    ngx_stat                *node = NULL;
    ngx_stat_head_t         *head = NULL;
    static unsigned int      next = 0;

    if (ngx_test_config || ngx_process == NGX_PROCESS_SIGNALLER) {
        return NULL;
    }

    head = ngx_stat_head;

    if (unlikely(head == NULL)) {
        return NULL;
    }

    if (head->total == head->current) {
        return NULL;
    }

    ngx_shmtx_lock(&head->mutex);

    for (; next < 2 * head->total; ++next) {
        next = next % head->total;
        node = &(head->stat[next]);
        if (node->used == 0) {
            memset(node, 0, sizeof(*node));
            node->used = 1;
            node->refcnt = 1;
            ++head->current;
            next = (next+1) % head->total;
            goto done;
        }
    }

done:
    ngx_shmtx_unlock(&head->mutex);

    return node;
}

void ngx_http_stats_del_server(void *data)
{
    return;
}

static void *
ngx_http_security_stats_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_security_stats_conf_t    *sscf;
    sscf = ngx_pcalloc(cf->pool, sizeof(ngx_http_security_stats_conf_t));
    if (sscf == NULL) {
        return NULL;
    }

    sscf->enable = NGX_CONF_UNSET;
    return sscf;
}

static char *
ngx_http_security_stats_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_core_srv_conf_t        *cscf = NULL;
    ngx_http_security_stats_conf_t  *prev = parent;
    ngx_http_security_stats_conf_t  *conf = child;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);

    if (conf->enable) {
        ngx_stat    *stat = NULL;
        char         policy_name[256] = {0};
        char         server_name[128] = "_";

        cscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_core_module);
        if (cscf == NULL) {
            return NGX_CONF_OK;
        }
        if (cscf->server_name.data && 
                cscf->server_name.len) {
            strncpy(server_name, (char*)cscf->server_name.data, cscf->server_name.len);
        }
        
        snprintf(policy_name, sizeof(policy_name) - 1, "%s-%lu",
                server_name, 
                cscf->line);
        
        stat = ngx_http_stats_add_server();
        if (stat) {
            strncpy(stat->name, policy_name, 256);
            conf->stat = stat;
        }
    }

    return NGX_CONF_OK;
}

static int ngx_stats_shm_create(void)
{
    shm_unlink(NGX_STATS_SHARE_MEM_PATH);
    stats_share_fd = shm_open(NGX_STATS_SHARE_MEM_PATH, O_CREAT|O_RDWR|O_TRUNC, 0666);
    if (stats_share_fd == -1) {
        return -1;
    }
    return ftruncate(stats_share_fd, NGX_STATS_SHARE_MEMORY_LEN);
}

int ngx_stats_shm_init(void)
{
    ngx_int_t    ret = 0;

    if (stats_share_ptr != NULL) {
        ret = 1;
    } else {
        stats_share_fd = shm_open(NGX_STATS_SHARE_MEM_PATH, O_RDWR, 0666);
        if (stats_share_fd == -1) {
            if (ngx_stats_shm_create() != 0 ) {
                return -1;
            }
        }

        stats_share_ptr = mmap(NULL, NGX_STATS_SHARE_MEMORY_LEN,
                PROT_READ|PROT_WRITE, MAP_SHARED,stats_share_fd, 0);
        if (stats_share_ptr == MAP_FAILED) {
	        stats_share_ptr = NULL;
            return -1;
        }
    }
    ngx_stat_head = stats_share_ptr;

    return ret;
}

int ngx_http_stats_head_init(void)
{
    unsigned int      i;
    ngx_stat_head_t  *head;

    head = ngx_stat_head;

    if (head == NULL) {
        return -1;
    }

    memset(head, 0, NGX_STATS_SHARE_MEMORY_LEN);

#if (NGX_HAVE_ATOMIC_OPS)
    ngx_stat_head->mutex.spin = (ngx_uint_t) -1;

    if (ngx_shmtx_create(&ngx_stat_head->mutex, &ngx_stat_head->mlock, NULL) != NGX_OK) {
        return -1;
    }
#else
#error "machine arch doesn't support atomic ops, fixthis lock!!!"
#endif

    head->total = (NGX_STATS_SHARE_MEMORY_LEN - sizeof(ngx_stat_head_t))/sizeof(ngx_stat);

    for (i = 0; i < head->total; i++) {
        head->stat[i].used           = 0;
        head->stat[i].last_requests  = 0;
        head->stat[i].last_req_cps  = 0;
        head->stat[i].last_bandwidth = 0;
        ngx_atomic_cmp_set(&head->stat[i].requests, head->stat[i].requests, 0);
        ngx_atomic_cmp_set(&head->stat[i].req_cps, head->stat[i].req_cps, 0);
        ngx_atomic_cmp_set(&head->stat[i].bandwidth, head->stat[i].bandwidth, 0);
    }

    return 0;
}

ngx_int_t
ngx_http_security_stats_init(void)
{
    ngx_int_t                   ret;

    ret = ngx_stats_shm_init();
    if (ret < 0) {
        return NGX_ERROR;
    }

    if (ngx_test_config || ngx_process == NGX_PROCESS_SIGNALLER || ret == 1) {
        return NGX_OK;
    }

    if ((ret = ngx_http_stats_head_init()) < 0) {
        return NGX_ERROR;
    }

    return NGX_OK;
}

