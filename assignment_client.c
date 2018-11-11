/*******************************
udp_client.c: the source file of the client in udp transmission
********************************/

#include "headsock.h"

float str_cli(FILE *fp, int sockfd, struct sockaddr *addr, int addrlen, long *len); //transmission function
void tv_sub(struct  timeval *out, struct timeval *in);	    //calcu the time interval between out and in

int main(int argc, char **argv)
{
	int sockfd;
	float ti, rt;
	long len;
	struct sockaddr_in ser_addr;
	char ** pptr;
	struct hostent *sh;
	struct in_addr **addrs;
	FILE *fp;

	if (argc != 2) {
		printf("parameters not match");
	}

	sh = gethostbyname(argv[1]);	                      //get host's information
	if (sh == NULL) {
		printf("error when gethostby name");
		exit(0);
	}

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);            //create the socket
	if (sockfd<0)
	{
		printf("error in socket");
		exit(1);
	}
  
	addrs = (struct in_addr **)sh->h_addr_list;
	printf("canonical name: %s\n", sh->h_name);					//print the remote host's information
	for (pptr=sh->h_aliases; *pptr != NULL; pptr++)
		printf("the aliases name is: %s\n", *pptr);
	switch(sh->h_addrtype)
	{
		case AF_INET:
			printf("AF_INET\n");
		break;
		default:
			printf("unknown addrtype\n");
		break;
	}
  
	ser_addr.sin_family = AF_INET;                                                      
	ser_addr.sin_port = htons(MYUDP_PORT);
	memcpy(&(ser_addr.sin_addr.s_addr), *addrs, sizeof(struct in_addr));
	bzero(&(ser_addr.sin_zero), 8);
	
	if((fp = fopen ("myfile.txt","r+t")) == NULL)
	{
		printf("File doesn't exit\n");
		exit(0);
	}

  //perform the transmission and receiving
	ti = str_cli(fp, sockfd, (struct sockaddr *)&ser_addr, sizeof(struct sockaddr_in), &len);
	rt = (len/(float)ti);           //caculate the average transmission rate
	printf("Time(ms) : %.3f, Data sent(byte): %d\nData rate: %f (Kbytes/s)\n", ti, (int)len, rt);

	close(sockfd);
	fclose(fp);
  
	exit(0);
}

float str_cli(FILE *fp, int sockfd, struct sockaddr *addr, int addrlen, long *len)
{
	char *buf;
	long lsize, ci;   //lsize=entire file size; ci=curr index of buf
	char sends[2*DATALEN];    // to send 1 or 2 DU
	struct ack_so ack;
	int n, slen, str_limit;      // slen=len of string to send (either 1DU or 2DU)
	float time_inv = 0.0;
	struct timeval sendt, recvt;
  int count = 0;
	ci = 0;

	fseek (fp , 0 , SEEK_END);
	lsize = ftell (fp);
	rewind (fp);
	printf("The file length is %d bytes\n", (int)lsize);
	printf("the packet length is %d bytes\n",DATALEN);

  // allocate memory to contain the whole file.
	buf = (char *) malloc (lsize);
	if (buf == NULL) exit (2);

  // copy the file into the buffer.
	fread (buf,1,lsize,fp);

  /*** the whole file is loaded in the buffer. ***/
	buf[lsize] ='\0';						  //append the end byte (extra byte sent to server)
	gettimeofday(&sendt, NULL);		//get the current time
	while(ci <= lsize)
	{
    // alternate between 1 and 2 DU
    str_limit = (count%2==0) ? DATALEN : 2*DATALEN;
		if ((lsize+1-ci) <= str_limit)  // the last part of file that is < 1 or 2 DU
			slen = lsize+1-ci;
		else 
			slen = str_limit;
		memcpy(sends, (buf+ci), slen);
    // printf("%d bytes of data sent: %s\n", slen, sends);
    
		n = sendto(sockfd, &sends, slen, 0, addr, addrlen);
		if(n == -1) {
			printf("send error!");								//send the data
      exit(1);
		}
    
    // wait for ack after every DU
    while((n = recvfrom(sockfd, &ack, 2, 0, addr, (socklen_t *)&addrlen))== -1 && 
        (ack.num != 1 || ack.len != 0))   //pause until receive the ack
    {
        printf("---------\nack receive error\n--------\n");
    }
    
		ci += slen;
    count += 1;
    // printf("received till %d bytes\n", (int)ci);
	}
  
  // printf("exited while loop to send data\n");
	if ((n= recvfrom(sockfd, &ack, 2, 0, addr, (socklen_t *)&addrlen))==-1)   //receive the final ack
	{
		printf("error when receiving\n");
		exit(1);
	}
  // else
    // printf("ack received\n");
  
	if (ack.num != 1|| ack.len != 0)      // it is not an ack
		printf("error in transmission\n");
    
	*len= ci;
  // printf("final total size %d bytes\n", (int)*len);
  
  // calculating time taken for transfer
	gettimeofday(&recvt, NULL);           //get current time
	tv_sub(&recvt, &sendt);               // get the whole trans time
	time_inv += (recvt.tv_sec)*1000.0 + (recvt.tv_usec)/1000.0;
	return(time_inv);
}

void tv_sub(struct  timeval *out, struct timeval *in)
{
	if ((out->tv_usec -= in->tv_usec) <0)
	{
		--out ->tv_sec;
		out ->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}
