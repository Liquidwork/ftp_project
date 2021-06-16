#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<dirent.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/stat.h>
#include<time.h>
#include<sys/procfs.h>
#include<fcntl.h>

#define MAXLINE 1024

// Users. Student can not upload any file but can download.
// Permission for mkdir, del, rename is also not limited.
static char STUDENT[32] = "student";
static char ADMIN[32] = "admin";
static char STUDENT_PASSWORD[32] = "111111";
static char ADMIN_PASSWORD[32] = "123456";
static int flag = 0; // 0 for not log in yet, 1 for user input, 2 for logged in

static char active_user[32] = "UNAUTHORIZED";
static char active_mode[32] = "ASCII";

static int listen_socket, ftp_pi;
static int passive_listen_socket, data_socket;
static int sleep_us;


static char oldfilen[256]=""; // Linux pathname must less than 4096 byte, but I assume there is no need to prepare so much memory for it.

void parse_command(char* input, char* command, char* param);
void do_CDUP();
void do_CWD(char* path);
void do_DELE(char* path);
void do_LIST();
void do_MKD(char* path);
int do_PASS(char* password);
int do_PASV();
int do_PORT(char* ip_and_port);
void do_PWD();
void do_QUIT();
void do_RETR(char* filename);
void do_STOR(char* filename);
void do_RNFR(char* path);
void do_RNTO(char* path);
void do_SYST();
void do_TYPE(char* type);
int do_USER(char* name);
void str_dot2comma(char* ip);
int validation();
int respond(int socket, int statue, char* msg);
void trim(char* msg);
char* trim_pathname(char* pathname);
const char* statbuf_get_perms(struct stat *sbuf);
const char* statbuf_get_date(struct stat *sbuf);
void strrpl(char* str, char* dest, char* from, char* to);
void limit_speed();
int check_permission();


int main(int argc, char** argv){
    struct sockaddr_in servaddr;
    char buff[MAXLINE];
    char command[16], param[128];
    int n;

    printf("\n---- C language FTP by Dongyu and Zerui ----\n\n");

    /*
     * Arg parsing
     */

    if(argc >= 2){
        int max_speed = atoi(argv[1]); // Max speed param is in kb/s
        sleep_us = 1000000 / max_speed;
        printf("Speed limit for uploading and downloading is %d kb/s.\n", max_speed);
    } else{
        sleep_us = 0; // Not sleep at all
        printf("No speed limit set.\n");
    }

    /*
     * TCP listen server part
     */

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

    printf("Server listening on port 21.\n");

    /*
     * TCP accept and command processing part
     */

    struct sockaddr_in src_addr = {0};
    unsigned int len = sizeof(src_addr);
    data_socket = -1;

    while(1){

        chdir("/home/student/"); // Initialize the folder from student root

        printf("Trying to accept new connections.\n");
        if((ftp_pi = accept(listen_socket, (struct sockaddr*) &src_addr, &len)) == -1){
            printf("accept socket error: %s(errno: %d)\n",strerror(errno),errno);
            close(ftp_pi);
            continue;
        }
        printf("Receiving new connection from %s:%d\n", inet_ntoa(src_addr.sin_addr),
               ntohs(src_addr.sin_port));
        if(respond(ftp_pi, 200, "C language FTP by Dongyu and Zerui.")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
            return -1;
        }
        while(1){
            n = recv(ftp_pi, buff, MAXLINE, 0);
            if(n == 0){
                // If the socket was closed from the other side, return a 0
                printf("The socket has been closed from the other side.\n");
                break;
            }
            if(n < 3){ // Not valid command obviously, or no more data to read. Just continue.
                continue;
            }
            buff[n] = '\0';
            trim(buff);

            printf("[%s]: %s\n", active_user, buff); // printing input

            // Parse the string to command and param.
            parse_command(buff, command, param);

            if (strcmp(command, "USER")==0){
                do_USER(param);
            } else if (strcmp(command, "PASS")==0){
                do_PASS(param);
            } else if (strcmp(command, "QUIT") == 0) {
                do_QUIT();
                break;
            } else if (strcmp(command, "SYST") == 0) {
                do_SYST();
            } else if (flag == 2) { // If login successfully
                // Try command list.
                if(strcmp(command, "CDUP") == 0) {
                    do_CDUP();
                } else if (strcmp(command, "CWD") == 0) {
                    do_CWD(param);
                } else if (strcmp(command, "DELE") == 0) {
                    if(check_permission()){
                        do_DELE(param);
                    }
                } else if (strcmp(command, "LIST") == 0) {
                    do_LIST();
                } else if (strcmp(command, "MKD") == 0) {
                    if(check_permission()){
                        do_MKD(param);
                    }
                } else if (strcmp(command, "PASV") == 0) {
                    do_PASV();
                } else if (strcmp(command, "PORT") == 0) {
                    do_PORT(param);
                } else if (strcmp(command, "PWD") == 0 ||strcmp(command, "PWDT") == 0) {
                    do_PWD();
                } else if (strcmp(command, "RNFR") == 0) {
                    if(check_permission()){
                        do_RNFR(param);
                    }
                } else if (strcmp(command, "RNTO") == 0) {
                    if(check_permission()){
                        do_RNTO(param);
                    }
                }else if (strcmp(command, "TYPE") == 0) {
                    do_TYPE(param);
                } else if (strcmp(command, "RETR") == 0) {
                    do_RETR(param);
                } else if (strcmp(command, "STOR") == 0) {
                    if(check_permission()){
                        do_STOR(param);
                    }else{
                        close(data_socket);
                        data_socket = -1;
                        printf("The data connection has been closed.\n");
                    }
                } else {
                    if (respond(ftp_pi, 503, "Unsupported command.")) {
                        printf("sending respond to pi error: %s(errno: %d)\n", strerror(errno), errno);
                        return -1;
                    }
                    printf("Unsupported command: %s\n", command);
                }
            } else {
                // Unauthorized
                validation();
            }
        }

        // Run the command
        close(ftp_pi);
        close(data_socket); // Close it avoid not closed before.
        data_socket = -1;
        printf("Socket closed.\n");
        // Reset login statue
        flag = 0;
        strcpy(active_user, "UNAUTHORIZED");
        strcpy(active_mode, "ASCII");
    }
    close(listen_socket);
    return 0;
}

