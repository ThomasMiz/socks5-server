#ifndef PASSWORD_DISSECTOR_H_
#define PASSWORD_DISSECTOR_H_

#include "buffer.h"
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>

#define PDS_MAX_PASS_LENGTH 255
#define PDS_MAX_USER_LENGTH 255
#define POP3_DEFAULT_PORT 110

typedef enum TPDStatus {
    /*  In pop3, e expect to receive first a + from the server. If that happens, the parser goes to the next state,
     *  otherwise it will be turned off.
     *  INTERESTS:
     *      clientFd: to turn the parser off if a character is received from the client (origin sends the first message in pop3
     *      originFd: to check if the first character is a +
     * */
    PDS_SERVER_PLUS,

    /* The dissector will consider that we are in pop3 if the port is 110 and the + condition previously described
     * is accomplished.
     * INTERESTS:
     *      clientFd: we want to know if the user is sending the command USER
     *      originFd: -
     * */
    PDS_USER_U,       //  waiting for U of USER
    PDS_USER_S,       //  waiting for S of USER
    PDS_USER_E,       //  waiting for E of USER
    PDS_USER_R,       //  waiting for R of USER
    PDS_USER_W,       //  waiting for ' ' of USER
    PDS_READING_USER, //  reading the username

    /* We expect the user to send \r\n at the end of a command, but since some implementations allow the
     * client to end the line with just a \n, the condition to leave the PDS_READING_USER is to find the \n.
     * */

    /* Same as user case
     * INTERESTS:
     *      clientFd: we want to know if the user is sending the command PASS
     *      originFd: -
     * */
    PDS_PASS_P,       //  waiting for P of PASS
    PDS_PASS_A,       //  waiting for A of PASS
    PDS_PASS_S,       //  waiting for first S of PASS
    PDS_PASS_S2,      //  waiting for second S of PASS
    PDS_PASS_W,       //  waiting for second ' ' of PASS
    PDS_READING_PASS, //  reading the password

    PDS_CHECK, //  waiting for the server to send a + to indicate user/pass are valid
               //  or a - otherwise

    PDS_END //  Password read completely, the dissector is turned off
} TPDStatus;

typedef struct TPDissector {
    TPDStatus state;
    uint8_t writeIdx; // used to store de password and username
    char password[PDS_MAX_PASS_LENGTH + 1];
    char username[PDS_MAX_USER_LENGTH + 1];

    /* PD will only be activated if the config indicates it to be active and if the
     * client is trying to connect to the POP3 default port: 110
     * It will be deactivated when the user/pass is obtained */
    bool isOn;

    /* Once the user sended the whole username, assuming that the server is going to return
     * a + for positive answers, it is possible to know if the username is valid */
    bool validUsername;

    int clientFd;
    int originFd;
} TPDissector;


/**
 * @brief Initializes password dissector
 * @param pd pointer to structure that holds dissector information
 * @param port port to listen for potential credentials
 * @param clientFd client file descriptor
 * @param originFd origin server file descriptor
 */
void initPDissector(TPDissector* pd, in_port_t port, int clientFd, int originFd);

/**
 * @brief Parses the buffer data to scan for credentials information
 * @param pd pointer to structure that holds dissector information
 * @param buffer buffer to scan content
 * @param fd file descriptor to which the buffer belongs (client or origin)
 * @returns status that resulted from the parsing
 */
TPDStatus parseUserData(TPDissector* pd, struct buffer* buffer, int fd);

/**
 * @brief Turns off password dissector
 */
void turnOffPDissector();

/**
 * @brief Turns on password dissector
 */
void turnOnPDissector();

/**
 * @brief checks the state of the password dissector
 * @returns current state of dissector
 */
bool isPDissectorOn();

#endif // PASSWORD_DISSECTOR_H_
