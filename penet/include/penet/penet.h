/**
 @file  penet.h
 @brief PENet public header file
*/
#ifndef __PENET_PENET_H__
#define __PENET_PENET_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>

#ifdef _WIN32
#include "penet/win32.h"
#else
#include "penet/unix.h"
#endif

#include "penet/types.h"
#include "penet/protocol.h"
#include "penet/list.h"
#include "penet/callbacks.h"

#define PENET_VERSION_MAJOR 1
#define PENET_VERSION_MINOR 3
#define PENET_VERSION_PATCH 13
#define PENET_VERSION_CREATE(major, minor, patch) (((major)<<16) | ((minor)<<8) | (patch))
#define PENET_VERSION_GET_MAJOR(version) (((version)>>16)&0xFF)
#define PENET_VERSION_GET_MINOR(version) (((version)>>8)&0xFF)
#define PENET_VERSION_GET_PATCH(version) ((version)&0xFF)
#define PENET_VERSION PENET_VERSION_CREATE(PENET_VERSION_MAJOR, PENET_VERSION_MINOR, PENET_VERSION_PATCH)

typedef penet_uint32 PENetVersion;

struct _PENetHost;
struct _PENetEvent;
struct _PENetPacket;

typedef enum _PENetSocketType
{
   PENET_SOCKET_TYPE_STREAM   = 1,
   PENET_SOCKET_TYPE_DATAGRAM = 2
} PENetSocketType;

typedef enum _PENetSocketWait
{
   PENET_SOCKET_WAIT_NONE      = 0,
   PENET_SOCKET_WAIT_SEND      = (1 << 0),
   PENET_SOCKET_WAIT_RECEIVE   = (1 << 1),
   PENET_SOCKET_WAIT_INTERRUPT = (1 << 2)
} PENetSocketWait;

typedef enum _PENetSocketOption
{
   PENET_SOCKOPT_NONBLOCK  = 1,
   PENET_SOCKOPT_BROADCAST = 2,
   PENET_SOCKOPT_RCVBUF    = 3,
   PENET_SOCKOPT_SNDBUF    = 4,
   PENET_SOCKOPT_REUSEADDR = 5,
   PENET_SOCKOPT_RCVTIMEO  = 6,
   PENET_SOCKOPT_SNDTIMEO  = 7,
   PENET_SOCKOPT_ERROR     = 8,
   PENET_SOCKOPT_NODELAY   = 9
} PENetSocketOption;

typedef enum _PENetSocketShutdown
{
    PENET_SOCKET_SHUTDOWN_READ       = 0,
    PENET_SOCKET_SHUTDOWN_WRITE      = 1,
    PENET_SOCKET_SHUTDOWN_READ_WRITE = 2
} PENetSocketShutdown;

#define PENET_HOST_ANY       0
#define PENET_HOST_BROADCAST 0xFFFFFFFFU
#define PENET_PORT_ANY       0

/**
 * Portable internet address structure.
 *
 * The host must be specified in network byte-order, and the port must be in host
 * byte-order. The constant PENET_HOST_ANY may be used to specify the default
 * server host. The constant PENET_HOST_BROADCAST may be used to specify the
 * broadcast address (255.255.255.255).  This makes sense for penet_host_connect,
 * but not for penet_host_create.  Once a server responds to a broadcast, the
 * address is updated from PENET_HOST_BROADCAST to the server's actual IP address.
 */
typedef struct _PENetAddress
{
   penet_uint32 host;
   penet_uint16 port;
} PENetAddress;

/**
 * Packet flag bit constants.
 *
 * The host must be specified in network byte-order, and the port must be in
 * host byte-order. The constant PENET_HOST_ANY may be used to specify the
 * default server host.

   @sa PENetPacket
*/
typedef enum _PENetPacketFlag
{
   /** packet must be received by the target peer and resend attempts should be
     * made until the packet is delivered */
   PENET_PACKET_FLAG_RELIABLE    = (1 << 0),
   /** packet will not be sequenced with other packets
     * not supported for reliable packets
     */
   PENET_PACKET_FLAG_UNSEQUENCED = (1 << 1),
   /** packet will not allocate data, and user must supply it instead */
   PENET_PACKET_FLAG_NO_ALLOCATE = (1 << 2),
   /** packet will be fragmented using unreliable (instead of reliable) sends
     * if it exceeds the MTU */
   PENET_PACKET_FLAG_UNRELIABLE_FRAGMENT = (1 << 3),

   /** whether the packet has been sent from all queues it has been entered into */
   PENET_PACKET_FLAG_SENT = (1<<8)
} PENetPacketFlag;

