#include <bits/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>

#include <err.h>

#include <sys/epoll.h>
#include <sys/timerfd.h>


static volatile sig_atomic_t should_send_packet;

void handle_sigalarm(int _sig) {
        should_send_packet = 1;

        // printf("--\n");
        // alarm is async-signal-safe.
        // alarm(1);
}

uint64_t get_timestamp_us(void) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000 * 1000 + tv.tv_usec;
}

int main(void) {

        int sock;
        STRICT(sock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_ICMP));

        struct sockaddr_in dst;
        memset(&dst, 0, sizeof(dst));
        dst.sin_addr.s_addr = inet_addr("192.168.44.128");
        dst.sin_family = AF_INET;

        // STRICT(connect(sock, (void *)&dst, sizeof(dst)));

        char send_buf[64];
        memset(send_buf, 0, sizeof(send_buf));
        struct icmphdr *send_icmphdr = (void *)send_buf;

        char recv_buf[64];
        memset(recv_buf, 0, sizeof(recv_buf));
        struct icmphdr *recv_icmphdr = (void *)recv_buf;

        signal(SIGALRM, handle_sigalarm);
        signal(SIGUSR1, handle_sigalarm);

        uint16_t seq = 0;
        struct timeval tv = {
                .tv_sec = 1,
                .tv_usec = 0
        };

        int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
        timerfd_settime(tfd, TFD_)

        int ep;
        STRICT(ep = epoll_create(16));

        struct epoll_event evs[16];
        evs[0].events = EPOLLIN;
        evs[0].data.fd = sock;

        epoll_ctl(ep, EPOLL_CTL_ADD, sock, struct epoll_event *event)
        timer
        // STRICT(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)));
        // alarm(1);
        for(;;) {
                socklen_t len = sizeof(dst);
                ssize_t r = recvfrom(sock, recv_buf, sizeof(recv_buf), 0, (void *)&dst, &len);
                printf("R = %ld\n", r);
                if (r < 0) {
                        if (errno != EAGAIN)
                                err(1, "recvfrom");
                        
                        send_icmphdr->un.echo.sequence = htons(seq);
                        send_icmphdr->type = ICMP_ECHO;
                        STRICT(sendto(sock, send_buf, sizeof(send_buf), 0, (void *)&dst, sizeof(dst)));
                        seq += 1;
                } else {
                        printf("ICMP: ECHO REPLY: ID=%d, SEQ=%d\n", htons(recv_icmphdr->un.echo.id), htons(recv_icmphdr->un.echo.sequence));
                }
        }


}