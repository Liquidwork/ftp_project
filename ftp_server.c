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

static char NAME[32] = "student";
static char PASSWORD[32] = "111111";
static int flag = 0;

int listen_socket, ftp_pi;
int passive_listen_socket, data_socket;

void parse_command(char* input, char* command, char* param);
int do_PASS(char* password);
int do_PASV();
int do_PORT(char* ip_and_port);
void do_QUIT();
void do_SYST();
int do_USER(char* name);
void str_dot2comma(char* ip);
int validation();


int main(int argc, char** argv){
    struct sockaddr_in servaddr;
    char buff[MAXLINE];
    char command[16], param[128];
    int n;

    if((listen_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(21);

    if(bind(listen_socket, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
        printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    if(listen(listen_socket, 10) == -1){
        printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    printf("Server listening on port 21\n");

    struct sockaddr_in src_addr = {0};
    int len = sizeof(src_addr);

    char res[128];

    while(1){
        if((ftp_pi = accept(listen_socket, (struct sockaddr*) &src_addr, &len)) == -1){
            printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
            close(ftp_pi);
            continue;
        }
        strcpy(res, "200 C language FTP by Dongyu and Zerui\r\n");
        if(send(ftp_pi, res, strlen(res) + 1, 0) != strlen(res) + 1){
            printf("sending response to pi error: %s(errno: %d)",strerror(errno),errno);
            return -1;
        }
        while(1){
            n = recv(ftp_pi, buff, MAXLINE, 0);
            buff[n] = '\0';

            printf("[%s:%d]: %s\n", inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port), buff); // printing input

            // Parse the string to command and param.
            parse_command(buff, command, param);

            if(strcmp(command, "USER")==0){
                do_USER(param);
            } else if(strcmp(command, "PASS")==0){
                do_PASS(param);
            } else if (flag == 2) { // If login successfully
                // Try command list.
                if (strcmp(command, "PASV") == 0) {
                    do_PASV();
                } else if (strcmp(command, "PORT") == 0) {
                    do_PORT(param);
                } else if (strcmp(command, "QUIT") == 0) {
                    do_QUIT();
                    break;
                }
                else if (strcmp(command, "SYST") == 0) {
                    do_SYST();
                } else {
                    strcpy(res, "503 Unsupported command.\r\n");
                    if (send(ftp_pi, res, strlen(res) + 1, 0) != strlen(res) + 1) {
                        printf("sending response to pi error: %s(errno: %d)", strerror(errno), errno);
                        return -1;
                    }
                }
            } else {
                // Unauthorized
                validation();
            }
        }


        // Run the command
        close(ftp_pi);
        sleep(1);
    }
    close(listen_socket);
    return 0;
}

// Parse input into command part and param part.
void parse_command(char* input, char* command, char* param){
    char flag = 0; // 0 for command part, 1 for param part.
    char *in_ptr = input;
    int bit = 0;
    while(*in_ptr != '\0'){
        if(*in_ptr == '\n' || *in_ptr == '\r'){
            in_ptr++;
            continue;
        }
        if(flag == 0){
            command[bit++] = *in_ptr;
            in_ptr++;
            if(*in_ptr == ' '){ // If next bit is space
                command[bit] = '\0';
                bit = 0;
                flag = 1; // Set to next mode: parsing param
                in_ptr++;
            }
        }else{
            param[bit++] = *in_ptr;
            in_ptr++;
        }
    }
    if(bit == 0){
        *command = '\0';
    }
    param[bit] = '\0';

    printf("Command: <%s>, Param: <%s>", command, param);
}


//Response to PASS, used to check password after username check
int do_PASS(char* password){
    char res[128];
    if(!strcmp(PASSWORD, password)){
        sprintf(res, "230 User logged in, proceed.\r\n");
        if(send(ftp_pi, res, strlen(res) + 1, 0) != strlen(res) + 1){
            printf("sending response to pi error: %s(errno: %d)",strerror(errno),errno);
        }
        printf("Login success");
        flag =2;
    } else{
        sprintf(res, "530  Not logged in.\r\n");
        if(send(ftp_pi, res, strlen(res) + 1, 0) != strlen(res) + 1){
            printf("sending response to pi error: %s(errno: %d)",strerror(errno),errno);
        }
        printf("Password invalid");
    }
    return flag;
}

// Start a new do_PORT data socket
int do_PORT(char* ip_and_port){
    struct sockaddr_in servaddr, clientaddr;
    unsigned char ip_seg[4], port_seg[2];
    char ip[32];
    char res[128];
    unsigned int port;

    printf("Active mode on, trying to establish connection\n");

    // Read the param and parse into clientaddr
    sscanf(ip_and_port, "%u,%u,%u,%u,%u,%u", &ip_seg[0], &ip_seg[1], &ip_seg[2], &ip_seg[3], &port_seg[0], &port_seg[1]);
    sprintf(ip, "%d.%d.%d.%d", ip_seg[0], ip_seg[1], ip_seg[2], ip_seg[3]);
    port = port_seg[0] * 256 + port_seg[1];
    memset(&clientaddr, 0, sizeof(servaddr));
    clientaddr.sin_family = AF_INET;
    inet_aton(ip, &clientaddr.sin_addr);
    clientaddr.sin_port = htons(port);

    printf("Trying to connect to %s:%u", ip, port);
    if((data_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(data_socket, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
        printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    if(connect(data_socket, (struct sockaddr*)&clientaddr, sizeof(clientaddr)) == -1){
        printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);

        // Providing a error code to show the error
        sprintf(res, "520 Data connection failed\r\n");
        if(send(ftp_pi, res, strlen(res) + 1, 0) != strlen(res) + 1){
            printf("sending response to pi error: %s(errno: %d)",strerror(errno),errno);
            return -1;
        }
        return -1;
    }
    sprintf(res, "200 PORT command successful. Consider using PASV.\r\n");
    if(send(ftp_pi, res, strlen(res) + 1, 0) != strlen(res) + 1){
        printf("sending response to pi error: %s(errno: %d)",strerror(errno),errno);
        return -1;
    }
    printf("Data connection established. Client IP: %s Port: %d\n", ip, port);
    return 0;
}

// Start a new do_PASV data socket
int do_PASV(){
    struct sockaddr_in servaddr;
    int len = sizeof(servaddr);

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

    getsockname(passive_listen_socket, (struct sockaddr*)&servaddr, &len);
    unsigned char first_seg, second_seg;
    first_seg = servaddr.sin_port / 256;
    second_seg = servaddr.sin_port % 256;

//    char *ip = inet_ntoa(servaddr.sin_addr);
//    str_dot2comma(ip);

    // Response in Pi
    char res[128];

    sprintf(res, "227 Entering Passive Mode (127,0,0,1,%d,%d)\r\n", first_seg, second_seg);
//    sprintf(res, "227 Entering Passive Mode (%s,%d,%d)\r\n", ip, first_seg, second_seg);
    if(send(ftp_pi, res, strlen(res) + 1, 0) != strlen(res) + 1){
        printf("sending response to pi error: %s(errno: %d)",strerror(errno),errno);
        return -1;
    }

    printf("Entering passive mode, listening 127.0.0.1, %d", servaddr.sin_port);

    if(listen(passive_listen_socket, 1) == -1){
        printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    struct sockaddr_in clientaddr = {0};
    len = sizeof(clientaddr);

    // Try to accept a new socket from data_connection
    if((data_socket = accept(passive_listen_socket, (struct sockaddr*) &clientaddr, &len)) == -1){
        printf("accept socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    printf("Data connection established. Client IP: %s Port: %d\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

    close(passive_listen_socket); // Close the passive listening

    return 0;
}

// Quiting response and server console output
void do_QUIT(){
    char res[128];
    sprintf(res, "221 Goodbye.\r\n");
    if(send(ftp_pi, res, strlen(res) + 1, 0) != strlen(res) + 1){
        printf("sending response to pi error: %s(errno: %d)",strerror(errno),errno);
    }
    printf("FTP client quit");
}

//showing the OS
void do_SYST(){
    char res[128];
    sprintf(res, "215 Remote System is Linux.\r\n");
    if(send(ftp_pi, res, strlen(res) + 1, 0) != strlen(res) + 1) {
        printf("sending response to pi error: %s(errno: %d)", strerror(errno), errno);
    }
}

//used to check username before any activities
int do_USER(char* name){
    char res[128];
    if(!strcmp(NAME,name)){
        sprintf(res, "331 User name okay, need password.\r\n");
        if(send(ftp_pi, res, strlen(res) + 1, 0) != strlen(res) + 1){
            printf("sending response to pi error: %s(errno: %d)",strerror(errno),errno);
        }
        printf("Username check");
        flag =1;
    } else{
        sprintf(res, "332 Need valid account for login.\r\n");
        if(send(ftp_pi, res, strlen(res) + 1, 0) != strlen(res) + 1){
            printf("sending response to pi error: %s(errno: %d)",strerror(errno),errno);
        }
        printf("Username invalid");
    }
    return flag;
}

// Replace dot in str to comma
void str_dot2comma(char* ip){
    char *ptr = ip;
    while (*ptr != '\0'){
        if(*ptr == '.'){
            *ptr = ',';
        }
        ptr++;
    }
}

//Used to check the status of user account
int validation(){
    if(flag==0){
        char res[128];
        sprintf(res, "332  Need account for login.\r\n");
        if(send(ftp_pi, res, strlen(res) + 1, 0) != strlen(res) + 1){
            printf("sending response to pi error: %s(errno: %d)",strerror(errno),errno);
        }
        printf("Invalid username");
    } else if(flag ==1){
        char res[128];
        sprintf(res, "332  Need account for login.\r\n");
        if(send(ftp_pi, res, strlen(res) + 1, 0) != strlen(res) + 1){
            printf("sending response to pi error: %s(errno: %d)",strerror(errno),errno);
        }
        printf("Invalid username");
    }
    return flag;
}

//A function that might reduce the code duplicate for sending messgae to client
//Reserved, I don't know if it can reduce duplicated code
//Beacuse I dont understanding the address things in C
void response(char* msg){
    char res[128];
    sprintf(res, "%s", msg);
    if(send(ftp_pi, res, strlen(res) + 1, 0) != strlen(res) + 1){
        printf("sending response to pi error: %s(errno: %d)",strerror(errno),errno);
    }
}

