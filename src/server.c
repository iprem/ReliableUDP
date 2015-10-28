//A2 Server

#include "unp.h"
#include "unpifiplus.h"
#include "unprtt.h"
#include  <setjmp.h>

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

struct s_conn {
  int send_base;
  int send_end;
  int seq_num;

  int dup_ack;
};


static struct hdr {
  uint32_t	seq;	/* sequence # */
  uint32_t	ts;		/* timestamp when sent */
  int fin;
  int window_size;
} sendhdr, recvhdr;


#define MAX_IF_NUM 10
#define PREM_PORT 2038
#define LOCALHOST_PORT 8888
#define TRUE 1
#define FALSE 0
#define ACK_SIZE 100
#define RTT_DEBUG
#define PAYLOAD_SIZE    20    
static int rttinit = 0;
static struct rtt_info   rttinfo;
static sigjmp_buf	jmpbuf;


void bind_sock(int * arr, struct udp_sock_info * sock_info);
void print_sock_info(struct udp_sock_info * sock_info, int buff_size);
void print_sockaddr_in(struct sockaddr_in * to_print);
static void sig_alrm(int signo);

int main(int argc, char ** argv){
  //maybe inintialize to -1
  int sock_fd_array[MAX_IF_NUM];
  int * sock_fd_array_iter = sock_fd_array;
  struct udp_sock_info udp_sock_info_arr[MAX_IF_NUM];
  struct udp_sock_info  * udp_sock_info_iter  = udp_sock_info_arr; 
  struct sockaddr_in  server, server_assigned;
  struct sockaddr_in client;
  fd_set rset;
  int port, s_window_size, i, max, local, connfd, numRead;
  const int on = 1;
  FILE *fp, *fp1;
  pid_t child = -1;
  in_addr_t ip_dest, subnet_dest;
  socklen_t addr_len;
  char file_name[30], buffer[4096], recv_ack[512], payload[PAYLOAD_SIZE-sizeof(struct hdr)];
  ssize_t n;
  recvhdr.fin = 0;

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
  if(fscanf(fp,"%d\n",&s_window_size) == 0)	err_quit("No window size found in the file\n");

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
    //Reset iterator
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
      struct s_conn connection;
      connection.send_base = 0;
      connection.send_end = connection.send_base + recvhdr.window_size;
      connection.dup_ack = 0;
      
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
	CONNECTION SETUP
      */

      Signal(SIGALRM, sig_alrm);
      
      int read_amount = 0;
      //fread(buffer, 512, 1, fp1);
      while (!recvhdr.fin){

      sendagain:
	if (rttinit == 0) {
	  rtt_init(&rttinfo);		/* first time we're called */
	  rttinit = 1;
	  rtt_d_flag = 1;
	}
	
	struct itimerval value = rtt_start(&rttinfo);
	printf("RTO %d \n", rttinfo.rtt_rto);
	printf(" ALARM TIMEOUT SECONDS: %d \n", value.it_value.tv_sec);
	printf(" ALARM TIMEOUT IN MS: %d \n", (value.it_value.tv_usec / 1000));
	setitimer(ITIMER_REAL, &value, NULL);
	//XX
	//Currently just sends buffer without new read until it hits send_end
	while(connection.seq_num <= connection.send_end){
	  read_amount = fread(payload, (sizeof(payload) -1) , 1, fp1);
	  payload[sizeof(payload) -1 ] = 0;
	  Server_send(connfd, payload, strlen(payload), (SA *) &client, sizeof(client), 
		      &connection);

	}
	

	//XX Right now this is a global timeout parameter not a packet specific timeout
	if(sigsetjmp(jmpbuf, 1) != 0) {
	  if (rtt_timeout(&rttinfo) < 0) {
	    err_msg("dg_send_recv: no response from server, giving up");
	    rttinit = 0;	/* reinit in case we're called again */
	    errno = ETIMEDOUT;
	    return(-1);
	  }
	  
	  /*
	    RETRANSMIT
	  */
	  //reset back to oldest unacked packet
	  connection.seq_num = connection.send_base;
	  sendhdr.seq = connection.send_base;
	  //restart from position in file corresponding to packet that needs to be retransmitted
	  int offset  = connection.seq_num * sizeof(payload);
	  fseek(fp1, offset, SEEK_SET);
#ifdef	RTT_DEBUG
	  err_msg("dg_send_recv: timeout, retransmitting");
#endif
	  goto sendagain;
	}	

	while(n = server_recv(connfd))
	  {
	    if (recvhdr.seq > connection.send_base){
	      connection.send_base = recvhdr.seq;
	      connection.send_end = connection.send_base + recvhdr.window_size;
	      //reset retransmit num
	      rtt_newpack(&rttinfo);
	      printf("ACK Received.");
	      break;
	    }
	  }
	

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

  return 0;    
}

static void 
sig_alrm(int signo)
{
  printf("sig_alrm called \n");
  siglongjmp(jmpbuf, 1);
 
}

ssize_t server_recv(int fd)
{
  
  ssize_t			n;
  static struct msghdr msgrecv;
  struct iovec  iovrecv[2];
  char inbuff[1024];

  msgrecv.msg_name = NULL;
  msgrecv.msg_namelen = 0;
  msgrecv.msg_iov = iovrecv;
  msgrecv.msg_iovlen = 1;
  iovrecv[0].iov_base = &recvhdr;
  iovrecv[0].iov_len = sizeof(struct hdr);
  iovrecv[1].iov_base = inbuff;
  iovrecv[1].iov_len = sizeof(inbuff);

  
  n = Recvmsg(fd, &msgrecv, 0);
  printf("INBUFFER!!! : %s", inbuff);

  rtt_stop(&rttinfo, rtt_ts(&rttinfo) - recvhdr.ts);

  //set duplicate ack field
  //set retransmit number


}


ssize_t Server_send(int fd, const void *outbuff,  size_t outbytes, 
		    const SA *destaddr, socklen_t destlen, struct s_conn *connection)
{

  ssize_t n;

  n = server_send(fd, outbuff, outbytes, destaddr, destlen, connection);

  if (n < 0)
    err_quit("server_send error");
  
  return(n);


}

ssize_t server_send(int fd, const void *outbuff, size_t outbytes, 
		    const SA *destaddr, socklen_t destlen, struct s_conn *connection)
{

  ssize_t			n;
  struct iovec	iovsend[2];
  static struct msghdr msgsend;

  //increment where we are in receiver window
  sendhdr.seq++;
  connection->seq_num = sendhdr.seq;

  sendhdr.seq++;
  msgsend.msg_name = destaddr;
  msgsend.msg_namelen = destlen;
  msgsend.msg_iov = iovsend;
  msgsend.msg_iovlen = 2;
  iovsend[0].iov_base = &sendhdr;
  iovsend[0].iov_len = sizeof(struct hdr);
  iovsend[1].iov_base = outbuff;
  iovsend[1].iov_len = outbytes;
  
  printf("OUTBUF: %s \n", outbuff);

#ifdef	RTT_DEBUG
  fprintf(stderr, "send %4d: ", sendhdr.seq);
#endif
  
  sendhdr.ts = rtt_ts(&rttinfo);
  Sendmsg(fd, &msgsend, 0);

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

