//
// Created by danie on 26/12/2025.
//

#ifndef SERVER_H
#define SERVER_H

int GetAndParseClientName();
static void disconnect_fd(int fd, int listen_fd);
#endif //SERVER_H
