#include "../include/ft_ping.h"

#define DATA_LEN   56
#define PACKET_LEN (sizeof(struct icmphdr) + DATA_LEN)

struct stats g_stats;

// local flags used across functions (don't depend on externs)
static bool        g_verbose = false;
static const char *g_target  = NULL;

static const char *icmp_error_desc(uint8_t type, uint8_t code)
{
    switch (type) {
        case ICMP_DEST_UNREACH:
            switch (code) {
                case ICMP_NET_UNREACH:   return "Destination Net Unreachable";
                case ICMP_HOST_UNREACH:  return "Destination Host Unreachable";
                case ICMP_PROT_UNREACH:  return "Destination Protocol Unreachable";
                case ICMP_PORT_UNREACH:  return "Destination Port Unreachable";
                case ICMP_FRAG_NEEDED:   return "Frag needed and DF set";
                case ICMP_SR_FAILED:     return "Source Route Failed";
                case ICMP_NET_UNKNOWN:   return "Destination Net Unknown";
                case ICMP_HOST_UNKNOWN:  return "Destination Host Unknown";
                case ICMP_HOST_ISOLATED: return "Source Host Isolated";
                case ICMP_NET_ANO:       return "Net Administratively Prohibited";
                case ICMP_HOST_ANO:      return "Host Administratively Prohibited";
                case ICMP_NET_UNR_TOS:   return "Net Unreachable for TOS";
                case ICMP_HOST_UNR_TOS:  return "Host Unreachable for TOS";
                case ICMP_PKT_FILTERED:  return "Packet filtered";
                case ICMP_PREC_VIOLATION:return "Precedence Violation";
                case ICMP_PREC_CUTOFF:   return "Precedence Cutoff";
                default:                 return "Destination Unreachable";
            }
        case ICMP_TIME_EXCEEDED:
            if (code == ICMP_EXC_TTL)      return "Time to live exceeded";
            if (code == ICMP_EXC_FRAGTIME) return "Frag reassembly time exceeded";
            return "Time exceeded";
        case ICMP_PARAMETERPROB: return "Parameter problem";
        case ICMP_SOURCE_QUENCH: return "Source quench";
        case ICMP_REDIRECT:
            switch (code) {
                case ICMP_REDIRECT_NET:    return "Redirect Network";
                case ICMP_REDIRECT_HOST:   return "Redirect Host";
                case ICMP_REDIRECT_TOSNET: return "Redirect Type of Service and Network";
                case ICMP_REDIRECT_TOSHOST:return "Redirect Type of Service and Host";
                default: return "Redirect";
            }
        default:
            return "ICMP error";
    }
}

