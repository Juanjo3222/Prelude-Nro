#include "nextendo_time.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define NTP_TIMESTAMP_DELTA 2208988800ull

// Instruct libnx to initialize the system time service instead of the user one,
// which is required to gain write access to the NetworkSystemClock.
TimeServiceType __nx_time_service_type = TimeServiceType_System;

typedef struct {
    uint8_t li_vn_mode;      // LI (2 bits), VN (3 bits), Mode (3 bits)
    uint8_t stratum;
    uint8_t poll;
    uint8_t precision;
    uint32_t rootDelay;
    uint32_t rootDispersion;
    uint32_t refId;
    uint32_t refTm_s;
    uint32_t refTm_f;
    uint32_t origTm_s;
    uint32_t origTm_f;
    uint32_t rxTm_s;
    uint32_t rxTm_f;
    uint32_t txTm_s;
    uint32_t txTm_f;
} ntp_packet;

Result nextendo_time_sync(void) {
    Result rs = socketInitializeDefault();
    if (R_FAILED(rs)) {
        return rs;
    }

    int sockfd = -1;
    const char *server_name = "time.cloudflare.com";
    const uint16_t port = 123;
    struct hostent *server = NULL;
    struct sockaddr_in serv_addr;
    ntp_packet packet;
    time_t resultTime = 0;

    memset(&packet, 0, sizeof(ntp_packet));
    packet.li_vn_mode = (0 << 6) | (4 << 3) | 3; // LI 0 | Version 4 | Mode 3
    packet.txTm_s = htonl(NTP_TIMESTAMP_DELTA + time(NULL));

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        goto failed;
    }

    // Set timeout to 5 seconds
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    server = gethostbyname(server_name);
    if (server == NULL) {
        goto failed;
    }

    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    memcpy((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr_list[0], 4);
    serv_addr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        goto failed;
    }

    if (send(sockfd, (char *)&packet, sizeof(ntp_packet), 0) < 0) {
        goto failed;
    }

    if (recv(sockfd, (char *)&packet, sizeof(ntp_packet), 0) < (ssize_t)sizeof(ntp_packet)) {
        goto failed;
    }

    packet.txTm_s = ntohl(packet.txTm_s);
    resultTime = (time_t)(packet.txTm_s - NTP_TIMESTAMP_DELTA);

    close(sockfd);
    socketExit();

    // Update the NetworkSystemClock and UserSystemClock using libnx time API
    rs = timeSetCurrentTime(TimeType_NetworkSystemClock, (uint64_t)resultTime);
    timeSetCurrentTime(TimeType_UserSystemClock, (uint64_t)resultTime);

    return rs;

failed:
    if (sockfd >= 0) close(sockfd);
    socketExit();
    return -2;
}
