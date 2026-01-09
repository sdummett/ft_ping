#include "ft_ping.h"

unsigned short calculate_checksum(void *b, int len)
{
	unsigned short *buf = b;
	unsigned int sum = 0;
	unsigned short result;

	// Sum 16-bit words
	while (len > 1)
	{
		sum += *buf++;
		len -= 2;
	}

	// Handle odd byte
	if (len == 1)
		sum += *(unsigned char *)buf;

	// Fold to 16 bits
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);

	result = ~sum;
	return result;
}
