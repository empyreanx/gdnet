/**
 @file  win32.c
 @brief PENet Win32 system specific functions
*/
#ifdef _WIN32

#define PENET_BUILDING_LIB 1
#include "penet/penet.h"
#include <windows.h>
#include <mmsystem.h>

static penet_uint32 timeBase = 0;

int
penet_initialize (void)
{
    WORD versionRequested = MAKEWORD (1, 1);
    WSADATA wsaData;

    if (WSAStartup (versionRequested, & wsaData))
       return -1;

    if (LOBYTE (wsaData.wVersion) != 1||
        HIBYTE (wsaData.wVersion) != 1)
    {
       WSACleanup ();

       return -1;
    }

    timeBeginPeriod (1);

    return 0;
}

void
penet_deinitialize (void)
{
    timeEndPeriod (1);

    WSACleanup ();
}

penet_uint32
penet_host_random_seed (void)
{
    return (penet_uint32) timeGetTime ();
}

penet_uint32
penet_time_get (void)
{
    return (penet_uint32) timeGetTime () - timeBase;
}

void
penet_time_set (penet_uint32 newTimeBase)
{
    timeBase = (penet_uint32) timeGetTime () - newTimeBase;
}

int
penet_address_set_host (PENetAddress * address, const char * name)
{
    struct hostent * hostEntry;

    hostEntry = gethostbyname (name);
    if (hostEntry == NULL ||
        hostEntry -> h_addrtype != AF_INET)
    {
        unsigned long host = inet_addr (name);
        if (host == INADDR_NONE)
            return -1;
        address -> host = host;
        return 0;
    }

    address -> host = * (penet_uint32 *) hostEntry -> h_addr_list [0];

    return 0;
}

int
penet_address_get_host_ip (const PENetAddress * address, char * name, size_t nameLength)
{
    char * addr = inet_ntoa (* (struct in_addr *) & address -> host);
    if (addr == NULL)
        return -1;
    else
    {
        size_t addrLen = strlen(addr);
        if (addrLen >= nameLength)
          return -1;
        memcpy (name, addr, addrLen + 1);
    }
    return 0;
}

int
penet_address_get_host (const PENetAddress * address, char * name, size_t nameLength)
{
    struct in_addr in;
    struct hostent * hostEntry;

    in.s_addr = address -> host;

    hostEntry = gethostbyaddr ((char *) & in, sizeof (struct in_addr), AF_INET);
    if (hostEntry == NULL)
      return penet_address_get_host_ip (address, name, nameLength);
    else
    {
       size_t hostLen = strlen (hostEntry -> h_name);
       if (hostLen >= nameLength)
         return -1;
       memcpy (name, hostEntry -> h_name, hostLen + 1);
    }

    return 0;
}

int
penet_socket_bind (PENetSocket socket, const PENetAddress * address)
{
    struct sockaddr_in sin;

    memset (& sin, 0, sizeof (struct sockaddr_in));

    sin.sin_family = AF_INET;

    if (address != NULL)
    {
       sin.sin_port = PENET_HOST_TO_NET_16 (address -> port);
       sin.sin_addr.s_addr = address -> host;
    }
    else
    {
       sin.sin_port = 0;
       sin.sin_addr.s_addr = INADDR_ANY;
    }

    return bind (socket,
                 (struct sockaddr *) & sin,
                 sizeof (struct sockaddr_in)) == SOCKET_ERROR ? -1 : 0;
}

int
penet_socket_get_address (PENetSocket socket, PENetAddress * address)
{
    struct sockaddr_in sin;
    int sinLength = sizeof (struct sockaddr_in);

    if (getsockname (socket, (struct sockaddr *) & sin, & sinLength) == -1)
      return -1;

    address -> host = (penet_uint32) sin.sin_addr.s_addr;
    address -> port = PENET_NET_TO_HOST_16 (sin.sin_port);

    return 0;
}

