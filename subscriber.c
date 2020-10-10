#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <zconf.h>
#include "helpers.h"

void usage()
{
    fprintf(stderr, "Rulare gresiat, format corect: ./subscriber <nume> <adresa> <server port>\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd, n, ret;
    struct sockaddr_in serv_addr;
    char buffer[MAXLEN];
    int flags = 1;

    TCP_response *reci;
    TCP_msg sent;

    if (argc < 4)
        usage();
    if (strlen(argv[1]) > 10)
        printf("Username too long, max size: 10, got:%ld\n", strlen(argv[1]));

    fd_set read_fds;
    fd_set tmp_fds;

    int fdmax;

    FD_ZERO(&tmp_fds);
    FD_ZERO(&read_fds);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    FD_SET(sockfd, &read_fds);
    fdmax = sockfd;
    FD_SET(0, &read_fds);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));
    ret = inet_aton(argv[2], &serv_addr.sin_addr);
    DIE(ret == 0, "inet_aton");

    ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "connect");
    ret = send(sockfd, argv[1], strlen(argv[1]) + 1, 0);
    DIE(ret < 0, "Name send error");
    //Pentru dezactivarea algoritmului lui Neagle
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flags, sizeof(int));

    while (1)
    {
        tmp_fds = read_fds;

        ret = select(sockfd + 1, &tmp_fds, NULL, NULL, NULL);
        if (FD_ISSET(0, &tmp_fds))
        {
            //ne asiguram ca bufferul e gol inate sa scriem in el
            memset(buffer, 0, MAXLEN);
            //citim comada de la client
            fgets(buffer, BUFLEN - 1, stdin);

            //verficam daca acesta doreste sa inchida
            if (!strncmp(buffer, "exit", 4))
                break;

            //daca nu este mesaj ce iesire atunci folosim functia msg sa facem un mesaj
            //de sub/unsub pentru server
            if (msg(buffer, &sent))
            {
                // se trimite mesaj la server
                n = send(sockfd, (char *)&sent, sizeof(sent), 0);
                DIE(n < 0, "send");

                if (sent.cmd_type)
                    printf("Subscribed to topic: %s\n", sent.name);
                else
                    printf("Unscribed from topic: %s\n", sent.name);
            }
        }

        if (FD_ISSET(sockfd, &tmp_fds))
        {
            //Primim o notificare
            //ne asiguram ca bufferul e gol inate sa scriem in el
            memset(buffer, 0, MAXLEN);
            n = recv(sockfd, buffer, sizeof(TCP_response), 0);
            //verficam pentru erori
            DIE(n < 0, "receive");
            printf("notif1\n");
            //inchidem clientul
            if (!n)
                break;

            //interpretam mesajul primit de la server
            reci = (TCP_response *)buffer;
            printf("%s:%hu --- %s --- %s --- %s\n", reci->ip_addr, reci->port_udp,
                   reci->name, reci->data_type, reci->content);
        }
    }
    //shutdown(sockfd, SHUT_RDWR);
    close(sockfd);

    return 0;
}