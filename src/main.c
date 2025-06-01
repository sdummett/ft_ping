#include "../include/ft_ping.h"

char *format =
	"PING google.com (142.250.201.174) 56(84) bytes of data.\n"
	"64 bytes from 142.250.201.174: icmp_seq=1 ttl=116 time=3.81 ms\n"

	"--- google.com ping statistics ---\n"
	"3 packets transmitted, 3 received, 0%% packet loss, time 2182ms\n"
	"rtt min/avg/max/mdev = 3.814/4.596/5.157/0.570 ms\n";

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>		 // AF_INET
#include <netinet/ip_icmp.h> // IPPROTO_ICMP
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h> // gettimeofday
#include <arpa/inet.h>
#include <netinet/ip.h> // struct iphdr

int internal_socket(void)
{
	int sockfd;

	// needs root privileges
	// tips: sudo setcap cap_net_raw+ep ./ft_ping
	sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sockfd < 0)
	{
		perror("socket");
		exit(EXIT_FAILURE);
	}
	return sockfd;
}

#define PACKET_SIZE 64

unsigned short calculate_checksum(void *b, int len)
{
	unsigned short *buf = b;
	unsigned int sum = 0;
	unsigned short result;

	for (sum = 0; len > 1; len -= 2)
		sum += *buf++;

	if (len == 1)
		sum += *(unsigned char *)buf;

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	result = ~sum;
	return result;
}

int create_icmp_packet(char *packet, int seq)
{
	struct icmphdr *icmp_hdr;
	struct timeval *time_payload;

	memset(packet, 0, PACKET_SIZE);

	icmp_hdr = (struct icmphdr *)packet;

	icmp_hdr->type = ICMP_ECHO;
	icmp_hdr->code = 0;
	icmp_hdr->un.echo.id = getpid() & 0xFFFF;
	icmp_hdr->un.echo.sequence = seq;

	time_payload = (struct timeval *)(packet + sizeof(struct icmphdr));
	gettimeofday(time_payload, NULL);

	icmp_hdr->checksum = calculate_checksum(packet, PACKET_SIZE);

	return PACKET_SIZE;
}

void internal_inet_pton(char *ip_address, struct sockaddr_in *dest_addr)
{
	if (inet_pton(AF_INET, ip_address, &(dest_addr->sin_addr)) < 0)
	{
		perror("inet_pton");
		exit(EXIT_FAILURE);
	}
}

void internal_sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t addr_len)
{
	if (sendto(fd, buf, n, flags, addr, addr_len) <= 0)
	{
		perror("sendto");
		close(fd);
		exit(EXIT_FAILURE);
	}
}

int receive_ping(int sockfd)
{
	char buffer[PACKET_SIZE];
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);
	int bytes_received;

	// wait response
	bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, &addr_len);
	if (bytes_received < 0)
	{
		perror("recvfrom");
		return -1;
	}

	// interpret buffer
	struct iphdr *ip_hdr = (struct iphdr *)buffer; // buffer start : IP header
	int ip_header_len = ip_hdr->ihl * 4;		   // size of IP header (ihl = Internet Header Length)

	struct icmphdr *icmp_hdr = (struct icmphdr *)(buffer + ip_header_len); // ICMP header is after IP header

	if (icmp_hdr->type == ICMP_ECHOREPLY)
	{
		printf("id: %d -> ", icmp_hdr->un.echo.id);
		printf("%d bytes from %s: icmp_seq=%d ttl=??? time=?.?? ms\n", PACKET_SIZE, inet_ntoa(addr.sin_addr), icmp_hdr->un.echo.sequence);
		// printf("Received ICMP Echo Reply from %s\n", inet_ntoa(addr.sin_addr));
		// printf("Sequence: %d, ID: %d\n", ntohs(icmp_hdr->un.echo.sequence), ntohs(icmp_hdr->un.echo.id));
		return 0;
	}
	else
	{
		// printf("id: %d\n", icmp_hdr->un.echo.id);
		//     printf("Received unexpected ICMP type: %d\n", icmp_hdr->type);
		return -1;
	}
	// return -1;
}

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	char *ip = "127.0.0.1";
	int sockfd;
	struct sockaddr_in dest_addr;
	char packet[PACKET_SIZE];
	int packet_len;

	// create the raw icmp socket
	sockfd = internal_socket();

	// prepare the destination address
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.sin_family = AF_INET;
	internal_inet_pton(ip, &dest_addr);

	int seq = 1;
	while (1)
	{
		// prepare the icmp packet
		packet_len = create_icmp_packet(packet, seq);

		internal_sendto(sockfd, packet, packet_len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

		while (receive_ping(sockfd) <= -1)
		{
			usleep(500000);
		}

		usleep(500000);
		seq++;
	}

	close(sockfd);
	return 0;
}
