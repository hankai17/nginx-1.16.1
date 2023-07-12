#include <nginx.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#define form_urlencoded_type "application/x-www-form-urlencoded"
#define form_urlencoded_type_len (sizeof(form_urlencoded_type) - 1)

#define MAX_URLENCODE_BUFF 32768

typedef struct {
    long             enable;
} ngx_http_urlencode_parse_conf_t; // 解析配置文件时分配

typedef struct urlencode_buf{
    int pos;
    int length;
    char *data;
}urlencode_buf;

typedef struct {
    urlencode_buf     st_urlencode_buf;
    ngx_http_request_body_filter_pt body_filter;
    unsigned          done:1;
    unsigned          waiting_more_body:1;
    unsigned          body_requested:1;
    unsigned          processed:1;
} ngx_http_urlencode_parse_ctx_t; // 执行业务时分配

ngx_module_t ngx_http_urlencode_parse_module;

static void *ngx_http_urlencode_parse_create_conf(ngx_conf_t *cf);
static char *ngx_http_urlencode_parse_merge_conf(ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t ngx_http_urlencode_parse_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_urlencode_rewrite_handler(ngx_http_request_t *r);
static void ngx_http_urlencode_parse_post_read(ngx_http_request_t *r);

static ngx_http_request_body_filter_pt ngx_http_next_request_body_filter;

static ngx_int_t
ngx_http_urlencode_process_request_body(ngx_http_request_t *r, ngx_chain_t *body);

void ngx_http_create_urlencode_data(ngx_http_urlencode_parse_ctx_t *ctx, char *pos, char *last)
{
    int len = last - pos;
    urlencode_buf *u = &ctx->st_urlencode_buf;

    if (u->length <= u->pos){
        ctx->done = 1;
        return;
    }

    if (u->length < (u->pos + len + 1)){
        len = u->length - 1 - u->pos;
    }

    memcpy(u->data+u->pos, pos, len);
    u->pos += len;
    u->data[u->pos] = '\0';

    if (u->length <= u->pos + 1){
        ctx->done = 1;
        return;
    }

    return;
}

static ngx_int_t
ngx_http_urlencode_body_filter(ngx_http_request_t *r, ngx_chain_t *body)
{
    ngx_int_t rc = 0;

    //if (r->reqbody_processor != REQUEST_PROCESSOR_TYPE_URLENCODED){
    //    goto next;
    //}

    rc = ngx_http_urlencode_process_request_body(r, body);
    if (rc != NGX_OK){
        goto failed;
    }

    rc = ngx_http_next_request_body_filter(r, body);
    if (rc != NGX_OK) {
        goto failed;
    }

    return NGX_OK;

failed:
    return rc;
}

int ngx_http_urlencode_init(ngx_pool_t *pool,
    ngx_http_urlencode_parse_ctx_t *urlencode_ctx, int content_length)
{
    char *p = NULL;
    int content_length_n = 0;

    if (content_length <= 0){
        goto declined;
    }

    content_length_n = content_length + 1;

    /* XXX maximum size 32KB */
    if (content_length_n >= 32768) {
	    content_length_n = 32768;
	}

    p = ngx_pcalloc(pool, content_length_n);
    if (p == NULL){
        goto declined;
    }

    urlencode_ctx->st_urlencode_buf.data = p;
    urlencode_ctx->st_urlencode_buf.pos = 0;
    urlencode_ctx->st_urlencode_buf.length = content_length_n;
    return NGX_OK;

declined:
    urlencode_ctx->done = 1;
    return NGX_DECLINED;
}

static ngx_command_t ngx_http_urlencode_parse_commands[] = {
    {
      ngx_string("urlencode_parse"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_urlencode_parse_conf_t, enable),
      NULL },
    ngx_null_command
};

static ngx_http_module_t ngx_http_urlencode_parse_module_ctx = {
    NULL,                                   /* preconfiguration */
    ngx_http_urlencode_parse_init,          /* postconfiguration */
    NULL,                                   /* create main configuration */
    NULL,                                   /* init main configuration */
    NULL,                                   /* create server configuration */
    NULL,                                   /* merge server configuration */
    ngx_http_urlencode_parse_create_conf,   /* create location configuration */
    ngx_http_urlencode_parse_merge_conf     /* merge location configuration */
};


ngx_module_t ngx_http_urlencode_parse_module = {
    NGX_MODULE_V1,
    &ngx_http_urlencode_parse_module_ctx,        /* module context */
    ngx_http_urlencode_parse_commands,           /* module directives */
    NGX_HTTP_MODULE,                        /* module type */
    NULL,                                   /* init master */
    NULL,                                   /* init module */
    NULL,                                   /* init process */
    NULL,                                   /* init thread */
    NULL,                                   /* exit thread */
    NULL,                                   /* exit precess */
    NULL,                                   /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_int_t
ngx_http_urlencode_process_request_body(ngx_http_request_t *r, ngx_chain_t *body)
{
    ngx_http_urlencode_parse_ctx_t *ctx = ngx_http_get_module_ctx(r, ngx_http_urlencode_parse_module);
    if (ctx == NULL || ctx->done == 1){
        goto fin;
    }

    // Feed all the buffers into data handler
    while(body) {
        ngx_http_create_urlencode_data(ctx, (char *)body->buf->pos, (char *)body->buf->last);
        body = body->next;
    }

    /*
    if (ctx->done == 1) {
        //ctx->modsec_transaction->msc_reqbody_buffer = u->data;
        //ctx->modsec_transaction->msc_reqbody_length = u->pos;
    }
    */
fin:
    r->request_body_no_buffering = 1;
    //return NGX_AGAIN;
    return NGX_OK;
}

static ngx_int_t
ngx_http_urlencode_preaccess_handle(ngx_http_request_t *r)
{
    ngx_http_urlencode_parse_ctx_t           *ctx;
    ngx_int_t                            rc = NGX_DECLINED;

    if (r->done) {
        return NGX_OK;
    }

    if (r->method != NGX_HTTP_POST ) {
        return NGX_DECLINED;
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_urlencode_parse_module);
    if (ctx == NULL) {
        return NGX_DECLINED;
    }

    if (ctx->waiting_more_body == 1) {
        return NGX_DONE;
    }

    if (ctx->body_requested == 0)
    {
        ctx->body_requested = 1;

        r->request_body_in_single_buf = 1;
        r->request_body_in_persistent_file = 1;
        r->request_body_in_clean_file = 1;

        rc = ngx_http_read_client_request_body(r,
            ngx_http_urlencode_parse_post_read);
        if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
#if (nginx_version < 1002006) ||                                             \
    (nginx_version >= 1003000 && nginx_version < 1003009)
            r->main->count--;
#endif
            ngx_http_top_request_body_filter = ctx->body_filter;
            return rc;
        }
        if (rc == NGX_AGAIN)
        {
            ctx->waiting_more_body = 1;
            return NGX_DONE;
        }
    }

    if (ctx->waiting_more_body == 0)
    {
        //print_argument(r->arguments);
    }

    return NGX_DECLINED;
}

/* register a new rewrite phase handler */
static ngx_int_t
ngx_http_urlencode_parse_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt             *h;
    ngx_http_handler_pt             *h_preaccess;
    ngx_http_core_main_conf_t       *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_REWRITE_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }
    *h = ngx_http_urlencode_rewrite_handler;

    h_preaccess = ngx_array_push(&cmcf->phases[NGX_HTTP_PREACCESS_PHASE].handlers);
    if (h_preaccess == NULL)
    {
        return NGX_ERROR;
    }
    *h_preaccess = ngx_http_urlencode_preaccess_handle;

    ngx_http_next_request_body_filter = ngx_http_top_request_body_filter;
    ngx_http_top_request_body_filter = ngx_http_urlencode_body_filter;

    return NGX_OK;
}

