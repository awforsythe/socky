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
	// Require a valid, non-empty string
	if (!s || !s[0])
	{
		return 0;
	}

	const size_t n = strlen(s);

	// Require a first dot beyond the start of the string
	const char* dot_a = strchr(s, '.');
	if (dot_a == NULL || dot_a == s)
	{
		return 0;
	}

	// Require a second dot at least one character past that
	const char* dot_b = strchr(dot_a + 1, '.');
	if (dot_b == NULL || dot_b == dot_a + 1)
	{
		return 0;
	}

	// Require a third dot at least one character past the second
	const char* dot_c = strchr(dot_b + 1, '.');
	if (dot_c == NULL || dot_c == dot_b + 1 || dot_c == s + n - 1)
	{
		return 0;
	}

	// Require exactly three dots: no fourth dot
	if (strchr(dot_c + 1, '.') != NULL)
	{
		return 0;
	}

	// Compute the length of each individual address part
	const size_t seg_a_len = dot_a - s;
	const size_t seg_b_len = dot_b - dot_a - 1;
	const size_t seg_c_len = dot_c - dot_b - 1;
	const size_t seg_d_len = s + n - dot_c - 1;

	// Ensure that each address part is a maximum of three digits
	if (seg_a_len > 3 || seg_b_len > 3 || seg_c_len > 3 || seg_d_len > 3)
	{
		return 0;
	}

	// Use a temporary array to store each address part as a null-terminated string for atoi
	char tmp[4];
	int val;
#ifdef _WIN32
#define IP_PART_ATOI(_Ptr, _Len) \
	strncpy_s(tmp, sizeof(tmp), _Ptr, _Len); \
	tmp[_Len] = '\0'; \
	val = atoi(tmp);
#else
#define IP_PART_ATOI(_Ptr, Len) \
	strncpy(tmp, _Ptr, _Len); \
	tmp[_Len] = '\0';
	val = atoi(tmp);
#endif

	// Ensure that the first part is a valid uint8
	IP_PART_ATOI(s, seg_a_len);
	if (val < 0 || val > 255 || (val == 0 && (tmp[0] != '0' || tmp[1] != '\0')))
	{
		return 0;
	}

	// Ensure that the second part is a valid uint8
	IP_PART_ATOI(dot_a + 1, seg_b_len);
	if (val < 0 || val > 255 || (val == 0 && (tmp[0] != '0' || tmp[1] != '\0')))
	{
		return 0;
	}

	// Ensure that the third part is a valid uint8
	IP_PART_ATOI(dot_b + 1, seg_c_len);
	if (val < 0 || val > 255 || (val == 0 && (tmp[0] != '0' || tmp[1] != '\0')))
	{
		return 0;
	}

	// Ensure that the fourth part is a valid uint8
	IP_PART_ATOI(dot_c + 1, seg_d_len);
	if (val < 0 || val > 255 || (val == 0 && (tmp[0] != '0' || tmp[1] != '\0')))
	{
		return 0;
	}

#undef IP_PART_ATOI

	return 1;
}

int main(int argc, char* argv[])
{
	printf("Running socky test suite...\n");

	announce_tests("is_valid_ip");
	{
		check_test(is_valid_ip("127.0.0.1"), "127.0.0.1 is valid");
		check_test(is_valid_ip("0.0.0.0"), "0.0.0.0 is valid");
		check_test(is_valid_ip("255.1.16.255"), "255.1.16.255 is valid");
		check_test(!is_valid_ip("1.1.1"), "1.1.1 is invalid");
		check_test(!is_valid_ip("1.1.1."), "1.1.1. is invalid");
		check_test(!is_valid_ip(".1.1.1"), ".1.1.1 is invalid");
		check_test(!is_valid_ip("255.255.255.256"), "255.255.255.256 is invalid");
		check_test(!is_valid_ip("127.0.-1.1"), "127.0.-1.1 is invalid");
		check_test(!is_valid_ip("127.0.99999.1"), "127.0.99999.1 is invalid");
		check_test(!is_valid_ip("127.0.foo.1"), "127.0.foo.1 is invalid");
		check_test(!is_valid_ip(""), "empty string is invalid");
		check_test(!is_valid_ip(NULL), "null is invalid");
	}

	announce_tests("socky_init");
	{
		const int init_result = socky_init();
		check_test(init_result == 0, "socky_init() returns 0");
	}

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
		SOCKET sock1 = socky_udp_open(7156, SOCKY_UDP_REUSEADDR | SOCKY_UDP_NONBLOCKING, error_str, sizeof(error_str));
		check_test(!IS_INVALID_SOCKET(sock1), "sock1: socky_udp_open(7156) succeeds");

		SOCKET sock2 = socky_udp_open(7156, SOCKY_UDP_REUSEADDR | SOCKY_UDP_NONBLOCKING, error_str, sizeof(error_str));
		check_test(!IS_INVALID_SOCKET(sock2), "sock2: socky_udp_open(7156) succeeds");

		socky_udp_close(sock1);
		check_test(1, "sock1: socky_udp_close() succeeds");

		socky_udp_close(sock2);
		check_test(1, "sock2: socky_udp_close() succeeds");
	}

	announce_tests("socky_cleanup");
	{
		const int cleanup_result = socky_cleanup();
		check_test(cleanup_result == 0, "socky_cleanup() returns 0");
	}

	printf("\nAll tests passed.\n");
	return 0;
}