int
penet_socket_listen (PENetSocket socket, int backlog)
{
    return listen (socket, backlog < 0 ? SOMAXCONN : backlog) == SOCKET_ERROR ? -1 : 0;
}

PENetSocket
penet_socket_create (PENetSocketType type)
{
    return socket (PF_INET, type == PENET_SOCKET_TYPE_DATAGRAM ? SOCK_DGRAM : SOCK_STREAM, 0);
}

int
penet_socket_set_option (PENetSocket socket, PENetSocketOption option, int value)
{
    int result = SOCKET_ERROR;
    switch (option)
    {
        case PENET_SOCKOPT_NONBLOCK:
        {
            u_long nonBlocking = (u_long) value;
            result = ioctlsocket (socket, FIONBIO, & nonBlocking);
            break;
        }

        case PENET_SOCKOPT_BROADCAST:
            result = setsockopt (socket, SOL_SOCKET, SO_BROADCAST, (char *) & value, sizeof (int));
            break;

        case PENET_SOCKOPT_REUSEADDR:
            result = setsockopt (socket, SOL_SOCKET, SO_REUSEADDR, (char *) & value, sizeof (int));
            break;

        case PENET_SOCKOPT_RCVBUF:
            result = setsockopt (socket, SOL_SOCKET, SO_RCVBUF, (char *) & value, sizeof (int));
            break;

        case PENET_SOCKOPT_SNDBUF:
            result = setsockopt (socket, SOL_SOCKET, SO_SNDBUF, (char *) & value, sizeof (int));
            break;

        case PENET_SOCKOPT_RCVTIMEO:
            result = setsockopt (socket, SOL_SOCKET, SO_RCVTIMEO, (char *) & value, sizeof (int));
            break;

        case PENET_SOCKOPT_SNDTIMEO:
            result = setsockopt (socket, SOL_SOCKET, SO_SNDTIMEO, (char *) & value, sizeof (int));
            break;

        case PENET_SOCKOPT_NODELAY:
            result = setsockopt (socket, IPPROTO_TCP, TCP_NODELAY, (char *) & value, sizeof (int));
            break;

        default:
            break;
    }
    return result == SOCKET_ERROR ? -1 : 0;
}

int
penet_socket_get_option (PENetSocket socket, PENetSocketOption option, int * value)
{
    int result = SOCKET_ERROR, len;
    switch (option)
    {
        case PENET_SOCKOPT_ERROR:
            len = sizeof(int);
            result = getsockopt (socket, SOL_SOCKET, SO_ERROR, (char *) value, & len);
            break;

        default:
            break;
    }
    return result == SOCKET_ERROR ? -1 : 0;
}

int
penet_socket_connect (PENetSocket socket, const PENetAddress * address)
{
    struct sockaddr_in sin;
    int result;

    memset (& sin, 0, sizeof (struct sockaddr_in));

    sin.sin_family = AF_INET;
    sin.sin_port = PENET_HOST_TO_NET_16 (address -> port);
    sin.sin_addr.s_addr = address -> host;

    result = connect (socket, (struct sockaddr *) & sin, sizeof (struct sockaddr_in));
    if (result == SOCKET_ERROR && WSAGetLastError () != WSAEWOULDBLOCK)
      return -1;

    return 0;
}

PENetSocket
penet_socket_accept (PENetSocket socket, PENetAddress * address)
{
    SOCKET result;
    struct sockaddr_in sin;
    int sinLength = sizeof (struct sockaddr_in);

    result = accept (socket,
                     address != NULL ? (struct sockaddr *) & sin : NULL,
                     address != NULL ? & sinLength : NULL);

    if (result == INVALID_SOCKET)
      return PENET_SOCKET_NULL;

    if (address != NULL)
    {
        address -> host = (penet_uint32) sin.sin_addr.s_addr;
        address -> port = PENET_NET_TO_HOST_16 (sin.sin_port);
    }

    return result;
}

