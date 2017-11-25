/** 
 @file host.c
 @brief PENet host management functions
*/
#define PENET_BUILDING_LIB 1
#include <string.h>
#include "penet/penet.h"

/** @defgroup host PENet host functions
    @{
*/

/** Creates a host for communicating to peers.

    @param address   the address at which other peers may connect to this host.  If NULL, then no peers may connect to the host.
    @param peerCount the maximum number of peers that should be allocated for the host.
    @param channelLimit the maximum number of channels allowed; if 0, then this is equivalent to PENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT
    @param incomingBandwidth downstream bandwidth of the host in bytes/second; if 0, PENet will assume unlimited bandwidth.
    @param outgoingBandwidth upstream bandwidth of the host in bytes/second; if 0, PENet will assume unlimited bandwidth.

    @returns the host on success and NULL on failure

    @remarks PENet will strategically drop packets on specific sides of a connection between hosts
    to ensure the host's bandwidth is not overwhelmed.  The bandwidth parameters also determine
    the window size of a connection which limits the amount of reliable packets that may be in transit
    at any given time.
*/
PENetHost *
penet_host_create (const PENetAddress * address, size_t peerCount, size_t channelLimit, penet_uint32 incomingBandwidth, penet_uint32 outgoingBandwidth)
{
    PENetHost * host;
    PENetPeer * currentPeer;

    if (peerCount > PENET_PROTOCOL_MAXIMUM_PEER_ID)
      return NULL;

    host = (PENetHost *) penet_malloc (sizeof (PENetHost));
    if (host == NULL)
      return NULL;
    memset (host, 0, sizeof (PENetHost));

    host -> peers = (PENetPeer *) penet_malloc (peerCount * sizeof (PENetPeer));
    if (host -> peers == NULL)
    {
       penet_free (host);

       return NULL;
    }
    memset (host -> peers, 0, peerCount * sizeof (PENetPeer));

    host -> socket = penet_socket_create (PENET_SOCKET_TYPE_DATAGRAM);
    if (host -> socket == PENET_SOCKET_NULL || (address != NULL && penet_socket_bind (host -> socket, address) < 0))
    {
       if (host -> socket != PENET_SOCKET_NULL)
         penet_socket_destroy (host -> socket);

       penet_free (host -> peers);
       penet_free (host);

       return NULL;
    }

    penet_socket_set_option (host -> socket, PENET_SOCKOPT_NONBLOCK, 1);
    penet_socket_set_option (host -> socket, PENET_SOCKOPT_BROADCAST, 1);
    penet_socket_set_option (host -> socket, PENET_SOCKOPT_RCVBUF, PENET_HOST_RECEIVE_BUFFER_SIZE);
    penet_socket_set_option (host -> socket, PENET_SOCKOPT_SNDBUF, PENET_HOST_SEND_BUFFER_SIZE);

    if (address != NULL && penet_socket_get_address (host -> socket, & host -> address) < 0)
      host -> address = * address;

    if (! channelLimit || channelLimit > PENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT)
      channelLimit = PENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT;
    else
    if (channelLimit < PENET_PROTOCOL_MINIMUM_CHANNEL_COUNT)
      channelLimit = PENET_PROTOCOL_MINIMUM_CHANNEL_COUNT;

    host -> randomSeed = (penet_uint32) (size_t) host;
    host -> randomSeed += penet_host_random_seed ();
    host -> randomSeed = (host -> randomSeed << 16) | (host -> randomSeed >> 16);
    host -> channelLimit = channelLimit;
    host -> incomingBandwidth = incomingBandwidth;
    host -> outgoingBandwidth = outgoingBandwidth;
    host -> bandwidthThrottleEpoch = 0;
    host -> recalculateBandwidthLimits = 0;
    host -> mtu = PENET_HOST_DEFAULT_MTU;
    host -> peerCount = peerCount;
    host -> commandCount = 0;
    host -> bufferCount = 0;
    host -> checksum = NULL;
    host -> receivedAddress.host = PENET_HOST_ANY;
    host -> receivedAddress.port = 0;
    host -> receivedData = NULL;
    host -> receivedDataLength = 0;

    host -> totalSentData = 0;
    host -> totalSentPackets = 0;
    host -> totalReceivedData = 0;
    host -> totalReceivedPackets = 0;

    host -> connectedPeers = 0;
    host -> bandwidthLimitedPeers = 0;
    host -> duplicatePeers = PENET_PROTOCOL_MAXIMUM_PEER_ID;
    host -> maximumPacketSize = PENET_HOST_DEFAULT_MAXIMUM_PACKET_SIZE;
    host -> maximumWaitingData = PENET_HOST_DEFAULT_MAXIMUM_WAITING_DATA;

    host -> compressor.context = NULL;
    host -> compressor.compress = NULL;
    host -> compressor.decompress = NULL;
    host -> compressor.destroy = NULL;

    host -> intercept = NULL;

    penet_list_clear (& host -> dispatchQueue);

    for (currentPeer = host -> peers;
         currentPeer < & host -> peers [host -> peerCount];
         ++ currentPeer)
    {
       currentPeer -> host = host;
       currentPeer -> incomingPeerID = currentPeer - host -> peers;
       currentPeer -> outgoingSessionID = currentPeer -> incomingSessionID = 0xFF;
       currentPeer -> data = NULL;

       penet_list_clear (& currentPeer -> acknowledgements);
       penet_list_clear (& currentPeer -> sentReliableCommands);
       penet_list_clear (& currentPeer -> sentUnreliableCommands);
       penet_list_clear (& currentPeer -> outgoingReliableCommands);
       penet_list_clear (& currentPeer -> outgoingUnreliableCommands);
       penet_list_clear (& currentPeer -> dispatchedCommands);

       penet_peer_reset (currentPeer);
    }

    return host;
}

