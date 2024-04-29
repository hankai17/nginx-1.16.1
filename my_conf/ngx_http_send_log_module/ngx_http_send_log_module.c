#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
    ngx_array_t                   *lengths;
    ngx_array_t                   *values;
} ngx_http_send_log_script_t;

typedef struct {
    ngx_http_upstream_conf_t       upstream;
    ngx_array_t                   *upstream_srv_confs;
} ngx_http_send_log_loc_conf_t;

typedef struct {
    ngx_http_upstream_srv_conf_t  *uscf;
} ngx_http_send_log_upstream_srv_conf_t;

static char * ngx_http_send_log_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void * ngx_http_send_log_create_loc_conf(ngx_conf_t *cf);
static char * ngx_http_send_log_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t ngx_http_send_log_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_send_log_init(ngx_conf_t *cf);
static ngx_uint_t ngx_http_create_fake_request(ngx_http_request_t *r, ngx_http_request_t **fake_request);
extern void ngx_http_upstream_finalize_request(ngx_http_request_t *r, ngx_http_upstream_t *u, ngx_int_t rc);
extern void ngx_http_upstream_connect(ngx_http_request_t *r, ngx_http_upstream_t *u);
static ngx_int_t ngx_http_stop_request(ngx_http_request_t *r);

static void ngx_http_close_fake_request(ngx_http_request_t *r);
static void ngx_http_free_fake_request(ngx_http_request_t *r);
static void ngx_http_close_fake_connection(ngx_connection_t *c);

extern ngx_int_t ngx_http_upstream_test_connect(ngx_connection_t *c);
extern void ngx_http_upstream_send_request_handler(ngx_http_request_t *r, ngx_http_upstream_t *u);
extern void ngx_http_upstream_process_header(ngx_http_request_t *r, ngx_http_upstream_t *u);

static u_char * 
ngx_http_log_error(ngx_log_t *log, u_char *buf, size_t len)
{
    u_char              *p;
    ngx_http_request_t  *r;
    ngx_http_log_ctx_t  *ctx;

    ngx_uint_t  port;

    if (log->action) {
        p = ngx_snprintf(buf, len, " while %s", log->action);
        len -= p - buf;
        buf = p;
    }

    ctx = log->data;

    port = ngx_inet_get_port(ctx->connection->sockaddr);

    p = ngx_snprintf(buf, len, ", client: %V:%ui", &ctx->connection->addr_text, port);
    len -= p - buf;

    r = ctx->request;

    if (r) {
        return r->log_handler(r, ctx->current_request, p, len);

    } else {
        p = ngx_snprintf(p, len, ", server: %V",
                &ctx->connection->listening->addr_text);
    }

    return p;
}

static ngx_command_t ngx_http_send_log_commands[] = {
    {
        ngx_string("send_log_pass"),
        NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
        ngx_http_send_log_pass,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL,
    },
    { ngx_string("send_log_connect_timeout"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_msec_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_send_log_loc_conf_t, upstream.connect_timeout),
        NULL },
    { ngx_string("send_log_send_timeout"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_msec_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_send_log_loc_conf_t, upstream.send_timeout),
        NULL },
    { ngx_string("send_log_read_timeout"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_msec_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_send_log_loc_conf_t, upstream.read_timeout),
        NULL },
    ngx_null_command,
};

static ngx_http_module_t  ngx_http_send_log_ctx = {
    NULL,
    ngx_http_send_log_init,
    NULL,
    NULL,
    NULL,
    NULL,
    ngx_http_send_log_create_loc_conf,
    ngx_http_send_log_merge_loc_conf
};

