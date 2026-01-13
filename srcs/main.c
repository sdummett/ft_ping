#include "ft_ping.h"

struct stats g_stats;

// local flags used across functions (don't depend on externs)
static bool g_verbose = false;
static const char *g_target = NULL;

static void sleep_interval(double seconds)
{
	if (seconds <= 0.0)
		return;

	struct timespec ts;
	ts.tv_sec = (time_t)seconds;
	ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1000000000.0);
	if (ts.tv_nsec < 0)
		ts.tv_nsec = 0;

	while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
		;
}

static struct timeval tv_add_seconds(struct timeval t, int seconds)
{
	t.tv_sec += seconds;
	return t;
}

static double tv_remaining_sec(struct timeval now, struct timeval deadline)
{
	double sec = (double)(deadline.tv_sec - now.tv_sec) +
				 (double)(deadline.tv_usec - now.tv_usec) / 1000000.0;
	return sec;
}

// static const char *icmp_error_desc(uint8_t type, uint8_t code)
// {
// 	switch (type)
// 	{
// 	case ICMP_DEST_UNREACH:
// 		switch (code)
// 		{
// 		case ICMP_NET_UNREACH:
// 			return "Destination Net Unreachable";
// 		case ICMP_HOST_UNREACH:
// 			return "Destination Host Unreachable";
// 		case ICMP_PROT_UNREACH:
// 			return "Destination Protocol Unreachable";
// 		case ICMP_PORT_UNREACH:
// 			return "Destination Port Unreachable";
// 		case ICMP_FRAG_NEEDED:
// 			return "Frag needed and DF set";
// 		case ICMP_SR_FAILED:
// 			return "Source Route Failed";
// 		case ICMP_NET_UNKNOWN:
// 			return "Destination Net Unknown";
// 		case ICMP_HOST_UNKNOWN:
// 			return "Destination Host Unknown";
// 		case ICMP_HOST_ISOLATED:
// 			return "Source Host Isolated";
// 		case ICMP_NET_ANO:
// 			return "Net Administratively Prohibited";
// 		case ICMP_HOST_ANO:
// 			return "Host Administratively Prohibited";
// 		case ICMP_NET_UNR_TOS:
// 			return "Net Unreachable for TOS";
// 		case ICMP_HOST_UNR_TOS:
// 			return "Host Unreachable for TOS";
// 		case ICMP_PKT_FILTERED:
// 			return "Packet filtered";
// 		case ICMP_PREC_VIOLATION:
// 			return "Precedence Violation";
// 		case ICMP_PREC_CUTOFF:
// 			return "Precedence Cutoff";
// 		default:
// 			return "Destination Unreachable";
// 		}
// 	case ICMP_TIME_EXCEEDED:
// 		if (code == ICMP_EXC_TTL)
// 			return "Time to live exceeded";
// 		if (code == ICMP_EXC_FRAGTIME)
// 			return "Frag reassembly time exceeded";
// 		return "Time exceeded";
// 	case ICMP_PARAMETERPROB:
// 		return "Parameter problem";
// 	case ICMP_SOURCE_QUENCH:
// 		return "Source quench";
// 	case ICMP_REDIRECT:
// 		switch (code)
// 		{
// 		case ICMP_REDIR_NET:
// 			return "Redirect Network";
// 		case ICMP_REDIR_HOST:
// 			return "Redirect Host";
// #ifdef ICMP_REDIR_TOSNET
// 		case ICMP_REDIR_TOSNET:
// 			return "Redirect Type of Service and Network";
// #endif
// #ifdef ICMP_REDIR_TOSHOST
// 		case ICMP_REDIR_TOSHOST:
// 			return "Redirect Type of Service and Host";
// #endif
// 		default:
// 			return "Redirect";
// 		}
// 	default:
// 		return "ICMP error";
// 	}
// }

static const char *icmp_error_desc(uint8_t type, uint8_t code)
{
	if (type == ICMP_DEST_UNREACH)
	{
		switch (code)
		{
		case ICMP_NET_UNREACH:
			return "Destination Net Unreachable";
		case ICMP_HOST_UNREACH:
			return "Destination Host Unreachable";
		case ICMP_PROT_UNREACH:
			return "Destination Protocol Unreachable";
		case ICMP_PORT_UNREACH:
			return "Destination Port Unreachable";
		default:
			return "Destination Unreachable";
		}
	}
	if (type == ICMP_TIME_EXCEEDED)
		return "Time to live exceeded";
	return "ICMP error";
}