static ngx_int_t
ngx_http_urlencode_rewrite_handler(ngx_http_request_t *r)
{
    ngx_http_urlencode_parse_conf_t *mumf = NULL;
    ngx_http_urlencode_parse_ctx_t  *ctx;
    ngx_str_t                        value;

    mumf = ngx_http_get_module_loc_conf(r, ngx_http_urlencode_parse_module);
    if (!mumf->enable) {
        return NGX_DECLINED;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http form_input rewrite phase handler");

    /*
    if (r->reqbody_processor_done) {
        return NGX_DECLINED;
    }
    */

    ctx = ngx_http_get_module_ctx(r, ngx_http_urlencode_parse_module);

    if (ctx != NULL) {
        if (ctx->done) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http form_input rewrite phase handler done");
            return NGX_DECLINED;
        }
        return NGX_OK;
    }

    if (r->method != NGX_HTTP_POST && r->method != NGX_HTTP_PUT) {
        return NGX_DECLINED;
    }

    if (r->headers_in.content_type == NULL
        || r->headers_in.content_type->value.data == NULL)
    {
        return NGX_DECLINED;
    }

    value = r->headers_in.content_type->value;

    /* just focus on x-www-form-urlencoded */
    if (value.len < 33 || ngx_strncasecmp(value.data, (u_char *)"application/x-www-form-urlencoded", 33)) {
        return NGX_DECLINED;
    }

    /* set processor done even may fail */
    //r->reqbody_processor_done = 1;
    //r->reqbody_processor = REQUEST_PROCESSOR_TYPE_URLENCODED;

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_urlencode_parse_ctx_t));
    if (ctx == NULL) {
        return NGX_ERROR;
    }

    if (ngx_http_urlencode_init(r->pool, ctx, r->headers_in.content_length_n) != NGX_OK){
        return NGX_DECLINED;
    }
    ngx_http_set_ctx(r, ctx, ngx_http_urlencode_parse_module);
    return NGX_DECLINED;
}

static void
ngx_http_urlencode_parse_post_read(ngx_http_request_t *r)
{
    ngx_http_urlencode_parse_ctx_t *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_urlencode_parse_module);

#if defined(nginx_version) && nginx_version >= 8011
    r->main->count--;
#endif

    if (ctx->waiting_more_body)
    {
        ctx->waiting_more_body = 0;
        ngx_http_core_run_phases(r);
    }
}

static void *
ngx_http_urlencode_parse_create_conf(ngx_conf_t *cf)
{
    ngx_http_urlencode_parse_conf_t    *fmcf;
    fmcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_urlencode_parse_conf_t));
    if (fmcf == NULL) {
        return NULL;
    }

    fmcf->enable = NGX_CONF_UNSET;
    return fmcf;
}

static char *
ngx_http_urlencode_parse_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_urlencode_parse_conf_t *prev = parent;
    ngx_http_urlencode_parse_conf_t *conf = child;
    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    return NGX_CONF_OK;
}
