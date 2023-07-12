#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAXLINE 8192
#define SERV_PORT 90

struct s_info {                     //定义一个结构体, 将地址结构跟cfd捆绑
    struct sockaddr_in cliaddr;
    int connfd;
};

void perr_exit(const char *s)
{
        perror(s);
        exit(-1);
}

int Accept(int fd, struct sockaddr *sa, socklen_t *salenptr)
{
        int n;

again:
        if ((n = accept(fd, sa, salenptr)) < 0) {
                if ((errno == ECONNABORTED) || (errno == EINTR))
                        goto again;
                else
                        perr_exit("accept error");
        }
        return n;
}

int Bind(int fd, const struct sockaddr *sa, socklen_t salen)
{
    int n;

        if ((n = bind(fd, sa, salen)) < 0)
                perr_exit("bind error");

    return n;
}

int Connect(int fd, const struct sockaddr *sa, socklen_t salen)
{
    int n;

        if ((n = connect(fd, sa, salen)) < 0)
                perr_exit("connect error");

    return n;
}

int Listen(int fd, int backlog)
{
    int n;

        if ((n = listen(fd, backlog)) < 0)
                perr_exit("listen error");

    return n;
}

int Socket(int family, int type, int protocol)
{
        int n;

        if ((n = socket(family, type, protocol)) < 0)
                perr_exit("socket error");

        return n;
}

ssize_t Read(int fd, void *ptr, size_t nbytes)
{
        ssize_t n;

again:
        if ( (n = read(fd, ptr, nbytes)) == -1) {
                if (errno == EINTR)
                        goto again;
                else
                        return -1;
        }
        return n;
}

ssize_t Write(int fd, const void *ptr, size_t nbytes)
{
        ssize_t n;

again:
        if ( (n = write(fd, ptr, nbytes)) == -1) {
                if (errno == EINTR)
                        goto again;
                else
                        return -1;
        }
        return n;
}

int Close(int fd)
{
    int n;
        if ((n = close(fd)) == -1)
                perr_exit("close error");

    return n;
}

ssize_t Readn(int fd, void *vptr, size_t n)
{
        size_t  nleft;              //usigned int 剩余未读取的字节数
        ssize_t nread;              //int 实际读到的字节数
        char   *ptr;

        ptr = vptr;
        nleft = n;

        while (nleft > 0) {
                if ((nread = read(fd, ptr, nleft)) < 0) {
                        if (errno == EINTR)
                                nread = 0;
                        else
                                return -1;
                } else if (nread == 0)
                        break;

                nleft -= nread;
                ptr += nread;
        }
        return n - nleft;
}

ssize_t Writen(int fd, const void *vptr, size_t n)
{
        size_t nleft;
        ssize_t nwritten;
        const char *ptr;

        ptr = vptr;
        nleft = n;
        while (nleft > 0) {
                if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
                        if (nwritten < 0 && errno == EINTR)
                                nwritten = 0;
                        else
                                return -1;
                }

                nleft -= nwritten;
                ptr += nwritten;
        }
        return n;
}