static void print_stats_and_exit(int status)
{
	(void)status;
	gettimeofday(&g_stats.t_end, NULL);

	int lost = g_stats.transmitted - g_stats.received;
	double loss = (g_stats.transmitted > 0)
					  ? (100.0 * lost / g_stats.transmitted)
					  : 0.0;

	long elapsed_ms = (g_stats.t_end.tv_sec - g_stats.t_start.tv_sec) * 1000L + (g_stats.t_end.tv_usec - g_stats.t_start.tv_usec) / 1000L;

	double rtt_avg = (g_stats.received > 0) ? g_stats.rtt_sum / g_stats.received : 0.0;
	double rtt_stddev = 0.0;
	if (g_stats.received > 0)
	{
		double mean = rtt_avg;
		double variance = (g_stats.rtt_sumsq / g_stats.received) - (mean * mean);
		if (variance > 0)
			rtt_stddev = sqrt(variance);
	}

	printf("\n--- %s ping statistics ---\n", g_target ? g_target : "ping");
	printf("%d packets transmitted, %d received, %.0f%% packet loss, time %ldms\n",
		   g_stats.transmitted, g_stats.received, loss, elapsed_ms);

	if (g_stats.received > 0)
	{
		printf("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",
			   g_stats.rtt_min, rtt_avg, g_stats.rtt_max, rtt_stddev);
	}

	exit(status);
}

void handle_sigint(int sig)
{
	(void)sig;
	print_stats_and_exit(EXIT_SUCCESS);
}

// Build an ICMP Echo Request packet (header + 56 bytes data)
static size_t create_icmp_packet(unsigned char *buf, int seq, int id)
{
	memset(buf, 0, PACKET_LEN);

	struct icmphdr *icmp_hdr = (struct icmphdr *)buf;
	icmp_hdr->type = ICMP_ECHO;
	icmp_hdr->code = 0;
	icmp_hdr->un.echo.id = htons(id);
	icmp_hdr->un.echo.sequence = htons(seq);

	// Put a timestamp in the first bytes of the payload
	struct timeval *tv = (struct timeval *)(buf + sizeof(*icmp_hdr));
	gettimeofday(tv, NULL);

	// Fill remaining payload with a simple pattern
	unsigned char *payload = (unsigned char *)tv + sizeof(*tv);
	size_t pattern_len = DATA_LEN - sizeof(*tv);
	for (size_t i = 0; i < pattern_len; i++)
		payload[i] = (unsigned char)(i & 0xFF);

	icmp_hdr->checksum = 0;
	icmp_hdr->checksum = calculate_checksum(buf, PACKET_LEN);
	return PACKET_LEN;
}

