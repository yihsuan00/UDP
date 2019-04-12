#include <iostream>
#include <sys/types.h>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include<fstream>
#include<map>
#include <sys/time.h> 
#include<cmath>
#include<ctime>
#include<stdio.h>
#include<time.h>
#include<arpa/inet.h>
#include<stdlib.h>

using namespace std;
typedef struct {
	int length;
	int seqNumber;
	int ackNumber;
	int fin;
	int syn;
	int ack;
} header;

typedef struct {
	header head;
	char data[1000];
} segment;

void setIP(char *dst, char *src) {
	if (strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0
		|| strcmp(src, "localhost")) {
		sscanf("127.0.0.1", "%s", dst);  //dst = 127.0.0.1,sscanf(要分析的字串,抓取的類型,變數名稱)
	}
	else {
		sscanf(src, "%s", dst);
	}
}
void bindaddress(int port, char* ip, struct sockaddr_in receiver,int receiversocket)
{
	receiver.sin_family = AF_INET;
	receiver.sin_port = htons(port);
	receiver.sin_addr.s_addr = inet_addr(ip);
	memset(receiver.sin_zero, '\0', sizeof(receiver.sin_zero));
	bind(receiversocket, (struct sockaddr *)&receiver, sizeof(receiver));
}
int main(int argc, char* argv[])
{
	int receiversocket, portNum, nBytes;
	int acknum = 0;
	segment* receiverbuffer = new segment[32];
	segment temp;
	struct sockaddr_in agent, receiver;
	socklen_t agent_size;
	char ip[2][50];
	int port[2];
	/*struct sockaddr_in {
	short            sin_family;   // 例如：AF_INET, AF_INET6
	unsigned short   sin_port;     // 例如：htons(3490)
	struct in_addr   sin_addr;     // 參考下列的 struct in_addr
	char             sin_zero[8];  // 若你想要的話，將這個設定為零
	};*/
	if (argc != 5) {
		fprintf(stderr, "用法: %s  <agent IP> <recv port> <agent port> <filename> \n", argv[0]);
		fprintf(stderr, "例如: ./receiver  local  8888 8889 \n");
		exit(1);
	}
	else {
		setIP(ip[0], "local");
		setIP(ip[1], argv[1]);
		sscanf(argv[2], "%d", &port[0]);
		sscanf(argv[3], "%d", &port[1]);
	}
	/*Create UDP socket*/
	receiversocket = socket(PF_INET, SOCK_DGRAM, 0);
	/*Configure settings in sender struct*/
	bindaddress(port[0], ip[0],receiver,receiversocket);

	/*Configure settings in agent struct*/
	agent.sin_family = AF_INET;
	agent.sin_port = htons(port[1]);
	agent.sin_addr.s_addr = inet_addr(ip[1]);
	memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));

	/*Initialize size variable to be used later on*/
	agent_size = sizeof(agent);
	int segment_size;
	int window = 0;
	ofstream file;
	agent_size = sizeof(agent);
	while (1)
	{
		segment_size = recvfrom(receiversocket, &temp, sizeof(temp), 0, (struct sockaddr *)&agent, &agent_size);
		if (segment_size > 0)
		{
			//代表收到data
			if (temp.head.seqNumber == acknum + 1)
			{
				//收到正確的資料
				if (window >= 32)
				{
					int drop = temp.head.seqNumber;
					printf("drop	data	#%d\n", drop);
					printf("flush\n");
					file.open(argv[4],ios::app | ios::binary);
					file.seekp(ios::end, ios::cur);
					for (int i = 0; i <window; i++)
					{
						file.write(receiverbuffer[i].data, receiverbuffer[i].head.length);
					}
					file.close();
					memset(receiverbuffer,'\0', 32);
					window = 0;
				}
				else {
					if (temp.head.fin == 1)
					{
						printf("recv    fin\n");
						temp.head.ack = 1;
						sendto(receiversocket, &temp, sizeof(temp), 0, (struct sockaddr *)&agent, agent_size);
						printf("send    finack\n");
						printf("flush\n");
						file.open(argv[4],ios::app | ios::binary);
						file.seekp(ios::end, ios::cur);
						for (int i = 0; i <window; i++)
						{
							file.write(receiverbuffer[i].data, receiverbuffer[i].head.length);
						}
						file.close();
						memset(receiverbuffer, '\0', 32);
						window = 0;
						break;
					 }
					printf("recv	data	#%d\n", temp.head.seqNumber);
					receiverbuffer[window] = temp;
					window++;
					acknum = temp.head.seqNumber;
					temp.head.ack = 1;
					temp.head.ackNumber = acknum;
					sendto(receiversocket, &temp, sizeof(temp), 0, (struct sockaddr *)&agent, agent_size);
					printf("send	ack	#%d\n", temp.head.seqNumber);
				}
			}
			else
			{
				if (temp.head.seqNumber > acknum + 1)
				{
					int drop = temp.head.seqNumber;
					printf("drop	data	#%d\n", drop);
					temp.head.ack = 1;
					temp.head.ackNumber = acknum;
					temp.head.seqNumber = acknum;
					sendto(receiversocket, &temp, sizeof(temp), 0, (struct sockaddr *)&agent, agent_size);
					printf("send	ack	#%d\n", acknum);
				}
				
			}

		}
		else
		{
			printf("receiving with problem\n");
		}

	}
}