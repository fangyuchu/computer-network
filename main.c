#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/if_ether.h>
#include <errno.h>
#include<netinet/in.h>
//#include <linux/in->h>
#define BUFFER_MAX 2048

#define PRINT_IP_FORMAT         "%u.%u.%u.%u"
#define  PRINT_HIP(x)\
   ((x >> 24) & 0xFF),\
   ((x >> 16) & 0xFF),\
   ((x >>  8) & 0xFF),\
   ((x >>  0) & 0xFF)


struct icmphead{
    unsigned char icmp_type;
    unsigned char icmp_code;
    unsigned short int icmp_cksum;
    unsigned short int icmp_id;
    unsigned short int icmp_seq;
};
struct iphead{   //该结构体模拟IP首部（代码中，控制套接字不添加IP数据包首部，需要自己添加），</span>
    unsigned char ip_hl:4, ip_version:4;  //ip_hl,ip_version各占四个bit位。
    unsigned char ip_tos;
    unsigned short int ip_len;
    unsigned short int ip_id;
    unsigned short int ip_off;
    unsigned char ip_ttl;
    unsigned char ip_pro;
    unsigned short int ip_sum;
    unsigned int ip_src;
    unsigned int ip_dst;
};
struct arphead{
    unsigned short arp_hrd;    /* format of hardware address */
    unsigned short arp_pro;    /* format of protocol address */
    unsigned char arp_hln;    /* length of hardware address */
    unsigned char arp_pln;    /* length of protocol address */
    unsigned short arp_op;     /* request/reply operation */
    unsigned char arp_sha[6];    /* sender hardware address */
    unsigned char arp_spa[4];    /* sender protocol address */                      //can't use unsigned int here
    unsigned char arp_tha[6];    /* target hardware address */
    unsigned char arp_tpa[4];    /* target protocol address */
};
unsigned short int cksum(char buffer[], int size){   //计算校验和，具体的算法可自行百度，或查阅资料
    unsigned long sum = 0;
    unsigned short int answer;
    unsigned short int *temp;
    temp = (short int *)buffer;
    for( ; temp<buffer+size; temp+=1){
        sum += *temp;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;
    return answer;
}

void receivePackage(){
    unsigned char *p=NULL;
    int sockfd=socket(PF_PACKET,SOCK_RAW,htons(ETH_P_ALL));                        //create socket
        if(sockfd<0){
        printf("Fail in creating the socket->\n");
        printf("Error Code:%d\n",errno);
        return;
    }

    char buffer[1600];
    int bytes_read=recvfrom(sockfd,buffer,1600,0,NULL,NULL);                       //read messages

    if(bytes_read < 42){                                                         //todo:why 42?{
        printf("error when recv msg \n");
        return -1;
    }
    p=buffer;
    printf("  Ethernet\n    Destination: %.2x:%02x:%02x:%02x:%02x:%02x\n    Source: %.2x:%02x:%02x:%02x:%02x:%02x\n",\
    p[0],p[1],p[2],p[3],p[4],p[5],\
    p[6],p[7],p[8],p[9],p[10],p[11]);

    short *type=(short *)(buffer+12);
    *type=ntohs(*type);
    if(*type==0x0800){                                                      //receive one IP package
        printf("        IP Protocol\n");
        struct iphead *ip=(struct iphead*)(buffer+14);
        printf("          Header Length:%d, IP Version:%d\n",ip->ip_hl,ip->ip_version);
        printf("          Message Length:%hu, TTL:%d\n",ntohs(ip->ip_len),ip->ip_ttl);

        char ip_str[64];
        sprintf(ip_str, PRINT_IP_FORMAT, PRINT_HIP(ntohl(ip->ip_src)));     //transform IP address
        printf("          Source:%s, ", ip_str);
        sprintf(ip_str, PRINT_IP_FORMAT, PRINT_HIP(ntohl(ip->ip_dst)));
        printf("Detination:%s\n",ip_str);
        printf("          Protocol:");
        switch(ip->ip_pro){
            case IPPROTO_ICMP:printf("ICMP");break;
            case IPPROTO_IGMP:printf("IGMP");break;
            case IPPROTO_IPIP:printf("IPIP");break;
            case IPPROTO_TCP:printf("TCP");break;
            case IPPROTO_UDP:printf("UDP");break;
            default:printf("unknow types");
        }
        printf("\n");
    }else if(*type==0x0806){                                                //receive one arp package
        printf("        ARP Protocol\n");
        struct arphead *arp=(struct arphead*)(buffer+14);
        if(ntohs(arp->arp_op)==1){
            printf("          Request From: ");
            printf("IP: %d.%d.%d.%d\n",arp->arp_spa[0],arp->arp_spa[1],arp->arp_spa[2],arp->arp_spa[3]);
            printf("                        MAC: %.2x:%02x:%02x:%02x:%02x:%02x\n",arp->arp_sha[0],arp->arp_sha[1],arp->arp_sha[2],arp->arp_sha[3],arp->arp_sha[4],arp->arp_sha[5]);
            printf("                  To:   ");
            printf("IP: %d.%d.%d.%d\n",arp->arp_tpa[0],arp->arp_tpa[1],arp->arp_tpa[2],arp->arp_tpa[3]);
        }else if(ntohs(arp->arp_op)==2){
            printf("          Reply From:   ");
            printf("IP: %d.%d.%d.%d\n",arp->arp_spa[0],arp->arp_spa[1],arp->arp_spa[2],arp->arp_spa[3]);
            printf("                        MAC: %.2x:%02x:%02x:%02x:%02x:%02x\n",arp->arp_sha[0],arp->arp_sha[1],arp->arp_sha[2],arp->arp_sha[3],arp->arp_sha[4],arp->arp_sha[5]);
            printf("                  To:   ");
            printf("IP: %d.%d.%d.%d\n",arp->arp_tpa[0],arp->arp_tpa[1],arp->arp_tpa[2],arp->arp_tpa[3]);
            printf("                        MAC: %.2x:%02x:%02x:%02x:%02x:%02x\n",arp->arp_tha[0],arp->arp_tha[1],arp->arp_tha[2],arp->arp_tha[3],arp->arp_tha[4],arp->arp_tha[5]);
        }
    }
}


void sendPingIP(int numPackages){
    unsigned char buffer[sizeof(struct iphead)+sizeof(struct icmphead)];    //create buffer for package
    memset(buffer,0,sizeof(buffer));
    struct iphead *ip=(struct iphead*)buffer;
    struct icmphead *icmp=(struct icmphead*)(buffer+sizeof(struct iphead));

    int sockfd=socket(AF_INET,SOCK_RAW,IPPROTO_ICMP);                        //create socket
    if(sockfd<0){
        printf("Fail in creating the socket->\n");
        printf("Error Code:%d\n",errno);
        return;
    }

    int one=1;
    if(setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0){  //设置套接字行为，此处设置套接字不zidong添加IP首部
        printf("setsockopt failed->\n");
        printf("Error Code:%d\n",errno);
        return;
    }

    struct sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=inet_addr("192.168.2.1");                          //what is this used for?
    for(int i=0;i<numPackages;i++){                                          //send icmp packages
        ip->ip_version=4;
        ip->ip_hl=5;                                                          //todo:why 5?
        ip->ip_tos=0;
        ip->ip_len=htons(sizeof(struct iphead) + sizeof(struct icmphead));
        ip->ip_id=htons(17852);                                                  //based on wireshark
        ip->ip_off=htons(0x0000);
        ip->ip_ttl=64;
        ip->ip_pro=IPPROTO_ICMP;
        //ip->ip_src=inet_addr("192.168.4.10");                                 //system will fill the src ip automatically
        ip->ip_dst=inet_addr("14.215.177.39");
        ip->ip_sum=0;                                                            //reset checksum
        ip->ip_sum=cksum(buffer,20);

        icmp->icmp_type=8;
        icmp->icmp_code=0;
        icmp->icmp_id=getpid();
        icmp->icmp_seq=i+1;
        icmp->icmp_cksum=0;                                                       //reset checksum
        icmp->icmp_cksum=cksum(buffer+20,8);

        if(sendto(sockfd,buffer,htons(ip->ip_len),0,(struct sockaddr *)&addr,sizeof(struct sockaddr) )<0){
            printf("Fail in sending the buffer->\n");
            printf("Error Code:%d\n",errno);
            return;
        }else{
            printf("Sent %d buffers!\n",i+1);
            sleep(1);
        }
    }
}


int main(int argc,char* argv[]){
    //sendPingIP(50);
    while(1){
        //sendPingIP(2);
        receivePackage();
    }
	return -1;
}