typedef void (PENET_CALLBACK * PENetPacketFreeCallback) (struct _PENetPacket *);

/**
 * PENet packet structure.
 *
 * An PENet data packet that may be sent to or received from a peer. The shown
 * fields should only be read and never modified. The data field contains the
 * allocated data for the packet. The dataLength fields specifies the length
 * of the allocated data.  The flags field is either 0 (specifying no flags),
 * or a bitwise-or of any combination of the following flags:
 *
 *    PENET_PACKET_FLAG_RELIABLE - packet must be received by the target peer
 *    and resend attempts should be made until the packet is delivered
 *
 *    PENET_PACKET_FLAG_UNSEQUENCED - packet will not be sequenced with other packets
 *    (not supported for reliable packets)
 *
 *    PENET_PACKET_FLAG_NO_ALLOCATE - packet will not allocate data, and user must supply it instead

   @sa PENetPacketFlag
 */
typedef struct _PENetPacket
{
   size_t                   referenceCount;  /**< internal use only */
   penet_uint32              flags;           /**< bitwise-or of PENetPacketFlag constants */
   penet_uint8 *             data;            /**< allocated data for packet */
   size_t                   dataLength;      /**< length of data */
   PENetPacketFreeCallback   freeCallback;    /**< function to be called when the packet is no longer in use */
   void *                   userData;        /**< application private data, may be freely modified */
} PENetPacket;

typedef struct _PENetAcknowledgement
{
   PENetListNode acknowledgementList;
   penet_uint32  sentTime;
   PENetProtocol command;
} PENetAcknowledgement;

typedef struct _PENetOutgoingCommand
{
   PENetListNode outgoingCommandList;
   penet_uint16  reliableSequenceNumber;
   penet_uint16  unreliableSequenceNumber;
   penet_uint32  sentTime;
   penet_uint32  roundTripTimeout;
   penet_uint32  roundTripTimeoutLimit;
   penet_uint32  fragmentOffset;
   penet_uint16  fragmentLength;
   penet_uint16  sendAttempts;
   PENetProtocol command;
   PENetPacket * packet;
} PENetOutgoingCommand;

typedef struct _PENetIncomingCommand
{
   PENetListNode     incomingCommandList;
   penet_uint16      reliableSequenceNumber;
   penet_uint16      unreliableSequenceNumber;
   PENetProtocol     command;
   penet_uint32      fragmentCount;
   penet_uint32      fragmentsRemaining;
   penet_uint32 *    fragments;
   PENetPacket *     packet;
} PENetIncomingCommand;

typedef enum _PENetPeerState
{
   PENET_PEER_STATE_DISCONNECTED                = 0,
   PENET_PEER_STATE_CONNECTING                  = 1,
   PENET_PEER_STATE_ACKNOWLEDGING_CONNECT       = 2,
   PENET_PEER_STATE_CONNECTION_PENDING          = 3,
   PENET_PEER_STATE_CONNECTION_SUCCEEDED        = 4,
   PENET_PEER_STATE_CONNECTED                   = 5,
   PENET_PEER_STATE_DISCONNECT_LATER            = 6,
   PENET_PEER_STATE_DISCONNECTING               = 7,
   PENET_PEER_STATE_ACKNOWLEDGING_DISCONNECT    = 8,
   PENET_PEER_STATE_ZOMBIE                      = 9
} PENetPeerState;

#ifndef PENET_BUFFER_MAXIMUM
#define PENET_BUFFER_MAXIMUM (1 + 2 * PENET_PROTOCOL_MAXIMUM_PACKET_COMMANDS)
#endif

