// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "mgmtAuth.h"
#include "../logging/logger.h"
#include "../logging/util.h"
#include "../users.h"
#include "mgmt.h"

void mgmtAuthReadInit(const unsigned state, TSelectorKey* key) {
    logf(LOG_DEBUG, "mgmtAuthReadInit: init at socket fd %d", key->fd);
    TMgmtClient* data = GET_ATTACHMENT(key);
    initAuthParser(&data->client.authParser, UPRIV_ADMIN);
}

unsigned mgmtAuthRead(TSelectorKey* key) {
    logf(LOG_DEBUG, "mgmtAuthRead: read at socket fd %d", key->fd);
    TMgmtClient* data = GET_ATTACHMENT(key);

    size_t readLimit;    // how many bytes can be stored in the buffer
    ssize_t readCount;   // how many bytes where read from the client socket
    uint8_t* readBuffer; // here are going to be stored the bytes read from the client

    readBuffer = buffer_write_ptr(&data->readBuffer, &readLimit);
    readCount = recv(key->fd, readBuffer, readLimit, 0);
    logf(LOG_DEBUG, "mgmtAuthRead: %ld bytes from client %d", readCount, key->fd);
    if (readCount <= 0) {
        return MGMT_ERROR;
    }

    buffer_write_adv(&data->readBuffer, readCount);
    authParse(&data->client.authParser, &data->readBuffer);
    if (hasAuthReadEnded(&data->client.authParser)) {
        TAuthParser* authpdata = &data->client.authParser;
        TUserPrivilegeLevel upl;
        TUserStatus userStatus = validateUserAndPassword(authpdata, &upl);

        switch (userStatus) {
            case EUSER_OK:
                logf(LOG_INFO, "Manager %d successfully authenticated as %s (%s)", key->fd, authpdata->uname, usersPrivilegeToString(upl));
                break;
            case EUSER_WRONGUSERNAME:
                logf(LOG_INFO, "Manager %d attempted to authenticate as %s but there's no such username", key->fd, authpdata->uname);
                break;
            case EUSER_WRONGPASSWORD:
                logf(LOG_INFO, "Manager %d attempted to authenticate as %s but had the wrong password", key->fd, authpdata->uname);
                break;
            default:
                logf(LOG_ERROR, "Manager %d attempted to authenticate as %s but an unknown error ocurred", key->fd, authpdata->uname);
                break;
        }

        if (selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS || fillAuthAnswer(&data->client.authParser, &data->writeBuffer)) {
            return MGMT_ERROR;
        }
        return MGMT_AUTH_WRITE;
    }
    return MGMT_AUTH_READ;
}

unsigned mgmtAuthWrite(TSelectorKey* key) {
    logf(LOG_DEBUG, "mgmtAuthWrite: send at fd %d", key->fd);
    TMgmtClient* data = GET_ATTACHMENT(key);

    size_t writeLimit;      // how many bytes we want to send
    ssize_t writeCount = 0; // how many bytes where written
    uint8_t* writeBuffer;   // buffer that stores the data to be sended

    writeBuffer = buffer_read_ptr(&data->writeBuffer, &writeLimit);
    writeCount = send(key->fd, writeBuffer, writeLimit, MSG_NOSIGNAL);

    if (writeCount < 0) {
        logf(LOG_ERROR, "mgmtAuthWrite: send() at fd %d", key->fd);
        return MGMT_ERROR;
    }
    if (writeCount == 0) {
        logf(LOG_ERROR, "mgmtAuthWrite: Failed to send(), client closed connection unexpectedly at fd %d", key->fd);
        return MGMT_ERROR;
    }
    logf(LOG_DEBUG, "mgmtAuthWrite: %ld bytes to client %d", writeCount, key->fd);
    buffer_read_adv(&data->writeBuffer, writeCount);

    if (buffer_can_read(&data->writeBuffer)) {
        return MGMT_AUTH_WRITE;
    }

    if (hasAuthReadErrors(&data->client.authParser) || data->client.authParser.verification == AUTH_ACCESS_DENIED || selector_set_interest_key(key, OP_READ) != SELECTOR_SUCCESS) {
        return MGMT_ERROR;
    }

    return MGMT_REQUEST_READ;
}
