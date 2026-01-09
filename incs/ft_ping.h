#ifndef FT_PING_H
#define FT_PING_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip.h>
#include <math.h>
#include <signal.h>
#include <sys/socket.h>

// Reference: inetutils ping default data size is 56 bytes (64 with ICMP header).
// See GNU inetutils manual.
#define DATA_LEN 56
#define PACKET_LEN (sizeof(struct icmphdr) + DATA_LEN)

typedef struct s_opts
{
	bool verbose;		// -v
	bool help;			// -?
	const char *target; // destination (FQDN or IPv4)
} t_opts;

struct stats
{
	int transmitted;
	int received;
	struct timeval t_start;
	struct timeval t_end;
	double rtt_min;
	double rtt_max;
	double rtt_sum;
	double rtt_sumsq;
};

// Globals (defined in main.c)
extern struct stats g_stats;
extern t_opts g_opts;

void print_usage(FILE *out);
int parse_args(int argc, char **argv, t_opts *opts);
bool forward_dns_resolution(const char *hostname, char ip_str[INET_ADDRSTRLEN], struct sockaddr_in *dst);
unsigned short calculate_checksum(void *b, int len);

#endif // FT_PING_H