ngx_module_t ngx_http_send_log_module = {
    NGX_MODULE_V1,
    &ngx_http_send_log_ctx,           /* module context */
    ngx_http_send_log_commands,       /* module directives */
    NGX_HTTP_MODULE,                     /* module type */
    NULL,                                /* init master */
    NULL,                                /* init module */
    NULL,                                /* init process */
    NULL,                                /* init thread */
    NULL,                                /* exit thread */
    NULL,                                /* exit process */
    NULL,                                /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_int_t
ngx_http_send_log_init(ngx_conf_t *cf)
{
    ngx_http_core_main_conf_t  *cmcf;
    ngx_http_handler_pt        *h;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_LOG_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_send_log_handler;

    return NGX_OK;
}

static char *
ngx_http_send_log_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_send_log_loc_conf_t             *fmlcf = conf;
    ngx_str_t                                   *value, *url;
    ngx_uint_t                                   i, n = 0;
    ngx_url_t                                    u;
    size_t                                       add;
    u_short                                      port;
    ngx_http_send_log_upstream_srv_conf_t    *send_log_upstream_srv_conf;

    if (fmlcf->upstream_srv_confs) {
        return "send log already configed";
    }

    if (cf->args->nelts - 1 > 2) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                "send log pass upstream sum exceed");
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {
        url = &value[i];

        n = ngx_http_script_variables_count(url);

        if (n) {
            if (cf->args->nelts > 2) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                        "send log pass variable only support one arg");
                return NGX_CONF_ERROR;
            }
        }
    }

    fmlcf->upstream_srv_confs = ngx_array_create(cf->pool, 5,
            sizeof(ngx_http_send_log_upstream_srv_conf_t));
    if (fmlcf->upstream_srv_confs == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                "array create send log upstream_srv_confs failed");
        return NGX_CONF_ERROR;
    }

    for (i = 1; i < cf->args->nelts; i++) {
        url = &value[i];

        if (ngx_strncasecmp(url->data, (u_char *) "http://", 7) == 0) {
            add = 7;
            port = 80;
        } else {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                    "send log not support URL");
            return NGX_CONF_ERROR;
        }

        ngx_memzero(&u, sizeof(ngx_url_t));

        u.url.len = url->len - add;
        u.url.data = url->data + add;
        u.default_port = port;
        u.uri_part = 1;
        u.no_resolve = 1;

        send_log_upstream_srv_conf = ngx_array_push(fmlcf->upstream_srv_confs);
        if (send_log_upstream_srv_conf == NULL) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                    "array push send log upstream_srv_confs failed");
            return NGX_CONF_ERROR;
        }
        send_log_upstream_srv_conf->uscf = ngx_http_upstream_add(cf, &u, 0);

        if (send_log_upstream_srv_conf->uscf == NULL) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                    "upstream add in upstream srv conf in send log pass failed");
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}

static void *
ngx_http_send_log_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_send_log_loc_conf_t *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_send_log_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->upstream.store = NGX_CONF_UNSET;
    conf->upstream.store_access = NGX_CONF_UNSET_UINT;
    conf->upstream.next_upstream_tries = NGX_CONF_UNSET_UINT;
    conf->upstream.buffering = NGX_CONF_UNSET;
    conf->upstream.request_buffering = NGX_CONF_UNSET;
    conf->upstream.ignore_client_abort = NGX_CONF_UNSET;
    conf->upstream.force_ranges = NGX_CONF_UNSET;

    conf->upstream.local = NGX_CONF_UNSET_PTR;
    conf->upstream.socket_keepalive = NGX_CONF_UNSET;

    conf->upstream.connect_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.send_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.read_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.next_upstream_timeout = NGX_CONF_UNSET_MSEC;

    conf->upstream.send_lowat = NGX_CONF_UNSET_SIZE;
    conf->upstream.buffer_size = NGX_CONF_UNSET_SIZE;
    conf->upstream.limit_rate = NGX_CONF_UNSET_SIZE;

    conf->upstream.busy_buffers_size_conf = NGX_CONF_UNSET_SIZE;
    conf->upstream.max_temp_file_size_conf = NGX_CONF_UNSET_SIZE;
    conf->upstream.temp_file_write_size_conf = NGX_CONF_UNSET_SIZE;

    conf->upstream.pass_request_headers = NGX_CONF_UNSET;
    conf->upstream.pass_request_body = NGX_CONF_UNSET;

#if (NGX_HTTP_CACHE)
    conf->upstream.cache = NGX_CONF_UNSET;
    conf->upstream.cache_min_uses = NGX_CONF_UNSET_UINT;
    conf->upstream.cache_max_range_offset = NGX_CONF_UNSET;
    conf->upstream.cache_bypass = NGX_CONF_UNSET_PTR;
    conf->upstream.no_cache = NGX_CONF_UNSET_PTR;
    conf->upstream.cache_valid = NGX_CONF_UNSET_PTR;
    conf->upstream.cache_lock = NGX_CONF_UNSET;
    conf->upstream.cache_lock_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.cache_lock_age = NGX_CONF_UNSET_MSEC;
    conf->upstream.cache_revalidate = NGX_CONF_UNSET;
    conf->upstream.cache_convert_head = NGX_CONF_UNSET;
    conf->upstream.cache_background_update = NGX_CONF_UNSET;