/** Destroys the host and all resources associated with it.
    @param host pointer to the host to destroy
*/
void
penet_host_destroy (PENetHost * host)
{
    PENetPeer * currentPeer;

    if (host == NULL)
      return;

    penet_socket_destroy (host -> socket);

    for (currentPeer = host -> peers;
         currentPeer < & host -> peers [host -> peerCount];
         ++ currentPeer)
    {
       penet_peer_reset (currentPeer);
    }

    if (host -> compressor.context != NULL && host -> compressor.destroy)
      (* host -> compressor.destroy) (host -> compressor.context);

    penet_free (host -> peers);
    penet_free (host);
}

/** Initiates a connection to a foreign host.
    @param host host seeking the connection
    @param address destination for the connection
    @param channelCount number of channels to allocate
    @param data user data supplied to the receiving host
    @returns a peer representing the foreign host on success, NULL on failure
    @remarks The peer returned will have not completed the connection until penet_host_service()
    notifies of an PENET_EVENT_TYPE_CONNECT event for the peer.
*/
PENetPeer *
penet_host_connect (PENetHost * host, const PENetAddress * address, size_t channelCount, penet_uint32 data)
{
    PENetPeer * currentPeer;
    PENetChannel * channel;
    PENetProtocol command;

    if (channelCount < PENET_PROTOCOL_MINIMUM_CHANNEL_COUNT)
      channelCount = PENET_PROTOCOL_MINIMUM_CHANNEL_COUNT;
    else
    if (channelCount > PENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT)
      channelCount = PENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT;

    for (currentPeer = host -> peers;
         currentPeer < & host -> peers [host -> peerCount];
         ++ currentPeer)
    {
       if (currentPeer -> state == PENET_PEER_STATE_DISCONNECTED)
         break;
    }

    if (currentPeer >= & host -> peers [host -> peerCount])
      return NULL;

    currentPeer -> channels = (PENetChannel *) penet_malloc (channelCount * sizeof (PENetChannel));
    if (currentPeer -> channels == NULL)
      return NULL;
    currentPeer -> channelCount = channelCount;
    currentPeer -> state = PENET_PEER_STATE_CONNECTING;
    currentPeer -> address = * address;
    currentPeer -> connectID = ++ host -> randomSeed;

    if (host -> outgoingBandwidth == 0)
      currentPeer -> windowSize = PENET_PROTOCOL_MAXIMUM_WINDOW_SIZE;
    else
      currentPeer -> windowSize = (host -> outgoingBandwidth /
                                    PENET_PEER_WINDOW_SIZE_SCALE) *
                                      PENET_PROTOCOL_MINIMUM_WINDOW_SIZE;

    if (currentPeer -> windowSize < PENET_PROTOCOL_MINIMUM_WINDOW_SIZE)
      currentPeer -> windowSize = PENET_PROTOCOL_MINIMUM_WINDOW_SIZE;
    else
    if (currentPeer -> windowSize > PENET_PROTOCOL_MAXIMUM_WINDOW_SIZE)
      currentPeer -> windowSize = PENET_PROTOCOL_MAXIMUM_WINDOW_SIZE;

    for (channel = currentPeer -> channels;
         channel < & currentPeer -> channels [channelCount];
         ++ channel)
    {
        channel -> outgoingReliableSequenceNumber = 0;
        channel -> outgoingUnreliableSequenceNumber = 0;
        channel -> incomingReliableSequenceNumber = 0;
        channel -> incomingUnreliableSequenceNumber = 0;

        penet_list_clear (& channel -> incomingReliableCommands);
        penet_list_clear (& channel -> incomingUnreliableCommands);

        channel -> usedReliableWindows = 0;
        memset (channel -> reliableWindows, 0, sizeof (channel -> reliableWindows));
    }

    command.header.command = PENET_PROTOCOL_COMMAND_CONNECT | PENET_PROTOCOL_COMMAND_FLAG_ACKNOWLEDGE;
    command.header.channelID = 0xFF;
    command.connect.outgoingPeerID = PENET_HOST_TO_NET_16 (currentPeer -> incomingPeerID);
    command.connect.incomingSessionID = currentPeer -> incomingSessionID;
    command.connect.outgoingSessionID = currentPeer -> outgoingSessionID;
    command.connect.mtu = PENET_HOST_TO_NET_32 (currentPeer -> mtu);
    command.connect.windowSize = PENET_HOST_TO_NET_32 (currentPeer -> windowSize);
    command.connect.channelCount = PENET_HOST_TO_NET_32 (channelCount);
    command.connect.incomingBandwidth = PENET_HOST_TO_NET_32 (host -> incomingBandwidth);
    command.connect.outgoingBandwidth = PENET_HOST_TO_NET_32 (host -> outgoingBandwidth);
    command.connect.packetThrottleInterval = PENET_HOST_TO_NET_32 (currentPeer -> packetThrottleInterval);
    command.connect.packetThrottleAcceleration = PENET_HOST_TO_NET_32 (currentPeer -> packetThrottleAcceleration);
    command.connect.packetThrottleDeceleration = PENET_HOST_TO_NET_32 (currentPeer -> packetThrottleDeceleration);
    command.connect.connectID = currentPeer -> connectID;
    command.connect.data = PENET_HOST_TO_NET_32 (data);

    penet_peer_queue_outgoing_command (currentPeer, & command, NULL, 0, 0);

    return currentPeer;
}

