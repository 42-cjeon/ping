#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
	enum {
		DEST_UNINIT,
		DEST_IPV4,
		DEST_DOMAIN,
	} type;
	union {
		uint32_t ipv4;
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

bool _is_digit(char ch) {
	return ('0' <= ch && ch <= '9');
}

ParseResult parse_uint(const char *str, int limit, uint32_t *result,
		       ParseError *err) {
	uint32_t num = 0;

	for (size_t i = 0; str[i]; ++i) {
		if (!_is_digit(str[i])) {
			err->arg = str;
			err->where = str + i;
			return PR_INVALID_VALUE;
		}
		num = (num * 10) + (str[i] - '0');
		if (num > limit) {
			err->arg = str;
			err->limit = limit;
			return PR_TOO_BIG;
		}
	}

	*result = num;

	return PR_OK;
}

ParseResult parse_arg_interval(ArgParser *parser) {
	parser->state = PS_OPT_OR_ADDR;
	return parse_uint(*parser->argp, 100000000, &parser->conf->interval,
			  &parser->err);
}

ParseResult parse_arg_count(ArgParser *parser) {
	parser->state = PS_OPT_OR_ADDR;
	return parse_uint(*parser->argp, 100000000, &parser->conf->count,
			  &parser->err);
}

ParseResult parse_arg_linger(ArgParser *parser) {
	parser->state = PS_OPT_OR_ADDR;
	return parse_uint(*parser->argp, 100000000, &parser->conf->linger,
			  &parser->err);
}

ParseResult parse_arg_timeout(ArgParser *parser) {
	parser->state = PS_OPT_OR_ADDR;
	return parse_uint(*parser->argp, 100000000, &parser->conf->timeout,
			  &parser->err);
}

ParseResult parse_arg_size(ArgParser *parser) {
	parser->state = PS_OPT_OR_ADDR;
	return parse_uint(*parser->argp, 65535 - 8, &parser->conf->size,
			  &parser->err);
}

ParseResult parse_ipv4_address(char *str, ArgParser *parser) {
	parser->conf->dest.type = DEST_IPV4;

	if (inet_pton(AF_INET, str, &parser->conf->dest.ipv4) != 1) {
		parser->err.arg = str;
		return PR_INVALID_IPV4_HOST;
	}

	return PR_OK;
}

ParseResult parse_domain_name(char *str, ArgParser *parser) {
	parser->conf->dest.type = DEST_DOMAIN;
	parser->conf->dest.domain = str;

	return PR_OK;
}

ParseResult parse_addr(char *str, ArgParser *parser) {
	for (size_t i = 0; str[i]; ++i) {
		if (!_is_digit(str[i]) && str[i] != '.')
			return parse_domain_name(str, parser);
	}
	return parse_ipv4_address(str, parser);
}

ParseResult parse_opt(char *str, ArgParser *parser) {
	if (str[0] == '\0' || str[1] != '\0') {
		parser->err.arg = str;
		return PR_INVALID_OPTION;
	}

	switch (str[0]) {
	case OPT_INTERVAL:
		parser->state = PS_ARG_INTERVAL;
		return PR_OK;
	case OPT_COUNT:
		parser->state = PS_ARG_COUNT;
		return PR_OK;
	case OPT_LINGER:
		parser->state = PS_ARG_LINGER;
		return PR_OK;
	case OPT_TIMEOUT:
		parser->state = PS_ARG_TIMEOUT;
		return PR_OK;
	case OPT_SIZE:
		parser->state = PS_ARG_SIZE;
		return PR_OK;
	case OPT_USAGE:
		parser->state = PS_OPT_OR_ADDR;
		parser->conf->usage = true;
		return PR_OK;
	case OPT_VERBOSE:
		parser->state = PS_OPT_OR_ADDR;
		parser->conf->verbose = true;
		return PR_OK;
	default:
		parser->err.arg = str;
		return PR_INVALID_OPTION;
	}
}

int parse_opt_or_addr(ArgParser *parser) {
	char *str = *parser->argp;

	if (str[0] == '-')
		return parse_opt(str + 1, parser);
	else
		return parse_addr(str, parser);
}

void ping_conf_init_default(PingConfig *self) {
	self->dest.type = DEST_UNINIT;
	self->count = 0;
	self->interval = 1;
	self->size = 56;
	self->linger = 1;
	self->timeout = 0;
	self->verbose = false;
	self->usage = false;
}

void arg_parser_init(ArgParser *self, PingConfig *conf, char **argp) {
	self->state = PS_OPT_OR_ADDR;
	self->conf = conf;
	self->argp = argp + 1;
}

int arg_parser_parse(ArgParser *parser) {
	int error;

	for (; *parser->argp; ++parser->argp) {
		switch (parser->state) {
		case PS_OPT_OR_ADDR:
			error = parse_opt_or_addr(parser);
			break;
		case PS_ARG_INTERVAL:
			error = parse_arg_interval(parser);
			break;
		case PS_ARG_COUNT:
			error = parse_arg_count(parser);
			break;
		case PS_ARG_LINGER:
			error = parse_arg_linger(parser);
			break;
		case PS_ARG_TIMEOUT:
			error = parse_arg_timeout(parser);
			break;
		case PS_ARG_SIZE:
			error = parse_arg_size(parser);
			break;
		}

		if (error)
			return error;
	}

	if (parser->state != PS_OPT_OR_ADDR) {
		parser->err.arg = *(parser->argp - 1) + 1;
		return PR_MISSING_ARGUMENT;
	}

	if (parser->conf->dest.type == DEST_UNINIT)
		return PR_MISSING_HOST;

	return PR_OK;
}

// typedef struct {
// 	Destination dest;
// 	uint16_t size;
// 	uint16_t interval;
// 	uint32_t linger;
// 	uint32_t count;
// 	uint32_t timeout;
// 	bool verbose;
// 	bool usage;
// } P

// destination_display

void ping_conf_print(PingConfig *self) {
	printf("PingConfig {\n");
	switch (self->dest.type) {
	case DEST_UNINIT:
		printf("  dest: uninit\n");
		break;
	case DEST_IPV4:
		printf("  dest: IPV4(0x%08x)\n", self->dest.ipv4);
		break;
	case DEST_DOMAIN:
		printf("  dest: DOMAIN(%s)\n", self->dest.domain);
		break;
	}
	printf("  size: %u\n", self->size);
	printf("  interval: %u\n", self->interval);
	printf("  linger: %u\n", self->linger);
	printf("  count: %u\n", self->count);
	printf("  timeout: %u\n", self->timeout);
	printf("  verbose: %c\n", self->verbose ? 'Y' : 'N');
	printf("  usage: %c\n", self->usage ? 'Y' : 'N');
	printf("}\n");
}

void print_parse_error(const char *binname, ParseResult pr, ParseError *err) {
	fprintf(stderr, "%s: ", binname);

	switch (pr) {
	case PR_OK:
		fprintf(stderr, "unknown error\n");
		break;
	case PR_INVALID_VALUE:
		fprintf(stderr, "invalid value (`%s' near `%s')\n", err->arg,
			err->where);
		break;
	case PR_TOO_BIG:
		fprintf(stderr, "value too big (`%s' is greater then limit - %u)\n",
			err->arg, err->limit);
		break;
	case PR_MISSING_HOST:
		fprintf(stderr,
			"missing host operand\n"
			"Try '%s -?' for more information.\n",
			binname);
		break;
	case PR_INVALID_IPV4_HOST:
		fprintf(stderr, "invalid ipv4 host -- '%s'\n", err->arg);
		break;
	case PR_UNKNOWN_HOST:
		fprintf(stderr, "unknown host -- '%s'\n", err->arg);
		break;
	case PR_INVALID_OPTION:
		fprintf(stderr,
			"invalid option -- '%s'\n"
			"Try '%s -?' for more information.\n",
			err->arg, binname);
		break;
	case PR_MISSING_ARGUMENT:
		fprintf(stderr,
			"option requires an argument -- '%s'\n"
			"Try '%s -? for more information.\n",
			err->arg, binname);
		break;
	}
}

int main(int argc, char **argv) {
	PingConfig conf;
	ArgParser parser;

	ping_conf_init_default(&conf);
	arg_parser_init(&parser, &conf, argv);

	ParseResult pr;
	if ((pr = arg_parser_parse(&parser))) {
		print_parse_error(argv[0], pr, &parser.err);
		exit(1);
	}

	ping_conf_print(&conf);
}