/*
 ============================================================================
 Name        : ping.c
 Author      : huh
 Version     : 0.01
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <sys/types.h>
#include <sys/select.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/ip_icmp.h>

#define PACKET_SIZE 1024*4

int pid;
int sockfd;
int datalen = 56;
int nsent = 1, nrecv = 1;
char sendbuf[PACKET_SIZE];
char recvbuf[PACKET_SIZE];
struct sockaddr_in dest_addr; //socket目的地址
struct sockaddr_in src_addr;
struct timeval tvrecv;

void send_packet();
void un_packet(int);
void tv_sub(struct timeval *out, struct timeval *in);
unsigned short in_chksum(unsigned short *addr, int len);

int main()
{
	pid = getpid();
	char str[20];
	unsigned int inaddr;
	struct hostent *host;
	int size = 1024 * 25;
	printf("ping: ");
	scanf("%s",str);
	inaddr = inet_addr(str);
	if (inaddr == INADDR_NONE)
	{
		host = gethostbyname(str);
		if (host == NULL)
		{
			printf("Address format was wrong, please try again.\n");
			return 0;
		}
		memcpy((char*)&inaddr, host->h_addr, sizeof(dest_addr.sin_addr));
	}
	//设置套接字地址
	dest_addr.sin_family = AF_INET;
	memcpy((char *)&dest_addr.sin_addr, (char *)&inaddr, sizeof(inaddr));
	//创建套接字
	sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	//改变socket缓冲区大小
	setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));

	printf("PING %s (%s) %d(84) bytes of data.\n", str, inet_ntoa(dest_addr.sin_addr), datalen);
	//不停的发送和接受ICMP数据包
	while (1)
	{
		send_packet();
		int src_addr_len = sizeof(struct sockaddr_in);
		//接收数据包，一直阻塞到有数据包到达为止
		int len = recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr *) &src_addr, (socklen_t *)&src_addr_len);
		if (len < 0)
			printf("recvfrom error!\n");
		un_packet(len);
		sleep(1); //间隔一秒，后面会用信号实现每秒发送一个ICMP包。
	}
	return 0;
}

//解包
void un_packet(int len)
{
	int hlen1;
	double rtt;
	struct ip *ip;
	struct icmp *icmp;
	struct timeval *tvsend;

	ip = (struct ip *) recvbuf;
	hlen1 = ip->ip_hl << 2;
	if (ip->ip_p != IPPROTO_ICMP)
		return;

	icmp = (struct icmp *) (recvbuf + hlen1);
	len -= hlen1;

	if ((icmp->icmp_type == ICMP_ECHOREPLY))
	{
		if (icmp->icmp_id != pid)
			return;
		tvsend = (struct timeval *) icmp->icmp_data;  //发送时间
		gettimeofday(&tvrecv, NULL);  //得到当前时间

		tv_sub(&tvrecv, tvsend); //计算接收和发送的时间差
		rtt = tvrecv.tv_sec * 1000.0 + tvrecv.tv_usec / 1000.0; //以毫秒单位计算rtt
		printf("%d byte from %s: icmp_seq=%u ttl=%d rtt=%.3fms\n", len, inet_ntoa(src_addr.sin_addr), icmp->icmp_seq, ip->ip_ttl, rtt);
		nrecv++;
	}
}

//手动构建数据包，并通过原始套接字发送
void send_packet()
{
	int len;
	struct icmp *icmp;
	icmp = (struct icmp *) (sendbuf);
	icmp->icmp_type = ICMP_ECHO;  //拼接icmp
	icmp->icmp_code = 0;
	icmp->icmp_id = pid;   //2字节
	icmp->icmp_seq = nsent++; //2字节
	memset(icmp->icmp_data, 0xa5, datalen);
	gettimeofday((struct timeval *) icmp->icmp_data, NULL);    //将发送时间作为数据传递过去

	len = datalen + 8;
	icmp->icmp_cksum = 0;  //校验和需要先置0
	icmp->icmp_cksum = in_chksum((unsigned short *)icmp, len);  //计算效验和

	sendto(sockfd, sendbuf, len, 0, (struct sockaddr *) &dest_addr,
		sizeof(dest_addr));  //将包发出去
//printf("package have sent!\n");
}

//计算效验和
unsigned short in_chksum(unsigned short *addr, int len)
{
	int nleft = len;
	int sum = 0;
	unsigned short *w = addr;
	unsigned short answer = 0;
	//把ICMP报头二进制数据以2字节为单位累加起来
	while (nleft > 1)
	{
		sum += *w++;
		nleft -= 2;
	}
	if (nleft == 1)
	{
		*(unsigned char *)(&answer) = *(unsigned char *)w;
		sum += answer;
	}
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	answer = ~sum;
	return answer;
}

//计算时间差
void tv_sub(struct timeval *out, struct timeval *in)
{
	if ((out->tv_usec -= in->tv_usec) < 0)
	{
		--out->tv_sec;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}