#endif

    conf->upstream.hide_headers = NGX_CONF_UNSET_PTR;
    conf->upstream.pass_headers = NGX_CONF_UNSET_PTR;

    conf->upstream.intercept_errors = NGX_CONF_UNSET;

#if (NGX_HTTP_SSL)
    conf->upstream.ssl_session_reuse = NGX_CONF_UNSET;
    conf->upstream.ssl_server_name = NGX_CONF_UNSET;
    conf->upstream.ssl_verify = NGX_CONF_UNSET;
#endif

    conf->upstream.cyclic_temp_file = 0;
    conf->upstream.change_buffering = 1;
    ngx_str_set(&conf->upstream.module, "send_log");

    return conf;
}

static char *
ngx_http_send_log_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_send_log_loc_conf_t *prev = parent;
    ngx_http_send_log_loc_conf_t *conf = child;

    ngx_conf_merge_msec_value(conf->upstream.connect_timeout,
            prev->upstream.connect_timeout, 1000);

    ngx_conf_merge_msec_value(conf->upstream.send_timeout,
            prev->upstream.send_timeout, 1000);

    ngx_conf_merge_msec_value(conf->upstream.read_timeout,
            prev->upstream.read_timeout, 1000);

    ngx_conf_merge_msec_value(conf->upstream.next_upstream_timeout,
            prev->upstream.next_upstream_timeout, 0);

    ngx_conf_merge_size_value(conf->upstream.buffer_size,
            prev->upstream.buffer_size,
            (size_t) ngx_pagesize);

    if (conf->upstream_srv_confs == NULL) {
        conf->upstream_srv_confs = prev->upstream_srv_confs;
    }

    return NGX_CONF_OK;
}

static void
ngx_http_send_log_connected_handler(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_int_t                    rc;
    ngx_connection_t            *c;

    c = u->peer.connection;

    if (c->write->timedout || c->read->timedout) {
        ngx_close_connection(u->peer.connection);
        u->peer.connection = NULL;
        ngx_http_close_fake_request(r);
        return;
    }

    if (c->write->timer_set) {
        ngx_del_timer(c->write);
    }

    rc = ngx_http_upstream_test_connect(c);
    if (rc != NGX_OK) {
        ngx_close_connection(u->peer.connection);
        u->peer.connection = NULL;
        ngx_http_close_fake_request(r);
        return;
    }

    u->write_event_handler = ngx_http_upstream_send_request_handler; // 写自己的吧  别用引擎的
    u->read_event_handler = ngx_http_upstream_process_header;
    u->write_event_handler(r, u);
}

static ngx_int_t
copy_bufs(ngx_pool_t *pool, ngx_chain_t *in, ngx_chain_t **out)
{
    size_t              len = 0;
    ngx_buf_t          *b = NULL;
    ngx_chain_t       **ll = NULL;

    ll = out;

    while (in) {
        len = (size_t) ngx_buf_size(in->buf);
        if (len == 0) {
            return 1;
        }

        if (in->buf && in->buf->file) {
            return 1;
        }

        ngx_chain_t *tl = ngx_alloc_chain_link(pool);
        if (tl == NULL) {
            return 1;
        }

        b = ngx_create_temp_buf(pool, len);
        if (b == NULL) {
            return 1;
        }

        tl->buf = b;
        tl->next = NULL;

        b->tag = (ngx_buf_tag_t)&ngx_http_send_log_module;
        //b->temporary = 1;
        //b->recycled = 1;
        b->last = ngx_copy(b->last, in->buf->pos, len);

        *ll = tl;
        ll = &tl->next;

        in = in->next;
    }

    return 0;
}

