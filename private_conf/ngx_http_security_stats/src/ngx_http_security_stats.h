#ifndef _NGX_HTTP_SECURITY_STATS_H
#define _NGX_HTTP_SECURITY_STATS_H 1

#define NGX_STATS_SHARE_MEM_PATH     "/stats_share_config"
#define NGX_STATS_SHARE_MEMORY_LEN   2*1024L*1024L
#define NGX_STATS_SHARE_MEMORY_START ((void *)0x7f00000000UL)
//#define NGX_STATS_SHARE_MEMORY_START ((void *)0x3f00000000UL)

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif


typedef struct  {
    char             name[256];                 /* server policy name */
    unsigned int     id;                        /* corresponding server policy id */
    unsigned int     ssl;                       /* mark server ssl or not */
    unsigned int     used;                      /* flags to mark if this entry is used */
    unsigned int     bypass;                    /* padding for streaming mode */
    unsigned int     refcnt;                    /* stats refcnt */
    ngx_atomic_t     req_cps;
    ngx_atomic_t     requests;                  /* requests per second */
    ngx_atomic_t     connections;               /* number of tcp connections */
    ngx_atomic_t     bandwidth;                 /* bandwith of the server */

    unsigned long    last_req_cps;              /* cps per second */
    unsigned long    last_requests;             /* requests per second */
    unsigned long    last_bandwidth;            /* bandwith of the server */
} ngx_stat;

typedef struct  {
    unsigned int   total;                   /* total number of stats in stat array */
    unsigned int   current;                 /* current number of server stat entries used */
    unsigned int   conf_weight;             /* conf weight */
    unsigned int   emergent_bypass;         /* bypass flag */
    int            worker_conns[128];
    union {
        struct {
            ngx_shmtx_sh_t    mlock;
            ngx_shmtx_t       mutex;
        };
    };
    ngx_stat stat[0];             /* stats array */
}ngx_stat_head_t;

extern ngx_stat *ngx_http_stats_add_server();
extern ngx_int_t ngx_http_security_stats_init(void);
extern void ngx_http_stats_timer(ngx_cycle_t *cycle);
extern void ngx_http_stats_del_server(void *data);

#endif /* _NGX_HTTP_SECURITY_STATS_H  */