enum
{
   PENET_HOST_RECEIVE_BUFFER_SIZE          = 256 * 1024,
   PENET_HOST_SEND_BUFFER_SIZE             = 256 * 1024,
   PENET_HOST_BANDWIDTH_THROTTLE_INTERVAL  = 1000,
   PENET_HOST_DEFAULT_MTU                  = 1400,
   PENET_HOST_DEFAULT_MAXIMUM_PACKET_SIZE  = 32 * 1024 * 1024,
   PENET_HOST_DEFAULT_MAXIMUM_WAITING_DATA = 32 * 1024 * 1024,

   PENET_PEER_DEFAULT_ROUND_TRIP_TIME      = 500,
   PENET_PEER_DEFAULT_PACKET_THROTTLE      = 32,
   PENET_PEER_PACKET_THROTTLE_SCALE        = 32,
   PENET_PEER_PACKET_THROTTLE_COUNTER      = 7,
   PENET_PEER_PACKET_THROTTLE_ACCELERATION = 2,
   PENET_PEER_PACKET_THROTTLE_DECELERATION = 2,
   PENET_PEER_PACKET_THROTTLE_INTERVAL     = 5000,
   PENET_PEER_PACKET_LOSS_SCALE            = (1 << 16),
   PENET_PEER_PACKET_LOSS_INTERVAL         = 10000,
   PENET_PEER_WINDOW_SIZE_SCALE            = 64 * 1024,
   PENET_PEER_TIMEOUT_LIMIT                = 32,
   PENET_PEER_TIMEOUT_MINIMUM              = 5000,
   PENET_PEER_TIMEOUT_MAXIMUM              = 30000,
   PENET_PEER_PING_INTERVAL                = 500,
   PENET_PEER_UNSEQUENCED_WINDOWS          = 64,
   PENET_PEER_UNSEQUENCED_WINDOW_SIZE      = 1024,
   PENET_PEER_FREE_UNSEQUENCED_WINDOWS     = 32,
   PENET_PEER_RELIABLE_WINDOWS             = 16,
   PENET_PEER_RELIABLE_WINDOW_SIZE         = 0x1000,
   PENET_PEER_FREE_RELIABLE_WINDOWS        = 8
};

typedef struct _PENetChannel
{
   penet_uint16  outgoingReliableSequenceNumber;
   penet_uint16  outgoingUnreliableSequenceNumber;
   penet_uint16  usedReliableWindows;
   penet_uint16  reliableWindows [PENET_PEER_RELIABLE_WINDOWS];
   penet_uint16  incomingReliableSequenceNumber;
   penet_uint16  incomingUnreliableSequenceNumber;
   PENetList     incomingReliableCommands;
   PENetList     incomingUnreliableCommands;
} PENetChannel;

/**
 * An PENet peer which data packets may be sent or received from.
 *
 * No fields should be modified unless otherwise specified.
 */
