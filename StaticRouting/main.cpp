#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/if_ether.h>
#include<linux/if_packet.h>
#include<errno.h>
#include <netinet/in.h>
#include<netinet/ip.h>
#include <arpa/inet.h>
#include<string.h>
#include <sys/ioctl.h>
#include<netdb.h>
#include<net/if.h>
#define BUFFER_MAX 4096
#define PACKET_SIZE 1024*4



using namespace std;

char host_ip_addr[][16]={"192.168.159.131","192.168.4.10"};
char recvbuf[PACKET_SIZE];
struct sockaddr_in dest_addr; //socket目的地址
struct sockaddr_in src_addr;

struct route_item{
    char destination[16];
    char gateway[16];
    char netmask[16];
    char interface[16];
}route_info[100]={
    {"192.168.4.0","0.0.0.0","255.255.255.0","ens39"},
    {"192.168.5.0","0.0.0.0","255.255.255.0","ens39"},
    {"192.168.159.0","0.0.0.0","255.255.255.0","ens33"},
    {"0.0.0.0","192.168.159.2","0.0.0.0","ens33"}
};
int route_item_index=4;

struct arp_table_item{
    char ip_addr[16];
    char mac_addr[7];
}arp_table[100]={
    {"192.168.159.254",{0x00,0x50,0x56,0xf5,0x20,0xe8,'\0'}},
    {"192.168.159.2",{0x00,0x50,0x56,0xea,0x2b,0x89,'\0'}}
};
int arp_item_index=2;

struct device_item{
    char interface[14];
    char mac_addr[18];
}device[100]={
    {"ens33",{0x00,0x0c,0x29,0x6d,0xab,0xa8,'\0'}},
    {"ens39",{0x00,0x0c,0x29,0x6d,0xab,0xbc,'\0'}}
};
int device_index=2;

bool packetToHost(char* ip){
    //judge whether the packet is sent to this router
    int device_num=2;
    int i;
    for(i=0;i<device_num;i++){
        if(strcmp(ip,host_ip_addr[i])==0)return true;
    }
    return false;
}

unsigned short checksum(unsigned short *addr, int len)
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




int main()
{
    //print information of network adapter
    struct if_nameindex  *ifni;
    ifni = if_nameindex();
    while (ifni->if_index != 0) {
        cout<< ifni->if_index<<','<< ifni->if_name<<endl;
        ifni++;
    }

	int sockfd;
	int n_read;
    if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
		cout<<"error create raw socket"<<endl;
		return -1;
	}

	while(1){
        int src_addr_len = sizeof(struct sockaddr_in);

        n_read = recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr *) &src_addr, (socklen_t *)&src_addr_len);
		if (n_read < 42)
		{
			cout<<"error when recv msg \n";
			//return -1;
		}
        struct ethhdr *ethhead=(struct ethhdr*)(recvbuf);
        struct iphdr *iphead=(struct iphdr*)(recvbuf+sizeof(struct ethhdr));
        //length of ethernet frame
        int frame_len=14+ntohs(iphead->tot_len);

        if(ethhead->h_proto==htons(0x0806)){
            cout<<"arp"<<endl;
            continue;
        }else if(ethhead->h_proto==htons(0x86dd)){
            cout<<"ipv6"<<endl;
            continue;
        }

        //aquire source and destination ip
        struct in_addr temp;
        temp.s_addr=iphead->daddr;
        char* d_ip=new char[16];
        strcpy(d_ip,inet_ntoa(temp));
        temp.s_addr=iphead->saddr;
        char* s_ip=new char[16];
        strcpy(s_ip,inet_ntoa(temp));
        if(packetToHost(d_ip))continue;

        //get interface and gateway for next hop
        char interface[16];
        char gateway[16];
	if(inet_addr(d_ip)==inet_addr("239.255.255.250")){
            cout<<"multicast address"<<endl;
            continue;
        }
        for(int i=0;i<route_item_index;i++){
            if((inet_addr(d_ip)&inet_addr(route_info[i].netmask))==inet_addr(route_info[i].destination)){
                //route founded
                strcpy(interface,route_info[i].interface);
                strcpy(gateway,route_info[i].gateway);
                break;
            }
        }

        //strcpy(interface,"ens39");

        /******************************************************************************************************
        //bind socket with designated interface
        struct ifreq req;
        strncpy(req.ifr_ifrn.ifrn_name, interface, sizeof(interface));
        if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, (char *)&req, sizeof(req))  < 0) {
            cout<<"BINDTODEVICE failed\n";
			return -1;
        }
        ******************************************************************************************************/

        //change data in ethernet header
        //change source mac
        //char src_mac[7];
        for(int i=0;i<device_index;i++){
            if(strcmp(interface,device[i].interface)==0){
                strcpy((char *)ethhead->h_source,device[i].mac_addr);
                //strcpy(src_mac,device[i].mac_addr);
                break;
            }
        }
        //change destination mac
        if(strcmp(gateway,"0.0.0.0")==0){
            //destination ip is in the same subnet
            for(int i=0;i<arp_item_index;i++){
                if(strcmp(d_ip,arp_table[i].ip_addr)==0){
                    strcpy((char *)ethhead->h_dest,arp_table[i].mac_addr);
                    break;
                }
            }
        }else{
            //send packet to gateway
            for(int i=0;i<arp_item_index;i++){
                if(strcmp(gateway,arp_table[i].ip_addr)==0){
                    strcpy((char *)ethhead->h_dest,arp_table[i].mac_addr);
                    break;
                }
            }
        }

        //change data in ip header
        iphead->ttl-=1;
        iphead->check=0;
        iphead->check=checksum((unsigned short *)iphead,20);

        int sock_send;
         if ((sock_send = socket(AF_PACKET, SOCK_RAW, htons(ethhead->h_proto))) < 0) {
            cout<<"Error: could not open socket\n";
            return -1;
        }

        struct ifreq buffer;
        memset(&buffer, 0x00, sizeof(buffer));
        strncpy(buffer.ifr_name, interface, IFNAMSIZ);
        //get index of interface
        if (ioctl(sock_send, SIOCGIFINDEX, &buffer) < 0) {
            cout<<"Error: could not get interface index"<<endl;
            close(sock_send);
            return -1;
        }
        int interface_index;
        interface_index = buffer.ifr_ifindex;

        struct sockaddr_ll saddrll;
        memset(&saddrll, 0, sizeof(saddrll));
        saddrll.sll_family = AF_PACKET;
        saddrll.sll_ifindex = interface_index;
        saddrll.sll_halen = ETH_ALEN;
        memcpy(saddrll.sll_addr, ethhead->h_dest, ETH_ALEN);

        int err_msg=sendto(sock_send, recvbuf, frame_len, 0, (struct sockaddr*)&saddrll, sizeof(saddrll));
        if ( err_msg> 0){
            cout<<"Success!\n";
            cout<<d_ip<<endl;
            close(sock_send);
            //sleep(5);
        }
        else{
            cout<<"Error, could not send\n"<<"Error Message:"<<errno<<endl;
        }

	}
    return 0;
}


