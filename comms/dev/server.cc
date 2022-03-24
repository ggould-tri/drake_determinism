#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  printf("Starting\n");

  int sockfd;
  struct sockaddr_un addr;

  memset(&addr, 0, sizeof(struct sockaddr_un));  /* Clear address structure */
  addr.sun_family = AF_UNIX;                     /* UNIX domain address */

  /* addr.sun_path[0] has already been set to 0 by memset() */
  const char str[] = "xyz";        /* Abstract name is "\0xyz" */
  strncpy(&addr.sun_path[1], str, strlen(str));
  sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  if (sockfd == -1)
    return 1;
  printf("Socket created\n");
  if (bind(sockfd, (struct sockaddr *) &addr,
           sizeof(sa_family_t) + strlen(str) + 1) == -1)
    return 1;
  printf("Socket bound to abstract address\n");

  if (listen(sockfd, 1) == -1)
    return 1;
  printf("Socket placed in listen mode\n");

  struct sockaddr_un client_sockaddr;
  socklen_t client_sockaddr_len = sizeof(client_sockaddr);
  int client_sock = accept(
      sockfd,
      (struct sockaddr *)(&client_sockaddr),
      &client_sockaddr_len);
  if (client_sock == -1) return 1;
  printf("accept bound new connection\n");

  write(client_sock, "foo!", 5);

  return 0;
}
