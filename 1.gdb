set args -c /usr/local/nginx/conf/nginx.conf
#set args -c /usr/local/tengine/conf/nginx.conf
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
#b ngx_http_add_listen 
#b ngx_http_add_server
## debug port at optimize time
#b ngx_http_add_listening
#b ngx_http_add_addrs

#conn
#b ngx_http_upstream_keepalive_create_conf
#b ngx_http_upstream_keepalive
#b ngx_http_upstream_init_main_conf

#b ngx_http_upstream.c:4349
#b ngx_http_upstream.c:4351
#b ngx_http_upstream_get_keepalive_peer
#b ngx_http_upstream_free_persistent_peer
#b ngx_http_ssl_client_hello_handler
#b ngx_http_parse_request_line

#b ngx_http_proxy_set_vars
#b ngx_http_proxy_pass

#b ngx_http_process_host
#b ngx_http_update_location_config
#b ngx_http_proxy_handler
b ngx_http_upstream_save_round_robin_peer_session
b ngx_http_upstream_set_round_robin_peer_session
b ngx_ssl_remove_cached_session
#b ngx_ssl_session_ticket_keys
#b ngx_ssl_free_buffer
#b ngx_ssl_shutdown
b ngx_ssl_new_client_session
b ngx_http_upstream_session_sticky_save_peer_session
b ngx_http_upstream_session_sticky_set_peer_session
