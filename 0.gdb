set args -c /usr/local/nginx/conf/nginx.conf
#set args -c /root/CLionProjects/nginx-1.16.1/nginx.conf
handle SIGPIPE noprint nopass nostop
set print pretty

#post
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
#b ngx_http_request_body_save_filter

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
#b ngx_http_sub_filter_module.c:357 
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
#b ngx_http_request.c:744
##b ngx_ssl_create
#b ngx_http_ssl_servername
#b ngx_http_ssl_alpn_select
#b ngx_http_ssl_npn_advertised
#b ngx_http_ssl_certificate
##b ngx_http_add_variable
##b ngx_http_variables_add_core_vars
##b ngx_http_variables_init_vars
#b ngx_ssl_handshake
#b ngx_http_process_request_line
#b ngx_http_ssl_client_server_name
#b ngx_http_ssl_client_hello_handler

#mirror
#b ngx_http_mirror_handler
#b ngx_http_mirror_handler_internal
#b ngx_http_request.c:2524
#b ngx_http_run_posted_requests
#b ngx_http_finalize_request
#b ngx_http_read_client_request_body
#b ngx_http_upstream_test_connect
#b ngx_http_core_content_phase
#b ngx_http_handler
#b ngx_http_request.c:2419
#b ngx_http_finalize_request
#b ngx_epoll_module.c:911
#b ngx_http_request.c:2543
#b ngx_http_request.c:2581
#b ngx_http_request.c:2587
#b ngx_http_mirror_module.c:146
#b ngx_http_postpone_filter
#b ngx_http_core_module.c:2339
#b ngx_http_request.c:2574
#b ngx_http_request.c:2522

#subreq
#b ngx_http_subreq_body_handler
#b ngx_http_read_client_request_body
#b ngx_http_mirror_module.c:126
#b ngx_http_subreq_handler
#b ngx_http_subreq_subrequest_done
#b ngx_http_upstream_process_non_buffered_request

#b auth_request
#b ngx_http_auth_request_handler
#b ngx_http_auth_request_done

#post client_body_buffer_size
#b pwrite
#b ngx_http_read_client_request_body
#b ngx_http_do_read_client_request_body
#b ngx_http_write_request_body
#b ngx_http_request_body_save_filter

#向os转发请求
#b ngx_http_request_body.c:1177
#b ngx_http_upstream_send_request_body
#b ngx_http_upstream.c:2135
#b ngx_http_proxy_create_request
#b ngx_http_log_handler

#b ngx_http_flow_mirror_pass
#b ngx_http_request.c:2768
#b copy_request_bufs
#b ngx_http_upstream.c:699
#b ngx_http_flow_mirror_log_handler
#b ngx_http_flow_mirror_pass_eval
#b ngx_http_finalize_connection
#b ngx_palloc.c:85 if l->alloc == 0x7f6a40
#b ngx_http_close_connection

#dns resolve
#b ngx_http_proxy_handler
#b ngx_http_upstream.c:1361
#b ngx_http_lua_socket_connected_handler
#b ngx_http_upstream_test_connect

#b ngx_http_cache_purge_handler
#b ngx_ssl_handshake

#reuse_port
#b bind
#b listen

#upstream buffer copy
#b ngx_http_proxy_non_buffered_copy_filter
#b ngx_http_proxy_non_buffered_chunked_filter
#b ngx_http_upstream_non_buffered_filter
#b ngx_http_upstream_process_non_buffered_request
#b ngx_http_upstream_process_non_buffered_upstream
b ngx_output_chain
#b ngx_http_sub_body_filter
b ngx_http_write_filter
