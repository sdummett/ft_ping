#include "ft_ping.h"

bool forward_dns_resolution(const char *hostname,
							char ip_str[INET_ADDRSTRLEN],
							struct sockaddr_in *dst)
{
	struct addrinfo hints, *res = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; // IPv4 only
	hints.ai_socktype = 0;	   // any
	hints.ai_protocol = 0;	   // any

	int err = getaddrinfo(hostname, NULL, &hints, &res);
	if (err != 0)
	{
		fprintf(stderr, "ft_ping: getaddrinfo: %s\n", gai_strerror(err));
		return false;
	}

	// take the first result
	struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;

	// copy the sockaddr_in into dst for later sendto()
	if (dst)
		memcpy(dst, addr, sizeof(struct sockaddr_in));

	inet_ntop(AF_INET, &addr->sin_addr, ip_str, INET_ADDRSTRLEN);

	freeaddrinfo(res);
	return true;
}
