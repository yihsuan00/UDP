#include <iostream>
#include <sys/types.h>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include<sstream>
#include<fstream>
#include <sys/time.h> 
#include<ctime>
#include<stdio.h>
#include<time.h>
#include<arpa/inet.h>
#include<stdlib.h>
#include<memory.h>
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

struct timeval timeoutInterval;
void setIP(char *dst, char *src) {
	if (strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0
		|| strcmp(src, "localhost")) {
		sscanf("127.0.0.1", "%s", dst);  //dst = 127.0.0.1,sscanf(要分析的字串,抓取的類型,變數名稱)
	}
	else {
		sscanf(src, "%s", dst);
	}
}
void bindaddress(int port, char* ip, struct sockaddr_in sender,int sendersocket)
{
	sender.sin_family = AF_INET;
	sender.sin_port = htons(port);
	sender.sin_addr.s_addr = inet_addr(ip);
	memset(sender.sin_zero, '\0', sizeof(sender.sin_zero));
	bind(sendersocket, (struct sockaddr *)&sender, sizeof(sender));
}
int getfilesize(char* filename)
{
	ifstream is;
	is.open(filename, ios::binary);
	is.seekg(0, ifstream::end);
	int size = is.tellg();
	is.seekg(0);
	is.close();
	return size;
}
segment setHeader(int len, int seqnum, int acknum, int fin, int syn, int ack, char* datause)
{
	segment a;
	memcpy(a.data, datause, 1000);
	a.head.length = len;
	a.head.seqNumber = seqnum;
	a.head.ackNumber = acknum;
	a.head.fin = fin;
	a.head.syn = syn;
	a.head.ack = ack;
	return a;
}
int getsize(char*filename)
{
	int size= getfilesize(filename);
	int num = size / 1000;
	if (size % 1000 != 0)
	{
		num = num + 1;
	}
	return num;
}
segment* createseg(char *filename, int ack)
{
	ifstream openfile;
	int Mss = 1000;
	int size = getfilesize(filename);
	//這個文件的長度大小
	int num = size / Mss;//總共會占幾個MSS(跟window size有關)
	char* data = new char[1000];
	char* fileContent;
	int seqnum = ack + 1;
	segment* sendbuffer;
	if (!openfile)
	{
		printf("wrong in opening file\n");
	}
	//要有一個暫時儲存的buffer來存
	else {
		openfile.open(filename, ios::binary);
		fileContent = new char[size];
		openfile.read(fileContent, size);
		openfile.close();
		if (size % Mss == 0)
		{
			sendbuffer = new segment[num];
		}
		else
		{
			sendbuffer = new segment[num + 1];
			int i = 0;
			for (int k = Mss*num; k < size; k++)
			{
				data[i] = fileContent[k];
				i++;
			}
			sendbuffer[num] = setHeader(size%Mss, num + 1, 0, 0, 0, 0, data);
		}
		memset(data, '\0', 1000 * sizeof(char));
		for (int i = 0; i < num; i++)
		{
			for (int j = 0; j < Mss; j++)
			{
				data[j] = fileContent[1000 * i + j];
			}
			sendbuffer[i] = setHeader(Mss, seqnum, 0, 0, 0, 0, data);
			seqnum++;
		}
	  }
	return sendbuffer;
}
struct timeval timediff(struct timeval &t1, struct timeval &t2)
{
	struct timeval t3;
	t3.tv_sec = t1.tv_sec - t2.tv_sec;
	t3.tv_usec = t1.tv_usec-t2.tv_usec;
	return t3;
}

