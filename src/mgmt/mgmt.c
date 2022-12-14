// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "mgmt.h"
#include "../logging/logger.h"
#include "../logging/util.h"
#include "mgmtAuth.h"
#include "mgmtRequest.h"

static void mgmtdoneArrival(const unsigned state, TSelectorKey* key) {
    log(LOG_DEBUG, "mgmtdoneArrival: Done state");
}
static void mgmterrorArrival(const unsigned state, TSelectorKey* key) {
    log(LOG_DEBUG, "mgmterrorArrival: Error state");
}

static void mgmtClose_connection(TSelectorKey* key);
static const struct state_definition client_statb1[] = {
    {
        .state = MGMT_AUTH_READ,
        .on_arrival = mgmtAuthReadInit,
        .on_read_ready = mgmtAuthRead,

    },
    {
        .state = MGMT_AUTH_WRITE,
        .on_write_ready = mgmtAuthWrite,
    },
    {
        .state = MGMT_REQUEST_READ,
        .on_arrival = mgmtRequestReadInit,
        .on_read_ready = mgmtRequestRead,
    },
    {
        .state = MGMT_REQUEST_WRITE,
        .on_arrival = mgmtRequestWriteInit,
        .on_write_ready = mgmtRequestWrite,
    },
    {
        .state = MGMT_DONE,
        .on_arrival = mgmtdoneArrival,
    },
    {
        .state = MGMT_ERROR,
        .on_arrival = mgmterrorArrival,
    }};

static void mgmt_read(TSelectorKey* key);
static void mgmt_write(TSelectorKey* key);
static void mgmt_close(TSelectorKey* key);
static void mgmt_block(TSelectorKey* key);
static TFdHandler handler = {
    .handle_read = mgmt_read,
    .handle_write = mgmt_write,
    .handle_close = mgmt_close,
    .handle_block = mgmt_block,
};

void mgmt_close(TSelectorKey* key) {
    struct state_machine* stm = &GET_ATTACHMENT(key)->stm;
    stm_handler_close(stm, key);
    mgmtClose_connection(key);
}

static void mgmt_read(TSelectorKey* key) {
    struct state_machine* stm = &GET_ATTACHMENT(key)->stm;
    const enum mgmt_state st = stm_handler_read(stm, key);
    if (st == MGMT_ERROR || st == MGMT_DONE) {
        mgmtClose_connection(key);
    }
}

static void mgmt_write(TSelectorKey* key) {
    struct state_machine* stm = &GET_ATTACHMENT(key)->stm;
    const enum mgmt_state st = stm_handler_write(stm, key);
    if (st == MGMT_ERROR || st == MGMT_DONE) {
        mgmtClose_connection(key);
    }
}

static void mgmt_block(TSelectorKey* key) {
    struct state_machine* stm = &GET_ATTACHMENT(key)->stm;
    const enum mgmt_state st = stm_handler_block(stm, key);
    if (st == MGMT_ERROR || st == MGMT_DONE) {
        mgmtClose_connection(key);
    }
}

void mgmtPassiveAccept(TSelectorKey* key) {
    struct sockaddr_storage clientAddress;
    socklen_t clientAddressLen = sizeof(clientAddress);
    int newClientSocket = accept(key->fd, (struct sockaddr*)&clientAddress, &clientAddressLen);

    if (newClientSocket < 0) {
        logf(LOG_WARNING, "Management socket: accept() returned negative value: %d", newClientSocket);
        return;
    }
    if (newClientSocket > 1023) {
        close(newClientSocket);
        return;
    }

    // Consider using a function to initialize the TClientData structure.
    TMgmtClient* clientData = calloc(1, sizeof(TMgmtClient));
    if (clientData == NULL) {
        close(newClientSocket);
        logf(LOG_WARNING, "Management new client from %s with fd %d rejected because fd was too high", printSocketAddress((struct sockaddr*)&clientAddress), newClientSocket);
        return;
    }

    clientData->stm.initial = MGMT_AUTH_READ;
    clientData->stm.max_state = MGMT_ERROR;
    clientData->closed = false;
    clientData->stm.states = client_statb1;
    clientData->clientFd = newClientSocket;

    buffer_init(&(clientData->readBuffer), MGMT_BUFFER_SIZE, clientData->readRawBuffer);
    buffer_init(&(clientData->writeBuffer), MGMT_BUFFER_SIZE, clientData->writeRawBuffer);

    stm_init(&clientData->stm);

    TSelectorStatus status = selector_register(key->s, newClientSocket, &handler, OP_READ, clientData);

    if (status != SELECTOR_SUCCESS) {
        logf(LOG_ERROR, "Management new client from %s with fd %d rejected because registering into selector failed: %s", printSocketAddress((struct sockaddr*)&clientAddress), newClientSocket, selector_error(status));
        free(clientData);
        return;
    }

    logf(LOG_INFO, "Management new client from %s assigned id %d", printSocketAddress((struct sockaddr*)&clientAddress), newClientSocket);
}

static void mgmtClose_connection(TSelectorKey* key) {
    TMgmtClient* data = GET_ATTACHMENT(key);
    if (data->closed)
        return;
    data->closed = true;
    logf(LOG_INFO, "Management client %d disconnected", key->fd);

    int clientSocket = data->clientFd;
    // int serverSocket = data->originFd;
    if (clientSocket != -1) {
        selector_unregister_fd(key->s, clientSocket);
        close(clientSocket);
    }

    // if (data->originResolution != NULL) {
    //     if(data->client.reqParser.atyp != REQ_ATYP_DOMAINNAME){
    //         free(data->originResolution->ai_addr);
    //         free(data->originResolution);
    //     }else {
    //         freeaddrinfo(data->originResolution);
    //     }
    // }

    free(data);
}
