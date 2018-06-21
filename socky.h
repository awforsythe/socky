/**
	This is a quick-and-dirty, source-only library that aims to provide cross-platform socket functionality.
	Only a limited subset is provided, since we only care about supporting the use cases we need.
	Tested platforms are Windows (VC++ 2017) and Android (NDK r12b).
*/
#pragma once

#ifdef _WIN32
  #include <winsock2.h>
  #include <Ws2tcpip.h>
  #include <iphlpapi.h>
  #define ioctl ioctlsocket
  #define IS_INVALID_SOCKET(_Sock) (_Sock == INVALID_SOCKET)
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/ioctl.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <ifaddrs.h>
  #include <unistd.h>
  #include <errno.h>
  typedef int SOCKET;
  #define IS_INVALID_SOCKET(_Sock) (_Sock < 0)
  #ifndef INVALID_SOCKET
    #define INVALID_SOCKET -1
  #endif
#endif

///
/// Library setup and teardown
///

/** Performs any required initialization of the network subsystem: should be called once on application start. */
int socky_init();

/** Counterpart to socky_init; should be called on application exit. */
int socky_cleanup();

///
/// Utility
///

/** Gets and returns the code associated with the last failing socket operation.
	If an errorStr buffer is provided, also formats a human-readable message describing the error,
	optionally prepending a user-supplied prefix for context. */
int socky_error_format(const char* prefix, char* errorStr, size_t errorStrLen);

///
/// Network adapter name lookup
///

/** Performs the initial system calls to populate the in-memory listing of network adapter addresses.
	Must be called before any other socky_adapters_* functions are used. */
void socky_adapters_init();

/** Returns the number of network adapters for which address information is available. */
int socky_adapters_num();

/** Returns the IP address associated with the network adapter at the given index,
	or NULL if the index is not valid. */
const char* socky_adapters_get_ip(int index);

///
/// UDP sockets (simple abstraction for sockets that bind to INADDR_ANY on a particular port)
///		Functionality exposed here is purposefully restrictive in order to limit the scope of this library to what
///		it's needed for at the moment.
///

// Flags for socky_udp_open
#define SOCKY_UDP_REUSEADDR		(1 << 0)
#define SOCKY_UDP_NONBLOCKING	(1 << 1)

/** Attempts to create a DGRAM socket and bind it to 0.0.0.0:<bindPort>. Check IS_INVALID_SOCKET on return.
	If invalid and caller-supplied errorStr is non-null, a human-readable error string will be passed out. */
SOCKET socky_udp_open(unsigned short bindPort, int flags, char* errorStr, size_t errorStrLen);

/** Closes the given socket, provided it's not invalid. */
void socky_udp_close(SOCKET sock);
