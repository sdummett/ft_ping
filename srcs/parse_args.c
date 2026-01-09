#include "ft_ping.h"

static char doc[] = "Send ICMP ECHO_REQUEST packets to network hosts.";
static char args_doc[] = "host";

static struct argp_option g_argp_options[] = {
	{"verbose", 'v', 0, 0, "Verbose output", 0},
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
	opts->help = false;
	opts->host = NULL;

	argp_parse(&g_argp, argc, argv, 0, 0, opts);
	return 0;
}
