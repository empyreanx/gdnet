/** 
 @file  unix.h
 @brief PENet Unix header
*/
#ifndef __PENET_UNIX_H__
#define __PENET_UNIX_H__

#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#ifdef MSG_MAXIOVLEN
#define PENET_BUFFER_MAXIMUM MSG_MAXIOVLEN
#endif

typedef int PENetSocket;

#define PENET_SOCKET_NULL -1

#define PENET_HOST_TO_NET_16(value) (htons (value)) /**< macro that converts host to net byte-order of a 16-bit value */
#define PENET_HOST_TO_NET_32(value) (htonl (value)) /**< macro that converts host to net byte-order of a 32-bit value */

#define PENET_NET_TO_HOST_16(value) (ntohs (value)) /**< macro that converts net to host byte-order of a 16-bit value */
#define PENET_NET_TO_HOST_32(value) (ntohl (value)) /**< macro that converts net to host byte-order of a 32-bit value */

typedef struct
{
    void * data;
    size_t dataLength;
} PENetBuffer;

#define PENET_CALLBACK

#define PENET_API extern

typedef fd_set PENetSocketSet;

#define PENET_SOCKETSET_EMPTY(sockset)          FD_ZERO (& (sockset))
#define PENET_SOCKETSET_ADD(sockset, socket)    FD_SET (socket, & (sockset))
#define PENET_SOCKETSET_REMOVE(sockset, socket) FD_CLR (socket, & (sockset))
#define PENET_SOCKETSET_CHECK(sockset, socket)  FD_ISSET (socket, & (sockset))

#endif /* __PENET_UNIX_H__ */
