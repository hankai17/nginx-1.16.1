
/*
 * Copyright (C) Topsec.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <assert.h>

#ifndef NGX_HTTP_UPSTREAM_long_conn
//#error need NGX_HTTP_UPSTREAM_long_conn macro enabled
#endif


typedef struct {
    ngx_http_upstream_init_pt          original_init_upstream;
    ngx_http_upstream_init_peer_pt     original_init_peer;
} ngx_http_upstream_long_conn_srv_conf_t;


typedef struct {
    ngx_http_upstream_long_conn_srv_conf_t  *conf;

    ngx_http_upstream_t               *upstream;
    ngx_http_request_t                  *req;

    void                              *data;

    ngx_event_get_peer_pt              original_get_peer;
    ngx_event_free_peer_pt             original_free_peer;

#if (NGX_HTTP_SSL)
    ngx_event_set_peer_session_pt      original_set_session;
    ngx_event_save_peer_session_pt     original_save_session;
#endif

} ngx_http_upstream_long_conn_peer_data_t;


static ngx_int_t ngx_http_upstream_init_long_conn_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us);
static ngx_int_t ngx_http_upstream_get_long_conn_peer(ngx_peer_connection_t *pc,
    void *data);
static void ngx_http_upstream_free_long_conn_peer(ngx_peer_connection_t *pc,
    void *data, ngx_uint_t state);

static void ngx_http_upstream_long_conn_dummy_handler(ngx_event_t *ev);
void ngx_http_upstream_long_conn_close_handler(ngx_event_t *ev);
static void ngx_http_upstream_long_conn_close(ngx_connection_t *c);


#if (NGX_HTTP_SSL)
static ngx_int_t ngx_http_upstream_long_conn_set_session(
    ngx_peer_connection_t *pc, void *data);
static void ngx_http_upstream_long_conn_save_session(ngx_peer_connection_t *pc,
    void *data);
#endif

static void *ngx_http_upstream_long_conn_create_conf(ngx_conf_t *cf);
static char *ngx_http_upstream_long_conn(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);


static ngx_command_t  ngx_http_upstream_long_conn_commands[] = {

    { ngx_string("long_conn"),
      NGX_HTTP_UPS_CONF|NGX_CONF_NOARGS,
      ngx_http_upstream_long_conn,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_upstream_long_conn_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    ngx_http_upstream_long_conn_create_conf, /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};


ngx_module_t  ngx_http_upstream_long_conn_module = {
    NGX_MODULE_V1,
    &ngx_http_upstream_long_conn_module_ctx, /* module context */
    ngx_http_upstream_long_conn_commands,    /* module directives */
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


static ngx_int_t
ngx_http_upstream_init_long_conn(ngx_conf_t *cf,
    ngx_http_upstream_srv_conf_t *us)
{
    ngx_http_upstream_long_conn_srv_conf_t  *pcf;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cf->log, 0,
                   "init long_conn");

    pcf = ngx_http_conf_upstream_srv_conf(us,
                                          ngx_http_upstream_long_conn_module);

    if (pcf->original_init_upstream(cf, us) != NGX_OK) {
        return NGX_ERROR;
    }

    pcf->original_init_peer = us->peer.init;

    us->peer.init = ngx_http_upstream_init_long_conn_peer;

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_init_long_conn_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us)
{
    ngx_http_upstream_long_conn_peer_data_t  *kp;
    ngx_http_upstream_long_conn_srv_conf_t   *pcf;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "init long_conn peer");

    pcf = ngx_http_conf_upstream_srv_conf(us,
                                          ngx_http_upstream_long_conn_module);

    kp = ngx_palloc(r->pool, sizeof(ngx_http_upstream_long_conn_peer_data_t));
    if (kp == NULL) {
        return NGX_ERROR;
    }

    if (pcf->original_init_peer(r, us) != NGX_OK) {
        return NGX_ERROR;
    }

    kp->conf = pcf;
    kp->upstream = r->upstream;
    kp->data = r->upstream->peer.data;
    kp->req = r;
    kp->original_get_peer = r->upstream->peer.get;
    kp->original_free_peer = r->upstream->peer.free;

    r->upstream->peer.data = kp;
    r->upstream->peer.get = ngx_http_upstream_get_long_conn_peer;
    r->upstream->peer.free = ngx_http_upstream_free_long_conn_peer;

#if (NGX_HTTP_SSL)
    kp->original_set_session = r->upstream->peer.set_session;
    kp->original_save_session = r->upstream->peer.save_session;
    r->upstream->peer.set_session = ngx_http_upstream_long_conn_set_session;
    r->upstream->peer.save_session = ngx_http_upstream_long_conn_save_session;
