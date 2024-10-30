set args -c /usr/local/nginx/conf/nginx.conf
handle SIGPIPE noprint nopass nostop
set print pretty
set debuginfod enabled off

# reuseport的实现是  nginx在fork之前的主进程业务层中 对listen的端口了 listen了n(配置的worker的个数)遍
b listen
