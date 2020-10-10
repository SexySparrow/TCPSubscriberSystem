#ifndef HELPERS_H
#define HELPERS_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define DIE(assertion, call_description)  \
	do                                    \
	{                                     \
		if (assertion)                    \
		{                                 \
			fprintf(stderr, "(%s, %d): ", \
					__FILE__, __LINE__);  \
			perror(call_description);     \
			exit(EXIT_FAILURE);           \
		}                                 \
	} while (0)

#define BUFLEN 1552	  // dimensiunea maxima a calupului de date
//#define MAX_CLIENTS 5 // numarul maxim de clienti in asteptare
#define MAXLEN (sizeof(TCP_response) + 1)  //numarul maxim de clienti si topicuri

#endif

typedef struct client
{
	int id;
	char name[50];
	int port;
	struct in_addr addr;
} Client;

typedef struct tcp_msg
{
	_Bool SF;
	_Bool cmd_type;
	char name[51];
} TCP_msg;

typedef struct udp_msg
{
	char name[50];
	uint8_t type;
	char content[1501];
} UDP_msg;

typedef struct tcp_response
{
	char ip_addr[16];
	uint16_t port_udp;
	char name[51];
	char data_type[11];
	char content[1501];
} TCP_response;

_Bool msg(char *buffer, TCP_msg *msg)
{

	buffer[strlen(buffer) - 1] = 0;
	char delim[] = " ";
	char *command = strtok(buffer, delim);
	DIE(command == NULL, "Invalid Command");

	if (command[0] == 's')
		msg->cmd_type = 1;
	else
		msg->cmd_type = 0;

	command = strtok(NULL, delim);
	DIE(command == NULL, "Invalid Command");
	DIE(strlen(command) > 50, "Invalid Topic name");
	strcpy(msg->name, command);

	if (msg->cmd_type)
	{
		command = strtok(NULL, " ");
		DIE(command == NULL, "Invalid Command");
		msg->SF = command[0] - '0';
	}
	return 1;
}


typedef struct subscribe
{
	char name[51];
	_Bool on;
} Subscribe;

typedef struct topic
{
	_Bool sf;
	int msg;
	char name[51];
} TOPIC;


typedef struct client_topic
{
	char name[51];
	int count;
} CLI_COUNT;

_Bool msg_udp(UDP_msg *reci, TCP_response *sent)
{
	DIE(reci->type > 3, "Got wrong message");

	double real_num;
	long long int_num;

	strncpy(sent->name, reci->name, 50);
	sent->name[50] = 0;

	if (reci->type)
	{
		if (reci->type == 1)
		{
			real_num = ntohs(*(uint16_t *)(reci->content));
			real_num /= 100;

			strcpy(sent->data_type, "SHORT_REAL");
			sprintf(sent->content, "%.2f", real_num);
		}
		else
		{
			if (reci->type == 2)
			{
				real_num = ntohl(*(uint32_t *)(reci->content + 1));
				long long power = 10;
				for(int h = 0; h < reci->content[5]; h++)
					real_num /=power;
				//real_num /= pow(10, reci->content[5]);

				if (reci->content[0])
				{
					real_num *= -1;
				}
				strcpy(sent->data_type, "FLOAT");
				sprintf(sent->content, "%lf", real_num);
			}
			else
			{

				strcpy(sent->data_type, "STRING");
				strcpy(sent->content, reci->content);
			}
		}
	}
	else
	{
		DIE(reci->content[0] > 1, "Worng sign Error");
		int_num = ntohl(*(uint32_t *)(reci->content + 1));

		if (reci->content[0])
		{
			int_num *= -1;
		}

		strcpy(sent->data_type, "INT");
		sprintf(sent->content, "%lld", int_num);
	}

	return true;
}


