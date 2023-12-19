#ifndef _PING_PING_H
#define _PING_PING_H

#include <stdint.h>
#include <stdbool.h>

#include <err.h>

#include <netinet/in.h>

#define STRICT(expr) do { if ((expr) < 0) err(1, "ping (line=%d): " #expr, __LINE__); } while (0)

typedef struct {
	enum {
		DEST_UNINIT,
		DEST_IPV4,
		DEST_DOMAIN,
	} type;
	union {
		struct sockaddr_in ipv4;
		char *domain;
	};
} Destination;

typedef struct {
	Destination dest;
	uint32_t size;
	uint32_t interval;
	uint32_t linger;
	uint32_t count;
	uint32_t timeout;
	bool verbose;
	bool usage;
} PingConfig;

typedef enum {
	OPT_INTERVAL = 'i',
	OPT_COUNT = 'c',
	OPT_LINGER = 'W',
	OPT_TIMEOUT = 'w',
	OPT_SIZE = 's',
	OPT_USAGE = '?',
	OPT_VERBOSE = 'v',
} PingOpt;

typedef enum {
	PS_OPT_OR_ADDR,
	PS_ARG_INTERVAL,
	PS_ARG_COUNT,
	PS_ARG_LINGER,
	PS_ARG_TIMEOUT,
	PS_ARG_SIZE,
} ParserState;

typedef enum {
	PR_OK,
	PR_INVALID_VALUE,
	PR_TOO_BIG,
	PR_MISSING_HOST,
	PR_INVALID_IPV4_HOST,
	PR_UNKNOWN_HOST,
	PR_INVALID_OPTION,
	PR_MISSING_ARGUMENT,
} ParseResult;

typedef struct {
	const char *arg;
	union {
		const char *where;
		uint32_t limit;
	};
} ParseError;

typedef struct {
	ParserState state;
	PingConfig *conf;
	ParseError err;
	char **argp;
} ArgParser;

#endif // _PING_PING_H
