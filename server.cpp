#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <zconf.h>
#include "helpers.h"
#include <vector>
#include <unordered_map>
#include <math.h>
#include <set>

using namespace std;

typedef struct message_buffer
{
    char name[11];
    vector<TCP_response> buffer;
} MSG_BUF;

typedef struct subscriber_set
{
    char name[50];
    vector<int> clients;
} SUB_SET;

typedef struct clients
{
    char name[11];
    int file;
    unordered_map<string, bool> topics;
} Clients;

typedef struct offline_client
{
    char name[11];
    vector<TOPIC> topics;
} OFF_CLI;

// abonari pentru clientii offline
unordered_map<string, vector<TOPIC>> offline_topics;
// bufer pentru memorarea mesajelor urilor cand clientii sunt offline
unordered_map<string, vector<TCP_response>> client_buffers;
//mapa pentru legarea topicurilor de file descriptorii clientilor
unordered_map<string, set<int>> topic_clients;
//"vectorul de frecventa" pentru utilizarea topics urilor
unordered_map<string, int> topic_usage;
//legatura intre file descriptori si datele clientului
unordered_map<int, Clients> clients;


Clients crt_client;

void usage(char *file)
{
    fprintf(stderr, "Servah is usin' pawrt: %s Bloody oath cobber.\n", file);
    exit(0);
}
//trecem userul in modul offline si ne asiguram ca nu pierdem date
void switch_subsciption(int fd)
{
    //facem o trecere intre starea de user online in starea de offline
    //adica mutam abonarile active in map ul de useri inactivi pentru a
    //avea apoi datele la reconectare
    for (auto &elem : clients[fd].topics)
    {
        //dezabonam utilizatorul din map-ul online
        if (topic_clients[elem.first].size() > 0)
        {
            topic_clients[elem.first].erase(fd);
        }
        //mutam topicul(creand o copie) in map ul clientilor offline
        if (elem.second)
        {
            TOPIC top;
            strcpy(top.name, elem.first.c_str());
            top.msg = client_buffers[elem.first].size();
            top.sf = true;
            offline_topics[clients[fd].name].emplace_back(top);
            topic_usage[elem.first]++;
            clients.erase(fd);
        }
        else
        {
            TOPIC top;
            strcpy(top.name, elem.first.c_str());
            top.msg = -1;
            top.sf = 0;
            offline_topics[clients[fd].name].emplace_back(top);
            clients.erase(fd);
        }
    }
}
void msg_buf_SF(int file, char *name)
{
    int size;
    //parcurgem topicurile salvate si trimitem mesajele unde e cazul(SF e 1)
    for (auto &topic_elem : offline_topics[name])
    {
        if (topic_elem.sf)
        {
            vector<TCP_response> &elems = client_buffers[topic_elem.name];
            size = elems.size();
            int ret;

            for (int j = topic_elem.msg; j < size; ++j)
                ret = send(file, (char *)&elems[j], sizeof(TCP_response), 0);

            //scoatem mesajul dupa ce l am trimis, 
            topic_usage[topic_elem.name]--;
            if (topic_usage[topic_elem.name])
            {
                string name(topic_elem.name);
                int ok = topic_elem.msg;
                vector<TCP_response> &msg_buffer = client_buffers[name];
                // gasim cel mai mic index
                for (auto &entry : offline_topics)
                    for (TOPIC &elem : entry.second)
                        if (elem.name == name && elem.msg < ok)
                            ok = elem.msg;
                // daca e 0 inseamna ca nu mai avem clienti care trebuie updatati
                if (ok == 0)
                    return;
                // stergem toate mesajele de la start la minim
                msg_buffer.erase(msg_buffer.begin(), msg_buffer.begin() + ok);

                // se modifica indecsi care vor fi folositi ca sa trimitem mesale la intoarecerea unui utilizator
                for (auto &entry : offline_topics)
                    for (TOPIC &elem : entry.second)
                        if (elem.name == name)
                            elem.msg -= ok;
            }
            else
                client_buffers.erase(topic_elem.name);
        }
        topic_clients[topic_elem.name].insert(file);
        crt_client.topics.insert({topic_elem.name, topic_elem.sf});
    }
}

