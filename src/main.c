// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "selector.h"
#include "socks5.h"
#include "args.h"
#include "users.h"
#include "logger.h"

static bool terminationRequested = false;


static void sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n", signal);
    terminationRequested = true;
}

int main(const int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);


    // no tenemos nada que leer de stdin
    close(STDIN_FILENO);
    usersInit(NULL);

    struct socks5args args;
    parse_args(argc, argv, &args);

    unsigned port = args.socksPort;

    for(int i=0 ; i<args.nusers ; ++i){
        usersCreate(args.users[i].name, args.users[i].pass, 0, UPRIV_USER, 0);
    }


    const char *err_msg = NULL;
    TSelectorStatus ss = SELECTOR_SUCCESS;
    TSelector selector = NULL;

    // Listening on just IPv6 allow us to handle both IPv6 and IPv4 connections!
    // https://stackoverflow.com/questions/50208540/cant-listen-on-ipv4-and-ipv6-together-address-already-in-use
    struct sockaddr_in addr4;
	struct sockaddr_in6 addr6;
    int ipv6 = strchr(args.socksAddr, ':') != NULL; 
    if(ipv6) {
		memset(&addr6, 0, sizeof(addr6));
		addr6.sin6_family = AF_INET6;
		addr6.sin6_addr = in6addr_any;
		addr6.sin6_port = htons(port);
		if(inet_pton(AF_INET6, args.socksAddr, &addr6.sin6_addr) != 1) {
			log(LOG_ERROR, "failed IP conversion for %s", "IPv6");
			return -1;
		}
	} else {
		memset(&addr4, 0, sizeof(addr4));
		addr4.sin_family =AF_INET;
		addr4.sin_addr.s_addr = INADDR_ANY;
		addr4.sin_port = htons(port);
		if(inet_pton(AF_INET, args.socksAddr, &addr4.sin_addr) != 1) {
			log(LOG_ERROR, "failed IP conversion for %s", "IPv4");
			return -1;
		}
	}
  

    const int server = socket(ipv6? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server < 0) {
        err_msg = "unable to create socket";
        goto finally;
    }

    fprintf(stdout, "Listening on TCP port %d\n", port);

    // man 7 ip. no importa reportar nada si falla.
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int));

    
    
    if (bind(server,  ipv6 ? (struct sockaddr *)(&addr6) : (struct sockaddr *)(&addr4), ipv6 ? sizeof(addr6) : sizeof(addr4)) < 0) {
        err_msg = "unable to bind socket";
        goto finally;
    }

    if (listen(server, 20) < 0) {
        err_msg = "unable to listen";
        goto finally;
    }

    // registrar sigterm es útil para terminar el programa normalmente.
    // esto ayuda mucho en herramientas como valgrind.
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    if (selector_fd_set_nio(server) == -1) {
        err_msg = "getting server socket flags";
        goto finally;
    }
    const TSelectorInit conf = {
            .signal = SIGALRM,
            .select_timeout = {
                    .tv_sec = 10,
                    .tv_nsec = 0,
            },
    };
    if (0 != selector_init(&conf)) {
        err_msg = "initializing selector";
        goto finally;
    }

    selector = selector_new(1024);
    if (selector == NULL) {
        err_msg = "unable to create selector";
        goto finally;
    }
    const TFdHandler socksv5 = {
            .handle_read = socksv5_passive_accept,
            .handle_write = NULL,
            .handle_close = NULL, // nada que liberar
    };
    ss = selector_register(selector, server, &socksv5, OP_READ, NULL);
    if (ss != SELECTOR_SUCCESS) {
        err_msg = "registering fd";
        goto finally;
    }
    while (!terminationRequested) {
        err_msg = NULL;
        ss = selector_select(selector);
        if (ss != SELECTOR_SUCCESS) {
            err_msg = "serving";
            goto finally;
        }
    }
    if (err_msg == NULL) {
        err_msg = "closing";
    }

    int ret = 0;
    finally:
    usersFinalize();
    if (ss != SELECTOR_SUCCESS) {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "" : err_msg,
                ss == SELECTOR_IO
                ? strerror(errno)
                : selector_error(ss));
        ret = 2;
    } else if (err_msg) {
        perror(err_msg);
        ret = 1;
    }
    if (selector != NULL) {
        selector_destroy(selector);
    }
    selector_close();


    // socksv5_pool_destroy();

    if (server >= 0) {
        close(server);
    }
    return ret;
}
