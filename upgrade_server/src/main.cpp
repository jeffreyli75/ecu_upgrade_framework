#include "iso14229.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
// #include <mbedtls/config.h>
// #include <mbedtls/platform.h>
// #include <mbedtls/pk.h>
// #include <mbedtls/rsa.h>
// #include <mbedtls/sha256.h>
// #include <mbedtls/error.h>
// #include "tp.h"
// #include "server.h"
#include "doiptp.h"
#include "upgrade_server.h"

static UDSServer_t srv;
static DoIPTp tp;
// static UpgradeServerT5 upgrade;
static UpgradeServerTest *upgrade = nullptr;
static bool done = false;

void sigint_handler(int signum) {
    printf("SIGINT received\n");
    done = true;
}

static uint8_t fn(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) {
    return upgrade->ProcessUdsEvent(srv, ev, arg);
}

static int sleep_ms(uint32_t tms) {
    struct timespec ts;
    int ret;
    ts.tv_sec = tms / 1000;
    ts.tv_nsec = (tms % 1000) * 1000000;
    do {
        ret = nanosleep(&ts, &ts);
    } while (ret && errno == EINTR);
    return ret;
}

int main(int ac, char **av) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    upgrade = new UpgradeServerTest;
    if (tp.InitServer(0x7E0, 0x7E8, 0x7DF)) {
        fprintf(stderr, "DoIPTp init failed\n");
        exit(-1);
    }

    if (UDSServerInit(&srv)) {
        fprintf(stderr, "UDSServerInit failed\n");
    }

    srv.tp = (UDSTpHandle_t *)&tp;
    srv.fn = fn;

    printf("server up, polling . . .\n");
    while (!done) {
        UDSServerPoll(&srv);
        sleep_ms(1);
    }
    printf("server exiting\n");
    return 0;
}
