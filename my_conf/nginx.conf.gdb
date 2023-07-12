
#user  nobody;
worker_processes  1;
daemon off;
master_process off;

worker_cpu_affinity 0001;
#worker_rlimit_nofile 65535;

#error_log  logs/error.log;
#error_log  logs/error.log  notice;
#error_log  logs/error.log  info;

#pid        logs/nginx.pid;


events {
    use epoll;
    worker_connections  102400;
}


http {
    include       mime.types;
    default_type  application/octet-stream;

    log_format  main  '$remote_addr - $remote_user [$time_local] "$request" '
                      '$status $body_bytes_sent "$http_referer" '
                      '"$http_user_agent" "$http_x_forwarded_for"'
                      '"$upstream_addr" "$upstream_connect_time" "$upstream_header_time"'
			'"$upstream_response_time" "$request_time" ';

    #access_log  logs/access.log  main;

    sendfile        off;
    tcp_nopush     on;

    #keepalive_timeout  0;
    keepalive_timeout  65;

    #gzip  on;
    access_log off;		

    charset_map koi8-r window-1251 {
        53 30;
        51 31;
        4c 32;
    }

    server {
        listen       90;
        server_name  localhost;
        #charset window-1251;
        #override_charset on;
        proxy_buffering off;

        location / {
	        #proxy_pass http://10.53.80.151:80/;
	        #proxy_pass http://0.0.0.0:90/;
	        #proxy_pass http://192.168.1.8:9080/;
            #more_set_input_headers "requestkkkkk: vvvvv";
            #more_set_headers "reponsekkkkk: vvvvv";
            #sub_filter 'pbcdefo' 'hijklmn';
            #sub_filter_types text/plain;
            root   html;
            index  index.html index.htm;
        }

        #error_page  404              /404.html;

        # redirect server error pages to the static page /50x.html
        #
        error_page   500 502 503 504  /50x.html;
        location = /50x.html {
            root   html;
        }

        # proxy the PHP scripts to Apache listening on 127.0.0.1:80
        #
        #location ~ \.php$ {
        #    proxy_pass   http://127.0.0.1;
        #}

        # pass the PHP scripts to FastCGI server listening on 127.0.0.1:9000
        #
        #location ~ \.php$ {
        #    root           html;
        #    fastcgi_pass   127.0.0.1:9000;
        #    fastcgi_index  index.php;
        #    fastcgi_param  SCRIPT_FILENAME  /scripts$fastcgi_script_name;
        #    include        fastcgi_params;
        #}

        # deny access to .htaccess files, if Apache's document root
        # concurs with nginx's one
        #
        #location ~ /\.ht {
        #    deny  all;
        #}
    }

    server {
        listen       81;
        server_name  localhost;
        proxy_buffering off;

        location / {
	    proxy_pass http://127.0.0.1:9081/;
            root   html;
            index  index.html index.htm;
        }
    }

    server {
        listen       443 ssl http2;
        server_name  localhost;

        ssl_certificate     server.crt;
        ssl_certificate_key  server.key;
    }


    # another virtual host using mix of IP-, name-, and port-based configuration
    #
    #server {
    #    listen       8000;
    #    listen       somename:8080;
    #    server_name  somename  alias  another.alias;

    #    location / {
    #        root   html;
    #        index  index.html index.htm;
    #    }
    #}


    # HTTPS server
    #
    #server {
    #    listen       443 ssl;
    #    server_name  localhost;

    #    ssl_certificate      cert.pem;
    #    ssl_certificate_key  cert.key;

    #    ssl_session_cache    shared:SSL:1m;
    #    ssl_session_timeout  5m;

    #    ssl_ciphers  HIGH:!aNULL:!MD5;
    #    ssl_prefer_server_ciphers  on;

    #    location / {
    #        root   html;
    #        index  index.html index.htm;
    #    }
    #}

}