// Receives one packet; returns:
//   1 on echo reply success
//   0 on non-echo packet processed (verbose info possibly printed)
//  -2 on timeout
//  -1 on error / ignore
static int receive_ping(int sockfd, int id, int expected_seq)
{
	unsigned char buffer[2048];
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);

	for (;;)
	{
		addr_len = sizeof(addr);
		int bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0,
									  (struct sockaddr *)&addr, &addr_len);
		if (bytes_received < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return -2; // timeout
			perror("ft_ping: recvfrom");
			return -1;
		}

		struct iphdr *ip_hdr = (struct iphdr *)buffer;
		int ip_header_len = ip_hdr->ihl * 4;
		if (bytes_received < ip_header_len + (int)sizeof(struct icmphdr))
		{
			if (g_verbose)
			{
				fprintf(stderr, "ft_ping: packet too short (%d bytes) from %s\n",
						bytes_received, inet_ntoa(addr.sin_addr));
			}
			continue;
		}

		struct icmphdr *icmp_hdr = (struct icmphdr *)(buffer + ip_header_len);
		int pkt_id = ntohs(icmp_hdr->un.echo.id);
		int pkt_seq = ntohs(icmp_hdr->un.echo.sequence);

		// Ignore packets not belonging to this process
		if (pkt_id != id)
			continue;

		// Ignore our own outgoing echo requests
		if (icmp_hdr->type == ICMP_ECHO)
			continue;

		if (icmp_hdr->type == ICMP_ECHOREPLY && pkt_seq == expected_seq)
		{
			struct timeval *tv_send =
				(struct timeval *)(buffer + ip_header_len + sizeof(*icmp_hdr));
			struct timeval tv_recv;
			gettimeofday(&tv_recv, NULL);

			double rtt_ms = (tv_recv.tv_sec - tv_send->tv_sec) * 1000.0 +
							(tv_recv.tv_usec - tv_send->tv_usec) / 1000.0;

			g_stats.received++;
			if (rtt_ms < g_stats.rtt_min)
				g_stats.rtt_min = rtt_ms;
			if (rtt_ms > g_stats.rtt_max)
				g_stats.rtt_max = rtt_ms;
			g_stats.rtt_sum += rtt_ms;
			g_stats.rtt_sumsq += (rtt_ms * rtt_ms);

			printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n",
				   bytes_received - ip_header_len,
				   inet_ntoa(addr.sin_addr),
				   pkt_seq,
				   ip_hdr->ttl,
				   rtt_ms);
			return 1;
		}

		// ICMP errors are sent back with the original IP header + 8 bytes of payload
		// Try to match the embedded Echo Request (id/seq) so we can treat it as a
		// network error notification for this probe
		if (icmp_hdr->type != ICMP_ECHOREPLY && icmp_hdr->type != ICMP_ECHO)
		{
			unsigned char *payload = buffer + ip_header_len + (int)sizeof(*icmp_hdr);
			int payload_len = bytes_received - (ip_header_len + (int)sizeof(*icmp_hdr));

			if (payload_len >= (int)sizeof(struct iphdr) + 8)
			{
				struct iphdr *inner_ip = (struct iphdr *)payload;
				int inner_ihl = inner_ip->ihl * 4;
				if (payload_len >= inner_ihl + (int)sizeof(struct icmphdr))
				{
					struct icmphdr *inner_icmp = (struct icmphdr *)(payload + inner_ihl);
					int inner_id = ntohs(inner_icmp->un.echo.id);
					int inner_seq = ntohs(inner_icmp->un.echo.sequence);
					if (inner_icmp->type == ICMP_ECHO && inner_id == id && inner_seq == expected_seq)
					{
						fprintf(stderr, "From %s: icmp_seq=%d %s\n",
								inet_ntoa(addr.sin_addr), expected_seq,
								icmp_error_desc(icmp_hdr->type, icmp_hdr->code));
						return -3;
					}
				}
			}
		}

		// Its our packet, but not the reply we're waiting for. Keep reading.
	}
}

