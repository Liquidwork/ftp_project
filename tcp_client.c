#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int main(int argc, char *argv[])
{
  int tcp_sock;
  struct sockaddr_in server = {0};
  unsigned int addrLength = sizeof(server);
  char sendString[256];
  sendString[0] = '\0';
  
  if (argc < 3){
    printf("Usage: %s <Server IP> <Echo Word>...", argv[0]);
    exit(1);
  }
  
  printf("Trying to concating string.");
  int i = 2;
  strcpy(sendString, argv[i]);
  for (i = 3; i < argc; i++) 
  {
  strcat(sendString, " ");
  strcat(sendString, argv[i]); // Concrating strings
  }
  // sendString = argv[2];
  
  if ((tcp_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) printf("socket() failed.\n"); // Create socket
  
  server.sin_family = AF_INET; 
  server.sin_addr.s_addr = inet_addr(argv[1]);
  server.sin_port = htons(2500);

  connect(tcp_sock, (struct sockaddr*) &server, addrLength);
  
  sleep(1);
  
  //printf("Sending to %d\n", server.sin_port);
  if (send(tcp_sock, sendString, strlen(sendString) + 1, 0) != strlen(sendString) + 1) printf("sendto() sent a different number of bytes than expected.\n"); // Try to send to the server.
  printf("Sending complete.\n");
  printf("%s\n", sendString);
  close(tcp_sock);
  
}
