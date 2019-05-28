#include "Winsock.h" 
#include "windows.h" 
#include "stdio.h"  
#define RECV_PORT 3312  
#define SEND_PORT 4302  
#pragma   comment(lib, "wsock32.lib")
SOCKET sockclient, sockserver;
 struct sockaddr_in ServerAddr;//服务器地址
 struct sockaddr_in ClientAddr;//客户端地址 

/***********************全局变量***********************/
int Addrlen;//地址长度 
char filename[20];//文件名 
char order[10];//命令  
char rbuff[1024];//接收缓冲区  
char sbuff[1024];//发送缓冲区  




void SetSocketSize(SOCKET sock, int nsize)
{
	int nErrCode = 0;//返回值
	unsigned int uiRcvBuf = 0;
	unsigned int uiNewRcvBuf = 0;
	int uiRcvBufLen = sizeof(uiRcvBuf);
	nErrCode= getsockopt(sock, SOL_SOCKET, SO_SNDBUF,(char*)&uiRcvBuf, &uiRcvBufLen);
	if ( SOCKET_ERROR == nErrCode )
	{
		printf("获取服务端设置SOCKET发送缓冲区大小失败\n");
		return;
	}
	printf("uiNewRcvBuf:%d\n",uiNewRcvBuf);
	printf("uiRcvBuf:%d\n",uiRcvBuf);
	uiRcvBuf *= nsize;//设置系统发送数据为默认的倍数
	nErrCode = setsockopt(sock, SOL_SOCKET, SO_SNDBUF,(char*)&uiRcvBuf, uiRcvBufLen);
	if ( SOCKET_ERROR == nErrCode )
	{
		printf("设置SOCKET发送缓冲区大小失败\n");
		return;
	}
	printf("uiNewRcvBuf:%d\n",uiNewRcvBuf);
	printf("uiRcvBuf:%d\n",uiRcvBuf);
	
	
	
	nErrCode= getsockopt(sock, SOL_SOCKET, SO_SNDBUF,(char*)&uiRcvBuf, &uiRcvBufLen);
	if ( SOCKET_ERROR == nErrCode )
	{
		printf("获取服务端设置SOCKET发送缓冲区大小失败\n");
		return;
	}
	printf("uiNewRcvBuf:%d\n",uiNewRcvBuf);
	printf("uiRcvBuf:%d\n",uiRcvBuf);

}




DWORD StartSock()    //初始化winsock   
{
    WSADATA WSAData;
    if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0)
    {
        printf("socket init fail!\n");
        return (-1);
    }
    return(1);
}

DWORD CreateSocket()
{
    sockclient = socket(AF_INET, SOCK_STREAM, 0);
    if (sockclient == SOCKET_ERROR)
    {
        printf("sockclient create fail ! \n");
        WSACleanup();
        return(-1);
    }
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    ServerAddr.sin_port = htons(RECV_PORT);
    if (bind(sockclient, (struct  sockaddr  FAR  *)&ServerAddr, sizeof(ServerAddr)) == SOCKET_ERROR)
    {                     //bind函数将套接字和地址结构绑定   
        printf("bind is the error");
        return(-1);
    }
    return (1);
}

int SendFileRecord(SOCKET datatcps, WIN32_FIND_DATA *pfd)     //用来发送当前文件记录 
{
    char filerecord[MAX_PATH + 32];
    FILETIME ft;         //文件建立时间   
    FileTimeToLocalFileTime(&pfd->ftLastWriteTime, &ft);
    SYSTEMTIME lastwtime;     //SYSTEMTIME系统时间数据结构   
    FileTimeToSystemTime(&ft, &lastwtime);
    char *dir = pfd->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY ? "<DIR>" : " ";
    sprintf(filerecord, "%04d-%02d-%02d %02d:%02d  %5s %10d   %-20s\n",
        lastwtime.wYear,
        lastwtime.wMonth,
        lastwtime.wDay,
        lastwtime.wHour,
        lastwtime.wMinute,
        dir,
        pfd->nFileSizeLow,
        pfd->cFileName);
    if (send(datatcps, filerecord, strlen(filerecord), 0) == SOCKET_ERROR)
    { //通过datatcps接口发送filerecord数据，成功返回发送的字节数   
        printf("Error occurs when sending file list!\n");
        return 0;
    }
    return 1;
}

int SendFileList(SOCKET datatcps)
{
    HANDLE hff;//建立一个线程  
    WIN32_FIND_DATA fd;   //搜索文件   
//可以通过FindFirstFile（）函数根据当前的文件存放路径查找该文件来把待操作文件的相关属性读取到WIN32_FIND_DATA结构中去  
    if (hff == INVALID_HANDLE_VALUE)//发生错误  
    {
        const char *errstr = "can't list files!\n";
        printf("list file error!\n");
        if (send(datatcps, errstr, strlen(errstr), 0) == SOCKET_ERROR)
        {
            printf("error occurs when senging file list!\n");
        }
        closesocket(datatcps);
        return 0;
    }

    BOOL fMoreFiles = TRUE;
    while (fMoreFiles)
    {//发送此项文件信息   
        if (!SendFileRecord(datatcps, &fd))
        {
            closesocket(datatcps);
            return 0;
        }
        //搜索下一个文件    
        fMoreFiles = FindNextFile(hff, &fd);
    }
    closesocket(datatcps);
    return 1;
}

