#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAX_NAME_LEN 258 // 256 + '\n' + '\0' = 258
#define MAX_MSG_LEN  258
#define MAX_NUM_OF_CLIENTS 16

static inline void sys_fail(const char *syscall_name) {
    fprintf(stderr, "hw3: %s failed, errno is %d\n", syscall_name, errno);
}

#endif