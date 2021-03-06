#include "socky.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#if _MSC_VER <= 1700
#define snprintf(_Buf, _Len, _Fmt, ...) sprintf_s(_Buf, _Len, _Fmt, __VA_ARGS__)
#endif
#endif

#define MAX_ADAPTER_ADDRESSES 8

static char s_adapter_addresses[MAX_ADAPTER_ADDRESSES][32];
static size_t s_num_adapter_addresses = 0;

int socky_init()
{
#ifdef _WIN32
	WSADATA wsaData;
	return WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
	return 0;
#endif
}

int socky_cleanup()
{
#ifdef _WIN32
	return WSACleanup();
#else
	return 0;
#endif
}

int socky_error_format(const char* prefix, char* errorStr, size_t errorStrLen)
{
	// Get the error code associated with the last socket operation that failed
#ifdef _WIN32
	const int code = WSAGetLastError();
#else
	const int code = errno;
#endif

	// If we've been supplied with a buffer to store the human-readable error string, attempt to populate it
	if (errorStr && errorStrLen)
	{
		// Write a prefix containing the error number and the user-specified message, if we have one
		int charsWritten;
		if (prefix)
		{
			charsWritten = snprintf(errorStr, errorStrLen, "%s (error %d): ", prefix, code);
		}
		else
		{
			charsWritten = snprintf(errorStr, errorStrLen, "Error %d: ", code);
		}
		errorStr += charsWritten;
		errorStrLen -= charsWritten;

		// If we still have any writeable space in the buffer, append the string representation of the error code
		if (errorStrLen)
		{
#ifdef _WIN32
			const DWORD formatFlags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
			FormatMessage(formatFlags, NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorStr, (DWORD)errorStrLen, NULL);
#else
			strerror_r(code, errorStr, errorStrLen);
#endif
		}
	}

	// Return the error code directly
	return code;
}

void socky_adapters_init()
{
	int i;
#ifdef _WIN32
	IP_ADAPTER_INFO infos[MAX_ADAPTER_ADDRESSES];
	PIP_ADAPTER_INFO info;
	ULONG bufferSize;
#endif

	// Explicitly reset our module-level state: clear all names and reset adapter count to zero
	s_num_adapter_addresses = 0;
	for (i = 0; i < MAX_ADAPTER_ADDRESSES; i++)
	{
		s_adapter_addresses[i][0] = '\0';
	}

#ifdef _WIN32
	bufferSize = sizeof(infos);
	if (GetAdaptersInfo(infos, &bufferSize) == ERROR_SUCCESS)
	{
		// Iterate over all internet adapters
		for (info = infos; info; info = info->Next)
		{
			// Copy the address string and increment our count of known adapter addresses
			strcpy_s(s_adapter_addresses[s_num_adapter_addresses], sizeof(s_adapter_addresses[0]), info->IpAddressList.IpAddress.String);
			s_num_adapter_addresses++;
		}
	}
#else
	struct ifaddrs* addrs;
	struct ifaddrs* item;
	getifaddrs(&addrs);
	for (item = addrs; item; item = item->ifa_next)
	{
		// Iterate over all internet adapters
		if (item->ifa_addr && item->ifa_addr->sa_family == AF_INET)
		{
			// Get the string representation of the adapter address, and ensure it's not too large
			struct sockaddr_in* addr = (struct sockaddr_in*)item->ifa_addr;
			const char* ip = inet_ntoa(addr->sin_addr);
			if (strlen(ip) + 1 > sizeof(s_adapter_addresses[0]))
			{
				continue;
			}

			// Copy the address string and increment our count of known adapter addresses
			strncpy(s_adapter_addresses[s_num_adapter_addresses], ip, sizeof(s_adapter_addresses[s_num_adapter_addresses]));
			s_num_adapter_addresses++;
			if (s_num_adapter_addresses >= MAX_ADAPTER_ADDRESSES)
			{
				break;
			}
		}
	}
	freeifaddrs(addrs);
#endif
}

int socky_adapters_num()
{
	return (int)s_num_adapter_addresses;
}

const char* socky_adapters_get_ip(int index)
{
	if (index >= 0 && index < s_num_adapter_addresses)
	{
		return s_adapter_addresses[index];
	}
	return NULL;
}

SOCKET socky_udp_open(unsigned short bindPort, int flags, char* errorStr, size_t errorStrLen)
{
	struct sockaddr_in bindAddr;

	// Attempt to create the socket
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (IS_INVALID_SOCKET(sock))
	{
		socky_error_format("Failed to create socket", errorStr, errorStrLen);
		return sock;
	}

	// If the socket should have SO_REUSEADDR set, attempt to set that socket option
	if (flags & SOCKY_UDP_REUSEADDR)
	{
		int reuseVal = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseVal, sizeof(reuseVal)) != 0)
		{
			socky_error_format("Failed to set SO_REUSEADDR", errorStr, errorStrLen);
			socky_udp_close(sock);
			return INVALID_SOCKET;
		}

		// Also set SO_REUSEPORT if it's part of the socket API on this platform
#ifdef SO_REUSEPORT
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuseVal, sizeof(reuseVal)) != 0)
		{
			socky_error_format("Failed to set SO_REUSEPORT", errorStr, errorStrLen);
			socky_udp_close(sock);
			return INVALID_SOCKET;
		}
#endif
	}

	// If the socket should be non-blocking, set FIONBIO
	if (flags & SOCKY_UDP_NONBLOCKING)
	{
		u_long nonBlockingMode = 1;
		if (ioctl(sock, FIONBIO, &nonBlockingMode) != 0)
		{
			socky_error_format("Failed to set FIONBIO", errorStr, errorStrLen);
			socky_udp_close(sock);
			return INVALID_SOCKET;
		}
	}

	// Prepare the address struct to define the address we'll bind to
	memset(&bindAddr, 0, sizeof(bindAddr));
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bindAddr.sin_port = htons(bindPort);

	// Attempt to bind the socket to that address
	if (bind(sock, (struct sockaddr*)&bindAddr, sizeof(bindAddr)) != 0)
	{
		char errorPrefix[64];
		snprintf(errorPrefix, sizeof(errorPrefix), "Failed to bind socket to port %hu", bindPort);
		socky_error_format(errorPrefix, errorStr, errorStrLen);
		socky_udp_close(sock);
		return INVALID_SOCKET;
	}

	// If all went well, return the socket
	return sock;
}

void socky_udp_close(SOCKET sock)
{
	if (!IS_INVALID_SOCKET(sock))
	{
#ifdef _WIN32
		if (shutdown(sock, SD_BOTH) == 0)
		{
			closesocket(sock);
		}
#else
		if (shutdown(sock, SHUT_RDWR) == 0)
		{
			close(sock);
		}
#endif
	}
}

int socky_udp_has_data(SOCKET sock, u_long* outNumBytesAvailable)
{
	u_long numBytes = 0;
	if (ioctl(sock, FIONREAD, &numBytes) == 0)
	{
		if (outNumBytesAvailable)
		{
			*outNumBytesAvailable = numBytes;
		}
		return numBytes > 0;
	}
	return 0;
}

int socky_udp_read(SOCKET sock, char* buffer, size_t bufferSize, struct sockaddr_in* outFromAddr)
{
	int fromAddrLen = sizeof(struct sockaddr_in);
	int numBytesReceived = recvfrom(sock, buffer, (int)bufferSize, 0, (struct sockaddr*)outFromAddr, &fromAddrLen);
	if (numBytesReceived != SOCKET_ERROR)
	{
		return numBytesReceived;
	}
	return 0;
}
