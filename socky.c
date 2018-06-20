#include "socky.h"

#include <stdio.h>

/*
struct WinsockError
{
	WinsockError()
		: message(NULL)
		, code(-1)
	{
		code = WSAGetLastError();
		DWORD formatFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
		DWORD size = FormatMessage(formatFlags, NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&message, 0, NULL);
		message[size - 2] = '\0';
	}

	~WinsockError()
	{
		LocalFree(message);
	}

	TCHAR* message;
	int code;
};
*/

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
			charsWritten = sprintf_s(errorStr, errorStrLen, "%s (error %d): ", prefix, code);
		}
		else
		{
			charsWritten = sprintf_s(errorStr, errorStrLen, "Error %d: ", code);
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
}

int socky_adapters_num()
{
	return 0;
}

const char* socky_adapters_get_ip(int index)
{
	return NULL;
}

SOCKET socky_udp_open(unsigned short bindPort, int flags, char* errorStr, size_t errorStrLen)
{
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
		if (ioctlsocket(sock, FIONBIO, &nonBlockingMode) != 0)
		{
			socky_error_format("Failed to set FIONBIO", errorStr, errorStrLen);
			socky_udp_close(sock);
			return INVALID_SOCKET;
		}
	}

	// Prepare the address struct to define the address we'll bind to
	struct sockaddr_in bindAddr;
	memset(&bindAddr, 0, sizeof(bindAddr));
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bindAddr.sin_port = htons(bindPort);

	// Attempt to bind the socket to that address
	if (bind(sock, (struct sockaddr*)&bindAddr, sizeof(bindAddr)) != 0)
	{
		char errorPrefix[64];
		sprintf_s(errorPrefix, sizeof(errorPrefix), "Failed to bind socket to port %hu", bindPort);
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