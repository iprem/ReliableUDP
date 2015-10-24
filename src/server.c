//A2 Server

#include "unp.h"
#include "unpifiplus.h"

// connect takes care of "asynchronous" error messages.
// asynchronous means sendto returns successfully message sent by sendto caused an error .
// Ex. Server not running
// pg 249

//recvfrom returns the *from sockaddr

//pselect is explained on pg 181
//shutdown 172

// race condition with SIGALRM must be avoided
// see pg 538
// optimal solution is to use pselect with ipc

//test by using random number generator to automatically drop packets 

//get_ifi_info discussed on pg 469

struct udp_sock_info{
  /*
    ALL ADDR STORED IN NETWORK BYTE ORDER
   */

  int sockfd;
  struct sockaddr * ifi_addr;    //IP address bound to the socket
  struct sockaddr * ifi_ntmaddr; //network mask for the IP address
  in_addr_t subnet_mask;        //subnet address (obtained by doing a bit-wise and between the IP address and its
                                //network mask)

};

#define MAX_IF_NUM 10
#define PREM_PORT 2038
#define LOCALHOST_PORT 8888

void bind_sock(int * arr, struct udp_sock_info * sock_info);
void print_sock_info(struct udp_sock_info * sock_info, int buff_size);
void bind_sock2(int * arr, struct udp_sock_info * sock_info);

int main(int argc, char ** argv){
  //maybe inintialize to -1
  int sock_fd_array[MAX_IF_NUM];
  int * sock_fd_array_iter = sock_fd_array;
  struct udp_sock_info udp_sock_info_arr[MAX_IF_NUM];
  struct udp_sock_info  * udp_sock_info_iter  = udp_sock_info_arr; 
  fd_set rset;
  int i, max;

  memset(sock_fd_array, 0, MAX_IF_NUM * sizeof(int));

  //initialize so we can loop and print later
  for(i = 0; i < MAX_IF_NUM; i++){
    (*udp_sock_info_iter).sockfd = -1;
    udp_sock_info_iter++;
  }

  //read arguments from server.in
  
  //bind all ip addrs to diff sockets. 
  //we only want unicast addrs. Get_ifi_info gets all interfaces
  bind_sock(sock_fd_array, udp_sock_info_arr);
  //print_sock_info(udp_sock_info_arr, MAX_IF_NUM);


  //GONNA WANNA DO THIS IN A LOOP SO MAKE SURE TO CLEAR THE RSET AT THE BOTTOM
  FD_ZERO(&rset);
  // stdout is 2
  max = 2;
  while((*sock_fd_array_iter) != -1)
    {
      if(*sock_fd_array_iter > max){
	max = *sock_fd_array_iter;
      }
      FD_SET(*sock_fd_array_iter, &rset);
      sock_fd_array_iter++;
    }
  sock_fd_array_iter = sock_fd_array;

  //not sure how pselect and signal race are related
  //read in book
  pselect(max + 1, &rset, 0, 0, 0, 0 );
  
  while(!FD_ISSET(*sock_fd_array_iter, &rset)){
    sock_fd_array_iter++;
  }
  
  printf("It worked! \n");
  char huge_buf[1024];
  Recv(*sock_fd_array_iter, huge_buf, 1024, 0);
  printf("%s", huge_buf);

  //inet_pton(AF_INET, "130.245.1.116", &(si->sin_addr.s_addr));
  //inet_pton(AF_INET, "130.245.1.181", &(si->sin_addr.s_addr));
  //inet_pton(AF_INET, "127.0.0.1", &(si->sin_addr.s_addr));

  return 0;    
}


