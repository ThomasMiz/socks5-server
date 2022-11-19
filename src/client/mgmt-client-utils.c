#include "mgmt-client-utils.h"

#define BUFFER_SIZE 256
#define MAX_TOKEN_LENGTH 20
#define PROTOCOL_VERSION 1
#define ERROR_STATUS 1


int tcpClientSocket(const char *host, const char *service) {
    struct addrinfo addrCriteria;                   // Criteria for address match
    memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out structure
    addrCriteria.ai_family = AF_UNSPEC;             // v4 or v6 is OK
    addrCriteria.ai_socktype = SOCK_STREAM;         // Only streaming sockets
    addrCriteria.ai_protocol = IPPROTO_TCP;         // Only TCP protocol

    // Get address(es)
    struct addrinfo *servAddr; // Holder for returned list of server addrs
    int rtnVal = getaddrinfo(host, service, &addrCriteria, &servAddr);
    if (rtnVal != 0) {
        return -1;
    }

    int sock = -1;
    for (struct addrinfo *addr = servAddr; addr != NULL && sock == -1; addr = addr->ai_next) {
        // Create a reliable, stream socket using TCP
        sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sock >= 0) {
            errno = 0;
            // Establish the connection to the server
            if (connect(sock, addr->ai_addr, addr->ai_addrlen) != 0) {
                close(sock); 	// Socket connection failed; try next address
                sock = -1;
            }
        }
    }

    freeaddrinfo(servAddr);
    return sock;
}

bool validToken(const char *token) {
    int i;
    for(i=0; token[i] && i < MAX_TOKEN_LENGTH; i++) {
        if(!isprint(token[i])) {
            return false;
        }
    }
    return i < MAX_TOKEN_LENGTH;
}

bool authenticate(char *username, char *password, int socket) {
    int usernameLength = strlen(username);
    int passwordLength = strlen(password);

    uint8_t credentials[BUFFER_SIZE] = {PROTOCOL_VERSION, usernameLength};
    int idx = 2;
    memcpy(credentials + idx, username, usernameLength);
    idx += usernameLength;
    credentials[idx++] = passwordLength; 
    memcpy(credentials + idx, password, passwordLength);
    idx += passwordLength;
    
    if(send(socket, credentials, idx, MSG_DONTWAIT) < 0){
        printf("Error sending credentials to server\n");
        return false;
    }

    uint8_t version, status;

    if((read(socket, &version, 1)) <= 0) {
        printf("Error recieving response from server\n");
        return false;
    }

    if((read(socket, &status, 1)) <= 0) {
        printf("Error recieving response from server\n");
        return false;
    }

    if(status == ERROR_STATUS){
        printf("Username/password is wrong\n");
        return false;
    }

    return true;
}

int closeConnection(const char *errorMessage, const int socket){
    if(errorMessage != NULL && errno)
        perror(errorMessage);
    else
        printf("%s\n",errorMessage);

    if(socket >= 0) close(socket);
    return -1;
}