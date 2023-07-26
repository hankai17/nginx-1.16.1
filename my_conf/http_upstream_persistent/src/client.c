#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define SERV_IP "0.0.0.0"
#define SERV_PORT 80

int main(void)
{
    int sfd, len;
    struct sockaddr_in serv_addr;
    char buf[BUFSIZ]; 

    sfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&serv_addr, sizeof(serv_addr)); 
    serv_addr.sin_family = AF_INET;      
    inet_pton(AF_INET, SERV_IP, &serv_addr.sin_addr.s_addr);   
    serv_addr.sin_port = htons(SERV_PORT);                    

    connect(sfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    //sleep(1000);
    char request_hdr[1024] = "GET / HTTP/1.1\r\nHost: 0.0.0.0\r\nUser-Agent: curl/7.61.1\r\nConnection: Keep-Alive\r\nAccept: */*\r\n\r\n";
    //char request_hdr[1024] = "GET / HTTP/1.0\r\nHost: 0.0.0.0\r\nUser-Agent: curl/7.61.1\r\nConnection: close\r\nAccept: */*\r\n\r\n";
    write(sfd, request_hdr, strlen(request_hdr));
    //sleep(5);
    while (1) 
    {
        len = read(sfd, buf, sizeof(buf));
        if (len <= 0) {
            break;
        }
        printf("buf: %s\n", buf);
    }

    sleep(100);

    close(sfd);
    return 0;
}