void bind_sock(int * arr, struct udp_sock_info * sock_info){

  struct ifi_info	*ifi, *ifihead;
  struct sockaddr	*sa;
  struct sockaddr_in    *si;
  u_char		*ptr;
  int		i, family, doaliases;
  const int		on = 1;
  int sockfd;

  for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1);
       ifi != NULL; ifi = ifi->ifi_next) {
    

    sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
    
    // store fd in int array
    *arr = sockfd;
    arr++;

    Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    
    si = (struct sockaddr_in *) ifi->ifi_addr;
    si->sin_family = AF_INET;
    si->sin_port = htons(PREM_PORT);
    Bind(sockfd, (SA *) si, sizeof(*si));
    printf("bound %s\n", Sock_ntop((SA *) si, sizeof(*si)));

    
    printf("%s: ", ifi->ifi_name);
    if (ifi->ifi_index != 0)
      printf("(%d) ", ifi->ifi_index);
    printf("<");
    if (ifi->ifi_flags & IFF_UP)			printf("UP ");
    if (ifi->ifi_flags & IFF_LOOPBACK)		printf("LOOP ");
    if (ifi->ifi_flags & IFF_POINTOPOINT)	printf("P2P ");
    printf(">\n");

    if ( (i = ifi->ifi_hlen) > 0) {
      ptr = ifi->ifi_haddr;
      do {
	printf("%s%x", (i == ifi->ifi_hlen) ? "  " : ":", *ptr++);
      } while (--i > 0);
      printf("\n");
      }
    if (ifi->ifi_mtu != 0)
      printf("  MTU: %d\n", ifi->ifi_mtu);
    
    if ( (sa = ifi->ifi_addr) != NULL)
      printf("  IP addr: %s\n",
	     Sock_ntop_host(sa, sizeof(*sa)));
    
    
    if ( (sa = ifi->ifi_ntmaddr) != NULL)
      printf("  network mask: %s\n",
	     Sock_ntop_host(sa, sizeof(*sa)));
    
  }
  *arr = -1;
  free_ifi_info_plus(ifihead);
}

void bind_sock2(int * arr, struct udp_sock_info * sock_info){
  struct sockaddr_in	*sa;
  struct ifi_info       *ifi, *ifihead;
  const int		on = 1;
  int sockfd;


for (ifihead = ifi = Get_ifi_info(AF_INET, 1); ifi != NULL; ifi = ifi->ifi_next)
    {	
    
      /*4bind unicast address */
      sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
      
      // store fd in int array
      *arr = sockfd;
      arr++;
      
      Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    
      sa = (struct sockaddr_in *) ifi->ifi_addr;
      sa->sin_family = AF_INET;
      sa->sin_port = htons(SERV_PORT);
      Bind(sockfd, (SA *) sa, sizeof(*sa));
      printf("bound %s\n", Sock_ntop((SA *) sa, sizeof(*sa)));
      
      sock_info->sockfd = sockfd;
      sock_info->ifi_addr =  malloc(sizeof(struct sockaddr_in));
      *(sock_info->ifi_addr) =  *(ifi->ifi_addr);
      
      //printf("  IP addr: %s\n",
      //     Sock_ntop_host(sock_info->ifi_addr, sizeof(struct sockaddr_in)));

      sock_info->ifi_ntmaddr = malloc(sizeof(struct sockaddr_in));
      *(sock_info->ifi_ntmaddr) =  *(ifi->ifi_ntmaddr);

      
      int ip_addr = 0;
      ip_addr = ((struct sockaddr_in *)sock_info->ifi_addr)->sin_addr.s_addr;
      int net_mask = 0;
      net_mask = ((struct sockaddr_in *)sock_info->ifi_ntmaddr)->sin_addr.s_addr;
      sock_info->subnet_mask = ip_addr & net_mask;
  }
  
  free_ifi_info_plus(ifihead);
}

void print_sock_info(struct udp_sock_info * sock_info, int buff_size){
  int i;
  char ip_presentation [40];
  char net_mask_presentation[40];
  char sub_mask_presentation[40];

  for(i = 0; i < buff_size; i++)
    {
      if(sock_info->sockfd == -1){
	break;
      }
      else{
	// IP address, network mask, and subnet address
	inet_ntop(AF_INET, &((struct sockaddr_in *)sock_info->ifi_addr)->sin_addr, ip_presentation, sizeof(ip_presentation));
	inet_ntop(AF_INET, &((struct sockaddr_in *)sock_info->ifi_ntmaddr)->sin_addr, net_mask_presentation, sizeof(net_mask_presentation));
	inet_ntop(AF_INET, &(sock_info->subnet_mask), sub_mask_presentation, sizeof(sub_mask_presentation));
	
	printf("IP Addr: %s \n", ip_presentation);
	printf("Net Mask Addr: %s \n", net_mask_presentation);
	printf("Subnet Mask Addr: %s \n", sub_mask_presentation);

	sock_info++;
      }

    }
}

