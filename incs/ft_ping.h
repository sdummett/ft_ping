#ifndef FT_PING_H
#define FT_PING_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <time.h>
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
#include <argp.h>

// Reference: inetutils ping default data size is 56 bytes (64 with ICMP header).
// See GNU inetutils-2.0 manual.
#define DEFAULT_DATA_LEN 56
#define MAX_IPV4_ICMP_DATA 65507

typedef struct s_opts
{
	bool verbose;	  // -v | --verbose
	bool help;		  // -h | -? | --help
	long count;		  // -c | --count <count> (stop after <count> number of echo reply)
	int deadline;	  // -w | --deadline <deadline> (timeout in seconds before ping exits)
	int ttl;		  // -t | --ttl <ttl> (IP time to live)
	double interval;  // -i | --interval <interval> (seconds between each packet)
	int data_len;	  // -s | --packetsize <packetsize> (number of data bytes to send)
	const char *host; // host (FQDN or IPv4)
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

int parse_args(int argc, char **argv, t_opts *opts);
bool forward_dns_resolution(const char *hostname, char ip_str[INET_ADDRSTRLEN], struct sockaddr_in *dst);
unsigned short calculate_checksum(void *b, int len);

#endif // FT_PING_H
