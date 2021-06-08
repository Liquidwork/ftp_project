#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<dirent.h>
#include<unistd.h>

#define MAXLINE 4096

int main(int argc, char** argv){
    int listen_socket, ftp_pi;
    struct sockaddr_in  servaddr;
    char buff[MAXLINE];
    int n;

    if( (listen_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(2500);

    if( bind(listen_socket, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
        printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    if( listen(listen_socket, 10) == -1){
        printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    printf("Server listening on port 2500\n");

    struct sockaddr_in src_addr = {0};
    int len = sizeof(src_addr);
    while(1){
        if( (ftp_pi = accept(listen_socket, (struct sockaddr*) &src_addr, &len)) == -1){
            printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
            continue;
        }
        n = recv(ftp_pi, buff, MAXLINE, 0);
        buff[n] = '\0';
        printf("[%s:%d]: %s\n", inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port), buff); // printing input

        // Try command list
        if(strcmp(buff, "ls") == 0){
            list();
        }
        
        // Run the command
        close(ftp_pi);
    }
    close(listen_socket);
    return 0;
}

int list(){
    printf("execute LIST\n");

    DIR *dir = NULL;
    dir = opendir("."); // Now opening the source dir
    struct dirent *ent = NULL; // file
    if (!dir) {
        return -1;
    }
    while ((ent = readdir(dir))) {
        printf("%s\n", ent->d_name);
        //send(server->data_sock, buf, strlen(buf), 0);
    }
    closedir(dir);
    return 0;
}
