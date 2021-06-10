#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<dirent.h>
#include<unistd.h>
#include <arpa/inet.h>

#define MAXLINE 4096

int listen_socket, ftp_pi;
int passive_listen_socket, data_socket;

int passive();

int main(int argc, char** argv){
    struct sockaddr_in servaddr;
    char buff[MAXLINE];
    int n;

    if((listen_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(2500);

    if(bind(listen_socket, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
        printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    if(listen(listen_socket, 10) == -1){
        printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    printf("Server listening on port 2500\n");

    struct sockaddr_in src_addr = {0};
    int len = sizeof(src_addr);
    while(1){
        if((ftp_pi = accept(listen_socket, (struct sockaddr*) &src_addr, &len)) == -1){
            printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
            close(ftp_pi);
            continue;
        }
        n = recv(ftp_pi, buff, MAXLINE, 0);
        buff[n] = '\0';

        printf("[%s:%d]: %s\n", inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port), buff); // printing input

        // Try command list
        if(strcmp(buff, "PASV") == 0){
            passive();
        }else{
            char res[128] = "???";
            if(send(ftp_pi, res, strlen(res) + 1, 0) != strlen(res) + 1){
                printf("sending reponse to pi error: %s(errno: %d)",strerror(errno),errno);
                return -1;
            }
        }
        
        


        // Run the command
        close(ftp_pi);
    }
    close(listen_socket);
    return 0;
}

// Start a new passive data socket
int passive(){
    struct sockaddr_in servaddr;

    printf("Passive mode on");

    if((passive_listen_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(passive_listen_socket, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
        printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    // Response in Pi
    char res[128];
    //sprintf(res, "227 Entering Passive Mode (%s,%d)\r\n", inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));
    sprintf(res, "227 Entering Passive Mode (127,0,0,1,25,233)\r\n");
    if(send(ftp_pi, res, strlen(res) + 1, 0) != strlen(res) + 1){
        printf("sending reponse to pi error: %s(errno: %d)",strerror(errno),errno);
        return -1;
    }

    if(listen(passive_listen_socket, 1) == -1){
        printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    struct sockaddr_in src_addr = {0};
    int len = sizeof(src_addr);

    // Try to accept a new socket from data_connection
    if((data_socket = accept(passive_listen_socket, (struct sockaddr*) &src_addr, &len)) == -1){
        printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
        return -1;
    } 

}