int SendFile(SOCKET datatcps, FILE* file)
{
    printf(" sending file data..");
    for (;;)   //从文件中循环读取数据并发送客户端   
    {
        int r = fread(sbuff, 1, 1024, file);//把file里面的内容读到sbuff缓冲区   
        if (send(datatcps, sbuff, r, 0) == SOCKET_ERROR)
        {
            printf("lost the connection to client!\n");
            closesocket(datatcps);
            return 0;
        }
        if (r<1024)//文件传送结束    
            break;
    }
    closesocket(datatcps);
    printf("done\n");
    return 1;
}

//连接  
DWORD ConnectProcess()
{
    Addrlen = sizeof(ClientAddr);
    if (listen(sockclient, 5)<0)
    {
        printf("Listen error");
        return(-1);
    }
    printf("服务器监听中...\n");
    for (;;)
    {
        sockserver = accept(sockclient, (struct sockaddr FAR *)&ClientAddr, &Addrlen);
        //accept函数取出连接队列的第一个连接请求，sockclient是处于监听的套接字ClientAddr 是监听的对象地址，         
        //Addrlen是对象地址的长度 
        
        
        SetSocketSize(sockserver,100);
        
        for (;;)
        {
            memset(rbuff, 0, 1024);
            memset(sbuff, 0, 1024);
            if (recv(sockserver, rbuff, 1024, 0) <= 0)
            {
                break;
            }
            printf("\n");
            printf("获取并执行的命令为：");
            printf(rbuff);
            if (strncmp(rbuff, "get", 3) == 0)
            {
                strcpy(filename, rbuff + 4); printf(filename);
                FILE *file; //定义一个文件访问指针    
                //处理下载文件请求    
                file = fopen(filename, "rb");//打开下载的文件，只允许读写   
                if (file)
                {
                    sprintf(sbuff, "get file %s\n", filename);
                    if (!send(sockserver, sbuff, 1024, 0))
                    {
                        fclose(file);      return 0;
                    }
                    else
                    {//创建额外数据连接传送数据     
                        if (!SendFile(sockserver, file))
                            return 0;
                        fclose(file);
                    }
                }//file   

                else//打开文件失败    
                {
                    strcpy(sbuff, "can't open file!\n");
                    if (send(sockserver, sbuff, 1024, 0))
                        return 0;
                } //lost 
            }//get

            if (strncmp(rbuff, "put", 3) == 0)
            {
                FILE *fd;
                int count;
                strcpy(filename, rbuff + 4);
                fd = fopen(filename, "wb");
                if (fd == NULL)
                {
                    printf("open file %s for weite failed!\n", filename);
                    return 0;
                }
                sprintf(sbuff, "put file %s", filename);
                if (!send(sockserver, sbuff, 1024, 0))
                {
                    fclose(fd);
                    return 0;
                }
                while ((count = recv(sockserver, rbuff, 1024, 0))>0)//recv函数返回接受的字节数赋给count          
                    fwrite(rbuff, sizeof(char), count, fd);
                //把count个数据长度为size0f（）的数据从rbuff输入到fd指向的目标文件
                printf(" get %s succed!\n", filename);
                fclose(fd);
            }//put 

            if (strncmp(rbuff, "pwd", 3) == 0){
            	
            	/***********************************************************************************************/
            	int nErrCode = 0;//返回值
				unsigned int uiRcvBuf = 0;
				unsigned int uiNewRcvBuf = 0;
				int uiRcvBufLen = sizeof(uiRcvBuf);
				nErrCode= getsockopt(sockserver, SOL_SOCKET, SO_SNDBUF,(char*)&uiRcvBuf, &uiRcvBufLen);
				if ( SOCKET_ERROR == nErrCode )
				{
					printf("获取服务端设置SOCKET发送缓冲区大小失败\n");
					return;
				}
				printf("uiNewRcvBuf:%d\n",uiNewRcvBuf);
				printf("uiRcvBuf:%d\n",uiRcvBuf);
            	
            	printf("end\n");
            	
            	/***********************************************************************************************/
            	
                char   path[1000];
                GetCurrentDirectory(1000, path);//找到当前进程的当前目录  
                strcpy(sbuff, path);
                send(sockserver, sbuff, 1024, 0);
            }//pwd   

            if (strncmp(rbuff, "dir", 3) == 0){
                strcpy(sbuff, rbuff);
                send(sockserver, sbuff, 1024, 0);
                SendFileList(sockserver);//发送当前列表 
            }//dir 

            if (strncmp(rbuff, "cd", 2) == 0)
            {
                strcpy(filename, rbuff + 3);
                strcpy(sbuff, rbuff);
                send(sockserver, sbuff, 1024, 0);
                SetCurrentDirectory(filename);//设置当前目录 
            }//cd  
            closesocket(sockserver);
        }//for 2
    }//for 1 
}


int main()
{
    if (StartSock() == -1)
        return(-1);
    if (CreateSocket() == -1)
        return(-1);
    if (ConnectProcess() == -1)
        return(-1);
    return(1);
}
