//
// Created by danie on 26/12/2025.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include "server_clients.h"
#include "protocol.h"


fd_set read_set;
fd_set master_set; //used to keep a copy of read_set
int max_fd;

int GetAndParseClientName(int client_fd, char *out_name, size_t maxlen)
{
    int pos = 0;

    while (pos < (int)maxlen - 1) {
        char c;
        int r = recv(client_fd, &c, 1, 0);

        if (r < 0) {
            perror("recv(name)");
            return -1;
        }
        if (r == 0) {
            /* client disconnected before sending name */
            return -1;
        }
        if (c == '\n')
            break;
        if (c == '\r')
            continue;

        out_name[pos++] = c;
    }

    out_name[pos] = '\0';

    if (pos == 0) {
        /* empty name not allowed */
        return -1;
    }

    return 0;
}

static void disconnect_fd(int fd, int listen_fd)
{
    /* find name for log (optional, but nice) */
    const char *name = NULL;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients.arr[i].in_use && clients.arr[i].fd == fd) {
            name = clients.arr[i].name;
            break;
        }
    }
    if (name) printf("client %s disconnected\n", name);

    FD_CLR(fd, &master_set);
    close(fd);
    RemoveClient(fd);

    if (fd == max_fd) {
        max_fd = listen_fd;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients.arr[i].in_use && clients.arr[i].fd > max_fd)
                max_fd = clients.arr[i].fd;
        }
    }
}



int main(int argc, char **argv){
  if (argc != 2){
      fprintf(stderr, "error command, wrong number of args\n");
      exit(1);
  };
    InitClients();

    int listen_fd;
    int port;
    struct sockaddr_in serv_addr;


    port = atoi(argv[1]);

    if (port <= 0 || port > 65535) {
        fprintf(stderr, "invalid port\n");
        exit(1);
    }
    /* create TCP socket */
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(listen_fd);
        exit(1);
    }

    /* build server address */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port        = htons(port);

    /* bind socket to port */
    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(listen_fd);
        exit(1);
    }


    FD_ZERO(&master_set);
    FD_SET(listen_fd, &master_set);
    max_fd = listen_fd;
    if (listen(listen_fd, SOMAXCONN) < 0) {
        perror("listen");
        exit(1);
    };
    while(1){
      read_set = master_set;
      int ready = select(max_fd + 1, &read_set, NULL, NULL, NULL); //TODO check error handling here
      if (ready < 0) {
        perror("select");
        continue;
      }

      if (FD_ISSET(listen_fd, &read_set)){ //start of accept new client block
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);

        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (client_fd < 0) {
          perror("accept");
          continue;
        }
          char ip[MAX_IP_LEN];
          if (inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip)) == NULL) {
              perror("inet_ntop");
              close(client_fd);
              continue;
          }
          char name[MAX_NAME_LEN];
          if (GetAndParseClientName(client_fd, name, sizeof(name)) < 0) {
              close(client_fd);
              continue;
          }
          if (AddClient(client_fd, name, ip) < 0) { close(client_fd); continue; }
          printf("client %s connected from %s\n", name, ip);
          FD_SET(client_fd, &master_set);
          if (client_fd > max_fd) max_fd = client_fd;


      } //end of accept new client block

      for(int i=0; i < MAX_CLIENTS; i++){ //Start of checking for every client if it had sent a message
        if (clients.arr[i].in_use == 1 && FD_ISSET(clients.arr[i].fd, &read_set)){
            int sender_fd = clients.arr[i].fd;
            const char *sender_name = clients.arr[i].name;

            char inbuf[1024];
            int n = recv(sender_fd, inbuf, sizeof(inbuf) - 1, 0);

            if (n == 0) {
                disconnect_fd(sender_fd, listen_fd);
                continue;
            }
            else if (n < 0) {
                if (errno != ECONNRESET) perror("recv");
                disconnect_fd(sender_fd, listen_fd);
                continue;
            }

            else {
                /* got data */
                inbuf[n] = '\0';

                /* (optional) ignore empty lines */
                if (inbuf[0] == '\0') {
                    /* do nothing */
                } else {

                    /* Build outgoing message: "sender: " + original */
                    char outbuf[1200]; /* enough for prefix + message */
                    int outlen = snprintf(outbuf, sizeof(outbuf), "%s: %s", sender_name, inbuf);
                    if (outlen < 0) outlen = 0;
                    if (outlen >= (int)sizeof(outbuf)) outlen = (int)sizeof(outbuf) - 1;

                    /* Check whisper syntax: starts with '@' */
                    if (inbuf[0] == '@') {
                        /* parse friend name up to first space */
                        char friend_name[MAX_NAME_LEN];
                        int k = 1; /* skip '@' */
                        int fn = 0;

                        while (inbuf[k] != '\0' && inbuf[k] != ' ' && inbuf[k] != '\n' && fn < MAX_NAME_LEN - 1) {
                            friend_name[fn++] = inbuf[k++];
                        }
                        friend_name[fn] = '\0';

                        /* whisper valid only if there is a space after friend */
                        if (fn > 0 && inbuf[k] == ' ') {
                            // shift left to remove "@friend " (that's k+1 chars)
                            memmove(inbuf, inbuf + (k + 1), strlen(inbuf + (k + 1)) + 1);


                            /* REBUILD after removing @friend */
                            outlen = snprintf(outbuf, sizeof(outbuf), "%s: %s", sender_name, inbuf);
                            if (outlen < 0) outlen = 0;
                            if (outlen >= (int)sizeof(outbuf)) outlen = (int)sizeof(outbuf) - 1;

                            // now inbuf contains only "secret...\n"

                            /* find destination fd by name */
                            int dest_fd = -1;
                            for (int j = 0; j < MAX_CLIENTS; j++) {
                                if (clients.arr[j].in_use && strcmp(clients.arr[j].name, friend_name) == 0) {
                                    dest_fd = clients.arr[j].fd;
                                    break;
                                }
                            }

                            if (dest_fd >= 0) {
                                /* send only to destination */
                                if (send(dest_fd, outbuf, outlen, 0) < 0){ perror("send"); disconnect_fd(dest_fd, listen_fd); }
                            } else {
                                /* friend not found: for HW3 you can ignore silently */
                            }
                        } else {
                            /* malformed whisper -> treat as normal broadcast */
                            for (int j = 0; j < MAX_CLIENTS; j++) {
                                if (clients.arr[j].in_use) {
                                    if (send(clients.arr[j].fd, outbuf, outlen, 0) < 0){ perror("send"); disconnect_fd(clients.arr[j].fd, listen_fd); }
                                }
                            }
                        }
                    } else {
                        /* normal message -> broadcast to all */
                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            if (clients.arr[j].in_use) {
                                if (send(clients.arr[j].fd, outbuf, outlen, 0) < 0){ perror("send"); disconnect_fd(clients.arr[j].fd, listen_fd); }
                            }
                        }
                    }
                }
            }

        }
      }

    }


    return 0;
    }
