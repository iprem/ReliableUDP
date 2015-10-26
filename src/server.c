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

/*
struct s_conn {
  
  // send window 
  int send_base = 0;
  int send_end = 0;

  //Fast Retransmit
  int duplicate_ack = 0;
  
};
*/

#define MAX_IF_NUM 10
#define PREM_PORT 2038
#define LOCALHOST_PORT 8888
#define TRUE 1
#define FALSE 0
void bind_sock(int * arr, struct udp_sock_info * sock_info);
void print_sock_info(struct udp_sock_info * sock_info, int buff_size);
void print_sockaddr_in(struct sockaddr_in * to_print);
ssize_t Dg_send_recv(int, const void *, size_t, void *, size_t, const SA *, socklen_t);

int main(int argc, char ** argv){
  //maybe inintialize to -1
  int sock_fd_array[MAX_IF_NUM];
  int * sock_fd_array_iter = sock_fd_array;
  struct udp_sock_info udp_sock_info_arr[MAX_IF_NUM];
  struct udp_sock_info  * udp_sock_info_iter  = udp_sock_info_arr; 
  struct sockaddr_in  server, server_assigned;
  struct sockaddr_in client;
  fd_set rset;
  int port, window, i, max, local, connfd, numRead;
  const int on = 1;
  FILE *fp, *fp1;
  pid_t child = -1;
  in_addr_t ip_dest, subnet_dest;
  socklen_t addr_len;
  char file_name[30], buffer[512], recv_ack[512];
  
  memset(sock_fd_array, 0, MAX_IF_NUM * sizeof(int));

  //initialize so we can loop and print later
  for(i = 0; i < MAX_IF_NUM; i++){
    (*udp_sock_info_iter).sockfd = -1;
    udp_sock_info_iter++;
  }
  udp_sock_info_iter = udp_sock_info_arr;


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

  FD_ZERO(&rset);
  while(1){
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
    //reset iterator
    sock_fd_array_iter = sock_fd_array;
    
    //not sure how pselect and signal race are related
    //read in book
    pselect(max + 1, &rset, 0, 0, 0, 0 );
    
    while(!FD_ISSET(*sock_fd_array_iter, &rset)){
      sock_fd_array_iter++;
    }
    

    /*

    CHILD 

    */

    if((child = Fork()) == 0){
      //struct s_conn connection;
      
      //get ip and subnet destination by checking against stored fd/ip information
      while (*sock_fd_array_iter != udp_sock_info_iter->sockfd)
	{
	  udp_sock_info_iter++;
	}
      subnet_dest = udp_sock_info_iter->subnet_mask;
      ip_dest = udp_sock_info_iter->ifi_addr.sin_addr.s_addr;

      //get client info
      int readsize = 0;
      readsize =Recvfrom(*sock_fd_array_iter, file_name, 1024,0, (SA *) &client, &addr_len);
      file_name[readsize] = 0;
      printf("File name requested: %s\n",file_name);

      printf("AFTER FILE NAME \n");

      if (subnet_dest == subnet_dest & client.sin_addr.s_addr){
	local = TRUE;
	printf("Client is local. \n");
      }
      else{
	local= FALSE;
	printf("Client is not local. \n");
      }
  
      //create new socket
      connfd = Socket(AF_INET, SOCK_DGRAM, 0);
      Setsockopt(connfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
      if(local == TRUE){
	Setsockopt(connfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on));
      }
      server.sin_addr.s_addr = udp_sock_info_iter->ifi_addr.sin_addr.s_addr;
      server.sin_port = 0;
      server.sin_family = AF_INET;
      Bind(connfd, (SA * ) &server, sizeof(server));

      getsockname(connfd, (SA *) &server_assigned, &addr_len);
      char server_ip_addr[40];
      inet_ntop(AF_INET, &server_assigned.sin_addr.s_addr, server_ip_addr, 40);
      printf("IP address after bind: %s \n", server_ip_addr);
      printf("Port after bind: %d \n", ntohs(server_assigned.sin_port));
      

      int server_port = ntohs(server_assigned.sin_port);
      Sendto(*sock_fd_array_iter, &server_port, sizeof(server_port),0, (SA *) &client, sizeof(client));
      
      /*

      SEND ON NEW CONNFD

      */

      //connection.send_end = connection.send_base + window; 
      //send and read 
      //send packets until you hit window end
      //for every ack, you increment connection.send_base and window

      /*ERROR */
      
      //if window locks
      //set persistence timer
      //if timer expires use window probes
      //use exponential backoff for window probes
      //keep sending until acked

      //if timeout, you resend not yet acked segment and reset timer
      
      Sendto(*sock_fd_array_iter, "Hi", sizeof(server_port),0, (SA *) &client, sizeof(client));
      
      fp1 = fopen(file_name,"r");
      if(fp1 == NULL){
	printf("\nError !! FILE NOT FOUND\n");
	exit(1);
      }
	
      /*
      fseek(fp, 0, SEEK_END);
      int fileSize = ftell(fp);
      fseek(fp, 0, SEEK_SET);
      printf("FileSize %d \n", fileSize);
      */
      
      int read_amount = 0;
      printf("FILE CONTENTS \n");
      char newbuf[10] = "whatup g";
      while (read_amount = fread(buffer, 512, 1, fp1)){
	Dg_send_recv(connfd, buffer, strlen(buffer), recv_ack, MAXLINE, (SA *) &client, sizeof(client));
      }

      /*
	
      END

      */      
      //reset for if statement clause
      udp_sock_info_iter = udp_sock_info_arr;
      
      exit(0);
    }

    //for while loop
    FD_CLR(*sock_fd_array_iter, &rset);
    sock_fd_array_iter = sock_fd_array;
  }


    /*    
	  printf("It worked! \n");
	  char huge_buf[1024];
	  Recv(*sock_fd_array_iter, huge_buf, 1024, 0);
	  printf("%s", huge_buf);
    */

  return 0;    
}


void print_sockaddr_in(struct sockaddr_in * to_print)
{
  char ip_addr[40];
  int port;
  
  inet_ntop(AF_INET, &(to_print->sin_addr.s_addr), ip_addr, sizeof(*to_print));
  port = ntohs(to_print->sin_port);
	
  printf("IP Addr: %s \n", ip_addr);
  printf("Port: %d \n", port);

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