int main(int argc, char* argv[])
{
	int sendersocket, portNum, nBytes;
	int acknum = 0;
	segment* senderbuffer ;
	segment temp;
	struct sockaddr_in agent, sender;
	socklen_t agent_size;
	char ip[2][50];
	int port[2];
	if (argc != 5) {
		fprintf(stderr, "用法: %s  <agent IP> <send port> <agent port> <data>\n", argv[0]);
		fprintf(stderr, "例如: ./sender  local  8888 8889  hello.txt\n");
		exit(1);
	}
	else {
		setIP(ip[0], "local");
		setIP(ip[1], argv[1]);
		sscanf(argv[2], "%d", &port[0]);
		sscanf(argv[3], "%d", &port[1]);
	}
	/*Create UDP socket*/
	sendersocket= socket(PF_INET, SOCK_DGRAM, 0);
	/*Configure settings in sender struct*/
	bindaddress(port[0], ip[0],sender,sendersocket);

	/*Configure settings in agent struct*/
	agent.sin_family = AF_INET;
	agent.sin_port = htons(port[1]);
	agent.sin_addr.s_addr = inet_addr(ip[1]);
	memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));
	int cwnd = 1;
	int threshold = 16;
	int ack = 0;
	int base = 0;
	int goal = 0;
	int last = 0;
	int segment_size;
	timeoutInterval.tv_sec = 0;
	timeoutInterval.tv_usec = 1000000;
	struct timeval start;
	struct timeval end;
	agent_size = sizeof(agent);
	fd_set fds;//select會用到的參數
	int segnum = getsize(argv[4]);
	senderbuffer = createseg(argv[4], ack);	
	bool havesend = false;
	int times = 0;
	int reacknum = 0;
	//int val = 0;
	int ret = 0;
	int i = 1;
	while (1) {
		while (base < segnum)
		{
			while (times < cwnd)
			{//要先把window該傳的資料傳出在去收ack
				if (havesend)
				{
						printf("resend	data	#%d    winsize= %d\n", senderbuffer[base+times].head.seqNumber, cwnd);
						sendto(sendersocket, &senderbuffer[base + times], sizeof(senderbuffer[base + times]), 0, (struct sockaddr *)&agent, agent_size);
						last = senderbuffer[base + times].head.seqNumber;
						times++;	
						if (last == goal) { havesend = false; }
						if (last == segnum) { times = cwnd; havesend = false; }
				}
				else
				{
					printf("send	data	#%d    winsize= %d\n", senderbuffer[base +times].head.seqNumber, cwnd);
					sendto(sendersocket, &senderbuffer[base + times], sizeof(senderbuffer[base + times]), 0, (struct sockaddr *)&agent, agent_size);
					last = senderbuffer[base + times].head.seqNumber;
					if (last == segnum) { times = cwnd; }
					times++;
				}
			}
			gettimeofday(&start, NULL);
		    setsockopt(sendersocket, SOL_SOCKET, SO_RCVTIMEO, &timeoutInterval, sizeof(struct timeval));
			ret = recvfrom(sendersocket, &temp, sizeof(temp), 0, (struct sockaddr *)&agent, &agent_size);
			if (ret == -1)
			{
				if (errno == EAGAIN)
				{
					threshold = (cwnd) / 2;
					base = acknum;
					if (!havesend)
					{
						goal = last;
					}
					last = acknum;
					if (threshold < 1) { threshold = 1; }
					printf("time	out	       threshold = %d\n", threshold);
					havesend = true;
					times = 0;
					cwnd = 1;
					reacknum = 0;
					timeoutInterval.tv_sec = 0;
					timeoutInterval.tv_usec = 1000000;;
				}
			}
			else
			{
				if (ret > 0)
				{//收到ack;
                    struct timeval diff;
					if (temp.head.ackNumber > acknum)
					{	
						acknum = temp.head.ackNumber;
						base = temp.head.ackNumber;
						printf("recv	ack	#%d\n", temp.head.ackNumber);
						reacknum++;
						gettimeofday(&end, NULL);
						diff = timediff(end, start);
						timeoutInterval.tv_sec = timeoutInterval.tv_sec - diff.tv_sec;
						timeoutInterval.tv_usec = timeoutInterval.tv_usec - diff.tv_usec;
					}
					else
					{
						diff = timediff(end, start);
						timeoutInterval.tv_sec = timeoutInterval.tv_sec - diff.tv_sec;
						timeoutInterval.tv_usec = timeoutInterval.tv_usec - diff.tv_usec;
						printf("recv	ack	#%d\n", temp.head.ackNumber);
					}
					if (reacknum == cwnd)//代表都收到了
					{
						if (cwnd < threshold) { cwnd = 2 * cwnd; }
						else
						{
							cwnd = cwnd + 1;
						}
						times = 0;reacknum = 0;
						timeoutInterval.tv_sec = 0;
						timeoutInterval.tv_usec = 1000000;;
					}
				}

			}
			
		}
		timeoutInterval.tv_sec = 1;
		timeoutInterval.tv_usec = 0;
		char*a = new char[1000];
		segment finr = setHeader(1000, last+1, 0, 1, 0, 0, a);
		sendto(sendersocket, &finr, sizeof(finr), 0, (struct sockaddr *)&agent, agent_size);
		printf("send    fin\n");
		FD_ZERO(&fds);
		FD_CLR(sendersocket, &fds);
		FD_SET(sendersocket, &fds);
		int t = select(sendersocket + 1, &fds, NULL, NULL, &timeoutInterval);
		if (t == 0) { printf("time	out	      threshold = %d\n", threshold); continue; }
		if (FD_ISSET(sendersocket, &fds) && t == 1)
		{
			recvfrom(sendersocket, &temp, sizeof(temp), 0, (struct sockaddr *)&agent, &agent_size);
			if (temp.head.ack = 1&&temp.head.fin==1)
			{ 
				printf("recv    finack\n");
				break; 
			}
		}
	}
}




