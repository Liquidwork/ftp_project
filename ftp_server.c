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

static char NAME[32] = "student";
static char PASSWORD[32] = "111111";
static int flag = 0; // 0 for not log in yet, 1 for user input, 2 for logged in

static char active_user[32] = "UNAUTHORIZED";
static char active_mode[32] = "ASCII";

static int listen_socket, ftp_pi;
static int passive_listen_socket, data_socket;

//HELP!
static char* oldfilen="";

void parse_command(char* input, char* command, char* param);
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
void do_RNFR(char* path);
void do_RNTO(char* path);
void do_SYST();
void do_TYPE(char* type);
int do_USER(char* name);
void str_dot2comma(char* ip);
int validation();
int respond(int socket, int statue, char* msg);
void trim(char* msg);
const char* statbuf_get_perms(struct stat *sbuf);
const char* statbuf_get_date(struct stat *sbuf);


int main(int argc, char** argv){
    struct sockaddr_in servaddr;
    char buff[MAXLINE];
    char command[16], param[128];
    int n;

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

    printf("Server listening on port 21\n");

    /*
     * TCP accept and command processing part
     */

    struct sockaddr_in src_addr = {0};
    unsigned int len = sizeof(src_addr);
    data_socket = -1;

    while(1){
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

            if(strcmp(command, "USER")==0){
                do_USER(param);
            } else if(strcmp(command, "PASS")==0){
                do_PASS(param);
            } else if (strcmp(command, "QUIT") == 0) {
                do_QUIT();
                break;
            } else if (strcmp(command, "SYST") == 0) {
                do_SYST();
            } else if (flag == 2) { // If login successfully
                // Try command list.
                if(strcmp(command, "CWD") == 0) {
                    do_CWD(param);
                } else if (strcmp(command, "DELE") == 0) {
                    do_DELE(param);
                } else if (strcmp(command, "LIST") == 0) {
                    do_LIST();
                } else if (strcmp(command, "MKD") == 0) {
                    do_MKD(param);
                } else if (strcmp(command, "PASV") == 0) {
                    do_PASV();
                } else if (strcmp(command, "PORT") == 0) {
                    do_PORT(param);
                } else if (strcmp(command, "PWD") == 0 ||strcmp(command, "PWDT") == 0) {
                    do_PWD();
                } else if (strcmp(command, "RNFR") == 0) {
                    do_RNFR(param);
                } else if (strcmp(command, "RNTO") == 0) {
                    do_RNTO(param);
                }else if (strcmp(command, "TYPE") == 0) {
                    do_TYPE(param);
                } else if (strcmp(command, "RETR") == 0) {
                    do_RETR(param);
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
    printf("Command: <%s>, Param: <%s>", command, param);
}

//Change working directory
void do_CWD(char* path){
    if(chdir(path)== -1){
        if(respond(ftp_pi, 550,"No such directory")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
    } else{
        char buf[1024];
        getcwd(buf,sizeof(buf));
        printf("change to  directory: %s\n",buf);
        if(respond(ftp_pi, 257,buf)){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
    }
}

//delete a file or directory
void do_DELE(char* path){
    int isRemove = remove(path);
    if( isRemove==0 ) {
        printf("delete :%s\n", path);
        if (respond(ftp_pi, 250, "delete success")) {
            printf("sending respond to pi error: %s(errno: %d)\n", strerror(errno), errno);
        }
    }else {
        printf("delete failed!");
        if (respond(ftp_pi, 450, "Can't delete file or directory, may not existed")) {
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
        printf("create path:%s\n", path);
        if (respond(ftp_pi, 250, path)) {
            printf("sending respond to pi error: %s(errno: %d)\n", strerror(errno), errno);
        }
    }else {
        printf("create path failed!");
        if (respond(ftp_pi, 550, "Create failed, invalid name or diretory existed")) {
            printf("sending respond to pi error: %s(errno: %d)\n", strerror(errno), errno);
        }
    }
}

// Response to PASS, used to check password after username check
int do_PASS(char* password){
    if(!strcmp(PASSWORD, password)){
        if(respond(ftp_pi, 230, "User logged in, proceed.")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
        printf("Login success! Active user: %s\n", active_user);
        flag = 2;
    } else{
        if(respond(ftp_pi, 530, "Not logged in.")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
        printf("Password invalid\n");
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

    printf("Active mode on, trying to establish connection\n");

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

    printf("Passive mode on\n");

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

// Display curretn working directory
void do_PWD(){
    char buf[1024];
    getcwd(buf,sizeof(buf));
    printf("current working directory: %s\n",buf);
    if(respond(ftp_pi, 257,buf)){
        printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
    }
}

// Quiting respond and server console output
void do_QUIT(){
    if(respond(ftp_pi, 221, "Goodbye.")){
        printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
    }
    flag = 0;
    strcpy(active_user, "");
    printf("FTP client quit\n");
}

// Retransmission a file from server.
void do_RETR(char* filename){
    int file;
    char buff[MAXLINE], res[64];
    if (data_socket == -1){ // If connection is off
        respond(ftp_pi, 425, "Transfer aborted.");
        printf("REST execution failed because of a unconnected transfer.");
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
    sprintf(res, "Opening %s mode data connection for %s (%i byte).", active_mode, filename, size);
    respond(ftp_pi, 150, res);
    printf("Trying to transmit file: %s (%i byte)\n", filename, size);
    lseek(file, 0, SEEK_SET); // Set to start again for transmission


    int ret;
    while((ret = read(file, buff, MAXLINE)) > 0)
    {
        if(send(data_socket, buff, ret, 0) == -1){
            ret = -1;
            break;
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
    respond(ftp_pi, 226, "Transfer complete.");
    printf("Transmission complete\n");

    // Close them while resetting the value.
    close(file);
    close(data_socket);
    data_socket = -1;
}

//
void do_RNFR(char* path){
    printf("ready to rename : %s\n", path);
    oldfilen = path;
    if(access(path, W_OK)==0){
        if(respond(ftp_pi, 350,"Ready for RNTO.")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
    } else{
        if(respond(ftp_pi, 550,"RNFR command faild.")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
    }
    printf("%s\n", oldfilen);
}

void do_RNTO(char* path){
    printf("from %s to %s\n", oldfilen, path);
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
    oldfilen = "";
}

// Showing the OS
void do_SYST(){
    if(respond(ftp_pi, 215, "Remote System is Linux.")) {
        printf("sending response to pi error: %s(errno: %d)", strerror(errno), errno);
    }
}

// Switch to different type of transmission. Actually it will not influence the transmission, for now.
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
    if(!strcmp(NAME,name)){
        if(respond(ftp_pi, 331, "User name okay, need password.")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
        printf("Username checked\n");
        strcpy(active_user, NAME);
        flag =1;
    }else{
        if(respond(ftp_pi, 332, "Need valid account for login.")){
            printf("sending respond to pi error: %s(errno: %d)\n",strerror(errno),errno);
        }
        printf("Username invalid\n");
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

// Generate permission string
const char* statbuf_get_perms(struct stat *sbuf)
{
    static char perms[] = "----------";
    perms[0] = '?';

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