typedef struct _PENetPeer
{
   PENetListNode  dispatchList;
   struct _PENetHost * host;
   penet_uint16   outgoingPeerID;
   penet_uint16   incomingPeerID;
   penet_uint32   connectID;
   penet_uint8    outgoingSessionID;
   penet_uint8    incomingSessionID;
   PENetAddress   address;            /**< Internet address of the peer */
   void *        data;               /**< Application private data, may be freely modified */
   PENetPeerState state;
   PENetChannel * channels;
   size_t        channelCount;       /**< Number of channels allocated for communication with peer */
   penet_uint32   incomingBandwidth;  /**< Downstream bandwidth of the client in bytes/second */
   penet_uint32   outgoingBandwidth;  /**< Upstream bandwidth of the client in bytes/second */
   penet_uint32   incomingBandwidthThrottleEpoch;
   penet_uint32   outgoingBandwidthThrottleEpoch;
   penet_uint32   incomingDataTotal;
   penet_uint32   outgoingDataTotal;
   penet_uint32   lastSendTime;
   penet_uint32   lastReceiveTime;
   penet_uint32   nextTimeout;
   penet_uint32   earliestTimeout;
   penet_uint32   packetLossEpoch;
   penet_uint32   packetsSent;
   penet_uint32   packetsLost;
   penet_uint32   packetLoss;          /**< mean packet loss of reliable packets as a ratio with respect to the constant PENET_PEER_PACKET_LOSS_SCALE */
   penet_uint32   packetLossVariance;
   penet_uint32   packetThrottle;
   penet_uint32   packetThrottleLimit;
   penet_uint32   packetThrottleCounter;
   penet_uint32   packetThrottleEpoch;
   penet_uint32   packetThrottleAcceleration;
   penet_uint32   packetThrottleDeceleration;
   penet_uint32   packetThrottleInterval;
   penet_uint32   pingInterval;
   penet_uint32   timeoutLimit;
   penet_uint32   timeoutMinimum;
   penet_uint32   timeoutMaximum;
   penet_uint32   lastRoundTripTime;
   penet_uint32   lowestRoundTripTime;
   penet_uint32   lastRoundTripTimeVariance;
   penet_uint32   highestRoundTripTimeVariance;
   penet_uint32   roundTripTime;            /**< mean round trip time (RTT), in milliseconds, between sending a reliable packet and receiving its acknowledgement */
   penet_uint32   roundTripTimeVariance;
   penet_uint32   mtu;
   penet_uint32   windowSize;
   penet_uint32   reliableDataInTransit;
   penet_uint16   outgoingReliableSequenceNumber;
   PENetList      acknowledgements;
   PENetList      sentReliableCommands;
   PENetList      sentUnreliableCommands;
   PENetList      outgoingReliableCommands;
   PENetList      outgoingUnreliableCommands;
   PENetList      dispatchedCommands;
   int           needsDispatch;
   penet_uint16   incomingUnsequencedGroup;
   penet_uint16   outgoingUnsequencedGroup;
   penet_uint32   unsequencedWindow [PENET_PEER_UNSEQUENCED_WINDOW_SIZE / 32];
   penet_uint32   eventData;
   size_t        totalWaitingData;
} PENetPeer;

/** An PENet packet compressor for compressing UDP packets before socket sends or receives.
 */
typedef struct _PENetCompressor
{
   /** Context data for the compressor. Must be non-NULL. */
   void * context;
   /** Compresses from inBuffers[0:inBufferCount-1], containing inLimit bytes, to outData, outputting at most outLimit bytes. Should return 0 on failure. */
   size_t (PENET_CALLBACK * compress) (void * context, const PENetBuffer * inBuffers, size_t inBufferCount, size_t inLimit, penet_uint8 * outData, size_t outLimit);
   /** Decompresses from inData, containing inLimit bytes, to outData, outputting at most outLimit bytes. Should return 0 on failure. */
   size_t (PENET_CALLBACK * decompress) (void * context, const penet_uint8 * inData, size_t inLimit, penet_uint8 * outData, size_t outLimit);
   /** Destroys the context when compression is disabled or the host is destroyed. May be NULL. */
   void (PENET_CALLBACK * destroy) (void * context);
} PENetCompressor;

/** Callback that computes the checksum of the data held in buffers[0:bufferCount-1] */
typedef penet_uint32 (PENET_CALLBACK * PENetChecksumCallback) (const PENetBuffer * buffers, size_t bufferCount);

/** Callback for intercepting received raw UDP packets. Should return 1 to intercept, 0 to ignore, or -1 to propagate an error. */
typedef int (PENET_CALLBACK * PENetInterceptCallback) (struct _PENetHost * host, struct _PENetEvent * event);

/** An PENet host for communicating with peers.
  *
  * No fields should be modified unless otherwise stated.

    @sa penet_host_create()
    @sa penet_host_destroy()
    @sa penet_host_connect()
    @sa penet_host_service()
    @sa penet_host_flush()
    @sa penet_host_broadcast()
    @sa penet_host_compress()
    @sa penet_host_compress_with_range_coder()
    @sa penet_host_channel_limit()
    @sa penet_host_bandwidth_limit()
    @sa penet_host_bandwidth_throttle()
  */
