/** 
 @file  win32.h
 @brief PENet Win32 header
*/
#ifndef __PENET_WIN32_H__
#define __PENET_WIN32_H__

#ifdef _MSC_VER
#ifdef PENET_BUILDING_LIB
#pragma warning (disable: 4267) // size_t to int conversion
#pragma warning (disable: 4244) // 64bit to 32bit int
#pragma warning (disable: 4018) // signed/unsigned mismatch
#pragma warning (disable: 4146) // unary minus operator applied to unsigned type
#endif
#endif

#include <stdlib.h>
#include <winsock2.h>

typedef SOCKET PENetSocket;

#define PENET_SOCKET_NULL INVALID_SOCKET

#define PENET_HOST_TO_NET_16(value) (htons (value))
#define PENET_HOST_TO_NET_32(value) (htonl (value))

#define PENET_NET_TO_HOST_16(value) (ntohs (value))
#define PENET_NET_TO_HOST_32(value) (ntohl (value))

typedef struct
{
    size_t dataLength;
    void * data;
} PENetBuffer;

#define PENET_CALLBACK __cdecl

#ifdef PENET_DLL
#ifdef PENET_BUILDING_LIB
#define PENET_API __declspec( dllexport )
#else
#define PENET_API __declspec( dllimport )
#endif /* PENET_BUILDING_LIB */
#else /* !PENET_DLL */
#define PENET_API extern
#endif /* PENET_DLL */

typedef fd_set PENetSocketSet;

#define PENET_SOCKETSET_EMPTY(sockset)          FD_ZERO (& (sockset))
#define PENET_SOCKETSET_ADD(sockset, socket)    FD_SET (socket, & (sockset))
#define PENET_SOCKETSET_REMOVE(sockset, socket) FD_CLR (socket, & (sockset))
#define PENET_SOCKETSET_CHECK(sockset, socket)  FD_ISSET (socket, & (sockset))

#endif /* __PENET_WIN32_H__ */
