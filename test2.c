#include <err.h>
#include <stdint.h>
#include <stdio.h>

#include <unistd.h>

#include <sys/timerfd.h>

#define STRICT(expr)                                                           \
	do {                                                                   \
		if ((expr) < 0)                                                \
			err(1, #expr);                                         \
	} while (0)

int main(void) {
	int tfd;

	STRICT(tfd = timerfd_create(CLOCK_MONOTONIC, 0));

	struct itimerspec ts = {.it_value = {.tv_sec = 1, .tv_nsec = 0},
				.it_interval = {.tv_sec = 1, .tv_nsec = 0}};

	STRICT(timerfd_settime(tfd, 0, &ts, NULL));

	uint64_t exp_count;
	sleep(3);
	for (;;) {
		STRICT(read(tfd, &exp_count, sizeof(exp_count)));
		printf("expired %lu times.\n", exp_count);
	}
}