typedef struct _PENetHost
{
   PENetSocket           socket;
   PENetAddress          address;                     /**< Internet address of the host */
   penet_uint32          incomingBandwidth;           /**< downstream bandwidth of the host */
   penet_uint32          outgoingBandwidth;           /**< upstream bandwidth of the host */
   penet_uint32          bandwidthThrottleEpoch;
   penet_uint32          mtu;
   penet_uint32          randomSeed;
   int                  recalculateBandwidthLimits;
   PENetPeer *           peers;                       /**< array of peers allocated for this host */
   size_t               peerCount;                   /**< number of peers allocated for this host */
   size_t               channelLimit;                /**< maximum number of channels allowed for connected peers */
   penet_uint32          serviceTime;
   PENetList             dispatchQueue;
   int                  continueSending;
   size_t               packetSize;
   penet_uint16          headerFlags;
   PENetProtocol         commands [PENET_PROTOCOL_MAXIMUM_PACKET_COMMANDS];
   size_t               commandCount;
   PENetBuffer           buffers [PENET_BUFFER_MAXIMUM];
   size_t               bufferCount;
   PENetChecksumCallback checksum;                    /**< callback the user can set to enable packet checksums for this host */
   PENetCompressor       compressor;
   penet_uint8           packetData [2][PENET_PROTOCOL_MAXIMUM_MTU];
   PENetAddress          receivedAddress;
   penet_uint8 *         receivedData;
   size_t               receivedDataLength;
   penet_uint32          totalSentData;               /**< total data sent, user should reset to 0 as needed to prevent overflow */
   penet_uint32          totalSentPackets;            /**< total UDP packets sent, user should reset to 0 as needed to prevent overflow */
   penet_uint32          totalReceivedData;           /**< total data received, user should reset to 0 as needed to prevent overflow */
   penet_uint32          totalReceivedPackets;        /**< total UDP packets received, user should reset to 0 as needed to prevent overflow */
   PENetInterceptCallback intercept;                  /**< callback the user can set to intercept received raw UDP packets */
   size_t               connectedPeers;
   size_t               bandwidthLimitedPeers;
   size_t               duplicatePeers;              /**< optional number of allowed peers from duplicate IPs, defaults to PENET_PROTOCOL_MAXIMUM_PEER_ID */
   size_t               maximumPacketSize;           /**< the maximum allowable packet size that may be sent or received on a peer */
   size_t               maximumWaitingData;          /**< the maximum aggregate amount of buffer space a peer may use waiting for packets to be delivered */
} PENetHost;

/**
 * An PENet event type, as specified in @ref PENetEvent.
 */
typedef enum _PENetEventType
{
   /** no event occurred within the specified time limit */
   PENET_EVENT_TYPE_NONE       = 0,

   /** a connection request initiated by penet_host_connect has completed.
     * The peer field contains the peer which successfully connected.
     */
   PENET_EVENT_TYPE_CONNECT    = 1,

   /** a peer has disconnected.  This event is generated on a successful
     * completion of a disconnect initiated by penet_pper_disconnect, if
     * a peer has timed out, or if a connection request intialized by
     * penet_host_connect has timed out.  The peer field contains the peer
     * which disconnected. The data field contains user supplied data
     * describing the disconnection, or 0, if none is available.
     */
   PENET_EVENT_TYPE_DISCONNECT = 2,

   /** a packet has been received from a peer.  The peer field specifies the
     * peer which sent the packet.  The channelID field specifies the channel
     * number upon which the packet was received.  The packet field contains
     * the packet that was received; this packet must be destroyed with
     * penet_packet_destroy after use.
     */
   PENET_EVENT_TYPE_RECEIVE    = 3
} PENetEventType;

/**
 * An PENet event as returned by penet_host_service().

   @sa penet_host_service
 */
typedef struct _PENetEvent
{
   PENetEventType        type;      /**< type of the event */
   PENetPeer *           peer;      /**< peer that generated a connect, disconnect or receive event */
   penet_uint8           channelID; /**< channel on the peer that generated the event, if appropriate */
   penet_uint32          data;      /**< data associated with the event, if appropriate */
   PENetPacket *         packet;    /**< packet associated with the event, if appropriate */
} PENetEvent;

