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
  struct sockaddr_in ifi_addr;    //IP address bound to the socket
  struct sockaddr_in ifi_ntmaddr; //network mask for the IP address
  in_addr_t subnet_mask;        //subnet address (obtained by doing a bit-wise and between the IP address and its
                                //network mask)

};

#define MAX_IF_NUM 10
#define PREM_PORT 2038
#define LOCALHOST_PORT 8888

void bind_sock(int * arr, struct udp_sock_info * sock_info);
void print_sock_info(struct udp_sock_info * sock_info, int buff_size);

int main(int argc, char ** argv){
  //maybe inintialize to -1
  int sock_fd_array[MAX_IF_NUM];
  int * sock_fd_array_iter = sock_fd_array;
  struct udp_sock_info udp_sock_info_arr[MAX_IF_NUM];
  struct udp_sock_info  * udp_sock_info_iter  = udp_sock_info_arr; 
  fd_set rset;
  int port, window, i, max;
  FILE *fp;
  pid_t child;

  memset(sock_fd_array, 0, MAX_IF_NUM * sizeof(int));

  //initialize so we can loop and print later
  for(i = 0; i < MAX_IF_NUM; i++){
    (*udp_sock_info_iter).sockfd = -1;
    udp_sock_info_iter++;
  }

  if(argc != 2)	err_quit("Usage: ./server server.in");
  
  fp = fopen(argv[1],"r");
  
  if(fp == NULL){
    printf("Error opening file\n");
    exit(1);
  }
  

  //read arguments from server.in
  if(fscanf(fp,"%d\n",&port) == 0)	err_quit("No port number found in the file\n");
  if(fscanf(fp,"%d\n",&window) == 0)	err_quit("No window size found in the file\n");
  
  //bind all ip addrs to diff sockets. 
  //we only want unicast addrs. Get_ifi_info gets all interfaces
  bind_sock(sock_fd_array, udp_sock_info_arr);
  print_sock_info(udp_sock_info_arr, MAX_IF_NUM);


  while(1){

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

    if(Fork() == 0){
      while

    }
  }


    /*    
	  printf("It worked! \n");
	  char huge_buf[1024];
	  Recv(*sock_fd_array_iter, huge_buf, 1024, 0);
	  printf("%s", huge_buf);
    */

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

  printf("\n");
  printf("Binding addresses.... \n");
  printf("============================================================ \n");
    

  for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1);
       ifi != NULL; ifi = ifi->ifi_next) {
    

    sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
    
    // store fd in int array
    *arr = sockfd;
    arr++;

    Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    //Create sockaddr to bind
    si = (struct sockaddr_in *) ifi->ifi_addr;
    si->sin_family = AF_INET;
    si->sin_port = htons(PREM_PORT);
    Bind(sockfd, (SA *) si, sizeof(*si));
    printf("bound %s\n", Sock_ntop((SA *) si, sizeof(*si)));

    //Add to udp_sock_info array
    sock_info->sockfd = sockfd;
    sock_info->ifi_addr = *si;
    sock_info->ifi_ntmaddr = *((struct sockaddr_in *) ifi->ifi_ntmaddr);
    sock_info->subnet_mask =     sock_info->ifi_addr.sin_addr.s_addr & sock_info->ifi_ntmaddr.sin_addr.s_addr;      
    sock_info++;

    /*
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
    */
    
  }
  //Put boundary of -1 in array so that we know where legitimate file descriptors end
  *arr = -1;
  free_ifi_info_plus(ifihead);
}


void print_sock_info(struct udp_sock_info * sock_info, int buff_size){
  int i;
  char ip_presentation [40];
  char net_mask_presentation[40];
  char sub_mask_presentation[40];
  
  printf("\n");
  printf("Recorded Data \n");
  printf("============================================================ \n");

  for(i = 0; i < buff_size; i++)
    {
      if(sock_info->sockfd == -1){
	break;
      }
      else{
	// IP address, network mask, and subnet address
	
	inet_ntop(AF_INET, &(sock_info->ifi_addr.sin_addr.s_addr), ip_presentation, sizeof(ip_presentation));
	inet_ntop(AF_INET, &(sock_info->ifi_ntmaddr.sin_addr.s_addr), net_mask_presentation, sizeof(net_mask_presentation));
	inet_ntop(AF_INET, &(sock_info->subnet_mask), sub_mask_presentation, sizeof(sub_mask_presentation));
	

	printf("File Descriptor: %d \n", sock_info->sockfd );
	printf("IP Addr: %s \n", ip_presentation);
	printf("Net Mask Addr: %s \n", net_mask_presentation);
	printf("Subnet Mask Addr: %s \n", sub_mask_presentation);

	printf("\n");

	sock_info++;
      }

    }
}

