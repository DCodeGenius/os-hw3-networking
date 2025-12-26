#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "common.h"

// We will use: first message after connect = "<name>\n"
#define HANDSHAKE_NEWLINE 1

static inline int is_whisper(const char *line) {
    return (line && line[0] == '@');
}

// Parse "@friend msg..." into friend and msg pointers within the same buffer.
// Returns 0 on success, -1 on malformed.
static inline int parse_whisper(char *line, char **friend_out, char **msg_out) {
    // Expected: "@friend msg"
    if (!line || line[0] != '@') return -1;

    char *p = line + 1;
    if (*p == '\0' || *p == '\n') return -1;

    // friend ends at first space
    char *space = strchr(p, ' ');
    if (!space) return -1;

    *space = '\0';            // terminate friend
    *friend_out = p;
    *msg_out = space + 1;     // msg begins after space
    return 0;
}

#endif