void handle_sigint(int sig)
{
    (void)sig; // unused
    gettimeofday(&g_stats.t_end, NULL);

    int lost = g_stats.transmitted - g_stats.received;
    double loss = (g_stats.transmitted > 0)
                      ? (100.0 * lost / g_stats.transmitted)
                      : 0.0;

    long elapsed_ms = (g_stats.t_end.tv_sec - g_stats.t_start.tv_sec) * 1000L
                    + (g_stats.t_end.tv_usec - g_stats.t_start.tv_usec) / 1000L;

    double rtt_avg = (g_stats.received > 0) ? g_stats.rtt_sum / g_stats.received : 0.0;
    double rtt_stddev = 0.0;
    if (g_stats.received > 0) {
        double mean = rtt_avg;
        double variance = (g_stats.rtt_sumsq / g_stats.received) - (mean * mean);
        if (variance > 0) rtt_stddev = sqrt(variance);
    }

    printf("\n--- %s ping statistics ---\n", g_target ? g_target : "ping");
    printf("%d packets transmitted, %d received, %.0f%% packet loss, time %ldms\n",
           g_stats.transmitted, g_stats.received, loss, elapsed_ms);

    if (g_stats.received > 0) {
        printf("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",
               g_stats.rtt_min, rtt_avg, g_stats.rtt_max, rtt_stddev);
    }

    exit(EXIT_SUCCESS);
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
static int receive_ping(int sockfd, int id)
{
    unsigned char buffer[2048];
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    int bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                  (struct sockaddr *)&addr, &addr_len);
    if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return -2; // timeout
        perror("ft_ping: recvfrom");
        return -1;
    }

    struct iphdr *ip_hdr = (struct iphdr *)buffer;
    int ip_header_len = ip_hdr->ihl * 4;
    if (bytes_received < ip_header_len + (int)sizeof(struct icmphdr)) {
        if (g_verbose) {
            fprintf(stderr, "ft_ping: packet too short (%d bytes) from %s\n",
                    bytes_received, inet_ntoa(addr.sin_addr));
        }
        return -1;
    }

    struct icmphdr *icmp_hdr = (struct icmphdr *)(buffer + ip_header_len);

    if (icmp_hdr->type == ICMP_ECHOREPLY && ntohs(icmp_hdr->un.echo.id) == id) {
        // RTT from our timestamp payload
        struct timeval *tv_send = (struct timeval *)(buffer + ip_header_len + sizeof(*icmp_hdr));
        struct timeval tv_recv; gettimeofday(&tv_recv, NULL);

        double rtt_ms = (tv_recv.tv_sec - tv_send->tv_sec) * 1000.0 +
                        (tv_recv.tv_usec - tv_send->tv_usec) / 1000.0;

        g_stats.received++;
        if (rtt_ms < g_stats.rtt_min) g_stats.rtt_min = rtt_ms;
        if (rtt_ms > g_stats.rtt_max) g_stats.rtt_max = rtt_ms;
        g_stats.rtt_sum += rtt_ms;
        g_stats.rtt_sumsq += (rtt_ms * rtt_ms);

        printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n",
               bytes_received - ip_header_len,
               inet_ntoa(addr.sin_addr),
               ntohs(icmp_hdr->un.echo.sequence),
               ip_hdr->ttl,
               rtt_ms);
        return 1;
    }

    // Non-echo ICMP (errors etc.) -- show with -v
    if (g_verbose) {
        const char *desc = icmp_error_desc(icmp_hdr->type, icmp_hdr->code);

        // Try to extract original sequence number (best-effort)
        int seq = -1;
        if (bytes_received >= ip_header_len + (int)sizeof(struct icmphdr) + (int)sizeof(struct iphdr) + (int)sizeof(struct icmphdr)) {
            struct iphdr *inner_ip = (struct iphdr *)(buffer + ip_header_len + sizeof(struct icmphdr));
            int ihl2 = inner_ip->ihl * 4;
            if (bytes_received >= ip_header_len + (int)sizeof(struct icmphdr) + ihl2 + (int)sizeof(struct icmphdr)) {
                struct icmphdr *inner_icmp = (struct icmphdr *)((unsigned char *)inner_ip + ihl2);
                seq = ntohs(inner_icmp->un.echo.sequence);
            }
        }

        if (seq >= 0)
            printf("From %s: icmp_seq=%d %s (type=%u code=%u)\n",
                   inet_ntoa(addr.sin_addr), seq, desc, icmp_hdr->type, icmp_hdr->code);
        else
            printf("From %s: %s (type=%u code=%u)\n",
                   inet_ntoa(addr.sin_addr), desc, icmp_hdr->type, icmp_hdr->code);
        return 0;
    }

    return -1; // ignore silently
}

int main(int argc, char *argv[])
{
    signal(SIGINT, handle_sigint);
    gettimeofday(&g_stats.t_start, NULL);
    g_stats.transmitted = 0;
    g_stats.received    = 0;
    g_stats.rtt_min     = 1e9;
    g_stats.rtt_max     = 0;
    g_stats.rtt_sum     = 0;
    g_stats.rtt_sumsq   = 0;

    t_opts o;
    if (parse_args(argc, argv, &o) != 0)
        return 1;
    if (o.help) {
        print_usage(stdout);
        return 0;
    }
    g_verbose = o.verbose;
    g_target  = o.target;

    struct sockaddr_in dst;
    char ip_str[INET_ADDRSTRLEN];

    if (!forward_dns_resolution(o.target, ip_str, &dst)) {
        fprintf(stderr, "ft_ping: DNS resolution failed for '%s'\n", o.target);
        return 1;
    }

    printf("PING %s (%s): %d data bytes\n", o.target, ip_str, DATA_LEN);

    // Prepare raw ICMP socket
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("ft_ping: socket");
        exit(EXIT_FAILURE);
    }

    // Optional but recommended: 1s receive timeout so we can show timeouts
    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int id  = getpid() & 0xFFFF;
    int seq = 1;

    while (1) {
        unsigned char packet[PACKET_LEN];
        size_t packet_len = create_icmp_packet(packet, seq, id);

        if (sendto(sockfd, packet, packet_len, 0, (struct sockaddr *)&dst, sizeof(dst)) <= 0) {
            perror("ft_ping: sendto");
            break;
        }
        g_stats.transmitted++;

        int rc = receive_ping(sockfd, id);
        if (rc == -2) {
            // timeout
            printf("Request timeout for icmp_seq %d\n", seq);
        }
        // rc==1 success, rc==0 verbose already printed, rc==-1 ignore

        seq++;
        sleep(1);
    }

    close(sockfd);
    return 0;
}