int main(int argc, char *argv[])
{
    DIE(argc == 1, "Use a port");
    DIE(argc > 2, "Wrong usage Error");

    char buffer[BUFLEN];
    UDP_msg *udp_msg;
    TCP_response tcp_msg;
    TCP_msg *client_msg;
    fd_set read_fds, tmp_fds;
    int sock_udp, portno, sock_tcp, fdmax, sock_client, n, flag = 1;
    string topic;
    socklen_t udp_socklen = sizeof(sockaddr), tcp_socklen = sizeof(sockaddr);
    sockaddr_in udp_addr, tcp_addr, cl_addr;
    int ret;
    FD_ZERO(&read_fds);
    // creeam socketi
    sock_tcp = socket(AF_INET, SOCK_STREAM, 0);
    sock_udp = socket(PF_INET, SOCK_DGRAM, 0);
    DIE(sock_udp < 0, "UDP Error\n");
    DIE(sock_tcp < 0, "TCP Error\n");

    portno = atoi(argv[1]);
    // completam structurile pt UDP si TCP
    tcp_addr.sin_family = AF_INET;
    udp_addr.sin_family = AF_INET;
    tcp_addr.sin_port = htons(portno);
    udp_addr.sin_port = htons(portno);
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    //legam socketi
    ret = bind(sock_tcp, (sockaddr *)&tcp_addr, sizeof(sockaddr_in));
    ret = bind(sock_udp, (sockaddr *)&udp_addr, sizeof(sockaddr_in));
    DIE(ret < 0, "UDP bind Error\n");
    DIE(ret < 0, "TCP bind Error\n");
    ret = listen(sock_tcp, 999999);
    DIE(ret < 0, "Listen error\n");

    FD_SET(sock_tcp, &read_fds);
    FD_SET(sock_udp, &read_fds);
    FD_SET(0, &read_fds);
    fdmax = sock_tcp;
    while (1)
    {
        tmp_fds = read_fds;
        memset(buffer, 0, BUFLEN);
        ret = select(fdmax + 1, &tmp_fds, nullptr, nullptr, nullptr);
        DIE(ret < 0, "Unable to select.\n");

        for (int i = 0; i <= fdmax; ++i)
        {
            if (FD_ISSET(i, &tmp_fds))
            {
                if (i == sock_tcp)
                {
                    // comanda de la un client TCP
                    sock_client = accept(i, (sockaddr *)&cl_addr, &tcp_socklen);
                    DIE(sock_client < 0, "Client connection Error\n");
                    //Pentru dezactivarea algoritmului lui Neagle
                    setsockopt(sock_client, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
                    FD_SET(sock_client, &read_fds);

                    if (sock_client > fdmax)
                        //socketul curent este mai mare
                        fdmax = sock_client;
                    ret = recv(sock_client, buffer, BUFLEN - 1, 0);
                    DIE(ret < 0, "Client name Error\n");

                    strcpy(crt_client.name, buffer);
                    crt_client.topics.clear();

                    // se trimite ce s-a bufferat
                    msg_buf_SF(sock_client, buffer);

                    // se se updateaza hash-ul pentru clienti si cel pentru topicuri
                    clients.insert({sock_client, crt_client});
                    offline_topics.erase(buffer);

                    printf("New connection from client %s via %s:%hu.\n", buffer,
                           inet_ntoa(cl_addr.sin_addr), ntohs(cl_addr.sin_port));
                }
                if (i == sock_udp)
                {
                    // vin date de la UDP
                    ret = recvfrom(sock_udp, buffer, BUFLEN, 0, (sockaddr *)&udp_addr, &udp_socklen);
                    DIE(ret < 0, "UDP data Error\n");

                    //interpretam mesajul ca sa il putem redirectiona la clienti
                    tcp_msg.port_udp = ntohs(udp_addr.sin_port);
                    strcpy(tcp_msg.ip_addr, inet_ntoa(udp_addr.sin_addr));
                    udp_msg = (UDP_msg *)buffer;
                    printf("UDP-1\n");
                    if (msg_udp(udp_msg, &tcp_msg))
                    {
                        printf("UDP-2\n");
                        if (client_buffers.find(tcp_msg.name) != client_buffers.end())
                        {
                            // daca topicul exista in hashul de buffere, mesajul trebuie bufferat
                            client_buffers[tcp_msg.name].push_back(tcp_msg);
                        }
                        if (topic_clients.find(tcp_msg.name) != topic_clients.end())
                        {
                            printf("UDP-3\n");
                            // transferam mesajul abonatior
                            for (int fd : topic_clients[tcp_msg.name])
                            {
                                printf("UDP-4\n");
                                ret = send(fd, (char *)&tcp_msg, sizeof(TCP_response), 0);
                                DIE(ret < 0, "TCP send Error");
                            }
                        }
                    }
                }
                if (i != sock_tcp && i != sock_udp && i)
                {
                    // se primeste o comanda de la un client TCP
                    n = recv(i, buffer, BUFLEN - 1, 0);
                    DIE(n < 0, "TCP recv Error\n");

                    if (!n)
                    {
                        printf("Client %s disconnected\n", clients[i].name);

                        FD_CLR(i, &read_fds);
                        int start = fdmax;
                        while (FD_ISSET(start, &read_fds))
                            start--;
                        fdmax = start;
                        printf("Clients wants to leave\n");
                        switch_subsciption(i);
                        printf("Client left\n");
                        close(i);
                    }
                    else
                    {
                        
                        client_msg = (TCP_msg *)buffer;

                        if (client_msg->cmd_type)
                        {
                            // a venit o comanda subscribe
                            clients[i].topics[client_msg->name] = client_msg->SF;
                            topic_clients[client_msg->name].insert(i);
                            continue;
                        }
                        else
                        {
                            auto topic = topic_clients.find(client_msg->name);
                            //a venit o comanda unsubscribe
                            if (topic != topic_clients.end() && topic->second.find(i) != topic->second.end())
                            {
                                topic->second.erase(i);
                                clients[i].topics.erase(client_msg->name);
                            }
                            continue;
                        }
                    }
                }
                if (!i)
                {
                    // comanda de la tastatura
                    fgets(buffer, BUFLEN - 1, stdin);

                    if (!strncmp(buffer, "exit", 4))
                    {
                        int index = 3;
                        while (index != fdmax)
                        {
                            if (FD_ISSET(index, &read_fds))
                                close(index);
                            index++;
                        }
                        if (FD_ISSET(fdmax, &read_fds))
                            close(fdmax);
                        return 0;
                    }
                }
            }
        }
    }
    return 0;
}
