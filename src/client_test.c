#include "unp.h"

#define PREM_PORT 2038

int main(int argc, char **argv){
  int sockfd;
  struct sockaddr_in servaddr;
  const int on=1; 

  sockfd = Socket(AF_INET, SOCK_DGRAM, 0);

  inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr.s_addr);
  servaddr.sin_family = AF_INET;
  //servaddr.sin_port = htons(8888);
  servaddr.sin_port = htons(PREM_PORT);

  Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  char buf[10] = "hello";
  Connect(sockfd, &servaddr, sizeof(servaddr));
  Send(sockfd, buf, 10, 0);
  

}