/** @defgroup global PENet global functions
    @{
*/

/**
  Initializes PENet globally.  Must be called prior to using any functions in
  PENet.
  @returns 0 on success, < 0 on failure
*/
PENET_API int penet_initialize (void);

/**
  Initializes PENet globally and supplies user-overridden callbacks. Must be called prior to using any functions in PENet. Do not use penet_initialize() if you use this variant. Make sure the PENetCallbacks structure is zeroed out so that any additional callbacks added in future versions will be properly ignored.

  @param version the constant PENET_VERSION should be supplied so PENet knows which version of PENetCallbacks struct to use
  @param inits user-overridden callbacks where any NULL callbacks will use PENet's defaults
  @returns 0 on success, < 0 on failure
*/
PENET_API int penet_initialize_with_callbacks (PENetVersion version, const PENetCallbacks * inits);

/**
  Shuts down PENet globally.  Should be called when a program that has
  initialized PENet exits.
*/
PENET_API void penet_deinitialize (void);

/**
  Gives the linked version of the PENet library.
  @returns the version number
*/
PENET_API PENetVersion penet_linked_version (void);

/** @} */

/** @defgroup private PENet private implementation functions */

/**
  Returns the wall-time in milliseconds.  Its initial value is unspecified
  unless otherwise set.
  */
PENET_API penet_uint32 penet_time_get (void);
/**
  Sets the current wall-time in milliseconds.
  */
PENET_API void penet_time_set (penet_uint32);

/** @defgroup socket PENet socket functions
    @{
*/
PENET_API PENetSocket penet_socket_create (PENetSocketType);
PENET_API int        penet_socket_bind (PENetSocket, const PENetAddress *);
PENET_API int        penet_socket_get_address (PENetSocket, PENetAddress *);
PENET_API int        penet_socket_listen (PENetSocket, int);
PENET_API PENetSocket penet_socket_accept (PENetSocket, PENetAddress *);
PENET_API int        penet_socket_connect (PENetSocket, const PENetAddress *);
PENET_API int        penet_socket_send (PENetSocket, const PENetAddress *, const PENetBuffer *, size_t);
PENET_API int        penet_socket_receive (PENetSocket, PENetAddress *, PENetBuffer *, size_t);
PENET_API int        penet_socket_wait (PENetSocket, penet_uint32 *, penet_uint32);
PENET_API int        penet_socket_set_option (PENetSocket, PENetSocketOption, int);
PENET_API int        penet_socket_get_option (PENetSocket, PENetSocketOption, int *);
PENET_API int        penet_socket_shutdown (PENetSocket, PENetSocketShutdown);
PENET_API void       penet_socket_destroy (PENetSocket);
PENET_API int        penet_socketset_select (PENetSocket, PENetSocketSet *, PENetSocketSet *, penet_uint32);

/** @} */

/** @defgroup Address PENet address functions
    @{
*/
/** Attempts to resolve the host named by the parameter hostName and sets
    the host field in the address parameter if successful.
    @param address destination to store resolved address
    @param hostName host name to lookup
    @retval 0 on success
    @retval < 0 on failure
    @returns the address of the given hostName in address on success
*/
PENET_API int penet_address_set_host (PENetAddress * address, const char * hostName);

/** Gives the printable form of the IP address specified in the address parameter.
    @param address    address printed
    @param hostName   destination for name, must not be NULL
    @param nameLength maximum length of hostName.
    @returns the null-terminated name of the host in hostName on success
    @retval 0 on success
    @retval < 0 on failure
*/
PENET_API int penet_address_get_host_ip (const PENetAddress * address, char * hostName, size_t nameLength);

/** Attempts to do a reverse lookup of the host field in the address parameter.
    @param address    address used for reverse lookup
    @param hostName   destination for name, must not be NULL
    @param nameLength maximum length of hostName.
    @returns the null-terminated name of the host in hostName on success
    @retval 0 on success
    @retval < 0 on failure
*/
PENET_API int penet_address_get_host (const PENetAddress * address, char * hostName, size_t nameLength);

