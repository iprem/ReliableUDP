//A2 Server

#include "unp.h"
#include "unpifiplus.h"
#include "unprtt.h"
#include  <setjmp.h>


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
  /* ack window */
  int send_base;
  int send_end;

  int seq_num;

  /* congestion window  */
  int cwnd;
  int cwnd_linear_counter;


  int ssthresh;
  
  int last_ack_num;
  int dup_ack;
};

static struct timeout{

  //time in ms
  int timeout;

}persistent_timer;

static struct hdr {
  uint32_t	seq;	/* sequence # */
  uint32_t	ts;		/* timestamp when sent */
  int fin;
  int window_size;
  int probe;
} sendhdr, recvhdr;


#define MAX_IF_NUM 10
#define MAX_CONNECTED_PEERS 10
#define PREM_PORT 2038
#define LOCALHOST_PORT 8888
#define TRUE 1
#define FALSE 0
#define ACK_SIZE 100
#define RTT_DEBUG
#define PAYLOAD_SIZE    512

static int rttinit = 0;
static int fin_state = 0;
static struct rtt_info   rttinfo;
static sigjmp_buf	jmpbuf, jmpbuf_probe, jmpbuf_init_conn;


void bind_sock(int * arr, struct udp_sock_info * sock_info);
void print_sock_info(struct udp_sock_info * sock_info, int buff_size);
void print_sockaddr_in(struct sockaddr_in * to_print);
static void sig_alrm(int signo);
static void window_probe(int signo);
static void  init_conn_timer(int signo);
void timeout_init(struct rtt_info *ptr, struct timeout *mytimer);
int timeout_timeout(struct timeout *ptr);
struct itimerval timeout_start(struct timeout *timeout);
void send_probe(int fd, const SA *destaddr, socklen_t destlen);
void recv_probe(int fd);
void init_connection(struct s_conn * connection, int s_window_size);
int add_connected_peer(int fd , struct udp_sock_info * connected_peer_list, int size, int peer_num);
int remove_connected_peer(struct udp_sock_info * connected_peer_list, int connfd, int peer_num);
ssize_t server_send(int fd, const void *outbuff, size_t outbytes, 
		    const SA *destaddr, socklen_t destlen, struct s_conn *connection, int flag);
ssize_t Server_send(int fd, const void *outbuff,  size_t outbytes, 
		    const SA *destaddr, socklen_t destlen, struct s_conn *connection, int flag);


