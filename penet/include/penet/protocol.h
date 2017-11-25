/** 
 @file  protocol.h
 @brief PENet protocol
*/
#ifndef __PENET_PROTOCOL_H__
#define __PENET_PROTOCOL_H__

#include "penet/types.h"

enum
{
   PENET_PROTOCOL_MINIMUM_MTU             = 576,
   PENET_PROTOCOL_MAXIMUM_MTU             = 4096,
   PENET_PROTOCOL_MAXIMUM_PACKET_COMMANDS = 32,
   PENET_PROTOCOL_MINIMUM_WINDOW_SIZE     = 4096,
   PENET_PROTOCOL_MAXIMUM_WINDOW_SIZE     = 65536,
   PENET_PROTOCOL_MINIMUM_CHANNEL_COUNT   = 1,
   PENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT   = 255,
   PENET_PROTOCOL_MAXIMUM_PEER_ID         = 0xFFF,
   PENET_PROTOCOL_MAXIMUM_FRAGMENT_COUNT  = 1024 * 1024
};

typedef enum _PENetProtocolCommand
{
   PENET_PROTOCOL_COMMAND_NONE               = 0,
   PENET_PROTOCOL_COMMAND_ACKNOWLEDGE        = 1,
   PENET_PROTOCOL_COMMAND_CONNECT            = 2,
   PENET_PROTOCOL_COMMAND_VERIFY_CONNECT     = 3,
   PENET_PROTOCOL_COMMAND_DISCONNECT         = 4,
   PENET_PROTOCOL_COMMAND_PING               = 5,
   PENET_PROTOCOL_COMMAND_SEND_RELIABLE      = 6,
   PENET_PROTOCOL_COMMAND_SEND_UNRELIABLE    = 7,
   PENET_PROTOCOL_COMMAND_SEND_FRAGMENT      = 8,
   PENET_PROTOCOL_COMMAND_SEND_UNSEQUENCED   = 9,
   PENET_PROTOCOL_COMMAND_BANDWIDTH_LIMIT    = 10,
   PENET_PROTOCOL_COMMAND_THROTTLE_CONFIGURE = 11,
   PENET_PROTOCOL_COMMAND_SEND_UNRELIABLE_FRAGMENT = 12,
   PENET_PROTOCOL_COMMAND_COUNT              = 13,

   PENET_PROTOCOL_COMMAND_MASK               = 0x0F
} PENetProtocolCommand;

typedef enum _PENetProtocolFlag
{
   PENET_PROTOCOL_COMMAND_FLAG_ACKNOWLEDGE = (1 << 7),
   PENET_PROTOCOL_COMMAND_FLAG_UNSEQUENCED = (1 << 6),

   PENET_PROTOCOL_HEADER_FLAG_COMPRESSED = (1 << 14),
   PENET_PROTOCOL_HEADER_FLAG_SENT_TIME  = (1 << 15),
   PENET_PROTOCOL_HEADER_FLAG_MASK       = PENET_PROTOCOL_HEADER_FLAG_COMPRESSED | PENET_PROTOCOL_HEADER_FLAG_SENT_TIME,

   PENET_PROTOCOL_HEADER_SESSION_MASK    = (3 << 12),
   PENET_PROTOCOL_HEADER_SESSION_SHIFT   = 12
} PENetProtocolFlag;

#ifdef _MSC_VER
#pragma pack(push, 1)
#define PENET_PACKED
#elif defined(__GNUC__) || defined(__clang__)
#define PENET_PACKED __attribute__ ((packed))
#else
#define PENET_PACKED
#endif

typedef struct _PENetProtocolHeader
{
   penet_uint16 peerID;
   penet_uint16 sentTime;
} PENET_PACKED PENetProtocolHeader;

typedef struct _PENetProtocolCommandHeader
{
   penet_uint8 command;
   penet_uint8 channelID;
   penet_uint16 reliableSequenceNumber;
} PENET_PACKED PENetProtocolCommandHeader;

typedef struct _PENetProtocolAcknowledge
{
   PENetProtocolCommandHeader header;
   penet_uint16 receivedReliableSequenceNumber;
   penet_uint16 receivedSentTime;
} PENET_PACKED PENetProtocolAcknowledge;

