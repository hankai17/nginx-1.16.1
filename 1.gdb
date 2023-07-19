set args -c /usr/local/nginx/conf/nginx.conf
set print pretty

#b ngx_http_security_stats_merge_loc_conf
b ngx_http_stats_init
b ngx_http_security_stats_handler
b ngx_http_security_stats.c:104

b ngx_http_upstream_bind_set_slot
b ngx_http_upstream_set_local
b ngx_http_variable_remote_addr