int main(int argc, char *argv[])
{
	signal(SIGINT, handle_sigint);
	gettimeofday(&g_stats.t_start, NULL);
	g_stats.transmitted = 0;
	g_stats.received = 0;
	g_stats.rtt_min = 1e9;
	g_stats.rtt_max = 0;
	g_stats.rtt_sum = 0;
	g_stats.rtt_sumsq = 0;

	t_opts opts;

	if (parse_args(argc, argv, &opts) != 0)
		return 1;

	g_verbose = opts.verbose;
	g_target = opts.host;

	struct sockaddr_in dst;
	char ip_str[INET_ADDRSTRLEN];

	if (!forward_dns_resolution(opts.host, ip_str, &dst))
	{
		fprintf(stderr, "ft_ping: DNS resolution failed for '%s'\n", opts.host);
		return 1;
	}

	// Validate -i/--interval constraints (similar to ping(8))
	if (geteuid() != 0)
	{
		// Only super-user may set interval to values less than 2 ms
		if (opts.interval > 0.0 && opts.interval < 0.002)
		{
			fprintf(stderr,
					"ft_ping: Only super-user may set interval to values less than 2 ms\n");
			exit(EXIT_FAILURE);
		}

		uint32_t addr = ntohl(dst.sin_addr.s_addr);
		bool is_multicast = IN_MULTICAST(addr);
		bool is_broadcast = (addr == 0xFFFFFFFFu) || ((addr & 0xFFu) == 0xFFu);

		// Broadcast/multicast ping have higher limitation for regular user: minimum is 1 sec
		if ((is_multicast || is_broadcast) && opts.interval < 1.0)
		{
			fprintf(stderr,
					"ft_ping: broadcast/multicast ping: interval should be >= 1 sec\n");
			exit(EXIT_FAILURE);
		}
	}

	// Prepare raw ICMP socket
	int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sockfd < 0)
	{
		perror("ft_ping: socket");
		exit(EXIT_FAILURE);
	}

	// Set outgoing IPv4 TTL (default: 64)
	if (opts.ttl > 0)
	{
		int ttl = opts.ttl;
		if (setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) != 0)
			perror("ft_ping: setsockopt(IP_TTL)");
	}

	if (g_verbose)
	{
		int so_type = 0;
		socklen_t optlen = sizeof(so_type);
		const char *stype = "UNKNOWN";
		if (getsockopt(sockfd, SOL_SOCKET, SO_TYPE, &so_type, &optlen) == 0)
		{
			if (so_type == SOCK_RAW)
				stype = "SOCK_RAW";
			else if (so_type == SOCK_DGRAM)
				stype = "SOCK_DGRAM";
			else if (so_type == SOCK_STREAM)
				stype = "SOCK_STREAM";
		}

		fprintf(stderr,
				"%s: sock4.fd: %d (socktype: %s), hints.ai_family: AF_INET\n", argv[0],
				sockfd, stype);

		fprintf(stderr, "%s: ai->ai_family: AF_INET, ai->ai_canonname: '%s'\n", argv[0], opts.host);
	}
	int ip_hlen = 20; // Size of a minimal IPv4 header
	int icmp_hlen = (int)sizeof(struct icmphdr);
	int total_ip_packet_size = ip_hlen + icmp_hlen + DATA_LEN;

	printf("PING %s (%s) %d(%d) bytes of data.\n",
		   opts.host, ip_str, DATA_LEN, total_ip_packet_size);

	bool use_deadline = (opts.deadline > 0);
	struct timeval t_deadline;
	if (use_deadline)
		t_deadline = tv_add_seconds(g_stats.t_start, opts.deadline);

	// Receive timeout to avoid blocking forever
	// If -w is used, this will be adjusted dynamically not to overshoot the deadline
	struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	int id = getpid() & 0xFFFF;
	int seq = 1;

	int exit_status = EXIT_SUCCESS;

	while (1)
	{
		if (use_deadline)
		{
			struct timeval now;
			gettimeofday(&now, NULL);
			if (tv_remaining_sec(now, t_deadline) <= 0.0)
				break;
		}

		if (!use_deadline)
		{
			if (opts.count != 0 && g_stats.transmitted >= opts.count)
				break;
		}
		else
		{
			if (opts.count != 0 && g_stats.received >= opts.count)
				break;
		}

		unsigned char packet[PACKET_LEN];
		size_t packet_len = create_icmp_packet(packet, seq, id);

		ssize_t sent = sendto(sockfd, packet, packet_len, 0, (struct sockaddr *)&dst, sizeof(dst));

		g_stats.transmitted++;

		if (sent >= 0)
		{
			if (use_deadline)
			{
				struct timeval now;
				gettimeofday(&now, NULL);
				double rem = tv_remaining_sec(now, t_deadline);
				if (rem <= 0.0)
					break;
				struct timeval rcv_to;
				rcv_to.tv_sec = (time_t)fmin(1.0, rem);
				rcv_to.tv_usec = (suseconds_t)((fmin(1.0, rem) - (double)rcv_to.tv_sec) * 1000000.0);
				if (rcv_to.tv_usec < 0)
					rcv_to.tv_usec = 0;
				setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &rcv_to, sizeof(rcv_to));
			}

			int rc = receive_ping(sockfd, id, seq);
			if (rc == -2)
			{
				// timeout
				// printf("Request timeout for icmp_seq %d\n", seq);
			}
			else if (rc == -3)
			{
				exit_status = EXIT_FAILURE;
				break;
			}
			// rc==1 success, rc==0 verbose already printed, rc==-1 ignore
		}

		seq++;
		if (use_deadline)
		{
			struct timeval now;
			gettimeofday(&now, NULL);
			double rem = tv_remaining_sec(now, t_deadline);
			if (rem <= 0.0)
				break;
			sleep_interval(fmin(opts.interval, rem));
		}
		else
		{
			sleep_interval(opts.interval);
		}
	}

	print_stats_and_exit(exit_status);

	close(sockfd);
	return 0;
}
