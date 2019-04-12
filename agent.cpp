/* NOTE: ���{���Ȩ�����榡�A�å��ˬd congestion control �P go back N */
/* NOTE: UDP socket is connectionless�A�C���ǰe�T�����n���wip�P��f */
/* HINT: ��ĳ�ϥ� sys/socket.h ���� bind, sendto, recvfrom */

/*
* �s�u�W�d�G
* �����@�~�� agent, sender, receiver ���|�j�w(bind)�@�� UDP socket ��U�۪���f�A�Ψӱ����T���C
* agent�����T���ɡA�H�o�H����}�P��f�ӿ�{�T���Ӧ�sender�٬Oreceiver�A�P�ǭ̫h�L�ݧ@���P�_�A�]���Ҧ��T�����w�Ӧ�agent�C
* �o�e�T���ɡAsender & receiver ���ݹw�����D agent ����}�P��f�A�M��Υ��e�j�w�� socket �ǰT�� agent �Y�i�C
* (�p agent.c ��126��)
*/

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>

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

//strcmp�O�ΨӤ���r��O�_�۵�
void setIP(char *dst, char *src) {
	if (strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0
		|| strcmp(src, "localhost")) {
		sscanf("127.0.0.1", "%s", dst);  //dst = 127.0.0.1,sscanf(�n���R���r��,���������,�ܼƦW��)
	}
	else {
		sscanf(src, "%s", dst);
	}
}

int main(int argc, char* argv[]) {
	int agentsocket, portNum, nBytes;
	float loss_rate;
	segment s_tmp;
	struct sockaddr_in sender, agent, receiver, tmp_addr;
	socklen_t sender_size, recv_size, tmp_size;
	char ip[3][50];
	int port[3], i;
	/*struct sockaddr_in {
	short            sin_family;   // �Ҧp�GAF_INET, AF_INET6
	unsigned short   sin_port;     // �Ҧp�Ghtons(3490)
	struct in_addr   sin_addr;     // �ѦҤU�C�� struct in_addr
	char             sin_zero[8];  // �Y�A�Q�n���ܡA�N�o�ӳ]�w���s
	};*/
	if (argc != 7) {
		fprintf(stderr, "�Ϊk: %s <sender IP> <recv IP> <sender port> <agent port> <recv port> <loss_rate>\n", argv[0]);
		fprintf(stderr, "�Ҧp: ./agent local local 8887 8888 8889 0.3\n");
		exit(1);
	}
	else {
		setIP(ip[0], argv[1]);
		setIP(ip[1], "local");
		setIP(ip[2], argv[2]);

		sscanf(argv[3], "%d", &port[0]);
		sscanf(argv[4], "%d", &port[1]);
		sscanf(argv[5], "%d", &port[2]);

		sscanf(argv[6], "%f", &loss_rate);
	}

	/*Create UDP socket*/
	agentsocket = socket(PF_INET, SOCK_DGRAM, 0);

	/*Configure settings in sender struct*/
	sender.sin_family = AF_INET;
	sender.sin_port = htons(port[0]);
	sender.sin_addr.s_addr = inet_addr(ip[0]);
	memset(sender.sin_zero, '\0', sizeof(sender.sin_zero));

	/*Configure settings in agent struct*/
	agent.sin_family = AF_INET;
	agent.sin_port = htons(port[1]);
	agent.sin_addr.s_addr = inet_addr(ip[1]);
	memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));

	/*bind socket*/
	bind(agentsocket, (struct sockaddr *)&agent, sizeof(agent));

	/*Configure settings in receiver struct*/
	receiver.sin_family = AF_INET;
	receiver.sin_port = htons(port[2]);
	receiver.sin_addr.s_addr = inet_addr(ip[2]);
	//sin_zero��??sockaddr_in?�۶�R���Ostruct sockaddr�P?��?�סA�i�H��bzero()��memset()��??��m?�s
	memset(receiver.sin_zero, '\0', sizeof(receiver.sin_zero));

	/*Initialize size variable to be used later on*/
	sender_size = sizeof(sender);
	recv_size = sizeof(receiver);
	tmp_size = sizeof(tmp_addr);

	printf("�i�H�}�l���o^Q^\n");
	printf("sender info: ip = %s port = %d and receiver info: ip = %s port = %d\n", ip[0], port[0], ip[2], port[2]);
	printf("agent info: ip = %s port = %d\n", ip[1], port[1]);

	int total_data = 0;
	int drop_data = 0;
	int segment_size, index;
	char ipfrom[1000];
	char *ptr;
	int portfrom;
	srand(time(NULL));
	while (1) {
		/*Receive message from receiver and sender*/
		memset(&s_tmp, 0, sizeof(s_tmp));//��s_tmp�ҥe�s���F����M��
		segment_size = recvfrom(agentsocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&tmp_addr, &tmp_size);
		//���\����T��
		if (segment_size > 0) {
			inet_ntop(AF_INET, &tmp_addr.sin_addr.s_addr, ipfrom, sizeof(ipfrom));
			portfrom = ntohs(tmp_addr.sin_port);
			//ipfrom�x�sip�r��A�Q��strcmp�P�_�ӷ�(���ӼƬ۵��ɷ|��X0)
			if (strcmp(ipfrom, ip[0]) == 0 && portfrom == port[0]) {
				/*segment from sender, not ack*/
				if (s_tmp.head.ack) {
					fprintf(stderr, "����Ӧ� sender �� ack segment\n");
					exit(1);
				}
				total_data++;
				if (s_tmp.head.fin == 1) {
					printf("get     fin\n");
					sendto(agentsocket, &s_tmp, segment_size, 0, (struct sockaddr *)&receiver, recv_size);
					printf("fwd     fin\n");
				}
				else {
					index = s_tmp.head.seqNumber;
					if (rand() % 100 < 100 * loss_rate) {
						drop_data++;
						printf("drop	data	#%d,	loss rate = %.4f\n", index, (float)drop_data / total_data);
					}
					else {
						printf("get	data	#%d\n", index);
						sendto(agentsocket, &s_tmp, segment_size, 0, (struct sockaddr *)&receiver, recv_size);
						printf("fwd	data	#%d,	loss rate = %.4f\n", index, (float)drop_data / total_data);
					}
				}
			}
			else if (strcmp(ipfrom, ip[2]) == 0 && portfrom == port[2]) {
				/*segment from receiver, ack*/
				if (s_tmp.head.ack == 0) {
					fprintf(stderr, "����Ӧ� receiver �� non-ack segment\n");
					exit(1);
				}
				if (s_tmp.head.fin == 1) {
					printf("get     finack\n");
					sendto(agentsocket, &s_tmp, segment_size, 0, (struct sockaddr *)&sender, sender_size);
					printf("fwd     finack\n");
					break;
				}
				else {
					index = s_tmp.head.seqNumber;
					printf("get     ack	#%d\n", index);
					sendto(agentsocket, &s_tmp, segment_size, 0, (struct sockaddr *)&sender, sender_size);
					printf("fwd     ack	#%d\n", index);
				}
			}
		}
	}

	return 0;
}
