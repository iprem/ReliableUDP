# include "unp.h"
# include "unpifiplus.h"

#define SERVER_PORT 2038	/*Change server port here*/
#define DOALIASES 1
#define MAXLENGTH 30
#define RETRY 3

void print(struct sockaddr_in *servaddr);
uint32_t parseIPV4string(char* ipAddress);	/*Function for parsing the IP address*/
static void recvfrom_alarm(int signo);

static int pipefd[2];

int main(int argc, char **argv){
	
	int sockfd, sockfd2, port, win_size, seed, mean, doaliases, i=0, family, n, serv_port, len, servlen, maxfd1;
	const int on = 1;
	struct ifi_info *ifi;
	struct sockaddr	*sa;
	u_char		*ptr;
	doaliases = DOALIASES;
	fd_set rset;
	float prob,p;
	int bool = 0;
	struct msghdr msg;
	uint32_t serverip, clientip[MAXALIASES], netmask[MAXALIASES];
	char IPserver[16], IPclient[MAXALIASES][16], ClientIP[16], IP[16], file[MAXLENGTH], buff[MAXLINE+1];
	struct sockaddr_in servaddr, cliaddr, peer;
	char send_ack[MAXLINE+1] = "ACK";
     
	
	if(argc != 2)	err_quit("Usage: udpcli client.in");
	
	FILE *fp = fopen(argv[1],"r");		/*Open the file passed as argument in read mode*/
	
	if(fp == NULL){				/*Check if file passed is valid*/
		printf("Error opening file\n");
		exit(1);
	}
	
	bzero(&servaddr, sizeof(servaddr));
	
	/*Check if the file client.in has all required values*/
	if(fscanf(fp,"%s\n",IPserver) == 0)	err_quit("No IP adrress found in the file\n");
	if(fscanf(fp,"%d\n",&port) == 0)	err_quit("No port number found in the file\n");
	if(fscanf(fp,"%s\n",file) == 0)		err_quit("No filename found in the passed file\n");
	if(fscanf(fp,"%d\n",&win_size) == 0)	err_quit("No receiving sliding-window size found\n");
	if(fscanf(fp,"%d\n",&seed) == 0)	err_quit("No random generator seed value found\n");
	if(fscanf(fp,"%f\n",&prob) == 0)	err_quit("No value for probablity of datagram loss found\n");
	if(fscanf(fp,"%d\n",&mean) == 0)	err_quit("No mean controlling rate found\n");
	
	if(prob<0.0 || prob>1.0)		err_quit("Probability should be in range[0,1]\n");
	if( (win_size < 0) || (mean < 0) )	err_quit("Window size or mean controlling rate should be positive\n");
	
	//print(&servaddr);
	
	family = AF_INET;

	/*Print information of all interfaces*/	
	//WHERE IS THIS FUNCTION?
	//prifinfo_plus(family, doaliases);

	ifi = Get_ifi_info_plus(family, doaliases);
	while((ifi = ifi->ifi_next) != NULL ){
		sa = ifi->ifi_addr;
		strcpy(IPclient[i], Sock_ntop_host(sa, sizeof(*sa)));
		clientip[i] = parseIPV4string(IPclient[i]);
		sa = ifi->ifi_ntmaddr;
		netmask[i++] = parseIPV4string(Sock_ntop_host(sa, sizeof(*sa)));	
	}

	serverip = parseIPV4string(IPserver);

	while(--i > 0){
		if((strcmp(IPserver,IPclient[i]) == 0) || (strcmp(IPserver,"127.0.0.1") == 0) ){
			printf("\nThe server is on the same host and therfore loopback address will be used \n");
			strcpy(IPserver,"127.0.0.1"); 
			strcpy(ClientIP,"127.0.0.1");
			bool = 1;
			break;
		}
		else if((serverip & netmask[i]) == (clientip[i] & netmask[i])){
			strcpy(ClientIP,IPclient[i]);
			printf("\nServer is on same subnet as client\n"); bool =1;
			break;
		}
	}
	if( bool == 0 ) /*set IP address of client to primary address of client if server is not on same host*/
		printf("\nServer and client are not on same subnet\n");
		strcpy(ClientIP,IPclient[1]);

	printf("Server address: %s\nClient address: %s\n", IPserver, ClientIP);

/*--------------------------------------------------------------------------------------------------------------*/	
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(SERVER_PORT);
	Inet_pton(AF_INET, IPserver, &servaddr.sin_addr.s_addr);

	cliaddr.sin_family = AF_INET;
	cliaddr.sin_port = htons(0);
	Inet_pton(AF_INET, ClientIP, &cliaddr.sin_addr.s_addr);

	/*Create UDP socket, bind it and use Getsockname() */
	sockfd = Socket(AF_INET, SOCK_DGRAM , 0);
	len = sizeof(cliaddr);
	Bind(sockfd, (SA *)&cliaddr, len);
	Getsockname(sockfd, (SA*)&cliaddr, &len);

	Inet_ntop(AF_INET, &(cliaddr.sin_addr), IP, INET_ADDRSTRLEN);
	printf("\nGetsockname Results: \nClient address: %s\nClient port number: %d\n", IP, ntohs(cliaddr.sin_port));

	Connect(sockfd, (SA *)&servaddr, sizeof(servaddr));
	
	Getpeername(sockfd, (SA*)&peer, &len);
	Inet_ntop(AF_INET, &(peer.sin_addr), IP, INET_ADDRSTRLEN);
	printf("\nGetpeername Results: \nPeer address: %s\nPeer port number: %d\n", IP, ntohs(peer.sin_port));
	
/*--------------------------------------------------------------------------------------------------------------*/	
	
	Pipe(pipefd);
	maxfd1 = max(sockfd, pipefd[0]) + 1;
	servlen = sizeof(servaddr);
	FD_ZERO(&rset);
	
   	Signal(SIGALRM, recvfrom_alarm);
	srand(seed);	/*Initialize random number generator*/
	i = 0;
		
 L1:	if( i > RETRY ){
		printf("\nServer not responding...Giving up !!!\n");
		exit(1);
		}
	if((p = (rand() % 100)/100.0) > prob ){
	  printf("\n%f \t %f\n", p, prob); 
	  Send(sockfd, file, strlen(file), 0);
	}
	else{
	  printf("\nThe packet has been dropped. Retrying... \n");
	  goto L1;
	}
	//This alarm protects the initial recvfrom for port number
	alarm(2);	/*Set timeout as 2 secs*/
	for ( ; ;){
		FD_SET(sockfd, &rset);
		FD_SET(pipefd[0], &rset);
		if( (n = select(maxfd1, &rset, NULL, NULL, NULL)) <0 ){
			if(errno == EINTR)
				continue;
		else
			err_sys("\nselect error\n");
		}
		if(FD_ISSET(sockfd, &rset)){
			len = servlen;
			n = Recv(sockfd, &serv_port, len, 0);		/*Port number received*/
			//serv_port = ntohs(serv_port);
			printf("Port number received: %d\n",serv_port);
			printf("Binding server on new port %d ... \n", serv_port);
			port = ntohs(servaddr.sin_port);
			servaddr.sin_port = htons(serv_port);
			Connect(sockfd, (SA *) &servaddr, sizeof(servaddr));

			// Get acknowledgement of new connection
			n = Recv(sockfd, buff, len, 0);
			if(n > 0){
				printf("Acknowledgement received on new connection: %s\n",buff);
			}

			printf("MY FILE NAME: %s  \n", file);
			char mynewbuf[1024];
			//while( (n = Dg_send_recv(sockfd, send_ack, sizeof(send_ack), buff, sizeof(buff), (SA *) &servaddr, sizeof(servaddr))) > 0 ){
			//while( (n = Dg_send_recv(sockfd, send_ack, sizeof(send_ack), buff, sizeof(buff), 0, 0)) > 0 ){
			static struct msghdr send;
			struct iovec iovsend[2];
			struct hdr{
			  uint32_t seq;
			  uint32_t ts;
			  } sendhdr;
			char inbuff[1024];
			while(1){
			  send.msg_name = 0;
			  send.msg_namelen = 0;
			  send.msg_iov = iovsend;
			  send.msg_iovlen = 2;
			  iovsend[0].iov_base = &sendhdr;
			  iovsend[0].iov_len = sizeof(sendhdr);
			  iovsend[1].iov_base = inbuff;
			  iovsend[1].iov_len = sizeof(inbuff);
			  
			  Recvmsg(sockfd, &send, 0);
			  //printf("%s \n", inbuff);
			  //Sendmsg(sockfd, &send, 0);
			  printf("SENT DATA BACK \n");
			}
			
			printf("Shouldn't get to here \n");
			break;
		}
		if(FD_ISSET(pipefd[0], &rset)){
			Read(pipefd[0], &n, 1);		/*timer expired*/
			printf("\nPacket lost!! Resending packet..\n");
			i++;				/*Count the number of retries*/
			goto L1;			/*Send file name again*/
		}	
	}

	free_ifi_info_plus(ifi);
	exit(0);
}

void print(struct sockaddr_in *servaddr){
	
	char server[20];
	Inet_ntop(AF_INET, &(servaddr->sin_addr), server, INET_ADDRSTRLEN);
	printf("Server address: %s\n", server); 
	printf("Port number: %d\n", ntohs(servaddr->sin_port));

}

uint32_t parseIPV4string(char *ipAddress) {

	char ipbytes[4];
	sscanf(ipAddress, "%hhu.%hhu.%hhu.%hhu", &ipbytes[3], &ipbytes[2], &ipbytes[1], &ipbytes[0]);
	return ipbytes[0] | ipbytes[1] << 8 | ipbytes[2] << 16 | ipbytes[3] << 24;
}

static void recvfrom_alarm(int signo){
	Write(pipefd[1],"",1);	/*Write one null byte data to pipe*/
	return;
}