/** Queues a packet to be sent to all peers associated with the host.
    @param host host on which to broadcast the packet
    @param channelID channel on which to broadcast
    @param packet packet to broadcast
*/
void
penet_host_broadcast (PENetHost * host, penet_uint8 channelID, PENetPacket * packet)
{
    PENetPeer * currentPeer;

    for (currentPeer = host -> peers;
         currentPeer < & host -> peers [host -> peerCount];
         ++ currentPeer)
    {
       if (currentPeer -> state != PENET_PEER_STATE_CONNECTED)
         continue;

       penet_peer_send (currentPeer, channelID, packet);
    }

    if (packet -> referenceCount == 0)
      penet_packet_destroy (packet);
}

/** Sets the packet compressor the host should use to compress and decompress packets.
    @param host host to enable or disable compression for
    @param compressor callbacks for for the packet compressor; if NULL, then compression is disabled
*/
void
penet_host_compress (PENetHost * host, const PENetCompressor * compressor)
{
    if (host -> compressor.context != NULL && host -> compressor.destroy)
      (* host -> compressor.destroy) (host -> compressor.context);

    if (compressor)
      host -> compressor = * compressor;
    else
      host -> compressor.context = NULL;
}

/** Limits the maximum allowed channels of future incoming connections.
    @param host host to limit
    @param channelLimit the maximum number of channels allowed; if 0, then this is equivalent to PENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT
*/
void
penet_host_channel_limit (PENetHost * host, size_t channelLimit)
{
    if (! channelLimit || channelLimit > PENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT)
      channelLimit = PENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT;
    else
    if (channelLimit < PENET_PROTOCOL_MINIMUM_CHANNEL_COUNT)
      channelLimit = PENET_PROTOCOL_MINIMUM_CHANNEL_COUNT;

    host -> channelLimit = channelLimit;
}


