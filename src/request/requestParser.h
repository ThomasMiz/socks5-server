#ifndef REQUEST_PARSER_H
#define REQUEST_PARSER_H

#include <sys/types.h>

#include "../buffer.h"
#include <netinet/ip.h>
#include <stdint.h>

enum TReqCmd {
    REQ_CMD_CONNECT = 0x01,
    REQ_CMD_BIND = 0x02,
    REQ_CMD_UDP = 0x03
};

enum TReqAtyp {
    REQ_ATYP_IPV4 = 0x01,
    REQ_ATYP_DOMAINNAME = 0x03,
    REQ_ATYP_IPV6 = 0x04
};

#define PORT_BYTE_LENGHT 2
#define REQ_MAX_DN_LENGHT 0xFF

typedef enum TReqState {
    REQ_SUCCEDED = 0,
    REQ_ERROR_GENERAL_FAILURE,
    REQ_ERROR_CONNECTION_NOT_ALLOWED,
    REQ_ERROR_NTW_UNREACHABLE,
    REQ_ERROR_HOST_UNREACHABLE,
    REQ_ERROR_CONNECTION_REFUSED,
    REQ_ERROR_TTL_EXPIRED,
    REQ_ERROR_COMMAND_NOT_SUPPORTED,
    REQ_ERROR_ADDRESS_TYPE_NOT_SUPPORTED,
    REQ_VERSION,   // The parser is waiting for the client version
    REQ_CMD,       // The parser is waiting for CMD (Connect/bind/udp)
    REQ_RSV,       // The parser is waiting for the reserved space X'00'
    REQ_ATYP,      // The parser is waiting for the address type
    REQ_DN_LENGHT, // If atype is 0x03, read the domainname length
    REQ_DST_ADDR,
    REQ_DST_PORT
} TReqState;

typedef union TAddress {
    struct in_addr ipv4;
    uint8_t domainname[REQ_MAX_DN_LENGHT + 1];
    struct in6_addr ipv6;
    // Used to set bytes without considering their meaning
    uint8_t bytes[REQ_MAX_DN_LENGHT + 1];
} TAddress;

typedef struct TReqParser {
    TReqState state;
    uint8_t atyp;
    uint8_t totalAtypBytes; // Used to know read bytes for atyp and port
    uint8_t readBytes;
    TAddress address;
    in_port_t port;
} TReqParser;

void initRequestParser(TReqParser* p);
TReqState requestParse(TReqParser* p, struct buffer* buffer);
uint8_t hasRequestReadEnded(TReqParser* p);
uint8_t hasRequestErrors(TReqParser* p);

/* 0 if ok 1 if errors */
uint8_t fillRequestAnswer(TReqParser* p, struct buffer* buffer);

#endif /* REQUEST_PARSER_H */