// Parse input into command part and param part.
void parse_command(char* input, char* command, char* param){
    char mode_flag = 0; // 0 for command part, 1 for param part.
    char *in_ptr = input;
    int bit = 0;
    *command = '\0'; // Reinitialize to avoid bug
    *param = '\0';
    while(*in_ptr != '\0'){
        if(mode_flag == 0){
            command[bit++] = *in_ptr;
            in_ptr++;
            if(*in_ptr == ' '){ // If next bit is space
                command[bit] = '\0';
                bit = 0;
                mode_flag = 1; // Set to next mode: parsing param
                in_ptr++;
            }
        }else{
            param[bit++] = *in_ptr;
            in_ptr++;
        }
    }
    param[bit] = '\0';
    //printf("Command: <%s>, Param: <%s>\n", command, param);
}

// Change working directory to upper level
void do_CDUP(){
    do_CWD("..");
}

// Change working directory
void do_CWD(char* path){
    if(!strcmp(path, "~")) strcpy(path, "/home/student/");
    if(chdir(path) == -1){
        if(respond(ftp_pi, 550,"No such directory")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
    } else{
        char buf[1024];
        char msg[2048] = "Change to directory: ";
        getcwd(buf, sizeof(buf));
        strcat(msg, buf);
        printf(msg);
        if(respond(ftp_pi, 257,msg)){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
    }
}

//delete a file or directory
void do_DELE(char* path){
    int isRemove = remove(path);
    if( isRemove == 0) {
        printf("Delete: %s\n", path);
        if (respond(ftp_pi, 250, "Delete success.")) {
            printf("sending respond to pi error: %s(errno: %d)\n", strerror(errno), errno);
        }
    }else {
        printf("Delete failed!");
        if (respond(ftp_pi, 450, "Can't delete file or directory, may not existed.")) {
            printf("sending respond to pi error: %s(errno: %d)\n", strerror(errno), errno);
        }
    }
}

// Send list of active path through data connection
// The method now is not completed, since file system not finished.
void do_LIST(){
    char res[128];
    if (data_socket != -1){ // If data connection is active.
        sprintf(res, "Opening %s mode data connection for /bin/ls.", active_mode);
        respond(ftp_pi, 150, res);
        printf("Executing LIST.\n");
        DIR *dir = opendir(".");

        struct dirent *dt;
        struct stat sbuf;
        while ((dt = readdir(dir)) != NULL)
        {
            if (lstat(dt->d_name, &sbuf) < 0)
            {
                continue;
            }
            if (dt->d_name[0] == '.')
                continue;

            char buf[1024] = {0};

            const char *perms = statbuf_get_perms(&sbuf);


            int off = 0;
            off += sprintf(buf, "%s ", perms);
            off += sprintf(buf + off, " %3d %-8d %-8d ", sbuf.st_nlink, sbuf.st_uid, sbuf.st_gid);
            off += sprintf(buf + off, "%8lu ", (unsigned long)sbuf.st_size);

            const char *datebuf = statbuf_get_date(&sbuf);
            off += sprintf(buf + off, "%s ", datebuf);
            if (S_ISLNK(sbuf.st_mode))
            {
                char tmp[1024] = {0};
                readlink(dt->d_name, tmp, sizeof(tmp));
                sprintf(buf + off, "%s -> %s\r\n", dt->d_name, tmp);
            }
            else
            {
                sprintf(buf + off, "%s\r\n", dt->d_name);
            }


            //printf("%s", buf);
            send(data_socket, buf, strlen(buf), 0);
        }
        respond(ftp_pi, 226, "Transfer complete.");
        printf("LIST transferred.\n");
        closedir(dir);
        // Close the socket.
        close(data_socket);
        data_socket = -1;
    }else{
        respond(ftp_pi, 425, "Transfer aborted.");
        printf("LIST execution failed because of a unconnected transfer.");
    }
}

//try Create a directory
void do_MKD(char* path){
    int isCreate = mkdir(path,775);
    if( isCreate==0 ) {
        printf("create path: %s\n", path);
        if (respond(ftp_pi, 250, path)) {
            printf("sending respond to pi error: %s(errno: %d)\n", strerror(errno), errno);
        }
    }else {
        printf("create path failed!");
        if (respond(ftp_pi, 550, "Create failed, invalid name or directory existed.")) {
            printf("sending respond to pi error: %s(errno: %d)\n", strerror(errno), errno);
        }
    }
}

// Response to PASS, used to check password after username check
int do_PASS(char* password){
    if(!strcmp(active_user, ADMIN) && !strcmp(password, ADMIN_PASSWORD)){
        if(respond(ftp_pi, 230, "User logged in, proceed.")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
        printf("Login success! Active user: %s\n", active_user);
        flag = 2;
    } else if(!strcmp(active_user, STUDENT) && !strcmp(password, STUDENT_PASSWORD)){
        if(respond(ftp_pi, 230, "User logged in, proceed.")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
        printf("Login success! Active user: %s\n", active_user);
        flag = 2;
    } else{
        if(respond(ftp_pi, 530, "Not logged in.")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
        printf("Password or username invalid.\n");
        strcpy(active_user, "UNAUTHORIZED");
        flag = 0;
    }
    return flag;
}

// Start a new do_PORT data socket
int do_PORT(char* ip_and_port){
    struct sockaddr_in servaddr, clientaddr;
    unsigned char ip_seg[4], port_seg[2];
    char ip[32];
    unsigned int port;

    printf("Active mode on, trying to establish connection.\n");

    // Read the param and parse into clientaddr
    sscanf(ip_and_port, "%u,%u,%u,%u,%u,%u", &ip_seg[0], &ip_seg[1], &ip_seg[2], &ip_seg[3], &port_seg[0], &port_seg[1]);
    sprintf(ip, "%d.%d.%d.%d", ip_seg[0], ip_seg[1], ip_seg[2], ip_seg[3]);
    port = port_seg[0] * 256 + port_seg[1];
    memset(&clientaddr, 0, sizeof(servaddr));
    clientaddr.sin_family = AF_INET;
    inet_aton(ip, &clientaddr.sin_addr);
    clientaddr.sin_port = htons(port);

    printf("Trying to connect to %s:%u\n", ip, port);
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
        if(respond(ftp_pi, 520, "Data connection failed")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
            return -1;
        }
        return -1;
    }
    if(respond(ftp_pi, 200, "PORT command successful. Consider using PASV.")){
        printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }
    printf("Data connection established. Client IP: %s Port: %d\n", ip, port);
    return 0;
}

// Start a new do_PASV data socket
int do_PASV(){
    struct sockaddr_in servaddr;
    unsigned int len = sizeof(servaddr);

    printf("Passive mode on.\n");

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
    int port = ntohs(servaddr.sin_port);
    first_seg = port / 256;
    second_seg = port % 256;

//    char *ip = inet_ntoa(servaddr.sin_addr);
//    str_dot2comma(ip);

    // Response in Pi
    char res[128];

    sprintf(res, "Entering Passive Mode (127,0,0,1,%d,%d)", first_seg, second_seg);
    if(respond(ftp_pi, 227, res)){
        printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    //printf("Entering passive mode, listening 127.0.0.1:%d\n", port);

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

// Display current working directory
void do_PWD(){
    char buf[1024];
    char msg[2048] = "Current directory is: ";
    getcwd(buf, sizeof(buf));
    strcat(msg, buf);
    printf(msg);
    if(respond(ftp_pi, 257, msg)){
        printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
    }
}

// Quiting respond and server console output
void do_QUIT(){
    if(respond(ftp_pi, 221, "Goodbye.")){
        printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
    }
    printf("FTP client quit.\n");
}

// Retransmission a file from server.
void do_RETR(char* filename){
    int file, throughput = 0;
    long start_time;
    char buff[MAXLINE], res[64];
    if (data_socket == -1){ // If connection is off
        respond(ftp_pi, 425, "Transfer aborted.");
        printf("REST execution failed because of a unconnected transfer.\n");
        return;
    }
    printf("Executing RETR. Trying to transfer file: %s in %s mode.\n", filename, active_mode);
    if((file = open(filename, O_RDONLY)) == -1)
    {
        respond(ftp_pi, 550, "File inaccessible.");
        printf("Transmission failed because of file is inaccessible.\n");
        close(file);
        close(data_socket);
        data_socket = -1;
        return;
    }
    // Getting the size of the file.
    int size = lseek(file, 0, SEEK_END); // Set to end and get size
    sprintf(res, "Opening %s mode data connection for %s (%i bytes).", active_mode, filename, size);
    respond(ftp_pi, 150, res);
    printf("Trying to transmit file: %s (%i bytes)\n", filename, size);
    lseek(file, 0, SEEK_SET); // Set to start again for transmission

    start_time = clock();
    int ret, sent_byte, count = 0;
    if(strcmp(active_mode, "ASCII") == 0){
        char asc_buff[MAXLINE * 2];
        while((ret = read(file, buff, MAXLINE - 1)) > 0)
        {
            buff[ret] = '\0';
            strrpl(buff, asc_buff, "\n", "\r\n"); // return value is bad
            if((sent_byte = send(data_socket, asc_buff, strlen(asc_buff), 0)) == -1){
                ret = -1;
                break;
            }
            throughput += sent_byte;
            count++;
            limit_speed();
        }
    }else{ // Default as binary
        while((ret = read(file, buff, MAXLINE)) > 0)
        {
            if((sent_byte = send(data_socket, buff, ret, 0)) == -1){
                ret = -1;
                break;
            }
            throughput += sent_byte;
            limit_speed();
            count++;
        }
    }

    if(ret == -1){
        respond(ftp_pi, 450, "An exception happened during the transfer");
        printf("Transfer aborted as exception happened during the transfer.\n");
        close(file);
        close(data_socket);
        data_socket = -1;
        return;
    }

    double time = (clock() - start_time + count * sleep_us) / 1000000.0; // Millisecond
    double through = throughput / 1024.0;
    double speed = through / time;
    printf("Transmission complete in %.2lf seconds. \n"
           "%d bytes sent, speed: %.2lf kb/s.\n", time, throughput, speed);
    respond(ftp_pi, 226, "Transfer complete.");
    // Close them while resetting the value.
    close(file);
    close(data_socket);
    data_socket = -1;
}

// Store a file to the server. If transmission failed, the tmp file will be deleted.
// It is a risky operation since unsuccessful transmission will overwrite a existing file before deletion.
void do_STOR(char* filename){
    int file;
    long start_time;
    char buff[MAXLINE];
    if (data_socket == -1){ // If connection is off
        respond(ftp_pi, 425, "Transfer aborted.");
        printf("REST execution failed because of a unconnected transfer.\n");
        return;
    }
    filename = trim_pathname(filename);
    printf("Executing STOR. Trying to receive file: %s in %s mode.\n", filename, active_mode);
    printf("The file will be stored as %s\n", filename);

    // Try to open the file, if file does not exist, create it.
    if((file = open(filename, O_WRONLY | O_CREAT, 774)) == -1)
    {
        respond(ftp_pi, 550, "File not writeable.");
        printf("Transmission failed because of file is inaccessible.\n");
        close(file);
        close(data_socket);
        data_socket = -1;
        return;
    }
    respond(ftp_pi, 150, "Ok to send data.");

    int ret, throughput = 0, count = 0;
    start_time = clock();
    if(strcmp(active_mode, "ASCII") == 0){
        char asc_buff[MAXLINE];
        char final = '\0';
        while((ret = recv(data_socket, buff, MAXLINE - 1, 0)) > 0)
        {
            buff[ret] = '\0';
            // If the buff read end between a '\r' and a '\n', remove that '\r'
            final = buff[ret - 1];
            if(final == '\r' && buff[0] == '\n') {
                lseek(file, -1, SEEK_CUR); // Later, the input to file will overwrite the value.
            }
            strrpl(buff, asc_buff, "\r\n", "\n");
            if(write(file, asc_buff, strlen(asc_buff)) == -1){
                ret = -1;
                break;
            }
            throughput += ret;
            limit_speed();
            count++;
        }
    }else{ // Default as binary
        while((ret = recv(data_socket, buff, MAXLINE, 0)) > 0)
        {
            if(write(file, buff, ret) == -1){
                ret = -1;
                break;
            }
            throughput += ret;
            limit_speed();
            count++;
        }
    }

    if(ret == -1){
        respond(ftp_pi, 450, "An exception happened during the transfer");
        printf("Transfer aborted as exception happened during the transfer.\n");
        if (remove(filename) == 0){
            printf("Tmp file removed.\n");
        }
        close(file);
        close(data_socket);
        data_socket = -1;
        return;
    }
    respond(ftp_pi, 226, "Transfer complete.");
    double time = (clock() - start_time + count * sleep_us) / 1000000.0; // Millisecond
    double through = throughput / 1024.0;
    double speed = through / time;
    printf("Transmission complete in %.2lf seconds. \n"
           "%d bytes received, speed: %.2lf kb/s.\n", time, throughput, speed);

    // Close them while resetting the value.
    close(file);
    close(data_socket);
    data_socket = -1;
}



void do_RNFR(char* path){
    printf("Ready to rename : %s\n", path);
    strcpy(oldfilen, path);
    if(access(path, W_OK)==0){
        if(respond(ftp_pi, 350,"Ready for RNTO.")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
    } else{
        if(respond(ftp_pi, 550,"RNFR command faild.")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
    }
}

void do_RNTO(char* path){
    printf("Trying to rename %s to %s\n", oldfilen, path);
    if (rename(oldfilen, path)==0){
        printf("%s rename to: %s\n",oldfilen, path);
        if(respond(ftp_pi, 250,"Rename successful.")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
    } else{
        if(respond(ftp_pi, 503,"Cannot find the file.")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
    }

    strcpy(oldfilen, "");
}

// Showing the OS, UNIX
void do_SYST(){
    if(respond(ftp_pi, 215, "UNIX Type: L8")) {
        printf("sending response to pi error: %s(errno: %d)", strerror(errno), errno);
    }
}

// Switch to different type of transmission.
void do_TYPE(char* type){
    if(strcmp(type, "I") == 0){
        respond(ftp_pi, 200, "Switching to Binary mode.");
        printf("Switching to Binary mode.\n");
        strcpy(active_mode, "BINARY");
    }else if(strcmp(type, "A") == 0){
        respond(ftp_pi, 200, "Switching to ASCII mode.");
        printf("Switching to ASCII mode.\n");
        strcpy(active_mode, "ASCII");
    }else{
        respond(ftp_pi, 501, "Unsupported mode specified, mode unchanged.");
        printf("Unsupported mode specified, mode unchanged.\n");
    }
}

// Used to check username before any activities
int do_USER(char* name){
    if(!strcmp(STUDENT, name)){
        flag = 1;
        strcpy(active_user, STUDENT);
    }else if (!strcmp(ADMIN, name)){
        flag =1;
        strcpy(active_user, ADMIN);
    }else{
        flag = 0;
        strcpy(active_user, "UNAUTHORIZED");
    }
    if(respond(ftp_pi, 331, "User name okay, need password.")){
        printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
    }
    printf("Username checked\n");

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

// Used to check the status of user account
int validation(){
    if(flag==0){
        if(respond(ftp_pi, 332, "Need valid account for login.")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
        printf("Invalid username");
    } else if(flag ==1){
        if(respond(ftp_pi, 332, "Need valid account for login.")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
        printf("Invalid username\n");
    }
    return flag;
}

// Send respond to specific socket, returning 0 indicate no error happens
int respond(int socket, int statue, char* msg){
    char res[128];
    sprintf(res, "%d %s\r\n", statue, msg);
    return send(socket, res, strlen(res), 0) - strlen(res);
}

// Trim the returns in a line, by replacing the first one with '\0'
void trim(char* msg){
    while (*msg != '\0'){
        if (*msg == '\r' || *msg == '\n'){
            *msg = '\0';
        }
        msg++;
    }
}

// Trim the path to file name, returning the char sequence which is the beginning of file name.
char* trim_pathname(char* pathname){
    char *last_dash = pathname - 1;
    while (*pathname != '\0'){
        if(*pathname == '/'){
            last_dash = pathname;
        }
        pathname++;
    }
    return last_dash + 1;
}

// Generate permission string
const char* statbuf_get_perms(struct stat *sbuf)
{
    static char perms[] = "----------";
    perms[0] = '?';

    strcpy(perms, "----------"); // Reset it.
    mode_t mode = sbuf->st_mode;
    switch (mode & S_IFMT)
    {
        case S_IFREG:
            perms[0] = '-';
            break;
        case S_IFDIR:
            perms[0] = 'd';
            break;
        case S_IFLNK:
            perms[0] = 'l';
            break;
        case S_IFIFO:
            perms[0] = 'p';
            break;
        case S_IFSOCK:
            perms[0] = 's';
            break;
        case S_IFCHR:
            perms[0] = 'c';
            break;
        case S_IFBLK:
            perms[0] = 'b';
            break;
    }

    if (mode & S_IRUSR)
    {
        perms[1] = 'r';
    }
    if (mode & S_IWUSR)
    {
        perms[2] = 'w';
    }
    if (mode & S_IXUSR)
    {
        perms[3] = 'x';
    }
    if (mode & S_IRGRP)
    {
        perms[4] = 'r';
    }
    if (mode & S_IWGRP)
    {
        perms[5] = 'w';
    }
    if (mode & S_IXGRP)
    {
        perms[6] = 'x';
    }
    if (mode & S_IROTH)
    {
        perms[7] = 'r';
    }
    if (mode & S_IWOTH)
    {
        perms[8] = 'w';
    }
    if (mode & S_IXOTH)
    {
        perms[9] = 'x';
    }
    if (mode & S_ISUID)
    {
        perms[3] = (perms[3] == 'x') ? 's' : 'S';
    }
    if (mode & S_ISGID)
    {
        perms[6] = (perms[6] == 'x') ? 's' : 'S';
    }
    if (mode & S_ISVTX)
    {
        perms[9] = (perms[9] == 'x') ? 't' : 'T';
    }

    return perms;
}

// Generate last edit date
const char* statbuf_get_date(struct stat *sbuf)
{
    static char datebuf[64] = {0};
    const char *p_date_format = "%b %e %H:%M";
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t local_time = tv.tv_sec;
    if (sbuf->st_mtime > local_time || (local_time - sbuf->st_mtime) > 60*60*24*182)
    {
        p_date_format = "%b %e  %Y";
    }

    struct tm* p_tm = localtime(&local_time);
    strftime(datebuf, sizeof(datebuf), p_date_format, p_tm);

    return datebuf;
}

// Replace segment of a string. Beware of memory leek when converting from small segment to a larger one.
void strrpl(char* str, char* dest, char* from, char* to){
    int i;

    *dest = '\0'; // Reset the string
    for(i = 0; i < strlen(str); i++){
        if(!strncmp(str+i, from,strlen(from))){ // compare string
            strcat(dest, to);
            i += strlen(from) - 1;
        }else{
            strncat(dest,str + i,1);
        }
    }
}

void limit_speed(){
    if(sleep_us > 0){
        usleep(sleep_us);
    }
}

// 1 for no access permission, 0 for permission
int check_permission(){
    if(strcmp(active_user, ADMIN) != 0){
        if(respond(ftp_pi, 550, "Permission denied.")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
        printf("Permission denied.\n");
        return 0;
    } else return 1;
}
