#include "ft_ping.h"

static char doc[] = "Send ICMP ECHO_REQUEST packets to network hosts.";
static char args_doc[] = "host";

static struct argp_option g_argp_options[] = {
	{"verbose", 'v', 0, 0, "Verbose output", 0},
	{"count", 'c', "COUNT", 0, "Stop after COUNT replies", 0},
	{"deadline", 'w', "DEADLINE", 0, "Timeout in seconds before ping exits", 0},
	{"ttl", 't', "TTL", 0, "Set IP time to live (default: 64)", 0},
	{"interval", 'i', "INTERVAL", 0, "Wait INTERVAL seconds between sending each packet (default: 1s)", 0},
	{"packetsize", 's', "PACKETSIZE", 0, "Specify number of data bytes to send (default: 56)", 0},
	{0, 'h', 0, OPTION_HIDDEN, 0, 0},
	{0}};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	t_opts *opts = (t_opts *)state->input;

	switch (key)
	{
	case 'v':
		opts->verbose = true;
		break;
	case 'c':
	{
		char *end = NULL;
		long v;

		if (!arg || *arg == '\0')
			argp_error(state, "invalid argument: '%s'", arg ? arg : "(null)");

		errno = 0;
		v = strtol(arg, &end, 10);
		if (end == arg || *end != '\0')
			argp_error(state, "invalid argument: '%s'", arg);

		// Valid integer, but out of allowed range
		if (errno == ERANGE)
		{
			fprintf(stderr, "%s: invalid argument: '%s': Numerical result out of range\n",
					state->name, arg);
			exit(EXIT_FAILURE);
		}
		if (v < 1 || v > INT_MAX)
		{
			fprintf(stderr,
					"%s: invalid argument: '%s': out of range: 1 <= value <= %d\n",
					state->name, arg, INT_MAX);
			exit(EXIT_FAILURE);
		}

		opts->count = v;
		break;
	}
	case 'w':
	{
		char *end = NULL;
		long v;

		if (!arg || *arg == '\0')
			argp_error(state, "invalid argument: '%s'", arg ? arg : "(null)");

		errno = 0;
		v = strtol(arg, &end, 10);
		if (errno == ERANGE)
		{
			fprintf(stderr, "%s: invalid argument: '%s': Numerical result out of range\n",
					state->name, arg);
			exit(EXIT_FAILURE);
		}
		if (end == arg || *end != '\0')
			argp_error(state, "invalid argument: '%s'", arg);

		if (v < 0 || v > INT_MAX)
		{
			fprintf(stderr,
					"%s: invalid argument: '%s': out of range: 0 <= value <= %d\n",
					state->name, arg, INT_MAX);
			exit(EXIT_FAILURE);
		}

		opts->deadline = (int)v;
		break;
	}
	case 't':
	{
		char *end = NULL;
		long v;

		if (!arg || *arg == '\0')
			argp_error(state, "invalid argument: '%s'", arg ? arg : "(null)");

		errno = 0;
		v = strtol(arg, &end, 10);
		if (errno == ERANGE)
		{
			fprintf(stderr, "%s: invalid argument: '%s': Numerical result out of range\n",
					state->name, arg);
			exit(EXIT_FAILURE);
		}
		if (end == arg || *end != '\0')
			argp_error(state, "invalid argument: '%s'", arg);
		if (v < 1 || v > 255)
		{
			fprintf(stderr,
					"%s: invalid argument: '%s': out of range: 1 <= value <= 255\n",
					state->name, arg);
			exit(EXIT_FAILURE);
		}
		opts->ttl = (int)v;
		break;
	}
	case 'i':
	{
		char *end = NULL;
		double v;

		if (!arg || *arg == '\0')
			argp_error(state, "invalid argument: '%s'", arg ? arg : "(null)");

		// Decimal separator must be '.' regardless of locale
		if (strchr(arg, ','))
			argp_error(state, "invalid argument: '%s'", arg);

		errno = 0;
		v = strtod(arg, &end);
		if (errno == ERANGE)
		{
			fprintf(stderr, "%s: invalid argument: '%s': Numerical result out of range\n",
					state->name, arg);
			exit(EXIT_FAILURE);
		}
		if (end == arg || *end != '\0' || !isfinite(v))
			argp_error(state, "invalid argument: '%s'", arg);
		if (v < 0.0)
		{
			fprintf(stderr,
					"%s: invalid argument: '%s': out of range: 0 <= value\n",
					state->name, arg);
			exit(EXIT_FAILURE);
		}

		opts->interval = v;
		break;
	}
	case 's':
	{
		char *end = NULL;
		long v;

		if (!arg || *arg == '\0')
			argp_error(state, "invalid argument: '%s'", arg ? arg : "(null)");

		errno = 0;
		v = strtol(arg, &end, 10);
		if (errno == ERANGE)
		{
			fprintf(stderr, "%s: invalid argument: '%s': Numerical result out of range\n",
					state->name, arg);
			exit(EXIT_FAILURE);
		}
		if (end == arg || *end != '\0')
			argp_error(state, "invalid argument: '%s'", arg);

		if (v < 0 || v > MAX_IPV4_ICMP_DATA)
		{
			fprintf(stderr,
					"%s: invalid argument: '%s': out of range: 0 <= value <= %d\n",
					state->name, arg, MAX_IPV4_ICMP_DATA);
			exit(EXIT_FAILURE);
		}

		opts->data_len = (int)v;
		break;
	}
	case 'h':
		argp_state_help(state, state->out_stream, ARGP_HELP_STD_HELP);
		break;
	case ARGP_KEY_ARG:
		if (opts->host)
			argp_error(state, "too many arguments: '%s'", arg);
		opts->host = arg;
		break;
	case ARGP_KEY_END:
		if (!opts->host)
			argp_error(state, "missing host");
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp g_argp = {g_argp_options, parse_opt, args_doc, doc, 0, 0, 0};

int parse_args(int argc, char **argv, t_opts *opts)
{
	// default options value
	opts->verbose = false;
	opts->count = 0;
	opts->deadline = 0;
	opts->help = false;
	opts->ttl = 64;
	opts->interval = 1.0;
	opts->data_len = DEFAULT_DATA_LEN;
	opts->host = NULL;

	argp_parse(&g_argp, argc, argv, 0, 0, opts);
	return 0;
}
