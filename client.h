//
// Created by danie on 26/12/2025.
//

#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"

typedef struct client {
    int server_fd;
    char name[MAX_NAME_LEN];
} client_t;

#endif //CLIENT_H
