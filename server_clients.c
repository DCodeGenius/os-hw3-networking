//
// Created by danie on 26/12/2025.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "server_clients.h"

ClientsTable clients;

int AddClient(int fd, char *name, char *ip)
{
  if (clients.count >= MAX_CLIENTS) {
    fprintf(stderr, "Too many clients\n");
    return -1;
  }

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients.arr[i].in_use == 0) {

      clients.arr[i].fd = fd;

      strncpy(clients.arr[i].name, name, MAX_NAME_LEN);
      clients.arr[i].name[MAX_NAME_LEN] = '\0';

      strncpy(clients.arr[i].ip, ip, MAX_IP_LEN);
      clients.arr[i].ip[MAX_IP_LEN] = '\0';

      clients.arr[i].in_use = 1;
      clients.count++;

      return i;
    }
  }

 fprintf(stderr, "did not find an available client\n");
  return -1;
}

int RemoveClient(int fd){
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients.arr[i].fd == fd && clients.arr[i].in_use == 1) {
      clients.arr[i].in_use = 0;
      clients.arr[i].fd = -1;
      clients.arr[i].name[0] = '\0';
      clients.arr[i].ip[0] = '\0';
      clients.count--;
      return i;
    }
  }
  fprintf(stderr, "did not find a client with requested fd\n");
  return -1;
}

void InitClients() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    clients.arr[i].fd = -1;
    clients.arr[i].in_use = 0;
    clients.arr[i].name[0] = '\0';
    clients.arr[i].ip[0] = '\0';
  }
}
