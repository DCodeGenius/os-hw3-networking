//
// Created by danie on 26/12/2025.
//

#ifndef SERVER_CLIENTS_H
#define SERVER_CLIENTS_H
#include <netinet/in.h>   // struct sockaddr_in
#include <sys/types.h>
#include <common.h>



typedef struct Client {
    int  fd;                          // accepted socket fd (server side)
    char name[MAX_NAME_LEN + 1];          // client name (null-terminated)
    char ip[MAX_IP_LEN + 1];              // printable IP string (e.g., "192.168.1.5")
    int  in_use;                      // 0 = free slot, 1 = occupied
} Client;

typedef struct ClientsTable {
    Client arr[MAX_CLIENTS];                      // array of clients
    int count;                    // number of active clients
} ClientsTable;

int AddClient(int fd,char* name, char* ip);
int RemoveClient(int fd);
char* GetClientName(int fd);
void InitClients();
static void disconnect_fd(int fd, int listen_fd);

#endif //SERVER_CLIENTS_H
