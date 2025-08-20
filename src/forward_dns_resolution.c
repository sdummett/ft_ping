#include "../include/ft_ping.h"

bool forward_dns_resolution(const char *hostname, char ip_str[INET_ADDRSTRLEN])
{
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;	  // IPv4 only
	hints.ai_socktype = SOCK_RAW; // raw socket later (ICMP)
	hints.ai_protocol = IPPROTO_ICMP;

	int err = getaddrinfo(hostname, NULL, &hints, &res);
	if (err != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
		return false;
	}

	// take the first result
	struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
	inet_ntop(AF_INET, &addr->sin_addr, ip_str, INET_ADDRSTRLEN);

	freeaddrinfo(res);
	return true;
}