static ngx_int_t 
ngx_http_send_log_handler(ngx_http_request_t *r)
{
    ngx_http_send_log_loc_conf_t          *fmlcf;
    ngx_http_upstream_t                      *upstream;
    ngx_uint_t                        		  i;
    ngx_http_upstream_srv_conf_t     		 *uscf;
    ngx_http_request_t                       *request;
    ngx_http_send_log_upstream_srv_conf_t *fmuscf;
    ngx_http_core_loc_conf_t                 *clcf;

    if (r->upstream == NULL || (r->err_status > 200 && r->err_status < 499)) {
        return NGX_DECLINED;
    }

    fmlcf = ngx_http_get_module_loc_conf(r, ngx_http_send_log_module);
    if (fmlcf->upstream_srv_confs == NULL) {
        return NGX_DECLINED;
    }

    fmuscf = fmlcf->upstream_srv_confs->elts;

    for (i = 0; i < fmlcf->upstream_srv_confs->nelts; i++) {
        uscf = fmuscf[i].uscf;

        if (ngx_http_create_fake_request(r, &request) != NGX_OK) {
            return NGX_ERROR;
        }

        if (ngx_http_upstream_create(request) != NGX_OK) {
            return NGX_ERROR;
        }

        upstream = request->upstream;
#if 1
        upstream->conf = &fmlcf->upstream;
        upstream->conf->upstream = uscf;
        upstream->upstream = uscf;
#else

        upstream->conf = ngx_palloc(request->pool, sizeof(ngx_http_upstream_conf_t));
        if (upstream->conf == NULL) {
            return NGX_ERROR;
        }
        ngx_memcpy(upstream->conf, &fmlcf->upstream, sizeof(ngx_http_upstream_conf_t));

        upstream->conf->upstream = ngx_palloc(request->pool, sizeof(ngx_http_upstream_srv_conf_t));
        if (upstream->conf->upstream == NULL) {
            return NGX_ERROR;
        }
        ngx_memcpy(upstream->conf->upstream, uscf, sizeof(ngx_http_upstream_srv_conf_t));

        upstream->upstream = upstream->conf->upstream;
#endif

        if (1) {
            copy_bufs(request->pool, r->out_mirror, &upstream->request_bufs);
        }

        upstream->process_header = ngx_http_stop_request;
        //upstream->keepalive = 1;

        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
        upstream->output.alignment = clcf->directio_alignment;
        upstream->output.pool = request->pool;
        upstream->output.bufs.num = 1;
        upstream->output.bufs.size = clcf->client_body_buffer_size;
        upstream->output.output_filter = ngx_chain_writer;
        upstream->output.filter_ctx = &upstream->writer;
        upstream->writer.pool = request->pool;

        upstream->output.buf = NULL;
        upstream->output.in = NULL;
        upstream->output.busy = NULL;

        //if (uscf->peer.init(request, uscf) != NGX_OK) 
        if (upstream->upstream->peer.init(request, upstream->upstream) != NGX_OK) {
            ngx_http_upstream_finalize_request(request, upstream, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return NGX_ERROR;
        }
        ngx_http_upstream_connect(request, upstream);

        upstream->write_event_handler = ngx_http_send_log_connected_handler;
        upstream->read_event_handler = ngx_http_send_log_connected_handler;
    }

    return NGX_DECLINED;
}

static ngx_uint_t 
ngx_http_create_fake_request(ngx_http_request_t *r,
        ngx_http_request_t **fake_request)
{
    ngx_http_request_t            *request;
    ngx_pool_t                    *request_pool, *connection_pool;
    ngx_log_t                     *log;
    ngx_connection_t              *c; // , *main_c;
    ngx_http_log_ctx_t            *log_ctx;

    ngx_connection_t              *saved_c = NULL;

    /* (we temporarily use a valid fd (0) to make ngx_get_connection happy) */
    if (ngx_cycle->files) {
        saved_c = ngx_cycle->files[0];
    }

    c = ngx_get_connection(0, ngx_cycle->log);
    if (ngx_cycle->files) {
        ngx_cycle->files[0] = saved_c;
    }

    if (c == NULL) {
        return NGX_ERROR;
    }

    connection_pool = ngx_create_pool(512, c->log);
    request_pool = ngx_create_pool(512, ngx_cycle->log);

    c->fd = (ngx_socket_t) -1;
    c->number = ngx_atomic_fetch_add(ngx_connection_counter, 1);

    c->pool = connection_pool;

    log = ngx_pcalloc(c->pool, sizeof(ngx_log_t));
    if (log == NULL) {
        return NGX_ERROR;
    }
    if (r->connection == NULL){
        return NGX_ERROR;
    }
    //main_c = r->connection;
    //*log = *main_c->log;      // TODO log的作用是什么? // 为何复用main_c?

    c->log = log;
    c->log->connection = c->number;
    c->log->action = "connect upstream";
    c->log->data = NULL;
    c->log->handler = ngx_http_log_error;

    c->read->log = log;
    c->write->log = log;
    c->pool->log = log;
    c->error = 1;

#if 0
    // 假戏真作
    c->socklen = main_c->socklen;
    c->local_socklen = main_c->local_socklen;
    c->sockaddr = ngx_palloc(c->pool, c->socklen);
    if (c->sockaddr == NULL) {
        return NGX_ERROR;
    }
    c->local_sockaddr = ngx_palloc(c->pool, c->local_socklen);
    if (c->local_sockaddr == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(c->sockaddr, main_c->sockaddr, c->socklen);
    ngx_memcpy(c->local_sockaddr, main_c->local_sockaddr, c->local_socklen);
#endif

    c->log_error = NGX_ERROR_INFO;

    request = ngx_pcalloc(request_pool, sizeof(ngx_http_request_t)); // 复用
    if (request == NULL) {
        return NGX_ERROR;
    }
    c->requests++;
    request->pool = request_pool;
    request->connection = c;
    c->data = request;

#if 1
    request->loc_conf = r->loc_conf;
    request->srv_conf = r->srv_conf;
    request->main_conf = r->main_conf;
#endif
    request->main = request;
    request->count = 1;
    request->request_body = NULL;

    request->discard_body = 0;
    request->lingering_close = 0;

    //request->log_handler = r->log_handler;
#if 1
    log_ctx = ngx_pcalloc(c->pool, sizeof(ngx_http_log_ctx_t));
    if (log_ctx == NULL) {
        return NGX_ERROR;
    }
    log_ctx->connection = c;
    log_ctx->request = request;
    log_ctx->current_request = request;
    c->log->data = log_ctx;
#endif
    /*
       clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
       ngx_set_connection_log(c, clcf->error_log);
       */

    request->logged = 1;                                // not log
    request->upstream_states = ngx_array_create(request->pool, 1, sizeof(ngx_http_upstream_state_t));
    if (request->upstream_states == NULL) {
        return NGX_ERROR;
    }

    *fake_request = request;
    return NGX_OK;
}

static 
ngx_int_t ngx_http_stop_request(ngx_http_request_t *r)
{
    ngx_http_upstream_t *u;

    u = r->upstream;

    if (u->peer.sockaddr) {
        u->peer.free(&u->peer, u->peer.data, 0); 
        u->peer.sockaddr = NULL;
    }
    // if keepalive, the connection has been set to null by u->peer.free
    if (u->peer.connection) {
        if (u->peer.connection->pool) {
            ngx_destroy_pool(u->peer.connection->pool);
        }

        ngx_close_connection(u->peer.connection);
        u->peer.connection = NULL;
    }

    ngx_http_close_fake_request(r);
    return NGX_ABORT;
}

static 
void ngx_http_close_fake_request(ngx_http_request_t *r)
{
    ngx_connection_t  *c;

    r = r->main;
    c = r->connection;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
            "http send log fake request count:%d", r->count);

    if (r->count == 0) {
        ngx_log_error(NGX_LOG_ALERT, c->log, 0, "http send log fake request "
                "count is zero");
    }

    r->count--;

    if (r->count) {
        return;
    }

    if (1) {
        ngx_http_free_fake_request(r);
        ngx_http_close_fake_connection(c);
    }
}

static void 
ngx_http_free_fake_request(ngx_http_request_t *r)
{
    ngx_log_t                 *log;
    ngx_http_cleanup_t        *cln;
    ngx_pool_t                *pool;

    log = r->connection->log;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0, "http send log close fake "
            "request");

    if (r->pool == NULL) {
        ngx_log_error(NGX_LOG_ALERT, log, 0, "http send log fake request "
                "already closed");
        return;
    }

    cln = r->cleanup;
    r->cleanup = NULL;

    while (cln) {
        if (cln->handler) {
            cln->handler(cln->data);
        }

        cln = cln->next;
    }

    r->request_line.len = 0;

    r->connection->destroyed = 1;

    pool = r->pool;
    r->pool = NULL;
    if (1) {
        ngx_destroy_pool(pool);
    }
}

static void 
ngx_http_close_fake_connection(ngx_connection_t *c)
{
    ngx_pool_t          *pool;
    ngx_connection_t    *saved_c = NULL;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
            "http send log close fake http connection %p", c);

    c->destroyed = 1;

    pool = c->pool;

    if (c->read->timer_set) {
        ngx_del_timer(c->read);
    }

    if (c->write->timer_set) {
        ngx_del_timer(c->write);
    }

    c->read->closed = 1;
    c->write->closed = 1;

    /* we temporarily use a valid fd (0) to make ngx_free_connection happy */

    c->fd = 0;

    if (ngx_cycle->files) {
        saved_c = ngx_cycle->files[0];
    }

    ngx_free_connection(c);

    c->fd = (ngx_socket_t) -1;

    if (ngx_cycle->files) {
        ngx_cycle->files[0] = saved_c;
    }

    if (pool) {
        ngx_destroy_pool(pool);
    }
}

