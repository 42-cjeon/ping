#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>

#include "ping.h"

typedef struct {
	void (*handle_read)(void *self);
} EventOps;

typedef struct {
	EventOps ops;
	void *private;
} EventHandler;

typedef struct {
	enum {
		ECHO_UNINIT = 0,
		ECHO_PENDING,
	} status;
	int linger_timer;

} Echo;

typedef struct {
	Echo echo[65536];
	PingConfig *conf;
	size_t sent;
	size_t receved;
	size_t waiting;
	uint16_t sequence;
	int icmp_sock;
	int ep;
} PingRunTime;

typedef struct {
	PingRunTime *rt;
	int tmr;
} IntervalTmr;

void sock_read(void *_self) {
	PingRunTime *self = _self;

	socklen_t len = sizeof(struct sockaddr_in);
	char buf[65000];
	STRICT(recvfrom(self->icmp_sock, buf,
			self->conf->count + sizeof(struct icmphdr), 0,
			(void *)&self->conf->dest.ipv4, &len));

	struct icmphdr *icmphdr = (void *)buf;
	printf("ECHO REPLAY SEQ=%hd\n", htons(icmphdr->un.echo.sequence));
}

void timer_read(void *_self) {
	IntervalTmr *self = _self;

	uint64_t exp_count;
	STRICT(read(self->tmr, &exp_count, sizeof(exp_count)));

	char buf[65000];
	struct icmphdr *icmphdr = (void *)buf;
	icmphdr->type = ICMP_ECHO;
	icmphdr->un.echo.sequence = htons(self->rt->sequence);

	for (size_t i = 0; i < self->rt->conf->size; ++i) {
		(buf + sizeof(struct icmphdr))[i] = i;
	}

	// TODO send via write event
	STRICT(sendto(self->rt->icmp_sock, buf,
		      self->rt->conf->size + sizeof(struct icmphdr), 0,
		      (void *)&self->rt->conf->dest.ipv4,
		      sizeof(struct sockaddr_in)));
	printf("ECHO SEQ=%hd\n", self->rt->sequence);
	self->rt->sequence += 1;
}

int main(void) {
	PingConfig conf = {
	    .dest = {.type = DEST_IPV4,
		     .ipv4 =
			 {
			     .sin_family = AF_INET,
			     .sin_addr = {.s_addr = inet_addr("192.168.45.1")},
			 }},
	    .count = 0,
	    .interval = 1,
	    .linger = 1,
	    .timeout = 0,
	    .size = 40,
	    .usage = false,
	    .verbose = false,
	};

	int icmp_sock;
	STRICT(icmp_sock =
		   socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_ICMP));
	// STRICT()

	int interval_timer;
	STRICT(interval_timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK));

	struct itimerspec timerspec = {
	    .it_interval = {.tv_nsec = 0, .tv_sec = conf.interval},
	    .it_value = {.tv_nsec = 1, .tv_sec = 0},
	};
	STRICT(timerfd_settime(interval_timer, 0, &timerspec, NULL));

	int ep;
	STRICT(ep = epoll_create(128));

	PingRunTime rt = {
	    .conf = &conf,
	    .ep = ep,
	    .icmp_sock = icmp_sock,
	};

	IntervalTmr tmr = {
	    .rt = &rt,
	    .tmr = interval_timer,
	};

	struct epoll_event evs[128];
	memset(evs, 0, sizeof(evs));

	evs[0].events = EPOLLIN;
	evs[0].data.ptr = &(EventHandler){
	    .private = &rt,
	    .ops = {.handle_read = sock_read},
	};

	evs[1].events = EPOLLIN;
	evs[1].data.ptr = &(EventHandler){
	    .private = &tmr,
	    .ops = {.handle_read = timer_read},
	};

	STRICT(epoll_ctl(ep, EPOLL_CTL_ADD, icmp_sock, &evs[0]));
	STRICT(epoll_ctl(ep, EPOLL_CTL_ADD, interval_timer, &evs[1]));

	uint16_t seq = 0;
	for (;;) {
		int nevents;
		STRICT(nevents = epoll_wait(ep, evs, 128, -1));
		for (int i = 0; i < nevents; ++i) {
			struct epoll_event *evp = &evs[i];
			EventHandler *evhp = evp->data.ptr;

			if (evp->events & EPOLLIN)
				evhp->ops.handle_read(evhp->private);
		}
	}
}