/** @} */

PENET_API PENetPacket * penet_packet_create (const void *, size_t, penet_uint32);
PENET_API void         penet_packet_destroy (PENetPacket *);
PENET_API int          penet_packet_resize  (PENetPacket *, size_t);
PENET_API penet_uint32  penet_crc32 (const PENetBuffer *, size_t);

PENET_API PENetHost * penet_host_create (const PENetAddress *, size_t, size_t, penet_uint32, penet_uint32);
PENET_API void       penet_host_destroy (PENetHost *);
PENET_API PENetPeer * penet_host_connect (PENetHost *, const PENetAddress *, size_t, penet_uint32);
PENET_API int        penet_host_check_events (PENetHost *, PENetEvent *);
PENET_API int        penet_host_service (PENetHost *, PENetEvent *, penet_uint32);
PENET_API void       penet_host_flush (PENetHost *);
PENET_API void       penet_host_broadcast (PENetHost *, penet_uint8, PENetPacket *);
PENET_API void       penet_host_compress (PENetHost *, const PENetCompressor *);
PENET_API int        penet_host_compress_with_range_coder (PENetHost * host);
PENET_API void       penet_host_channel_limit (PENetHost *, size_t);
PENET_API void       penet_host_bandwidth_limit (PENetHost *, penet_uint32, penet_uint32);
extern   void       penet_host_bandwidth_throttle (PENetHost *);
extern  penet_uint32 penet_host_random_seed (void);

PENET_API int                 penet_peer_send (PENetPeer *, penet_uint8, PENetPacket *);
PENET_API PENetPacket *        penet_peer_receive (PENetPeer *, penet_uint8 * channelID);
PENET_API void                penet_peer_ping (PENetPeer *);
PENET_API void                penet_peer_ping_interval (PENetPeer *, penet_uint32);
PENET_API void                penet_peer_timeout (PENetPeer *, penet_uint32, penet_uint32, penet_uint32);
PENET_API void                penet_peer_reset (PENetPeer *);
PENET_API void                penet_peer_disconnect (PENetPeer *, penet_uint32);
PENET_API void                penet_peer_disconnect_now (PENetPeer *, penet_uint32);
PENET_API void                penet_peer_disconnect_later (PENetPeer *, penet_uint32);
PENET_API void                penet_peer_throttle_configure (PENetPeer *, penet_uint32, penet_uint32, penet_uint32);
extern int                   penet_peer_throttle (PENetPeer *, penet_uint32);
extern void                  penet_peer_reset_queues (PENetPeer *);
extern void                  penet_peer_setup_outgoing_command (PENetPeer *, PENetOutgoingCommand *);
extern PENetOutgoingCommand * penet_peer_queue_outgoing_command (PENetPeer *, const PENetProtocol *, PENetPacket *, penet_uint32, penet_uint16);
extern PENetIncomingCommand * penet_peer_queue_incoming_command (PENetPeer *, const PENetProtocol *, const void *, size_t, penet_uint32, penet_uint32);
extern PENetAcknowledgement * penet_peer_queue_acknowledgement (PENetPeer *, const PENetProtocol *, penet_uint16);
extern void                  penet_peer_dispatch_incoming_unreliable_commands (PENetPeer *, PENetChannel *);
extern void                  penet_peer_dispatch_incoming_reliable_commands (PENetPeer *, PENetChannel *);
extern void                  penet_peer_on_connect (PENetPeer *);
extern void                  penet_peer_on_disconnect (PENetPeer *);

PENET_API void * penet_range_coder_create (void);
PENET_API void   penet_range_coder_destroy (void *);
PENET_API size_t penet_range_coder_compress (void *, const PENetBuffer *, size_t, size_t, penet_uint8 *, size_t);
PENET_API size_t penet_range_coder_decompress (void *, const penet_uint8 *, size_t, penet_uint8 *, size_t);

extern size_t penet_protocol_command_size (penet_uint8);

#ifdef __cplusplus
}
#endif

#endif /* __PENET_PENET_H__ */
