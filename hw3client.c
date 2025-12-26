#include "client.h"
#include "protocol.h"
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

static void perror2(const char *what) {
    // HW style often wants: "<prog>: <syscall> failed, errno is %d"
    // But HW3 PDF doesn't strictly mandate the HW1 exact format.
    // Still useful:
    fprintf(stderr, "hw3client: %s failed, errno is %d\n", what, errno);
}

// Connect to server and return connected socket fd
// Returns -1 on failure
int connect_to_server(const char *addr, int port){
    int fd = -1;
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));

    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)port);

    // Try numeric IP first
    if (inet_pton(AF_INET, addr, &sa.sin_addr) != 1) {
        // Fallback: resolve hostname
        struct hostent *he = gethostbyname(addr);
        if (!he || he->h_addrtype != AF_INET || !he->h_addr_list[0]) {
            fprintf(stderr, "hw3client: invalid addr '%s'\n", addr);
            return -1;
        }
        memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror2("socket"); return -1; }

    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror2("connect");
        close(fd);
        return -1;
    }

    return fd;
}


// Send entire buffer (handle partial send)
// Returns 0 on success, -1 on failure
int send_all(int fd, const char *buf, size_t len){
    size_t off = 0;

    while (off < len) {
        ssize_t got_sent = send(fd, buf + off, len - off, 0);
        if (got_sent < 0) {
            if (errno == EINTR)
                continue;              // interrupted by signal, retry
            return -1;                 // real error
        }
        if (got_sent == 0) {
            // For TCP, send() returning 0 is unusual, treat as failure / disconnect
            return -1;
        }
        off += (size_t)got_sent;
    }
    return 0;
}

// Returns 0 on success, -1 if server disconnected or fatal error
int handle_server_input(int server_fd){
    char buffer[MAX_MSG_LEN + 1]; // +1 for safety null-terminator

    ssize_t nbytes = recv(server_fd, buffer, MAX_MSG_LEN, 0);

    if (nbytes < 0) {
        if (errno == EINTR) return 0;   // retry later
        perror2("recv");
        return -1;
    }
    else if (nbytes == 0) {
        // Server closed the connection (TCP FIN)
        // Standard socket behavior: recv returns 0 on disconnect
        return -1;
    }
    buffer[nbytes] = '\0';
    printf("%s", buffer);

    // Flush stdout to ensure the text appears immediately on the screen
    // without waiting for the buffer to fill up.
    fflush(stdout);
    return 0; // Success
}

// Returns 0 on success, 1 if !exit requested, -1 on fatal error
int handle_user_input(int server_fd){
    char buffer[MAX_MSG_LEN];

    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        // Treat EOF (Ctrl-D) like !exit
        const char *exit_msg = "!exit\n";
        if (send_all(server_fd, exit_msg, strlen(exit_msg)) < 0) return -1;
        printf("client exiting\n");
        return 1;
    }

    //Check for the "!exit" command
    if (strcmp(buffer, "!exit\n") == 0) {
        // Send "!exit" to the server so it knows to remove us
        if (send_all(server_fd, buffer, strlen(buffer)) < 0) {
            return -1;
        }

        // Print the required disconnection message locally
        printf("client exiting\n"); //

        // Return 1 to tell the main loop to break/exit
        return 1;
    }

    // Handle Normal and Whisper Messages
    // Normal: "just a text line"
    // Whisper: "@friend msg"
    // In both cases, we send the buffer exactly as typed.
    // The server handles the parsing and routing.
    if (send_all(server_fd, buffer, strlen(buffer)) < 0) {
        return -1;
    }

    return 0; // Continue the chat loop
}

// Main client loop using select()
// Returns only on exit or fatal error
void client_run(int server_fd){
    fd_set read_fds;
    int max_fd = (server_fd > STDIN_FILENO) ? server_fd : STDIN_FILENO;

    while (1) {
        // Prepare the set of file descriptors
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds); // Watch keyboard
        FD_SET(server_fd, &read_fds);    // Watch server

        // Wait for activity on either channel
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue; // Ignore interrupted system calls
            perror2("select");
            break;
        }

        // Check for Server Data
        if (FD_ISSET(server_fd, &read_fds)) {
            if (handle_server_input(server_fd) < 0) {
                break; // Exit loop if server disconnects
            }
        }

        // Check for User Input
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            int status = handle_user_input(server_fd);
            if (status == 1) {
                break; // User typed !exit
            } else if (status == -1) {
                break; // Send error
            }
        }
    }
}

/**
 * Parses command line arguments for the client.
 * * Arguments expected:
 * 1. addr: Server IP address or hostname
 * 2. port: Server port number
 * 3. name: Client display name
 * * Returns: The port number on success, or -1 on failure.
 */
int parse_arguments(int argc, char *argv[], client_t *client, const char **addr_out) {

    if (argc != 4) {
        fprintf(stderr, "Usage: %s addr port name\n", argv[0]);
        return -1;
    }

    *addr_out = argv[1];
    const char *port_str = argv[2];
    const char *name_arg = argv[3];

    char *endp = NULL;
    errno = 0;
    long port_long = strtol(port_str, &endp, 10);

    // Check for conversion errors or garbage characters
    if (errno != 0 || endp == port_str || *endp != '\0') {
        fprintf(stderr, "Error: Invalid port number '%s'\n", port_str);
        return -1;
    }

    // Check valid TCP port range (1-65535)
    if (port_long < 1 || port_long > 65535) {
        fprintf(stderr, "Error: Port '%ld' is out of range (1-65535)\n", port_long);
        return -1;
    }

    // Validate and Store Name
    size_t name_len = strlen(name_arg);
    if (name_len == 0) {
        fprintf(stderr, "Error: Name cannot be empty.\n");
        return -1;
    }
    if (name_len >= MAX_NAME_LEN) {
        fprintf(stderr, "Error: Name too long (limit is %d chars).\n", MAX_NAME_LEN - 1);
        return -1;
    }

    // Copy name into the struct safely
    memset(client->name, 0, MAX_NAME_LEN);
    strncpy(client->name, name_arg, MAX_NAME_LEN - 1);

    // Return the validated port as an integer
    return (int)port_long;
}

int main(int argc, char **argv){
    client_t client;
    const char *server_addr = NULL;

    // Call the parser
    int port = parse_arguments(argc, argv, &client, &server_addr);
    if (port == -1) {
        return 1;
    }

    //Connect to server:
    int server_fd = connect_to_server(server_addr, port);
    if (server_fd < 0) return 1;
    char message[MAX_NAME_LEN + 2];
    snprintf(message, sizeof(message), "%s\n", client.name);
    if (send_all(server_fd, message, strlen(message)) < 0) {
        perror2("send");
        close(server_fd);
        return 1;
    }



    // Start the multiplexing loop
    client_run(server_fd);
    // Clean up
    close(server_fd);

    return 0;
}