int
penet_socket_shutdown (PENetSocket socket, PENetSocketShutdown how)
{
    return shutdown (socket, (int) how) == SOCKET_ERROR ? -1 : 0;
}

void
penet_socket_destroy (PENetSocket socket)
{
    if (socket != INVALID_SOCKET)
      closesocket (socket);
}

int
penet_socket_send (PENetSocket socket,
                  const PENetAddress * address,
                  const PENetBuffer * buffers,
                  size_t bufferCount)
{
    struct sockaddr_in sin;
    DWORD sentLength;

    if (address != NULL)
    {
        memset (& sin, 0, sizeof (struct sockaddr_in));

        sin.sin_family = AF_INET;
        sin.sin_port = PENET_HOST_TO_NET_16 (address -> port);
        sin.sin_addr.s_addr = address -> host;
    }

    if (WSASendTo (socket,
                   (LPWSABUF) buffers,
                   (DWORD) bufferCount,
                   & sentLength,
                   0,
                   address != NULL ? (struct sockaddr *) & sin : NULL,
                   address != NULL ? sizeof (struct sockaddr_in) : 0,
                   NULL,
                   NULL) == SOCKET_ERROR)
    {
       if (WSAGetLastError () == WSAEWOULDBLOCK)
         return 0;

       return -1;
    }

    return (int) sentLength;
}

int
penet_socket_receive (PENetSocket socket,
                     PENetAddress * address,
                     PENetBuffer * buffers,
                     size_t bufferCount)
{
    INT sinLength = sizeof (struct sockaddr_in);
    DWORD flags = 0,
          recvLength;
    struct sockaddr_in sin;

    if (WSARecvFrom (socket,
                     (LPWSABUF) buffers,
                     (DWORD) bufferCount,
                     & recvLength,
                     & flags,
                     address != NULL ? (struct sockaddr *) & sin : NULL,
                     address != NULL ? & sinLength : NULL,
                     NULL,
                     NULL) == SOCKET_ERROR)
    {
       switch (WSAGetLastError ())
       {
       case WSAEWOULDBLOCK:
       case WSAECONNRESET:
          return 0;
       }

       return -1;
    }

    if (flags & MSG_PARTIAL)
      return -1;

    if (address != NULL)
    {
        address -> host = (penet_uint32) sin.sin_addr.s_addr;
        address -> port = PENET_NET_TO_HOST_16 (sin.sin_port);
    }

    return (int) recvLength;
}

int
penet_socketset_select (PENetSocket maxSocket, PENetSocketSet * readSet, PENetSocketSet * writeSet, penet_uint32 timeout)
{
    struct timeval timeVal;

    timeVal.tv_sec = timeout / 1000;
    timeVal.tv_usec = (timeout % 1000) * 1000;

    return select (maxSocket + 1, readSet, writeSet, NULL, & timeVal);
}

int
penet_socket_wait (PENetSocket socket, penet_uint32 * condition, penet_uint32 timeout)
{
    fd_set readSet, writeSet;
    struct timeval timeVal;
    int selectCount;

    timeVal.tv_sec = timeout / 1000;
    timeVal.tv_usec = (timeout % 1000) * 1000;

    FD_ZERO (& readSet);
    FD_ZERO (& writeSet);

    if (* condition & PENET_SOCKET_WAIT_SEND)
      FD_SET (socket, & writeSet);

    if (* condition & PENET_SOCKET_WAIT_RECEIVE)
      FD_SET (socket, & readSet);

    selectCount = select (socket + 1, & readSet, & writeSet, NULL, & timeVal);

    if (selectCount < 0)
      return -1;

    * condition = PENET_SOCKET_WAIT_NONE;

    if (selectCount == 0)
      return 0;

    if (FD_ISSET (socket, & writeSet))
      * condition |= PENET_SOCKET_WAIT_SEND;

    if (FD_ISSET (socket, & readSet))
      * condition |= PENET_SOCKET_WAIT_RECEIVE;

    return 0;
}

#endif
