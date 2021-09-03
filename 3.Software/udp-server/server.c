#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#define SERVER_PORT 8080
#define BUFF_LEN 1030
struct sockaddr_in clent_addr; //接收方的地址信息
struct sockaddr_in sender_addr;//发送方的地址信息
char pacque[100][BUFF_LEN]={0};
/*将pc端发送的数据保存在二维char数组中，等待分光棱镜连接本服务器*/
void forward_msg(int fd)
{
    char buf[BUFF_LEN];//接收缓冲区，1030字节
    socklen_t len;//地址结构体长度
    int count;//接收到的数据（字节）长度
    memset(pacque, 0, sizeof(pacque));//清空pacque
    while(1)
    {
        memset(buf, 0, BUFF_LEN);//清空缓冲区
        len = sizeof(sender_addr);//赋予len以地址结构体长度
        count = recvfrom(fd, buf, BUFF_LEN, 0, (struct sockaddr*)&sender_addr, &len);  //recvfrom是拥塞函数，没有数据就一直拥塞
        if(count == -1)//若监听出错，返回-1
        {
            printf("recieve data fail!\n");
            return;
        }
        else if(count>10)
        {
            /*解析包头*/
            int PacSum=buf[0];
            PacSum=(PacSum<<8)+buf[1];

            int PacSeq=buf[2];
            PacSeq=(PacSeq<<8)+buf[3];

            int PacLen=buf[4];
            PacLen=(PacLen<<8)+buf[5];

            printf("PacSum:%d\t",PacSum);
            printf("PacSeq:%d\t",PacSeq);
            printf("PacLen:%d\n",PacLen);
            memcpy(pacque[PacSeq],buf,BUFF_LEN);//将buf中数据复制到pacque中
            if(PacSeq==PacSum)break;
        }
    }
}
/*将缓冲区里的数据发送至分光棱镜*/
void send_pac(int fd)
{
    printf("ip:%x\tport:%d",clent_addr.sin_addr.s_addr,clent_addr.sin_port);//打印分光棱镜的ip和端口
    socklen_t len;//地址结构体长度
    len = sizeof(clent_addr);//赋予len以地址结构体长度
    int PacSum=pacque[0][0];//保存包总数
    PacSum=(PacSum<<8)+pacque[0][1];
    for(int i=0;i<=PacSum;i++)
    {
        /*解析包头*/
        int PacSeq=pacque[i][2];
        PacSeq=(PacSeq<<8)+pacque[i][3];

        int PacLen=pacque[i][4];
        PacLen=(PacLen<<8)+pacque[i][5];

        printf("PacSum:%d\t",PacSum);
        printf("PacSeq:%d\t",PacSeq);
        printf("PacLen:%d\n",PacLen);
        sendto(fd, pacque[PacSeq], BUFF_LEN, 0, (struct sockaddr*)&clent_addr, len);  //发送信息给client，注意使用了clent_addr结构体指针
   	usleep(200000);
    }
}

int handle_client(int fd)
{
    socklen_t len= sizeof(clent_addr);
    char buf[100];  //接收缓冲区，1030字节
    char suc[5]="espok";
    char svr[]="notok";
    int count;
    int svrflg=0;
    while(!svrflg)
    {
        memset(buf, 0, 100);
        count = recvfrom(fd, buf, 100, 0, (struct sockaddr*)&clent_addr, &len);  //recvfrom是拥塞函数，没有数据就一直拥塞
        memcpy(svr,buf,5);
	    printf("client:%s\n",svr);
        printf("cmp:%d\n",strcmp(svr,"svrok"));
        if(!strcmp(svr,"svrok"))
        {
            svrflg=1;
        }
        else if(count)
        {
            printf("ip:%x\n",clent_addr.sin_addr.s_addr);  //打印ip(hex)
            printf("port:%d\n\n",clent_addr.sin_port);  //打印
            sendto(fd, suc, 5, 0, (struct sockaddr*)&clent_addr, len);  //发送信息给client，注意使用了clent_addr结构体指针
        }
    }
    return 1;
}

/*
    server:
            socket-->bind-->recvfrom-->sendto-->close
*/

int main(int argc, char* argv[])
{
    int server_fd, ret;
    struct sockaddr_in ser_addr;
    
    server_fd = socket(AF_INET, SOCK_DGRAM, 0); //AF_INET:IPV4;SOCK_DGRAM:UDP
    if(server_fd < 0)
    {
        printf("create socket fail!\n");
        return -1;
    }

    memset(&ser_addr, 0, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY); //IP地址，需要进行网络序转换，INADDR_ANY：本地地址
    ser_addr.sin_port = htons(SERVER_PORT);  //端口号，需要网络序转换


    ret = bind(server_fd, (struct sockaddr*)&ser_addr, sizeof(ser_addr));
    if(ret < 0)
    {
        printf("socket bind fail!\n");
        return -1;
    }
    int connect_flag=0;
while(1)
{
    printf("等待pc端发送数据包：\n");
    forward_msg(server_fd);   //处理接收到的数据
    printf("数据包接收完成！\n");
    if(connect_flag==0)
    {
    	printf("等待分光棱镜连接：\n");
    	handle_client(server_fd);
    	printf("connect ok!Start sending data to holocubic...\n");
    	connect_flag=1;
    }
    printf("开始向分光棱镜发送数据:\n");
    send_pac(server_fd);
    printf("数据发送完毕!");
}
    close(server_fd);
    return 0;
}
