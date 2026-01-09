#include "ft_ping.h"

void print_usage(FILE *out)
{
	fprintf(out,
			"Usage: ft_ping [-v] [-?] destination\n"
			"  -v   verbose output (list non-echo ICMP replies)\n"
			"  -?   help (show this usage message)\n");
}

int parse_args(int argc, char **argv, t_opts *opts)
{
	// defaults
	opts->verbose = false;
	opts->help = false;
	opts->target = NULL;

	for (int i = 1; i < argc; ++i)
	{
		const char *arg = argv[i];

		if (strcmp(arg, "--") == 0)
		{
			if (i + 1 < argc)
			{
				if (opts->target)
				{
					fprintf(stderr, "ft_ping: too many arguments: '%s'\n", argv[i + 1]);
					print_usage(stderr);
					return -1;
				}
				opts->target = argv[++i];
			}
			continue;
		}

		if (arg[0] == '-' && arg[1] != '\0')
		{
			for (size_t j = 1; arg[j] != '\0'; ++j)
			{
				switch (arg[j])
				{
				case 'v':
					opts->verbose = true;
					break;
				case '?':
					opts->help = true;
					break;
				default:
					fprintf(stderr, "ft_ping: illegal option -- %c\n", arg[j]);
					print_usage(stderr);
					return -1;
				}
			}
			continue;
		}

		// target
		if (!opts->target)
		{
			opts->target = arg;
		}
		else
		{
			fprintf(stderr, "ft_ping: too many arguments: '%s'\n", arg);
			print_usage(stderr);
			return -1;
		}
	}

	if (!opts->help && !opts->target)
	{
		fprintf(stderr, "ft_ping: missing destination\n");
		print_usage(stderr);
		return -1;
	}

	return 0;
}
