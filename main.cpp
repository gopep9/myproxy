//
//  main.cpp
//  lightsocks
//
//  Created by 黄剑 on 2018/7/30.
//  Copyright © 2018年 黄剑. All rights reserved.
//

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/tcp.h>

#define TIME_OUT 6000000

using namespace std;
string targetIpAddress="0.0.0.0";
static pthread_mutex_t g_mutex;
static pthread_cond_t g_cond;

struct ClientAndRemoteSocks{
    int clientSock;
    int remoteSock;
    bool stopConnect;
};

//读取socket一行
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    
    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';
    
    return(i);
}
//根据网址和端口号获取socket
int Socket(const char *host, int clientPort)
{
    int sock;
    unsigned long inaddr;
    struct sockaddr_in ad;
    struct hostent *hp;
    
    memset(&ad, 0, sizeof(ad));
    ad.sin_family = AF_INET;
    
    inaddr = inet_addr(host);
    if (inaddr != INADDR_NONE)
        memcpy(&ad.sin_addr, &inaddr, sizeof(inaddr));
    else
    {
        hp = gethostbyname(host);
        if (hp == NULL)
            return -1;
        memcpy(&ad.sin_addr, hp->h_addr, hp->h_length);
    }
    ad.sin_port = htons(clientPort);
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return sock;
    if (connect(sock, (struct sockaddr *)&ad, sizeof(ad)) < 0)
        return -1;
    return sock;
}

void *acceptRemotePack(void *arg)
{
    struct ClientAndRemoteSocks clientAndRemoteSocks=*(ClientAndRemoteSocks*)arg;
    int clientSock=clientAndRemoteSocks.clientSock;
    int remoteSock=clientAndRemoteSocks.remoteSock;
    ssize_t readwordnum=0;
    char buf[2048];
    while((readwordnum=read(clientSock,buf,sizeof(buf)))>0)
    {
        write(remoteSock, buf, readwordnum);
    }
    return NULL;
}

bool IsSocketClosed(int clientSocket)
{
    char buff[32];
    int recvBytes = recv(clientSocket, buff, sizeof(buff), MSG_PEEK);
    
    int sockErr = errno;
    
    //cout << "In close function, recv " << recvBytes << " bytes, err " << sockErr << endl;
    
    if( recvBytes > 0) //Get data
        return false;
    
    if( (recvBytes == -1) && (sockErr == EWOULDBLOCK) ) //No receive data
        return false;
    
    return true;
}