static ssize_t my_read(int fd, char *ptr)
{
        static int read_cnt;
        static char *read_ptr;
        static char read_buf[100];

        if (read_cnt <= 0) {
again:
                if ( (read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0) {
                        if (errno == EINTR)
                                goto again;
                        return -1;
                } else if (read_cnt == 0)
                        return 0;
                read_ptr = read_buf;
        }
        read_cnt--;
        *ptr = *read_ptr++;

        return 1;
}

ssize_t Readline(int fd, void *vptr, size_t maxlen)
{
        ssize_t n, rc;
        char    c, *ptr;

        ptr = vptr;
        for (n = 1; n < maxlen; n++) {
                if ( (rc = my_read(fd, &c)) == 1) {
                        *ptr++ = c;
                        if (c  == '\n')
                                break;
                } else if (rc == 0) {
                        *ptr = 0;
                        return n - 1;
                } else
                        return -1;
        }
        *ptr  = 0;

        return n;
}
//////////////////////////////////////////////////////////////////////

int file_size(const char* filePath) {
        if(filePath == NULL) return 0;
        struct stat sb;
        if(stat(filePath, &sb) < 0)
                return 0;
        return sb.st_size;
}

void *do_work_file(void *arg)
{
    int n,i;
    struct s_info *ts = (struct s_info*)arg;
    char buf[MAXLINE];
    char str[INET_ADDRSTRLEN];

    memset(buf, 0, sizeof(buf));
    n = Read(ts->connfd, buf, MAXLINE);                     //读客户端
    if (n == 0) 
    {
        printf("the client %d closed...\n", ts->connfd);
        goto end;
    }
    printf("received from %s at PORT %d\n",
            inet_ntop(AF_INET, &(*ts).cliaddr.sin_addr, str, sizeof(str)),
            ntohs((*ts).cliaddr.sin_port));                 //打印客户端信息(IP/PORT)

    printf("%s\n", buf);

    ///  open file & send it
    const char *filename = "1.log";
    int size = file_size(filename);
    if (size == 0) {
        goto end;
    }

    char resp_1024[1024] = {0};
    sprintf(resp_1024, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", size);
    Write(ts->connfd, resp_1024, strlen(resp_1024));                              //回写给客户端

    printf("%s\n", resp_1024);
    sleep(10);

	FILE *fin = fopen(filename, "r");
	if (!fin) {
        goto end;
    }

	char fbuf[1024*4]={0};
	int blockSize = sizeof(fbuf) - 1;
	int writelen = 0;
    i = 0;
	while (size > 0)
	{
		if (size < blockSize)
			blockSize = size;

		int t = fread(fbuf, sizeof(char), blockSize, fin);
        writelen = Write(ts->connfd, fbuf, blockSize);                              //回写给客户端
		size = size - blockSize;
        i++;
        if (i == 10) {
            //break;
        }
		if(writelen != blockSize)
		{
            goto end;
		}
        //sleep(1);
        break;
	}
    printf("i: %d\n", i);
    sleep(5);
    Write(ts->connfd, "c", 1);

end:
    if (fin) {
	    fclose(fin);
    }
    //sleep(10);
    Close(ts->connfd);
    return (void *)0;
}

void *do_post(void *arg)
{
    int n,i;
    struct s_info *ts = (struct s_info*)arg;
    char buf[MAXLINE];
    char str[INET_ADDRSTRLEN];
    char resp_1024[1024] = "HTTP/1.1 200 OK\r\nSet-Cookie:k1=v1\r\nContent-Length: 12\r\n\r\nabcdefghijkldef";

    if (1) 
    {
        memset(buf, 0, sizeof(buf));
        for (i = 0; i < 10; i++) {
            n = Read(ts->connfd, buf, MAXLINE);                     //读客户端
            if (n == 0) 
            {
                printf("the client %d closed...\n", ts->connfd);
                break;                                              //跳出循环,关闭cfd
            }
        }
        printf("received from %s at PORT %d\n",
                inet_ntop(AF_INET, &(*ts).cliaddr.sin_addr, str, sizeof(str)),
                ntohs((*ts).cliaddr.sin_port));                 //打印客户端信息(IP/PORT)

        printf("%s\n", buf);
        Write(ts->connfd, resp_1024, strlen(resp_1024));                              //回写给客户端
        printf("%s\n", resp_1024);

    }
    Close(ts->connfd);

    return (void *)0;
}

void *do_work(void *arg)
{
    int n,i;
    struct s_info *ts = (struct s_info*)arg;
    char buf[MAXLINE];
    char str[INET_ADDRSTRLEN];
    char resp_1024[1024] = "HTTP/1.1 200 OK\r\nSet-Cookie:k1=v1\r\nContent-Length: 21\r\nContent-Type: text/html\r\n\r\ncccabcdefghijkldefab";

    while (1) 
    {
        memset(buf, 0, sizeof(buf));
        n = Read(ts->connfd, buf, MAXLINE);                     //读客户端
        if (n == 0) 
        {
            printf("the client %d closed...\n", ts->connfd);
            break;                                              //跳出循环,关闭cfd
        }
        printf("received from %s at PORT %d\n",
                inet_ntop(AF_INET, &(*ts).cliaddr.sin_addr, str, sizeof(str)),
                ntohs((*ts).cliaddr.sin_port));                 //打印客户端信息(IP/PORT)

        printf("%s\n", buf);
        Write(ts->connfd, resp_1024, strlen(resp_1024));                              //回写给客户端
        printf("%s\n", resp_1024);

        break;
    }
    sleep(10);
    Write(ts->connfd, "c", 1);
    
    Close(ts->connfd);

    return (void *)0;
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);
    struct sockaddr_in servaddr, cliaddr;
    socklen_t cliaddr_len;
    int listenfd, connfd;
    pthread_t tid;
    struct s_info ts[256];      //根据最大线程数创建结构体数组.
    int i = 0;

    listenfd = Socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    Bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    Listen(listenfd, 128);

    printf("Accepting client connect ...\n");
    while (1) 
    {
        cliaddr_len = sizeof(cliaddr);
        connfd = Accept(listenfd, (struct sockaddr *)&cliaddr, &cliaddr_len);
        ts[i].cliaddr = cliaddr;
        ts[i].connfd = connfd;
        /* 达到线程最大数时，pthread_create出错处理, 增加服务器稳定性 */
        pthread_create(&tid, NULL, do_work, (void*)&ts[i]);
        //pthread_create(&tid, NULL, do_post, (void*)&ts[i]);
        //pthread_create(&tid, NULL, do_work_file, (void*)&ts[i]);
        pthread_detach(tid);
        i++;
    }

    return 0;
}
