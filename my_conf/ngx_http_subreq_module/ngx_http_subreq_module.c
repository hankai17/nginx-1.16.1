#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
    ngx_str_t    subreq_intercept;
} ngx_http_subreq_loc_conf_t;


typedef struct {
    ngx_int_t             done;
    ngx_chain_t          *sub_response;
    ngx_http_request_t   *subrequest;
} ngx_http_subreq_ctx_t;


static ngx_int_t ngx_http_subreq_handler(ngx_http_request_t *r);
static void ngx_http_subreq_body_handler(ngx_http_request_t *r);
static char *ngx_http_subreq(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_subreq_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_subreq_subrequest_done(ngx_http_request_t *r,
       void *data, ngx_int_t rc);
static void *ngx_http_subreq_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_subreq_merge_loc_conf(ngx_conf_t *cf,
       void *parent, void *child);

ngx_module_t  ngx_http_subreq_module;

static ngx_command_t  ngx_http_subreq_commands[] = {

    { ngx_string("subreq_intercept"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_http_subreq,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_subreq_module_ctx = {
    NULL,                               /* preconfiguration */
    ngx_http_subreq_init,                   /* postconfiguration */

    NULL,                               /* create main configuration */
    NULL,                               /* init main configuration */

    NULL,                               /* create server configuration */
    NULL,                               /* merge server configuration */

    ngx_http_subreq_create_loc_conf,        /* create location configuration */
    ngx_http_subreq_merge_loc_conf          /* merge location configuration */
};


ngx_module_t  ngx_http_subreq_module = {
    NGX_MODULE_V1,
    &ngx_http_subreq_module_ctx,            /* module context */
    ngx_http_subreq_commands,               /* module directives */
    NGX_HTTP_MODULE,                    /* module type */
    NULL,                               /* init master */
    NULL,                               /* init module */
    NULL,                               /* init process */
    NULL,                               /* init thread */
    NULL,                               /* exit thread */
    NULL,                               /* exit process */
    NULL,                               /* exit master */
    NGX_MODULE_V1_PADDING
};


static void *
ngx_http_subreq_create_loc_conf(ngx_conf_t *cf) {
    ngx_http_subreq_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_subreq_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }
    return conf;
}

static char *
ngx_http_subreq_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child) {
    ngx_http_subreq_loc_conf_t *prev = parent;
    ngx_http_subreq_loc_conf_t *conf = child;

    ngx_conf_merge_str_value(conf->subreq_intercept,
                             prev->subreq_intercept, "");
    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_subreq_subrequest_done(ngx_http_request_t *r,
                                    void *data,
                                    ngx_int_t rc) {             // wait subreq完成后的回调 用来处理返回结果
    ngx_http_subreq_ctx_t *ctx = (ngx_http_subreq_ctx_t *)data; 
    //ngx_http_subreq_loc_conf_t  *mlcf = ngx_http_get_module_loc_conf(r, ngx_http_subreq_module);

    ctx->done = 1;
    //if (rc != NGX_OK && rc != NGX_HTTP_OK) {
    //    ngx_log_error(NGX_LOG_WARN, r->main->connection->log, 0,
    //                "[RETURN_RC]:%d", rc);
    //    return NGX_OK;
    //}

    r->parent->write_event_handler = ngx_http_core_run_phases;  // 重置主请求的写回调

    return NGX_OK;
}

static ngx_int_t
ngx_http_subreq_handler(ngx_http_request_t *r) {
    ngx_int_t                rc;
    ngx_http_subreq_ctx_t       *ctx;
    ngx_http_subreq_loc_conf_t  *mlcf;

    if (r != r->main || r->internal) { // 忽略子请求 内部请求
        return NGX_DECLINED;
    }

    mlcf = ngx_http_get_module_loc_conf(r, ngx_http_subreq_module);

    if (mlcf->subreq_intercept.len == 0) { // 获取subreq的locate
        return NGX_DECLINED;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "subreq handler");

    ctx = ngx_http_get_module_ctx(r, ngx_http_subreq_module);
    if (ctx != NULL) {
        if (!ctx->done) {
            return NGX_AGAIN;
        }
        return NGX_DECLINED;
    }

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_subreq_ctx_t));
    if (ctx == NULL) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      "ngx_pcalloc error.");
        return NGX_ERROR;
    }
    ctx->done = 0;
    ngx_http_set_ctx(r, ctx, ngx_http_subreq_module);

    rc = ngx_http_read_client_request_body(r, ngx_http_subreq_body_handler);    // hankai1.0 读取post请求 // 读完整后分配sr并注册到post上
    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                      "err: %d",
                      rc);
        return rc;
    }

    r->main->count--;
    return NGX_AGAIN;                                                           // hankai2 非常暴力直接把phase断掉了
}

static void
ngx_http_subreq_body_handler(ngx_http_request_t *r) {   // 读取完整个post body后
    ngx_http_subreq_ctx_t          *ctx;
    ngx_http_post_subrequest_t *psr;
    ngx_http_subreq_loc_conf_t     *mlcf;
    ngx_http_request_t         *sr;

    ctx = ngx_http_get_module_ctx(r, ngx_http_subreq_module);
    mlcf = ngx_http_get_module_loc_conf(r, ngx_http_subreq_module);

    psr = ngx_palloc(r->pool, sizeof(ngx_http_post_subrequest_t));
    if (psr == NULL) {
        return ;
    }
    psr->handler = ngx_http_subreq_subrequest_done;
    psr->data = ctx;
    if (ngx_http_subrequest(r,
            &mlcf->subreq_intercept,        // uri 是 subreq配置的 location // 跟 mirror一样
            NULL,
            &sr,                            // subrequest的 r
            psr,                            // 因为是waited类型的subreq 所以要有一个回调来处理返回结果
            NGX_HTTP_SUBREQUEST_IN_MEMORY | NGX_HTTP_SUBREQUEST_WAITED)         // hankai1.1 分配sr注册到post上
        != NGX_OK) {
        return ;
    }

    sr->method = r->method;
    sr->method_name = r->method_name;
    ctx->subrequest = sr;

    r->preserve_body = 1;

#if 0
    ngx_http_run_posted_requests(r->connection);                                // hankai1.2 手动触发post流程
#else
    r->write_event_handler = ngx_http_core_run_phases;
    ngx_http_core_run_phases(r);                                                // hankai1.2 手动触发core_run_phase流程
#endif
}


static char *
ngx_http_subreq(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_http_subreq_loc_conf_t *mlcf = conf;

    ngx_str_t  *value;

    value = cf->args->elts;

    if (ngx_strcmp(value[1].data, "off") == 0) {
        if (mlcf->subreq_intercept.data != NULL) {
            return "is duplicate";
        }

        mlcf->subreq_intercept.data = (u_char *) "";
        mlcf->subreq_intercept.len = 0;
        return NGX_CONF_OK;
    }

    mlcf->subreq_intercept = value[1];

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_subreq_init(ngx_conf_t *cf) {
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_PREACCESS_PHASE].handlers); //默认checker: ngx_http_core_generic_phase 
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_subreq_handler;

    return NGX_OK;
}


