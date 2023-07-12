set args -c /usr/local/nginx/conf/nginx.conf
set print pretty

#b ngx_http_read_client_request_body
#b ngx_http_modsecurity_pre_access.c:119
#b ngx_http_modsecurity_pre_access_handler
#b ngx_http_modsecurity_request_read
#b ngx_http_modsecurity_process_intervention
#b modsecurity::Transaction::processRequestBody
#b transaction.cc:979
#b Rx::evaluate
#b rx.cc:39
#b Deny::evaluate
#b deny.cc:31


#b ngx_http_request_body.c:338
#b ngx_http_proxy_handler
#b ngx_http_urlencode_process_request_body
#b ngx_http_modsecurity_pre_access_worker

#b ngx_http_urlencode_rewrite_handler
#b ngx_http_urlencode_preaccess_handle
#b ngx_http_do_read_client_request_body

#线程池分析
#b pthread_create
#b ngx_http_modsecurity_pre_access_worker
#b ngx_http_modsecurity_pre_access_handler

#生命周期分析
#b ngx_get_connection
#b ngx_http_init_connection
#b ngx_http_alloc_request
#b ngx_http_named_location

#event初始化 以及 并发数分析
#b ngx_worker_process_cycle
#b ngx_event_process_init

#pipe研究
#b ngx_event_pipe
#b ngx_event_pipe_write_chain_to_temp_file
#b ngx_event_pipe_read_upstream
#b ngx_event_pipe.c:216
#b ngx_event_pipe.c:312
#b ngx_event_pipe.c:371
#b ngx_event_pipe.c:257
#b ngx_http_proxy_copy_filter
#b ngx_event_pipe_write_to_downstream
#b ngx_event_pipe.c:697
#b ngx_event_pipe.c:275

#shadown
b ngx_http_sub_filter_module.c:357 
#b ngx_http_sub_filter_module.c:478
#b ngx_http_sub_filter_module.c:560
#b ngx_http_upstream.c:3588
#b ngx_http_sub_filter_module.c:522
#b ngx_http_sub_filter_module.c:583

#charset
#b ngx_output_chain
#b ngx_output_chain_add_copy
#b ngx_http_charset_header_filter
#b ngx_http_charset_body_filter
#b ngx_output_chain_as_is

#event
#b ngx_event_module_init
#b ngx_events_block
#b ngx_event_core_create_conf
#b ngx_event_core_init_conf
#b ngx_event_use
#b ngx_event_connections
#b ngx_event_process_init
#b ngx_event_init_conf

#listen
#b ngx_event_process_init
#b ngx_add_inherited_sockets
#b ngx_create_listening
#b ngx_event.c:779
#b ngx_event_accept.c:308
#b ngx_http_init_connection

#b ngx_http_init_listening

#os conn
#b ngx_http_upstream_init_request
#b ngx_http_upstream_set_local
#b ngx_http_upstream_create_round_robin_peer
#b ngx_http_upstream_init_round_robin_peer
#b ngx_event_connect_peer
#b ngx_http_upstream_free_round_robin_peer
#b ngx_http_upstream_get_persistent_peer
#b ngx_http_upstream_free_persistent_peer
#
#b ngx_http_core_module.c:812
#b ngx_http_core_module.c:816
#b ngx_http_core_module.c:820
#b ngx_http_core_module.c:1250
#b ngx_http_core_module.c:1253
#b ngx_http_core_module.c:1264
#b ngx_http_core_module.c:1275
#b ngx_http_request.c:2720   
#b ngx_http_request.c:3076   
#b ngx_http_upstream.c:3267  
#b ngx_http_upstream.c:4473  
#b ngx_http_upstream.c:4451

#transparent proxy
#b ngx_event_connect_peer

#ssl
b ngx_http_request.c:744
#b ngx_ssl_create
b ngx_http_ssl_servername
b ngx_http_ssl_alpn_select
b ngx_http_ssl_npn_advertised
b ngx_http_ssl_certificate
#b ngx_http_add_variable
#b ngx_http_variables_add_core_vars
#b ngx_http_variables_init_vars
b ngx_ssl_handshake
b ngx_http_process_request_line
b ngx_http_ssl_client_server_name
b ngx_http_ssl_client_hello_handler
