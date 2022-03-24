#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

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

  if (connect(sockfd, (struct sockaddr *) &addr,
              sizeof(sa_family_t) + strlen(str) + 1) == -1) {
    printf("error %d", errno);
    return 1;
  }
  printf("Socket connected to abstract address\n");

  char buf[256];
  int received = recv(sockfd, &buf, 256, 0);
  printf("Received %s\n", buf);

  return 0;
}