void *proxyThread(void *arg)
{
    pthread_mutex_lock(&g_mutex);
    int connectSock=*(int*)arg;
    pthread_cond_signal(&g_cond);
    pthread_mutex_unlock(&g_mutex);
    
    ssize_t readwordnum=0;
    char buf[2048]={};
    int bufIndex=0;
    memset(buf, 0, sizeof(buf));
//    int tmpfileflag=fcntl(connectSock, F_GETFD);
//    tmpfileflag|=O_NONBLOCK;
//    fcntl(connectSock,F_SETFD,tmpfileflag);
//    tmpfileflag=fcntl(remoteSock, F_GETFD);
//    tmpfileflag|=O_NONBLOCK;
//    fcntl(remoteSock,F_SETFD,tmpfileflag);
    //可能要多搞一个线程来处理双工的链接
    pthread_t pthread_tid=0;
    //这里是获取client的包并且发送到remote，在client断开的时候断开？

    memset(buf, 0, sizeof(buf));
    readwordnum=get_line(connectSock, buf, sizeof(buf));
    printf("get first line:%s",buf);
    
    int methodIndex=0;
    char method[255]={};
    memset(method, 0, sizeof(255));
    while(!isspace(buf[methodIndex])&&(methodIndex<sizeof(method)-1))
    {
        method[methodIndex]=buf[methodIndex];
        methodIndex++;
    }
    bufIndex=methodIndex;
    method[methodIndex]='\0';
    
    //在这里判断是否是connect方法，假如是，返回HTTP/1.1 200 Connection Established
    //不知道为什么访问百度会访问这个
    if(strcmp(method, "CONNECT")==0)
    {
        while((readwordnum=get_line(connectSock,buf,sizeof(buf)))>0)
        {
            
        }
        const char *retMsg="HTTP/1.1 200 Connection Established";
//        write(connectSock,retMsg,strlen(retMsg));
        close(connectSock);
        return NULL;
    }
    if(strcmp(method,"GET")==0)
    {
//        printf("get status:\n");
//        while((readwordnum=get_line(connectSock,buf,sizeof(buf)))>0)
//        {
//            puts(buf);
//        }
//        printf("get end\n");
        char secondLine[2048]={};
        int readwordnumline2=get_line(connectSock, secondLine, sizeof(secondLine));
        //获得ip地址
        string hostAddr=string(secondLine).substr(6);
//        hostAddr=hostAddr.erase(hostAddr.end()-1);
        hostAddr.erase(hostAddr.end()-1);
        int remoteSock=Socket(hostAddr.c_str(),80);
        write(remoteSock, buf, readwordnum);
//        write(remoteSock, "\n", 1);
        write(remoteSock, secondLine,readwordnumline2);
//        write(remoteSock, "\n", 1);
        struct ClientAndRemoteSocks clientAndRemoteSocks={connectSock,remoteSock,false};
//        pthread_create(&pthread_tid, nullptr, acceptRemotePack, &clientAndRemoteSocks);
//        while((readwordnum=read(remoteSock,buf,sizeof(buf)))>0)
//        {
//            write(connectSock, buf, readwordnum);
//        }
        
        //在这里设置两个socket非阻塞
//        int tmpfileflag=fcntl(connectSock, F_GETFD);
//        tmpfileflag|=O_NONBLOCK;
//        fcntl(connectSock,F_SETFD,tmpfileflag);
//        tmpfileflag=fcntl(remoteSock, F_GETFD);
//        tmpfileflag|=O_NONBLOCK;
//        fcntl(remoteSock,F_SETFD,tmpfileflag);

        struct timeval time_out={};
        fd_set fd_read;
        int ret=0;
        
        while(true)
        {
//            if(IsSocketClosed(connectSock)||IsSocketClosed(remoteSock))
//                break;
            
//            可以用select来进行两个socket读取，超时的时候就断开
            
            time_out.tv_sec=0;
            time_out.tv_usec=TIME_OUT;
            FD_ZERO(&fd_read);
            FD_SET(connectSock,&fd_read);
            FD_SET(remoteSock,&fd_read);
            int maxSock=connectSock;
            if(remoteSock>maxSock)
            {
                maxSock=remoteSock;
            }
            maxSock++;
            ret=select(maxSock, &fd_read, NULL, NULL, NULL);
            if(-1==ret)
            {
                perror("select socket error");
            }
            else if(0==ret)
            {
                printf("select time out.\n");
                continue;
            }
            if(FD_ISSET(connectSock,&fd_read))
            {
                readwordnum=read(connectSock,buf,sizeof(buf));
                if(readwordnum>0)
                {
                    readwordnum=write(remoteSock, buf, readwordnum);
                    if(readwordnum==-1)
                    {
                        perror("send data to real server error");
                        break;
                    }
                }
                else if(readwordnum==0)
                {
                    //关闭端口，退出
                    break;
                }
                else {
                    perror("read connectSock error");
                    break;
                }
            }
            else if(FD_ISSET(remoteSock,&fd_read))
            {
                readwordnum=read(remoteSock,buf,sizeof(buf));
                if(readwordnum>0)
                {
                    readwordnum=write(connectSock, buf, readwordnum);
                    if(readwordnum==-1)
                    {
                        perror("send data to client error");
                        break;
                    }
                }
                else if(readwordnum==0)
                {
                    //关闭端口，退出
                    break;
                }
                else{
                    perror("read remoteSock error");
                    break;
                }
            }
            
//            readwordnum=read(connectSock,buf,sizeof(buf));
//            int writedWord=0;
//            while(writedWord<readwordnum)
//            {
//                ssize_t currentWriteWord=write(remoteSock, buf+writedWord, readwordnum-writedWord);
//                writedWord+=currentWriteWord;
//            }
//            readwordnum=read(remoteSock,buf,sizeof(buf));
//            writedWord=0;
//            while(writedWord<readwordnum)
//            {
//                ssize_t currentWriteWord=write(connectSock, buf+writedWord, readwordnum-writedWord);
//                writedWord+=currentWriteWord;
//            }
        }
//        pthread_cancel(pthread_tid);
//        pthread_join(pthread_tid, NULL);
        close(remoteSock);
        close(connectSock);
    }
    
    return NULL;
    
    
    /*
    bufIndex++;
    char url[255]={};
    int urlIndex=0;
    memset(url,0,sizeof(255));
    while(!isspace(buf[bufIndex])&&(urlIndex<sizeof(url)-1)&&(buf[bufIndex]!=':'))
    {
        url[urlIndex]=buf[bufIndex];
        bufIndex++;
        urlIndex++;
    }
    url[urlIndex]='\0';
    printf("url:%s\n",url);
    
//    int port=80;
    char strport[6]={};
    memset(strport,0,sizeof(strport));
    if(buf[bufIndex]==':')
    {
        bufIndex++;
        for(int portIndex=0;portIndex<6;portIndex++,bufIndex++)
        {
            if(!isnumber(buf[bufIndex]))
                break;
            strport[portIndex]=buf[bufIndex];
        }
    }else{
        strcpy(strport,"80");
    }
    printf("port is %s\n",strport);
    
    int remoteSock=Socket(url, atoi(strport));
    printf("remoteSock:%d\n",remoteSock);
    //把之前get的缓存发到服务器，可能还要改下http头部
    write(remoteSock,buf,readwordnum);
    
    memset(buf, 0, sizeof(buf));
    readwordnum=get_line(connectSock, buf, sizeof(buf));
    printf("second line:%s",buf);
    write(remoteSock, buf, readwordnum);
    
    struct ClientAndRemoteSocks clientAndRemoteSocks={connectSock,remoteSock};
    pthread_create(&pthread_tid, nullptr, acceptRemotePack, &clientAndRemoteSocks);

//    while(!isspace(buf))
    
    while((readwordnum=read(connectSock,buf,sizeof(buf)))>0)
    {
        write(remoteSock, buf, readwordnum);
//        write(remoteSock,buf)
        //在这里判断是否断开了tcp连接
//        struct tcp_connection_info info;
//        int len=sizeof(tcp_connection_info);
//        getsockopt(connectSock, IPPROTO_TCP, TCP_CONNECTION_INFO, &info, (socklen_t *)&len);
//        if(info.tcpi_state!=TCP_ESTABLISHED)
//        {
//
//        }
        
    }
    //等待另一个线程
    pthread_join(pthread_tid, NULL);
    close(remoteSock);
    close(connectSock);
    printf("close connectSock:%d remoteSock:%d\n",connectSock,remoteSock);
    return NULL;
     */
}