#endif

    return NGX_OK;
}

/* when downstream connection closed */
static void
ngx_http_upstream_long_conn_pool_dclose(void *data)
{
    ngx_connection_t *dc, *uc; /* downstream connection */


    dc = data;
    if (dc->peer_connection) {
        dc->peer_connection->peer_connection = NULL;
        uc = dc->peer_connection;
        dc->peer_connection = NULL;
        /* close upstream connection */
        ngx_http_upstream_long_conn_close(uc);
    }
    // else normal case, uconnection has been closed already
}

/* when upstream connection closed */
static void
ngx_http_upstream_long_conn_pool_uclose(void *data)
{
    ngx_connection_t *uc; /* upstream connection */

    uc = data;
    if (uc->peer_connection) {
        uc->peer_connection->peer_connection = NULL;
        uc->peer_connection = NULL;
    }
}

static ngx_int_t
ngx_http_upstream_get_long_conn_peer(ngx_peer_connection_t *pc, void *data)
{
    ngx_http_upstream_long_conn_peer_data_t  *kp = data;

    ngx_int_t          rc;
    ngx_connection_t  *c, *dc;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "get long_conn peer");

    rc = kp->original_get_peer(pc, kp->data);
    if (rc != NGX_OK) {
        return rc;
    }

    /* set so_keepalive */
    pc->so_keepalive = 1;

    dc = kp->req->connection;
    c = dc->peer_connection;

    if (c && kp->req == kp->req->main) {
        if (c->fd != -1) {
            c->idle = 0;
            c->sent = 0;
            c->log = pc->log;
            c->read->log = pc->log;
            c->write->log = pc->log;
            c->pool->log = pc->log;

            if (c->read->timer_set) {
                ngx_del_timer(c->read);
            }

            pc->connection = c;
            pc->cached = 1;     /* may be closed already, when success request cones, for ngx_http_upstream_next */
            assert(c->sockaddr);
            pc->sockaddr = c->sockaddr; // in ngx_sche_module, sockaddr is malloc from r->connection->pool, so reuse it

            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                    "get long_conn connection %p", c);

            return NGX_DONE;
        } else {

            ngx_log_error(NGX_LOG_ALERT, pc->log, 0,
                    "get long_conn connection fd is -1");
            dc->peer_connection = NULL;
        }
    }

    assert(pc->connection == NULL);

    return NGX_OK;
}


static void
ngx_http_upstream_free_long_conn_peer(ngx_peer_connection_t *pc, void *data,
    ngx_uint_t state)
{
    ngx_http_upstream_long_conn_peer_data_t  *kp = data;

    ngx_connection_t     *c;
    ngx_connection_t     *dc; // downstream connection
    ngx_http_upstream_t  *u;
    ngx_pool_cleanup_t  *ucln, *dcln;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "free long_conn peer");

    /* cache valid connections */

    u = kp->upstream;
    c = pc->connection;
    dc = kp->req->connection;

    if (state & NGX_PEER_FAILED
        || c == NULL
        || c->read->eof
        || c->read->error
        || c->read->timedout
        || c->write->error
        || c->write->timedout
        || kp->req != kp->req->main

        || dc->read->eof
        || dc->read->error
        || dc->read->timedout
        || dc->write->error
        || dc->write->timedout)
    {
        dc->peer_connection = NULL;
        goto invalid;
    }


    // TODO: change to u->keepalive
    if (!u->keepalive) {
        dc->peer_connection = NULL;
        /*
         * in this confition, original free will not set pc->connection as NULL
         * so 'ngx_http_upstream_finalize_request fill close this connection
         */
       	ngx_log_debug1(NGX_LOG_DEBUG_HTTP,  pc->log, 0,
                   "free long_conn peer: u->keepalive is 0 , don't saving connection %p", c);
        goto invalid;
    }

    if (ngx_terminate || ngx_exiting) {
        dc->peer_connection = NULL;
        goto invalid;
    }

    if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
        dc->peer_connection = NULL;
        goto invalid;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "free long_conn peer: saving connection %p", c);

    /* prevent upstream finalize close connection */
    pc->connection = NULL;

    if (c->read->timer_set) {
        ngx_del_timer(c->read);
    }
    if (c->write->timer_set) {
        ngx_del_timer(c->write);
    }

    c->write->handler = ngx_http_upstream_long_conn_dummy_handler;
    c->read->handler = ngx_http_upstream_long_conn_close_handler;
	
    c->idle = 1;
    /*
     * downstream connection may be closed. so set log as ngx_cycle log.
     */
    c->log = ngx_cycle->log;
    c->read->log = ngx_cycle->log;
    c->write->log = ngx_cycle->log;
    c->pool->log = ngx_cycle->log;

    /* in ngx_sche_module, sockaddr is malloc from r->connection->pool,
     * and in ngx original module, sockaddr malloc from cycle->pool, so cache it */
    c->sockaddr = pc->sockaddr;

    /* splice */
    if (dc->peer_connection == NULL) {

        ucln = ngx_pool_cleanup_add(c->pool, sizeof(ngx_connection_t));
        dcln = ngx_pool_cleanup_add(dc->pool, sizeof(ngx_connection_t));
        if (ucln && dcln) {

            ucln->handler = ngx_http_upstream_long_conn_pool_uclose;
            ucln->data = c;

            dcln->handler = ngx_http_upstream_long_conn_pool_dclose;
            dcln->data = dc;

            dc->peer_connection = c;
            c->peer_connection = dc;

        }
    }

    if (c->read->ready) {
        ngx_http_upstream_long_conn_close_handler(c->read);
    }

