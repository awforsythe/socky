#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "socky.h"

static char error_str[2048];

void announce_tests(const char* desc)
{
	printf("\n--- %s ---\n", desc);
}

void check_test(int cond, const char* desc)
{
	if (cond)
	{
		printf(" [  OK  ] %s\n", desc);
	}
	else
	{
		printf(" [ FAIL ] %s\n", desc);
		if (error_str[0])
		{
			printf("\n  %s\n", error_str);
		}
		exit(1);
	}
}

int is_valid_ip(const char* s)
{
	return 1;
}

int main(int argc, char* argv[])
{
	printf("Running socky test suite...\n");

	announce_tests("socky_init");
	const int init_result = socky_init();
	check_test(init_result == 0, "socky_init() returns 0");

	/*
	announce_tests("socky_adapters_*");
	{
		socky_adapters_init();
		check_test(1, "socky_adapters_init() completes");

		const int num_adapters = socky_adapters_num();
		check_test(num_adapters > 0, "socky_adapters_num() returns >0");

		const char* adapter_ip = socky_adapters_get_ip(0);
		check_test(adapter_ip != NULL, "socky_adapters_get_ip(0) returns non-null");
		check_test(adapter_ip[0], "socky_adapters_get_ip(0) returns non-empty");
		check_test(is_valid_ip(adapter_ip), "socky_adapters_get_ip(0) returns a valid IPv4 address string");
	}
	*/

	announce_tests("socky_udp_* (flags: 0)");
	{
		error_str[0] = '\0';

		SOCKET sock1 = socky_udp_open(5517, 0, error_str, sizeof(error_str));
		check_test(!IS_INVALID_SOCKET(sock1), "sock1: socky_udp_open(5517) succeeds");
		check_test(!error_str[0], "sock1: socky_udp_open(5517) writes no error string when successful");

		SOCKET sock2 = socky_udp_open(5517, 0, error_str, sizeof(error_str));
		check_test(IS_INVALID_SOCKET(sock2), "sock2: socky_udp_open(5517) returns an invalid socket when port is already bound");
		check_test(error_str[0], "sock2: socky_udp_open(5517) writes an error string when unsuccessful");
		check_test(strstr(error_str, "Failed to bind socket to port 5517") == error_str, "sock2: socky_udp_open(5517) returns a failed-to-bind error message");
		check_test(strlen(error_str) > 36 && strlen(error_str) < sizeof(error_str), "sock2: socky_udp_open(5517) returns an error message of a finite, reasonable length");

		error_str[0] = '\0';

		socky_udp_close(sock1);
		sock1 = INVALID_SOCKET;
		check_test(1, "sock1: socky_udp_close() succeeds");
		check_test(IS_INVALID_SOCKET(sock1), "sock1 is invalid after being set to INVALID_SOCKET");

		socky_udp_close(sock2);
		sock2 = INVALID_SOCKET;
		check_test(1, "sock2: socky_udp_close() succeeds");
		check_test(IS_INVALID_SOCKET(sock2), "sock2 is invalid after being set to INVALID_SOCKET");
	}

	announce_tests("socky_udp_* (flags: SOCKY_UDP_REUSEADDR | SOCKY_UDP_NONBLOCKING)");
	{
		SOCKET sock1 = socky_udp_open(7156, 0, error_str, sizeof(error_str));
		check_test(!IS_INVALID_SOCKET(sock1), "sock1: socky_udp_open(7156) succeeds");

		SOCKET sock2 = socky_udp_open(7156, SOCKY_UDP_REUSEADDR | SOCKY_UDP_NONBLOCKING, error_str, sizeof(error_str));
		check_test(!IS_INVALID_SOCKET(sock2), "sock2: socky_udp_open(7156) succeeds");

		socky_udp_close(sock1);
		check_test(1, "sock1: socky_udp_close() succeeds");

		socky_udp_close(sock2);
		check_test(1, "sock2: socky_udp_close() succeeds");
	}

	announce_tests("socky_cleanup");
	const int cleanup_result = socky_cleanup();
	check_test(cleanup_result == 0, "socky_cleanup() returns 0");

	printf("\nAll tests passed.\n");
	return 0;
}