int main(int argc, const char * argv[]) {
    if(argc!=2)
    {
        cout// <<"first arg is target ip address\n"
        <<"first arg is listen port\n";
        return NULL;
    }
    pthread_mutex_init(&g_mutex, NULL);
    pthread_cond_init(&g_cond, NULL);
//    targetIpAddress=argv[1];
    int listenPort=atoi(argv[1]);
    
    int listenSock=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listenSock<0)
    {
        perror("socket");
        exit(1);
    }
    struct sockaddr_in listenSockaddr={};
    listenSockaddr.sin_family=AF_INET;
    listenSockaddr.sin_port=htons(listenPort);
    listenSockaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    socklen_t len=sizeof(listenSockaddr);
    if(::bind(listenSock, (struct sockaddr*)&listenSockaddr, len)<0)
    {
        perror("bind");
        exit(2);
    }
    
    if(::listen(listenSock, 1000)<0)
    {
        perror("listen");
        exit(3);
    }
    
    struct sockaddr_in remoteSockaddr={};
    
    while(true){
        int connectSock=accept(listenSock,(struct sockaddr*)&remoteSockaddr, &len);
        std::cout<<"receive file or document from ip address: "<<inet_ntoa(remoteSockaddr.sin_addr)<<"\nand port: "<<ntohs(remoteSockaddr.sin_port)<<"\n";
        if(connectSock<0)
        {
            perror("accept");
            return -1;
        }
        pthread_t pthread_tid=0;
        pthread_mutex_lock(&g_mutex);
        pthread_create(&pthread_tid, nullptr, proxyThread, &connectSock);
        pthread_cond_wait(&g_cond, &g_mutex);
        pthread_mutex_unlock(&g_mutex);
    }
}