invalid:
	if (!pc->cached) {
		/* original_get_peer is called only on the first request, and original_free_peer is called only when original_get_peer is called */
	    kp->original_free_peer(pc, kp->data, state);
	}

}


static void
ngx_http_upstream_long_conn_dummy_handler(ngx_event_t *ev)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ev->log, 0,
                   "long_conn dummy handler");
}


void
ngx_http_upstream_long_conn_close_handler(ngx_event_t *ev)
{

    int                n;
    char               buf[1];
    ngx_connection_t  *c;
    ngx_connection_t  *dc;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ev->log, 0,
                   "long_conn close handler");

    c = ev->data;

    if (c->close) {
        goto close;
    }

    n = recv(c->fd, buf, 1, MSG_PEEK);

    if (n == -1 && ngx_socket_errno == NGX_EAGAIN) {
        ev->ready = 0;

        if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
            goto close;
        }

        return;
    }

close:
    if (c->peer_connection) {
        // clear downstream connection's long_conn connection
        dc = c->peer_connection;
        c->peer_connection->peer_connection = NULL;
        c->peer_connection = NULL;
        ngx_http_upstream_long_conn_close(dc);
     }

    ngx_http_upstream_long_conn_close(c);
}


void
ngx_http_upstream_long_conn_close(ngx_connection_t *c)
{

#if (NGX_HTTP_SSL)

    if (c->ssl) {
        c->ssl->no_wait_shutdown = 1;
        c->ssl->no_send_shutdown = 1;

        if (ngx_ssl_shutdown(c) == NGX_AGAIN) {
            c->ssl->handler = ngx_http_upstream_long_conn_close;
            return;
        }
    }

#endif


    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->pool->log, 0,
                   "close long_conn connection %p", c);
    ngx_destroy_pool(c->pool);
    ngx_close_connection(c);
}


#if (NGX_HTTP_SSL)

static ngx_int_t
ngx_http_upstream_long_conn_set_session(ngx_peer_connection_t *pc, void *data)
{
    ngx_http_upstream_long_conn_peer_data_t  *kp = data;

    return kp->original_set_session(pc, kp->data);
}


static void
ngx_http_upstream_long_conn_save_session(ngx_peer_connection_t *pc, void *data)
{
    ngx_http_upstream_long_conn_peer_data_t  *kp = data;

    kp->original_save_session(pc, kp->data);
    return;
}

#endif


static void *
ngx_http_upstream_long_conn_create_conf(ngx_conf_t *cf)
{
    ngx_http_upstream_long_conn_srv_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool,
                       sizeof(ngx_http_upstream_long_conn_srv_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->original_init_upstream = NULL;
     *     conf->original_init_peer = NULL;
     */

    //conf->max_cached = 1;

    return conf;
}


static char *
ngx_http_upstream_long_conn(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_upstream_srv_conf_t            *uscf;
    ngx_http_upstream_long_conn_srv_conf_t  *pcf = conf;


    uscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_upstream_module);

    if (pcf->original_init_upstream) {
        return "is duplicate";
    }

    pcf->original_init_upstream = uscf->peer.init_upstream
                                  ? uscf->peer.init_upstream
                                  : ngx_http_upstream_init_round_robin;

    uscf->peer.init_upstream = ngx_http_upstream_init_long_conn;

    return NGX_CONF_OK;
}