int main(int argc, char ** argv){

  int sock_fd_array[MAX_IF_NUM];
  int * sock_fd_array_iter = sock_fd_array;
  struct udp_sock_info udp_sock_info_arr[MAX_IF_NUM];
  struct udp_sock_info  * udp_sock_info_iter  = udp_sock_info_arr; 
  struct sockaddr_in  server, server_assigned;
  struct sockaddr_in client;
  struct sockaddr_in ack_client;  
  struct udp_sock_info  connected_peer_list[MAX_CONNECTED_PEERS];
  struct udp_sock_info  *connected_peer_iter = connected_peer_list;
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
  struct itimerval value;
  struct s_conn connection;
  int retry =0;  
  char ack_buf[1024];


  memset(sock_fd_array, 0, MAX_IF_NUM * sizeof(int));
  
  //initialize so we can loop and print later
  for(i = 0; i < MAX_IF_NUM; i++)
    {
      (*udp_sock_info_iter).sockfd = -1;
      udp_sock_info_iter++;
    }
  //reset iter
  udp_sock_info_iter = udp_sock_info_arr;

  memset(connected_peer_list, 0, MAX_CONNECTED_PEERS * sizeof(struct udp_sock_info));
  for(i=0; i < MAX_CONNECTED_PEERS; i++)
    {
      connected_peer_iter->sockfd = -1;
      connected_peer_iter++;
    }
  //reset iter
  connected_peer_iter = connected_peer_list;

  if(argc != 2)	err_quit("Usage: ./server server.in");
  
  fp = fopen(argv[1],"r");
  
  if(fp == NULL)
    {
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
    
    while(!FD_ISSET(*sock_fd_array_iter, &rset))
      {
	sock_fd_array_iter++;
      }

    //DO NOT FORK A NEW CHILD FOR AN ALREADY CONNECTED PEER
    if(in_connected_peers(*sock_fd_array_iter, connected_peer_list, MAX_CONNECTED_PEERS))
      {
	//char junkbuf[1024];
	//recv(*sock_fd_array_iter, junkbuf, 1024, 0);
	printf("Peer is already connected. \n"); 
	//printf("Throwing away following data.... \n");
	//printf("%s \n", junkbuf);

      }
    else
      {
	add_connected_peer(*sock_fd_array_iter, connected_peer_list, sizeof(struct sockaddr_in), MAX_CONNECTED_PEERS);
      }

    /*

    CHILD 

    */

    if((child = Fork()) == 0){

      //get ip and subnet destination by checking against stored fd/ip information
      while (*sock_fd_array_iter != udp_sock_info_iter->sockfd)
	{
	  udp_sock_info_iter++;
	}
      subnet_dest = udp_sock_info_iter->subnet_mask;
      ip_dest = udp_sock_info_iter->ifi_addr.sin_addr.s_addr;

      //get client info
      int readsize = 0;
      addr_len = sizeof(client);
      readsize =Recvfrom(*sock_fd_array_iter, file_name, 1024,0, (SA *) &client, &addr_len);
      
      file_name[readsize] = 0;
      printf("File name requested: %s\n",file_name);

      if (subnet_dest == subnet_dest & client.sin_addr.s_addr){
	local = TRUE;
	printf("Client is local. \n");
      }
      else{
	local= FALSE;
	printf("Client is not local. \n");
      }
  

      /*
	CONNECTION SETUP
      */


      //create new socket
      connfd = Socket(AF_INET, SOCK_DGRAM, 0);
      Setsockopt(connfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
      if(local == TRUE)
	{
	  Setsockopt(connfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on));
	}


      server.sin_addr.s_addr = udp_sock_info_iter->ifi_addr.sin_addr.s_addr;
      server.sin_port = 0;
      server.sin_family = AF_INET;
      Bind(connfd, (SA * ) &server, sizeof(struct sockaddr_in));
      getsockname(connfd, (SA *) &server_assigned, &addr_len);
      char server_ip_addr[40];
      inet_ntop(AF_INET, &server_assigned.sin_addr.s_addr, server_ip_addr, 40);

      printf("IP address after bind: %s \n", server_ip_addr);
      printf("Port after bind: %d \n", ntohs(server_assigned.sin_port));
      
      int server_port = ntohs(server_assigned.sin_port);

      Sendto(*sock_fd_array_iter, &server_port, sizeof(server_port),0, (SA *) &client, sizeof(client));

      init_connection(&connection, s_window_size);
      
      // ZERO OUT FOR DATA TRANSFER
      bzero(&sendhdr, sizeof(struct hdr));
      bzero(&recvhdr, sizeof(struct hdr));
      
      /*

      CONNECTED ON NEW FD

      */

      
      //Sendto(*sock_fd_array_iter, "Hi", sizeof(server_port),0, (SA *) &client, sizeof(client));
      
      fp1 = fopen(file_name,"r");
      if(fp1 == NULL){
	printf("\nError !! FILE NOT FOUND\n");
	exit(1);
      }
      
      int read_amount = 0;
      recvhdr.window_size = 10;
      while (!recvhdr.fin){
	
	Signal(SIGALRM, sig_alrm);

      sendagain:
	if (rttinit == 0) {
	  rtt_init(&rttinfo);		/* first time we're called */
	  rttinit = 1;
	  rtt_d_flag = 1;
	}

	printf("=============================================================== \n");
	printf("SEND INFORMATION. \n");
	printf("=============================================================== \n");

	timerclear(&value.it_value);
	setitimer(ITIMER_REAL, &value, NULL);

	value = rtt_start(&rttinfo);
	printf("RTO:  %d \n", rttinfo.rtt_rto);
	//printf("ALARM TIMEOUT SECONDS: %d \n", value.it_value.tv_sec);
	//printf("ALARM TIMEOUT IN MS: %d \n", (value.it_value.tv_usec / 1000));
	setitimer(ITIMER_REAL, &value, NULL);

	int cwnd_left = connection.cwnd;
	while((connection.seq_num <= connection.send_end) && recvhdr.window_size && cwnd_left){
	  read_amount = fread(payload, 1 , (sizeof(payload) -1), fp1);
	  if (read_amount == 0 && fin_state == 0)
	    {
	      //we have sent a fin
	      fin_state = 1;
	      payload[0] = '\0';
	      Server_send(connfd, payload, strlen(payload), (SA *) &client, sizeof(client), 
			  &connection, 1);
	      printf("Read amount: %d \n", read_amount);
	    }
	  else{
	    //more data needed so exit fin
	    fin_state = 0;
	    payload[sizeof(payload) -1 ] = 0;
	    Server_send(connfd, payload, strlen(payload), (SA *) &client, sizeof(client), 
			&connection, 0);
	    printf("FREAD AMOUNT: %d \n", read_amount);
	  }
	  cwnd_left--;
	}

	if(!cwnd_left)
	  {
	    printf("HIT CWND \n");
	    printf("CWND SIZE: %d \n", connection.cwnd);
	  }
	else if(!recvhdr.window_size)
	  {
	    printf("HIT RCWND \n");
	  }
	else if(connection.seq_num = connection.send_end)
	  {
	    printf("HIT SERVER WINDOW SIZE \n");
	    printf("SEND END: %d \n",  connection.send_end);
	    printf("SEND SEQ: %d \n",  connection.seq_num);
	  }

	//XX Right now this is a global timeout parameter not a packet specific timeout
	if(sigsetjmp(jmpbuf, 1) != 0) {
	  if (rtt_timeout(&rttinfo) < 0) {
	    err_msg("Too many timeouts: no response from server, giving up");
	    rttinit = 0;	/* reinit in case we're called again */
	    errno = ETIMEDOUT;
	    return(-1);
	  }
	  
	  /*
	    RETRANSMIT IF TIMEOUT OCCURS
	  */

	  //reset back to oldest unacked packet
	  connection.seq_num = connection.send_base;
	  sendhdr.seq = connection.send_base;
	  
	  //congestion avoidance
	  connection.ssthresh = connection.cwnd / 2;
	  connection.cwnd = 1;

	  //restart from position in file corresponding to packet that needs to be retransmitted
	  int offset  = connection.seq_num * sizeof(payload);
	  fseek(fp1, offset, SEEK_SET);

#ifdef	RTT_DEBUG
	  err_msg("timeout, retransmitting");
#endif
	  goto sendagain;
	}

	/*
	  PROCESS RECEIVED PACKETS FROM CLIENT
	*/
	
	while(n = server_recv(connfd))
	  {
	    

	    if (recvhdr.fin == 1)
	      {
		printf("Client has finished. \n");
		exit(0);
	      }
	      
	    /* 
	       DUPLICATE ACK 
	    */

	    if(recvhdr.seq == connection.last_ack_num)
	      {
		connection.dup_ack++;
		if (connection.dup_ack == 3)
		  {
		    //reset back to oldest unacked packet
		    connection.seq_num = connection.send_base;
		    sendhdr.seq = connection.send_base;
		  
		    //congestion avoidance
		    connection.ssthresh = connection.cwnd / 2;
		    connection.cwnd = connection.ssthresh;
		    
		  //restart from position in file corresponding to packet that needs to be retransmitted
		    int offset  = connection.seq_num * sizeof(payload);
		    fseek(fp1, offset, SEEK_SET);
		    
		    goto sendagain;
		  }
	      }
	    
	    //NEW ACK
	    if (recvhdr.seq > connection.send_base)
	      {
		printf("=============================================================== \n");
		printf("New ack received. \n");
		printf("=============================================================== \n");
		
		timerclear(&value.it_value);
		setitimer(ITIMER_REAL, &value, NULL);
				
		//update ack window
		connection.send_base = recvhdr.seq;
		connection.send_end = connection.send_base + recvhdr.window_size;
		
		//reset duplicate ack counter
		connection.dup_ack = 0;
		
		//increment cwnd
		//if you increment cwnd endlessly you could have an integer overflow
		if(connection.cwnd < connection.ssthresh){ 
		  if (connection.cwnd < s_window_size)
		    {
		      connection.cwnd++;
		    }
		  printf("Connection is in slow start. \n");
		  printf("CWND Parameter: %d \n", connection.cwnd);
		  printf("SSTHRESH Parameter: %d \n", connection.ssthresh);
		}
		//increment cwnd if linear
		else if(connection.cwnd >= connection.ssthresh){
		  printf("Connection is in congestion avoidance. \n");
		  printf("CWND Parameter: %d \n", connection.cwnd);
		  printf("SSTHRESH Parameter: %d \n", connection.ssthresh);
		  
		  connection.cwnd_linear_counter++;
		  if(connection.cwnd_linear_counter == connection.cwnd)
		    {
		      if (connection.cwnd < s_window_size)
			{
			  connection.cwnd++;
			}
		      connection.cwnd_linear_counter = 0;
		    }
		}
		
		//reset retransmit num
		rtt_newpack(&rttinfo);
		
		//record seq num for comparison to next ack
		connection.last_ack_num = recvhdr.seq;

		break;
		
	      }

	    /*
	      RECV WINDOW 
	    */
	    
	    //send probe forever until window size opens up
	    if(!recvhdr.window_size)
	      {

		printf("=============================================================== \n");
		printf("RECV Window is zero. \n");
		printf("Begin probe.\n");
		printf("=============================================================== \n");

		timeout_init(&rttinfo, &persistent_timer);
		printf("Sending window probe...\n");
		Signal(SIGALRM, window_probe);
	      send_probe:

		value = timeout_start(&persistent_timer);
		setitimer(ITIMER_REAL, &value, NULL);
		
		printf(" PROBE TIMEOUT SECONDS FIELD: %d \n", value.it_value.tv_sec);
		printf(" PROBE TIMEOUT MS FIELD: %d \n", (value.it_value.tv_usec / 1000));
		
		//timeout
		if(sigsetjmp(jmpbuf_probe, 1) != 0)
		  {
		    printf("PROBE TIMEOUT \n");
		    timeout_timeout(&persistent_timer);
		    goto send_probe;
		  }
		
		//send probe
		send_probe(connfd, (SA *) &client, sizeof(client));
		//recv		 
		//window size is still 0
		while(recvhdr.window_size == 0)
		  {
		    recv_probe(connfd);
		  }
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


void
init_connection(struct s_conn *connection, int s_window_size)
{
  
  connection->send_base = 0;
  connection->send_end = connection->send_base + s_window_size;
  connection->dup_ack = 0;
  connection->cwnd = 1;
  connection->cwnd_linear_counter = 1;
  connection->ssthresh = s_window_size / 2;

}

int in_connected_peers(int fd, struct udp_sock_info * connected_peer_list, int peer_num)
{
  struct sockaddr_in client;
  int i;
  socklen_t len = sizeof(client);
  char junkbuf[1024];
  

  //memset(&peer, 0, sizeof(struct sockaddr_in));
  Recvfrom(fd, junkbuf, 1024 , MSG_PEEK, (SA *) &client, &len);
  //Recvfrom(fd, junkbuf, 1024 , 0, (SA *) &client, &len);

  
  for(i=0; i < peer_num; i++)
    {
      if (ntohl(client.sin_addr.s_addr) == ntohl(connected_peer_list->ifi_addr.sin_addr.s_addr))
	{
	  return 1;
	}
      connected_peer_list++;
    }
  
  return 0;
  
}
 
int
add_connected_peer(int fd , struct udp_sock_info * connected_peer_list, int size, int peer_num){
  struct udp_sock_info * connected_peer_iter = connected_peer_list; 
  int err = 1;
  int i; 
  char junkbuf[10];
  

  for(i=0; i < peer_num; i++)
    {
     if( connected_peer_iter->sockfd = -1)
       {
	 Recvfrom(fd, junkbuf, 10 , MSG_DONTWAIT | MSG_PEEK, (SA *) &(connected_peer_iter->ifi_addr), &size );	 
	 printf("Adding peer...\n");
	 err = 0;
	 break;
       }
      connected_peer_iter++;
    }

  return err;
}

int
remove_connected_peer(struct udp_sock_info * connected_peer_list, int connfd, int peer_num){

  struct udp_sock_info  * connected_peer_iter = connected_peer_list; 
  int err = 1;
  int i;

  for(i=0; i < peer_num; i++)
    {
     if( connected_peer_iter->sockfd = connfd)
       {
	 memset(connected_peer_iter, 0, sizeof(struct udp_sock_info));
	 err = 0;
	 break;
       }
      connected_peer_iter++;
    }

  return err;
}




void
timeout_init(struct rtt_info *ptr, struct timeout *mytimer)
{
  mytimer->timeout  = ptr->rtt_rto;
}

struct itimerval 
timeout_start( struct timeout *mytimer)
{
  struct itimerval timerval; 

  // You cannot specify more than one second in microseconds
  timerval.it_value.tv_sec = mytimer->timeout / 1000;
  // put leftover time in microsecond field
  // remember, we are working with milliseconds
  timerval.it_value.tv_usec = (mytimer->timeout % 1000) * 1000;
  
  timerval.it_interval.tv_sec = 0;
  timerval.it_interval.tv_usec = 0;

  return timerval;
}

int 
timeout_timeout(struct timeout *ptr)
{
  ptr->timeout *= 2;		/* next RTO */
  
  return(0);
}

static void 
init_conn_timer(int signo)
{
  siglongjmp(jmpbuf_init_conn, 1);
 }


static void 
window_probe(int signo)
{
  printf("window probe called \n");
  siglongjmp(jmpbuf_probe, 1);
 
}


static void 
sig_alrm(int signo)
{

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

  //printf("Data received: %s \n", inbuff);
  rtt_stop(&rttinfo, rtt_ts(&rttinfo) - recvhdr.ts);
  
  //set retransmit number


}

void send_probe(int fd, const SA *destaddr, socklen_t destlen)
{

  static struct msghdr msgsend;
  struct iovec iovsend[1];

  msgsend.msg_name = destaddr;
  msgsend.msg_namelen = destlen;
  msgsend.msg_iov = iovsend;
  msgsend.msg_iovlen = 1;
  
  sendhdr.probe = 1;
  iovsend[0].iov_base = &sendhdr;
  iovsend[0].iov_len = sizeof(struct hdr);

  Sendmsg(fd, &msgsend, 0);

  sendhdr.probe = 0;
}

void recv_probe(int fd)
{
  static struct msghdr msgrecv;
  struct iovec iovrecv[1];
  int n;

  msgrecv.msg_name = NULL;
  msgrecv.msg_namelen = 0;
  msgrecv.msg_iov = iovrecv;
  msgrecv.msg_iovlen = 1;
  
  iovrecv[0].iov_base = &recvhdr;
  iovrecv[0].iov_len = sizeof(struct hdr);

  n = Recvmsg(fd, &msgrecv, 0);

}

ssize_t Server_send(int fd, const void *outbuff,  size_t outbytes, 
		    const SA *destaddr, socklen_t destlen, struct s_conn *connection, int flag)
{

  ssize_t n;

  n = server_send(fd, outbuff, outbytes, destaddr, destlen, connection, flag);

  if (n < 0)
    err_quit("server_send error");
  
  return(n);


}

ssize_t server_send(int fd, const void *outbuff, size_t outbytes, 
		    const SA *destaddr, socklen_t destlen, struct s_conn *connection, int flag)
{

  ssize_t			n;
  struct iovec	iovsend[2];
  static struct msghdr msgsend;

  //increment where we are in sender window
  sendhdr.seq++;
  connection->seq_num = sendhdr.seq;
  //decrement how much space recvwindow has
  recvhdr.window_size--;


  msgsend.msg_name = destaddr;
  msgsend.msg_namelen = destlen;
  msgsend.msg_iov = iovsend;
  msgsend.msg_iovlen = 2;
  iovsend[0].iov_base = &sendhdr;
  iovsend[0].iov_len = sizeof(struct hdr);
  iovsend[1].iov_base = outbuff;
  iovsend[1].iov_len = outbytes;

#ifdef	RTT_DEBUG
  fprintf(stderr, "send %4d \n", sendhdr.seq);
#endif
  
  sendhdr.ts = rtt_ts(&rttinfo);

  if(flag == 1){
    sendhdr.fin = 1;
  }

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

