set args -c /usr/local/nginx/conf/nginx.conf
set print pretty

#b ngx_http_security_stats_merge_loc_conf
#b ngx_http_stats_init
#b ngx_http_security_stats_handler
#b ngx_http_security_stats.c:104
#
#b ngx_http_upstream_bind_set_slot
#b ngx_http_upstream_set_local
#b ngx_http_variable_remote_addr


# debug port
#b src/http/ngx_http.c:1402
#b ngx_http_init_listening
#b ngx_http_add_addresses
## debug port at parsing time
# parse word listen and after parse word listen
b ngx_http_add_listen 
b ngx_http_add_server
## debug port at optimize time
b ngx_http_add_listening
#b ngx_http_add_addrs