/** Adjusts the bandwidth limits of a host.
    @param host host to adjust
    @param incomingBandwidth new incoming bandwidth
    @param outgoingBandwidth new outgoing bandwidth
    @remarks the incoming and outgoing bandwidth parameters are identical in function to those
    specified in penet_host_create().
*/
void
penet_host_bandwidth_limit (PENetHost * host, penet_uint32 incomingBandwidth, penet_uint32 outgoingBandwidth)
{
    host -> incomingBandwidth = incomingBandwidth;
    host -> outgoingBandwidth = outgoingBandwidth;
    host -> recalculateBandwidthLimits = 1;
}

void
penet_host_bandwidth_throttle (PENetHost * host)
{
    penet_uint32 timeCurrent = penet_time_get (),
           elapsedTime = timeCurrent - host -> bandwidthThrottleEpoch,
           peersRemaining = (penet_uint32) host -> connectedPeers,
           dataTotal = ~0,
           bandwidth = ~0,
           throttle = 0,
           bandwidthLimit = 0;
    int needsAdjustment = host -> bandwidthLimitedPeers > 0 ? 1 : 0;
    PENetPeer * peer;
    PENetProtocol command;

    if (elapsedTime < PENET_HOST_BANDWIDTH_THROTTLE_INTERVAL)
      return;

    host -> bandwidthThrottleEpoch = timeCurrent;

    if (peersRemaining == 0)
      return;

    if (host -> outgoingBandwidth != 0)
    {
        dataTotal = 0;
        bandwidth = (host -> outgoingBandwidth * elapsedTime) / 1000;

        for (peer = host -> peers;
             peer < & host -> peers [host -> peerCount];
            ++ peer)
        {
            if (peer -> state != PENET_PEER_STATE_CONNECTED && peer -> state != PENET_PEER_STATE_DISCONNECT_LATER)
              continue;

            dataTotal += peer -> outgoingDataTotal;
        }
    }

    while (peersRemaining > 0 && needsAdjustment != 0)
    {
        needsAdjustment = 0;

        if (dataTotal <= bandwidth)
          throttle = PENET_PEER_PACKET_THROTTLE_SCALE;
        else
          throttle = (bandwidth * PENET_PEER_PACKET_THROTTLE_SCALE) / dataTotal;

        for (peer = host -> peers;
             peer < & host -> peers [host -> peerCount];
             ++ peer)
        {
            penet_uint32 peerBandwidth;

            if ((peer -> state != PENET_PEER_STATE_CONNECTED && peer -> state != PENET_PEER_STATE_DISCONNECT_LATER) ||
                peer -> incomingBandwidth == 0 ||
                peer -> outgoingBandwidthThrottleEpoch == timeCurrent)
              continue;

            peerBandwidth = (peer -> incomingBandwidth * elapsedTime) / 1000;
            if ((throttle * peer -> outgoingDataTotal) / PENET_PEER_PACKET_THROTTLE_SCALE <= peerBandwidth)
              continue;

            peer -> packetThrottleLimit = (peerBandwidth *
                                            PENET_PEER_PACKET_THROTTLE_SCALE) / peer -> outgoingDataTotal;

            if (peer -> packetThrottleLimit == 0)
              peer -> packetThrottleLimit = 1;

            if (peer -> packetThrottle > peer -> packetThrottleLimit)
              peer -> packetThrottle = peer -> packetThrottleLimit;

            peer -> outgoingBandwidthThrottleEpoch = timeCurrent;

            peer -> incomingDataTotal = 0;
            peer -> outgoingDataTotal = 0;

            needsAdjustment = 1;
            -- peersRemaining;
            bandwidth -= peerBandwidth;
            dataTotal -= peerBandwidth;
        }
    }

    if (peersRemaining > 0)
    {
        if (dataTotal <= bandwidth)
          throttle = PENET_PEER_PACKET_THROTTLE_SCALE;
        else
          throttle = (bandwidth * PENET_PEER_PACKET_THROTTLE_SCALE) / dataTotal;

        for (peer = host -> peers;
             peer < & host -> peers [host -> peerCount];
             ++ peer)
        {
            if ((peer -> state != PENET_PEER_STATE_CONNECTED && peer -> state != PENET_PEER_STATE_DISCONNECT_LATER) ||
                peer -> outgoingBandwidthThrottleEpoch == timeCurrent)
              continue;

            peer -> packetThrottleLimit = throttle;

            if (peer -> packetThrottle > peer -> packetThrottleLimit)
              peer -> packetThrottle = peer -> packetThrottleLimit;

            peer -> incomingDataTotal = 0;
            peer -> outgoingDataTotal = 0;
        }
    }

    if (host -> recalculateBandwidthLimits)
    {
       host -> recalculateBandwidthLimits = 0;

       peersRemaining = (penet_uint32) host -> connectedPeers;
       bandwidth = host -> incomingBandwidth;
       needsAdjustment = 1;

       if (bandwidth == 0)
         bandwidthLimit = 0;
       else
       while (peersRemaining > 0 && needsAdjustment != 0)
       {
           needsAdjustment = 0;
           bandwidthLimit = bandwidth / peersRemaining;

           for (peer = host -> peers;
                peer < & host -> peers [host -> peerCount];
                ++ peer)
           {
               if ((peer -> state != PENET_PEER_STATE_CONNECTED && peer -> state != PENET_PEER_STATE_DISCONNECT_LATER) ||
                   peer -> incomingBandwidthThrottleEpoch == timeCurrent)
                 continue;

               if (peer -> outgoingBandwidth > 0 &&
                   peer -> outgoingBandwidth >= bandwidthLimit)
                 continue;

               peer -> incomingBandwidthThrottleEpoch = timeCurrent;

               needsAdjustment = 1;
               -- peersRemaining;
               bandwidth -= peer -> outgoingBandwidth;
           }
       }

       for (peer = host -> peers;
            peer < & host -> peers [host -> peerCount];
            ++ peer)
       {
           if (peer -> state != PENET_PEER_STATE_CONNECTED && peer -> state != PENET_PEER_STATE_DISCONNECT_LATER)
             continue;

           command.header.command = PENET_PROTOCOL_COMMAND_BANDWIDTH_LIMIT | PENET_PROTOCOL_COMMAND_FLAG_ACKNOWLEDGE;
           command.header.channelID = 0xFF;
           command.bandwidthLimit.outgoingBandwidth = PENET_HOST_TO_NET_32 (host -> outgoingBandwidth);

           if (peer -> incomingBandwidthThrottleEpoch == timeCurrent)
             command.bandwidthLimit.incomingBandwidth = PENET_HOST_TO_NET_32 (peer -> outgoingBandwidth);
           else
             command.bandwidthLimit.incomingBandwidth = PENET_HOST_TO_NET_32 (bandwidthLimit);

           penet_peer_queue_outgoing_command (peer, & command, NULL, 0, 0);
       }
    }
}

/** @} */
