#ifndef FT_PING_H
#define FT_PING_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

typedef struct s_opts
{
	bool verbose;		// -v
	bool help;			// -?
	const char *target; // final argument (FQDN/IP)
} t_opts;

void print_usage(FILE *out);
int parse_args(int argc, char **argv, t_opts *opts);

#endif // FT_PING_H