typedef struct _PENetProtocolConnect
{
   PENetProtocolCommandHeader header;
   penet_uint16 outgoingPeerID;
   penet_uint8  incomingSessionID;
   penet_uint8  outgoingSessionID;
   penet_uint32 mtu;
   penet_uint32 windowSize;
   penet_uint32 channelCount;
   penet_uint32 incomingBandwidth;
   penet_uint32 outgoingBandwidth;
   penet_uint32 packetThrottleInterval;
   penet_uint32 packetThrottleAcceleration;
   penet_uint32 packetThrottleDeceleration;
   penet_uint32 connectID;
   penet_uint32 data;
} PENET_PACKED PENetProtocolConnect;

typedef struct _PENetProtocolVerifyConnect
{
   PENetProtocolCommandHeader header;
   penet_uint16 outgoingPeerID;
   penet_uint8  incomingSessionID;
   penet_uint8  outgoingSessionID;
   penet_uint32 mtu;
   penet_uint32 windowSize;
   penet_uint32 channelCount;
   penet_uint32 incomingBandwidth;
   penet_uint32 outgoingBandwidth;
   penet_uint32 packetThrottleInterval;
   penet_uint32 packetThrottleAcceleration;
   penet_uint32 packetThrottleDeceleration;
   penet_uint32 connectID;
} PENET_PACKED PENetProtocolVerifyConnect;

typedef struct _PENetProtocolBandwidthLimit
{
   PENetProtocolCommandHeader header;
   penet_uint32 incomingBandwidth;
   penet_uint32 outgoingBandwidth;
} PENET_PACKED PENetProtocolBandwidthLimit;

typedef struct _PENetProtocolThrottleConfigure
{
   PENetProtocolCommandHeader header;
   penet_uint32 packetThrottleInterval;
   penet_uint32 packetThrottleAcceleration;
   penet_uint32 packetThrottleDeceleration;
} PENET_PACKED PENetProtocolThrottleConfigure;

typedef struct _PENetProtocolDisconnect
{
   PENetProtocolCommandHeader header;
   penet_uint32 data;
} PENET_PACKED PENetProtocolDisconnect;

typedef struct _PENetProtocolPing
{
   PENetProtocolCommandHeader header;
} PENET_PACKED PENetProtocolPing;

typedef struct _PENetProtocolSendReliable
{
   PENetProtocolCommandHeader header;
   penet_uint16 dataLength;
} PENET_PACKED PENetProtocolSendReliable;

typedef struct _PENetProtocolSendUnreliable
{
   PENetProtocolCommandHeader header;
   penet_uint16 unreliableSequenceNumber;
   penet_uint16 dataLength;
} PENET_PACKED PENetProtocolSendUnreliable;

typedef struct _PENetProtocolSendUnsequenced
{
   PENetProtocolCommandHeader header;
   penet_uint16 unsequencedGroup;
   penet_uint16 dataLength;
} PENET_PACKED PENetProtocolSendUnsequenced;

typedef struct _PENetProtocolSendFragment
{
   PENetProtocolCommandHeader header;
   penet_uint16 startSequenceNumber;
   penet_uint16 dataLength;
   penet_uint32 fragmentCount;
   penet_uint32 fragmentNumber;
   penet_uint32 totalLength;
   penet_uint32 fragmentOffset;
} PENET_PACKED PENetProtocolSendFragment;

typedef union _PENetProtocol
{
   PENetProtocolCommandHeader header;
   PENetProtocolAcknowledge acknowledge;
   PENetProtocolConnect connect;
   PENetProtocolVerifyConnect verifyConnect;
   PENetProtocolDisconnect disconnect;
   PENetProtocolPing ping;
   PENetProtocolSendReliable sendReliable;
   PENetProtocolSendUnreliable sendUnreliable;
   PENetProtocolSendUnsequenced sendUnsequenced;
   PENetProtocolSendFragment sendFragment;
   PENetProtocolBandwidthLimit bandwidthLimit;
   PENetProtocolThrottleConfigure throttleConfigure;
} PENET_PACKED PENetProtocol;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

#endif /* __PENET_PROTOCOL_H__ */
