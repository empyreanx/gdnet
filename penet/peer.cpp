/** 
 @file  peer.c
 @brief PENet peer management functions
*/
#include <string.h>
#define PENET_BUILDING_LIB 1
#include "penet/penet.h"

/** @defgroup peer PENet peer functions
    @{
*/

/** Configures throttle parameter for a peer.

    Unreliable packets are dropped by PENet in response to the varying conditions
    of the Internet connection to the peer.  The throttle represents a probability
    that an unreliable packet should not be dropped and thus sent by PENet to the peer.
    The lowest mean round trip time from the sending of a reliable packet to the
    receipt of its acknowledgement is measured over an amount of time specified by
    the interval parameter in milliseconds.  If a measured round trip time happens to
    be significantly less than the mean round trip time measured over the interval,
    then the throttle probability is increased to allow more traffic by an amount
    specified in the acceleration parameter, which is a ratio to the PENET_PEER_PACKET_THROTTLE_SCALE
    constant.  If a measured round trip time happens to be significantly greater than
    the mean round trip time measured over the interval, then the throttle probability
    is decreased to limit traffic by an amount specified in the deceleration parameter, which
    is a ratio to the PENET_PEER_PACKET_THROTTLE_SCALE constant.  When the throttle has
    a value of PENET_PEER_PACKET_THROTTLE_SCALE, no unreliable packets are dropped by
    PENet, and so 100% of all unreliable packets will be sent.  When the throttle has a
    value of 0, all unreliable packets are dropped by PENet, and so 0% of all unreliable
    packets will be sent.  Intermediate values for the throttle represent intermediate
    probabilities between 0% and 100% of unreliable packets being sent.  The bandwidth
    limits of the local and foreign hosts are taken into account to determine a
    sensible limit for the throttle probability above which it should not raise even in
    the best of conditions.

    @param peer peer to configure
    @param interval interval, in milliseconds, over which to measure lowest mean RTT; the default value is PENET_PEER_PACKET_THROTTLE_INTERVAL.
    @param acceleration rate at which to increase the throttle probability as mean RTT declines
    @param deceleration rate at which to decrease the throttle probability as mean RTT increases
*/
void
penet_peer_throttle_configure (PENetPeer * peer, penet_uint32 interval, penet_uint32 acceleration, penet_uint32 deceleration)
{
    PENetProtocol command;

    peer -> packetThrottleInterval = interval;
    peer -> packetThrottleAcceleration = acceleration;
    peer -> packetThrottleDeceleration = deceleration;

    command.header.command = PENET_PROTOCOL_COMMAND_THROTTLE_CONFIGURE | PENET_PROTOCOL_COMMAND_FLAG_ACKNOWLEDGE;
    command.header.channelID = 0xFF;

    command.throttleConfigure.packetThrottleInterval = PENET_HOST_TO_NET_32 (interval);
    command.throttleConfigure.packetThrottleAcceleration = PENET_HOST_TO_NET_32 (acceleration);
    command.throttleConfigure.packetThrottleDeceleration = PENET_HOST_TO_NET_32 (deceleration);

    penet_peer_queue_outgoing_command (peer, & command, NULL, 0, 0);
}

int
penet_peer_throttle (PENetPeer * peer, penet_uint32 rtt)
{
    if (peer -> lastRoundTripTime <= peer -> lastRoundTripTimeVariance)
    {
        peer -> packetThrottle = peer -> packetThrottleLimit;
    }
    else
    if (rtt < peer -> lastRoundTripTime)
    {
        peer -> packetThrottle += peer -> packetThrottleAcceleration;

        if (peer -> packetThrottle > peer -> packetThrottleLimit)
          peer -> packetThrottle = peer -> packetThrottleLimit;

        return 1;
    }
    else
    if (rtt > peer -> lastRoundTripTime + 2 * peer -> lastRoundTripTimeVariance)
    {
        if (peer -> packetThrottle > peer -> packetThrottleDeceleration)
          peer -> packetThrottle -= peer -> packetThrottleDeceleration;
        else
          peer -> packetThrottle = 0;

        return -1;
    }

    return 0;
}

/** Queues a packet to be sent.
    @param peer destination for the packet
    @param channelID channel on which to send
    @param packet packet to send
    @retval 0 on success
    @retval < 0 on failure
*/
int
penet_peer_send (PENetPeer * peer, penet_uint8 channelID, PENetPacket * packet)
{
   PENetChannel * channel = & peer -> channels [channelID];
   PENetProtocol command;
   size_t fragmentLength;

   if (peer -> state != PENET_PEER_STATE_CONNECTED ||
       channelID >= peer -> channelCount ||
       packet -> dataLength > peer -> host -> maximumPacketSize)
     return -1;

   fragmentLength = peer -> mtu - sizeof (PENetProtocolHeader) - sizeof (PENetProtocolSendFragment);
   if (peer -> host -> checksum != NULL)
     fragmentLength -= sizeof(penet_uint32);

   if (packet -> dataLength > fragmentLength)
   {
      penet_uint32 fragmentCount = (packet -> dataLength + fragmentLength - 1) / fragmentLength,
             fragmentNumber,
             fragmentOffset;
      penet_uint8 commandNumber;
      penet_uint16 startSequenceNumber;
      PENetList fragments;
      PENetOutgoingCommand * fragment;

      if (fragmentCount > PENET_PROTOCOL_MAXIMUM_FRAGMENT_COUNT)
        return -1;

      if ((packet -> flags & (PENET_PACKET_FLAG_RELIABLE | PENET_PACKET_FLAG_UNRELIABLE_FRAGMENT)) == PENET_PACKET_FLAG_UNRELIABLE_FRAGMENT &&
          channel -> outgoingUnreliableSequenceNumber < 0xFFFF)
      {
         commandNumber = PENET_PROTOCOL_COMMAND_SEND_UNRELIABLE_FRAGMENT;
         startSequenceNumber = PENET_HOST_TO_NET_16 (channel -> outgoingUnreliableSequenceNumber + 1);
      }
      else
      {
         commandNumber = PENET_PROTOCOL_COMMAND_SEND_FRAGMENT | PENET_PROTOCOL_COMMAND_FLAG_ACKNOWLEDGE;
         startSequenceNumber = PENET_HOST_TO_NET_16 (channel -> outgoingReliableSequenceNumber + 1);
      }

      penet_list_clear (& fragments);

      for (fragmentNumber = 0,
             fragmentOffset = 0;
           fragmentOffset < packet -> dataLength;
           ++ fragmentNumber,
             fragmentOffset += fragmentLength)
      {
         if (packet -> dataLength - fragmentOffset < fragmentLength)
           fragmentLength = packet -> dataLength - fragmentOffset;

         fragment = (PENetOutgoingCommand *) penet_malloc (sizeof (PENetOutgoingCommand));
         if (fragment == NULL)
         {
            while (! penet_list_empty (& fragments))
            {
               fragment = (PENetOutgoingCommand *) penet_list_remove (penet_list_begin (& fragments));

               penet_free (fragment);
            }

            return -1;
         }

         fragment -> fragmentOffset = fragmentOffset;
         fragment -> fragmentLength = fragmentLength;
         fragment -> packet = packet;
         fragment -> command.header.command = commandNumber;
         fragment -> command.header.channelID = channelID;
         fragment -> command.sendFragment.startSequenceNumber = startSequenceNumber;
         fragment -> command.sendFragment.dataLength = PENET_HOST_TO_NET_16 (fragmentLength);
         fragment -> command.sendFragment.fragmentCount = PENET_HOST_TO_NET_32 (fragmentCount);
         fragment -> command.sendFragment.fragmentNumber = PENET_HOST_TO_NET_32 (fragmentNumber);
         fragment -> command.sendFragment.totalLength = PENET_HOST_TO_NET_32 (packet -> dataLength);
         fragment -> command.sendFragment.fragmentOffset = PENET_NET_TO_HOST_32 (fragmentOffset);

         penet_list_insert (penet_list_end (& fragments), fragment);
      }

      packet -> referenceCount += fragmentNumber;

      while (! penet_list_empty (& fragments))
      {
         fragment = (PENetOutgoingCommand *) penet_list_remove (penet_list_begin (& fragments));

         penet_peer_setup_outgoing_command (peer, fragment);
      }

      return 0;
   }

   command.header.channelID = channelID;

   if ((packet -> flags & (PENET_PACKET_FLAG_RELIABLE | PENET_PACKET_FLAG_UNSEQUENCED)) == PENET_PACKET_FLAG_UNSEQUENCED)
   {
      command.header.command = PENET_PROTOCOL_COMMAND_SEND_UNSEQUENCED | PENET_PROTOCOL_COMMAND_FLAG_UNSEQUENCED;
      command.sendUnsequenced.dataLength = PENET_HOST_TO_NET_16 (packet -> dataLength);
   }
   else
   if (packet -> flags & PENET_PACKET_FLAG_RELIABLE || channel -> outgoingUnreliableSequenceNumber >= 0xFFFF)
   {
      command.header.command = PENET_PROTOCOL_COMMAND_SEND_RELIABLE | PENET_PROTOCOL_COMMAND_FLAG_ACKNOWLEDGE;
      command.sendReliable.dataLength = PENET_HOST_TO_NET_16 (packet -> dataLength);
   }
   else
   {
      command.header.command = PENET_PROTOCOL_COMMAND_SEND_UNRELIABLE;
      command.sendUnreliable.dataLength = PENET_HOST_TO_NET_16 (packet -> dataLength);
   }

   if (penet_peer_queue_outgoing_command (peer, & command, packet, 0, packet -> dataLength) == NULL)
     return -1;

   return 0;
}

/** Attempts to dequeue any incoming queued packet.
    @param peer peer to dequeue packets from
    @param channelID holds the channel ID of the channel the packet was received on success
    @returns a pointer to the packet, or NULL if there are no available incoming queued packets
*/
PENetPacket *
penet_peer_receive (PENetPeer * peer, penet_uint8 * channelID)
{
   PENetIncomingCommand * incomingCommand;
   PENetPacket * packet;

   if (penet_list_empty (& peer -> dispatchedCommands))
     return NULL;

   incomingCommand = (PENetIncomingCommand *) penet_list_remove (penet_list_begin (& peer -> dispatchedCommands));

   if (channelID != NULL)
     * channelID = incomingCommand -> command.header.channelID;

   packet = incomingCommand -> packet;

   -- packet -> referenceCount;

   if (incomingCommand -> fragments != NULL)
     penet_free (incomingCommand -> fragments);

   penet_free (incomingCommand);

   peer -> totalWaitingData -= packet -> dataLength;

   return packet;
}

static void
penet_peer_reset_outgoing_commands (PENetList * queue)
{
    PENetOutgoingCommand * outgoingCommand;

    while (! penet_list_empty (queue))
    {
       outgoingCommand = (PENetOutgoingCommand *) penet_list_remove (penet_list_begin (queue));

       if (outgoingCommand -> packet != NULL)
       {
          -- outgoingCommand -> packet -> referenceCount;

          if (outgoingCommand -> packet -> referenceCount == 0)
            penet_packet_destroy (outgoingCommand -> packet);
       }

       penet_free (outgoingCommand);
    }
}

static void
penet_peer_remove_incoming_commands (PENetList * queue, PENetListIterator startCommand, PENetListIterator endCommand)
{
    PENetListIterator currentCommand;

    for (currentCommand = startCommand; currentCommand != endCommand; )
    {
       PENetIncomingCommand * incomingCommand = (PENetIncomingCommand *) currentCommand;

       currentCommand = penet_list_next (currentCommand);

       penet_list_remove (& incomingCommand -> incomingCommandList);

       if (incomingCommand -> packet != NULL)
       {
          -- incomingCommand -> packet -> referenceCount;

          if (incomingCommand -> packet -> referenceCount == 0)
            penet_packet_destroy (incomingCommand -> packet);
       }

       if (incomingCommand -> fragments != NULL)
         penet_free (incomingCommand -> fragments);

       penet_free (incomingCommand);
    }
}

static void
penet_peer_reset_incoming_commands (PENetList * queue)
{
    penet_peer_remove_incoming_commands(queue, penet_list_begin (queue), penet_list_end (queue));
}

void
penet_peer_reset_queues (PENetPeer * peer)
{
    PENetChannel * channel;

    if (peer -> needsDispatch)
    {
       penet_list_remove (& peer -> dispatchList);

       peer -> needsDispatch = 0;
    }

    while (! penet_list_empty (& peer -> acknowledgements))
      penet_free (penet_list_remove (penet_list_begin (& peer -> acknowledgements)));

    penet_peer_reset_outgoing_commands (& peer -> sentReliableCommands);
    penet_peer_reset_outgoing_commands (& peer -> sentUnreliableCommands);
    penet_peer_reset_outgoing_commands (& peer -> outgoingReliableCommands);
    penet_peer_reset_outgoing_commands (& peer -> outgoingUnreliableCommands);
    penet_peer_reset_incoming_commands (& peer -> dispatchedCommands);

    if (peer -> channels != NULL && peer -> channelCount > 0)
    {
        for (channel = peer -> channels;
             channel < & peer -> channels [peer -> channelCount];
             ++ channel)
        {
            penet_peer_reset_incoming_commands (& channel -> incomingReliableCommands);
            penet_peer_reset_incoming_commands (& channel -> incomingUnreliableCommands);
        }

        penet_free (peer -> channels);
    }

    peer -> channels = NULL;
    peer -> channelCount = 0;
}

void
penet_peer_on_connect (PENetPeer * peer)
{
    if (peer -> state != PENET_PEER_STATE_CONNECTED && peer -> state != PENET_PEER_STATE_DISCONNECT_LATER)
    {
        if (peer -> incomingBandwidth != 0)
          ++ peer -> host -> bandwidthLimitedPeers;

        ++ peer -> host -> connectedPeers;
    }
}

void
penet_peer_on_disconnect (PENetPeer * peer)
{
    if (peer -> state == PENET_PEER_STATE_CONNECTED || peer -> state == PENET_PEER_STATE_DISCONNECT_LATER)
    {
        if (peer -> incomingBandwidth != 0)
          -- peer -> host -> bandwidthLimitedPeers;

        -- peer -> host -> connectedPeers;
    }
}

/** Forcefully disconnects a peer.
    @param peer peer to forcefully disconnect
    @remarks The foreign host represented by the peer is not notified of the disconnection and will timeout
    on its connection to the local host.
*/
void
penet_peer_reset (PENetPeer * peer)
{
    penet_peer_on_disconnect (peer);

    peer -> outgoingPeerID = PENET_PROTOCOL_MAXIMUM_PEER_ID;
    peer -> connectID = 0;

    peer -> state = PENET_PEER_STATE_DISCONNECTED;

    peer -> incomingBandwidth = 0;
    peer -> outgoingBandwidth = 0;
    peer -> incomingBandwidthThrottleEpoch = 0;
    peer -> outgoingBandwidthThrottleEpoch = 0;
    peer -> incomingDataTotal = 0;
    peer -> outgoingDataTotal = 0;
    peer -> lastSendTime = 0;
    peer -> lastReceiveTime = 0;
    peer -> nextTimeout = 0;
    peer -> earliestTimeout = 0;
    peer -> packetLossEpoch = 0;
    peer -> packetsSent = 0;
    peer -> packetsLost = 0;
    peer -> packetLoss = 0;
    peer -> packetLossVariance = 0;
    peer -> packetThrottle = PENET_PEER_DEFAULT_PACKET_THROTTLE;
    peer -> packetThrottleLimit = PENET_PEER_PACKET_THROTTLE_SCALE;
    peer -> packetThrottleCounter = 0;
    peer -> packetThrottleEpoch = 0;
    peer -> packetThrottleAcceleration = PENET_PEER_PACKET_THROTTLE_ACCELERATION;
    peer -> packetThrottleDeceleration = PENET_PEER_PACKET_THROTTLE_DECELERATION;
    peer -> packetThrottleInterval = PENET_PEER_PACKET_THROTTLE_INTERVAL;
    peer -> pingInterval = PENET_PEER_PING_INTERVAL;
    peer -> timeoutLimit = PENET_PEER_TIMEOUT_LIMIT;
    peer -> timeoutMinimum = PENET_PEER_TIMEOUT_MINIMUM;
    peer -> timeoutMaximum = PENET_PEER_TIMEOUT_MAXIMUM;
    peer -> lastRoundTripTime = PENET_PEER_DEFAULT_ROUND_TRIP_TIME;
    peer -> lowestRoundTripTime = PENET_PEER_DEFAULT_ROUND_TRIP_TIME;
    peer -> lastRoundTripTimeVariance = 0;
    peer -> highestRoundTripTimeVariance = 0;
    peer -> roundTripTime = PENET_PEER_DEFAULT_ROUND_TRIP_TIME;
    peer -> roundTripTimeVariance = 0;
    peer -> mtu = peer -> host -> mtu;
    peer -> reliableDataInTransit = 0;
    peer -> outgoingReliableSequenceNumber = 0;
    peer -> windowSize = PENET_PROTOCOL_MAXIMUM_WINDOW_SIZE;
    peer -> incomingUnsequencedGroup = 0;
    peer -> outgoingUnsequencedGroup = 0;
    peer -> eventData = 0;
    peer -> totalWaitingData = 0;

    memset (peer -> unsequencedWindow, 0, sizeof (peer -> unsequencedWindow));

    penet_peer_reset_queues (peer);
}

/** Sends a ping request to a peer.
    @param peer destination for the ping request
    @remarks ping requests factor into the mean round trip time as designated by the
    roundTripTime field in the PENetPeer structure.  PENet automatically pings all connected
    peers at regular intervals, however, this function may be called to ensure more
    frequent ping requests.
*/
void
penet_peer_ping (PENetPeer * peer)
{
    PENetProtocol command;

    if (peer -> state != PENET_PEER_STATE_CONNECTED)
      return;

    command.header.command = PENET_PROTOCOL_COMMAND_PING | PENET_PROTOCOL_COMMAND_FLAG_ACKNOWLEDGE;
    command.header.channelID = 0xFF;

    penet_peer_queue_outgoing_command (peer, & command, NULL, 0, 0);
}

/** Sets the interval at which pings will be sent to a peer.

    Pings are used both to monitor the liveness of the connection and also to dynamically
    adjust the throttle during periods of low traffic so that the throttle has reasonable
    responsiveness during traffic spikes.

    @param peer the peer to adjust
    @param pingInterval the interval at which to send pings; defaults to PENET_PEER_PING_INTERVAL if 0
*/
void
penet_peer_ping_interval (PENetPeer * peer, penet_uint32 pingInterval)
{
    peer -> pingInterval = pingInterval ? pingInterval : PENET_PEER_PING_INTERVAL;
}

/** Sets the timeout parameters for a peer.

    The timeout parameter control how and when a peer will timeout from a failure to acknowledge
    reliable traffic. Timeout values use an exponential backoff mechanism, where if a reliable
    packet is not acknowledge within some multiple of the average RTT plus a variance tolerance,
    the timeout will be doubled until it reaches a set limit. If the timeout is thus at this
    limit and reliable packets have been sent but not acknowledged within a certain minimum time
    period, the peer will be disconnected. Alternatively, if reliable packets have been sent
    but not acknowledged for a certain maximum time period, the peer will be disconnected regardless
    of the current timeout limit value.

    @param peer the peer to adjust
    @param timeoutLimit the timeout limit; defaults to PENET_PEER_TIMEOUT_LIMIT if 0
    @param timeoutMinimum the timeout minimum; defaults to PENET_PEER_TIMEOUT_MINIMUM if 0
    @param timeoutMaximum the timeout maximum; defaults to PENET_PEER_TIMEOUT_MAXIMUM if 0
*/

void
penet_peer_timeout (PENetPeer * peer, penet_uint32 timeoutLimit, penet_uint32 timeoutMinimum, penet_uint32 timeoutMaximum)
{
    peer -> timeoutLimit = timeoutLimit ? timeoutLimit : PENET_PEER_TIMEOUT_LIMIT;
    peer -> timeoutMinimum = timeoutMinimum ? timeoutMinimum : PENET_PEER_TIMEOUT_MINIMUM;
    peer -> timeoutMaximum = timeoutMaximum ? timeoutMaximum : PENET_PEER_TIMEOUT_MAXIMUM;
}

/** Force an immediate disconnection from a peer.
    @param peer peer to disconnect
    @param data data describing the disconnection
    @remarks No PENET_EVENT_DISCONNECT event will be generated. The foreign peer is not
    guaranteed to receive the disconnect notification, and is reset immediately upon
    return from this function.
*/
void
penet_peer_disconnect_now (PENetPeer * peer, penet_uint32 data)
{
    PENetProtocol command;

    if (peer -> state == PENET_PEER_STATE_DISCONNECTED)
      return;

    if (peer -> state != PENET_PEER_STATE_ZOMBIE &&
        peer -> state != PENET_PEER_STATE_DISCONNECTING)
    {
        penet_peer_reset_queues (peer);

        command.header.command = PENET_PROTOCOL_COMMAND_DISCONNECT | PENET_PROTOCOL_COMMAND_FLAG_UNSEQUENCED;
        command.header.channelID = 0xFF;
        command.disconnect.data = PENET_HOST_TO_NET_32 (data);

        penet_peer_queue_outgoing_command (peer, & command, NULL, 0, 0);

        penet_host_flush (peer -> host);
    }

    penet_peer_reset (peer);
}

/** Request a disconnection from a peer.
    @param peer peer to request a disconnection
    @param data data describing the disconnection
    @remarks An PENET_EVENT_DISCONNECT event will be generated by penet_host_service()
    once the disconnection is complete.
*/
void
penet_peer_disconnect (PENetPeer * peer, penet_uint32 data)
{
    PENetProtocol command;

    if (peer -> state == PENET_PEER_STATE_DISCONNECTING ||
        peer -> state == PENET_PEER_STATE_DISCONNECTED ||
        peer -> state == PENET_PEER_STATE_ACKNOWLEDGING_DISCONNECT ||
        peer -> state == PENET_PEER_STATE_ZOMBIE)
      return;

    penet_peer_reset_queues (peer);

    command.header.command = PENET_PROTOCOL_COMMAND_DISCONNECT;
    command.header.channelID = 0xFF;
    command.disconnect.data = PENET_HOST_TO_NET_32 (data);

    if (peer -> state == PENET_PEER_STATE_CONNECTED || peer -> state == PENET_PEER_STATE_DISCONNECT_LATER)
      command.header.command |= PENET_PROTOCOL_COMMAND_FLAG_ACKNOWLEDGE;
    else
      command.header.command |= PENET_PROTOCOL_COMMAND_FLAG_UNSEQUENCED;

    penet_peer_queue_outgoing_command (peer, & command, NULL, 0, 0);

    if (peer -> state == PENET_PEER_STATE_CONNECTED || peer -> state == PENET_PEER_STATE_DISCONNECT_LATER)
    {
        penet_peer_on_disconnect (peer);

        peer -> state = PENET_PEER_STATE_DISCONNECTING;
    }
    else
    {
        penet_host_flush (peer -> host);
        penet_peer_reset (peer);
    }
}

/** Request a disconnection from a peer, but only after all queued outgoing packets are sent.
    @param peer peer to request a disconnection
    @param data data describing the disconnection
    @remarks An PENET_EVENT_DISCONNECT event will be generated by penet_host_service()
    once the disconnection is complete.
*/
void
penet_peer_disconnect_later (PENetPeer * peer, penet_uint32 data)
{
    if ((peer -> state == PENET_PEER_STATE_CONNECTED || peer -> state == PENET_PEER_STATE_DISCONNECT_LATER) &&
        ! (penet_list_empty (& peer -> outgoingReliableCommands) &&
           penet_list_empty (& peer -> outgoingUnreliableCommands) &&
           penet_list_empty (& peer -> sentReliableCommands)))
    {
        peer -> state = PENET_PEER_STATE_DISCONNECT_LATER;
        peer -> eventData = data;
    }
    else
      penet_peer_disconnect (peer, data);
}

PENetAcknowledgement *
penet_peer_queue_acknowledgement (PENetPeer * peer, const PENetProtocol * command, penet_uint16 sentTime)
{
    PENetAcknowledgement * acknowledgement;

    if (command -> header.channelID < peer -> channelCount)
    {
        PENetChannel * channel = & peer -> channels [command -> header.channelID];
        penet_uint16 reliableWindow = command -> header.reliableSequenceNumber / PENET_PEER_RELIABLE_WINDOW_SIZE,
                    currentWindow = channel -> incomingReliableSequenceNumber / PENET_PEER_RELIABLE_WINDOW_SIZE;

        if (command -> header.reliableSequenceNumber < channel -> incomingReliableSequenceNumber)
           reliableWindow += PENET_PEER_RELIABLE_WINDOWS;

        if (reliableWindow >= currentWindow + PENET_PEER_FREE_RELIABLE_WINDOWS - 1 && reliableWindow <= currentWindow + PENET_PEER_FREE_RELIABLE_WINDOWS)
          return NULL;
    }

    acknowledgement = (PENetAcknowledgement *) penet_malloc (sizeof (PENetAcknowledgement));
    if (acknowledgement == NULL)
      return NULL;

    peer -> outgoingDataTotal += sizeof (PENetProtocolAcknowledge);

    acknowledgement -> sentTime = sentTime;
    acknowledgement -> command = * command;

    penet_list_insert (penet_list_end (& peer -> acknowledgements), acknowledgement);

    return acknowledgement;
}

void
penet_peer_setup_outgoing_command (PENetPeer * peer, PENetOutgoingCommand * outgoingCommand)
{
    PENetChannel * channel = & peer -> channels [outgoingCommand -> command.header.channelID];

    peer -> outgoingDataTotal += penet_protocol_command_size (outgoingCommand -> command.header.command) + outgoingCommand -> fragmentLength;

    if (outgoingCommand -> command.header.channelID == 0xFF)
    {
       ++ peer -> outgoingReliableSequenceNumber;

       outgoingCommand -> reliableSequenceNumber = peer -> outgoingReliableSequenceNumber;
       outgoingCommand -> unreliableSequenceNumber = 0;
    }
    else
    if (outgoingCommand -> command.header.command & PENET_PROTOCOL_COMMAND_FLAG_ACKNOWLEDGE)
    {
       ++ channel -> outgoingReliableSequenceNumber;
       channel -> outgoingUnreliableSequenceNumber = 0;

       outgoingCommand -> reliableSequenceNumber = channel -> outgoingReliableSequenceNumber;
       outgoingCommand -> unreliableSequenceNumber = 0;
    }
    else
    if (outgoingCommand -> command.header.command & PENET_PROTOCOL_COMMAND_FLAG_UNSEQUENCED)
    {
       ++ peer -> outgoingUnsequencedGroup;

       outgoingCommand -> reliableSequenceNumber = 0;
       outgoingCommand -> unreliableSequenceNumber = 0;
    }
    else
    {
       if (outgoingCommand -> fragmentOffset == 0)
         ++ channel -> outgoingUnreliableSequenceNumber;

       outgoingCommand -> reliableSequenceNumber = channel -> outgoingReliableSequenceNumber;
       outgoingCommand -> unreliableSequenceNumber = channel -> outgoingUnreliableSequenceNumber;
    }

    outgoingCommand -> sendAttempts = 0;
    outgoingCommand -> sentTime = 0;
    outgoingCommand -> roundTripTimeout = 0;
    outgoingCommand -> roundTripTimeoutLimit = 0;
    outgoingCommand -> command.header.reliableSequenceNumber = PENET_HOST_TO_NET_16 (outgoingCommand -> reliableSequenceNumber);

    switch (outgoingCommand -> command.header.command & PENET_PROTOCOL_COMMAND_MASK)
    {
    case PENET_PROTOCOL_COMMAND_SEND_UNRELIABLE:
        outgoingCommand -> command.sendUnreliable.unreliableSequenceNumber = PENET_HOST_TO_NET_16 (outgoingCommand -> unreliableSequenceNumber);
        break;

    case PENET_PROTOCOL_COMMAND_SEND_UNSEQUENCED:
        outgoingCommand -> command.sendUnsequenced.unsequencedGroup = PENET_HOST_TO_NET_16 (peer -> outgoingUnsequencedGroup);
        break;

    default:
        break;
    }

    if (outgoingCommand -> command.header.command & PENET_PROTOCOL_COMMAND_FLAG_ACKNOWLEDGE)
      penet_list_insert (penet_list_end (& peer -> outgoingReliableCommands), outgoingCommand);
    else
      penet_list_insert (penet_list_end (& peer -> outgoingUnreliableCommands), outgoingCommand);
}

PENetOutgoingCommand *
penet_peer_queue_outgoing_command (PENetPeer * peer, const PENetProtocol * command, PENetPacket * packet, penet_uint32 offset, penet_uint16 length)
{
    PENetOutgoingCommand * outgoingCommand = (PENetOutgoingCommand *) penet_malloc (sizeof (PENetOutgoingCommand));
    if (outgoingCommand == NULL)
      return NULL;

    outgoingCommand -> command = * command;
    outgoingCommand -> fragmentOffset = offset;
    outgoingCommand -> fragmentLength = length;
    outgoingCommand -> packet = packet;
    if (packet != NULL)
      ++ packet -> referenceCount;

    penet_peer_setup_outgoing_command (peer, outgoingCommand);

    return outgoingCommand;
}

void
penet_peer_dispatch_incoming_unreliable_commands (PENetPeer * peer, PENetChannel * channel)
{
    PENetListIterator droppedCommand, startCommand, currentCommand;

    for (droppedCommand = startCommand = currentCommand = penet_list_begin (& channel -> incomingUnreliableCommands);
         currentCommand != penet_list_end (& channel -> incomingUnreliableCommands);
         currentCommand = penet_list_next (currentCommand))
    {
       PENetIncomingCommand * incomingCommand = (PENetIncomingCommand *) currentCommand;

       if ((incomingCommand -> command.header.command & PENET_PROTOCOL_COMMAND_MASK) == PENET_PROTOCOL_COMMAND_SEND_UNSEQUENCED)
         continue;

       if (incomingCommand -> reliableSequenceNumber == channel -> incomingReliableSequenceNumber)
       {
          if (incomingCommand -> fragmentsRemaining <= 0)
          {
             channel -> incomingUnreliableSequenceNumber = incomingCommand -> unreliableSequenceNumber;
             continue;
          }

          if (startCommand != currentCommand)
          {
             penet_list_move (penet_list_end (& peer -> dispatchedCommands), startCommand, penet_list_previous (currentCommand));

             if (! peer -> needsDispatch)
             {
                penet_list_insert (penet_list_end (& peer -> host -> dispatchQueue), & peer -> dispatchList);

                peer -> needsDispatch = 1;
             }

             droppedCommand = currentCommand;
          }
          else
          if (droppedCommand != currentCommand)
            droppedCommand = penet_list_previous (currentCommand);
       }
       else
       {
          penet_uint16 reliableWindow = incomingCommand -> reliableSequenceNumber / PENET_PEER_RELIABLE_WINDOW_SIZE,
                      currentWindow = channel -> incomingReliableSequenceNumber / PENET_PEER_RELIABLE_WINDOW_SIZE;
          if (incomingCommand -> reliableSequenceNumber < channel -> incomingReliableSequenceNumber)
            reliableWindow += PENET_PEER_RELIABLE_WINDOWS;
          if (reliableWindow >= currentWindow && reliableWindow < currentWindow + PENET_PEER_FREE_RELIABLE_WINDOWS - 1)
            break;

          droppedCommand = penet_list_next (currentCommand);

          if (startCommand != currentCommand)
          {
             penet_list_move (penet_list_end (& peer -> dispatchedCommands), startCommand, penet_list_previous (currentCommand));

             if (! peer -> needsDispatch)
             {
                penet_list_insert (penet_list_end (& peer -> host -> dispatchQueue), & peer -> dispatchList);

                peer -> needsDispatch = 1;
             }
          }
       }

       startCommand = penet_list_next (currentCommand);
    }

    if (startCommand != currentCommand)
    {
       penet_list_move (penet_list_end (& peer -> dispatchedCommands), startCommand, penet_list_previous (currentCommand));

       if (! peer -> needsDispatch)
       {
           penet_list_insert (penet_list_end (& peer -> host -> dispatchQueue), & peer -> dispatchList);

           peer -> needsDispatch = 1;
       }

       droppedCommand = currentCommand;
    }

    penet_peer_remove_incoming_commands (& channel -> incomingUnreliableCommands, penet_list_begin (& channel -> incomingUnreliableCommands), droppedCommand);
}

void
penet_peer_dispatch_incoming_reliable_commands (PENetPeer * peer, PENetChannel * channel)
{
    PENetListIterator currentCommand;

    for (currentCommand = penet_list_begin (& channel -> incomingReliableCommands);
         currentCommand != penet_list_end (& channel -> incomingReliableCommands);
         currentCommand = penet_list_next (currentCommand))
    {
       PENetIncomingCommand * incomingCommand = (PENetIncomingCommand *) currentCommand;

       if (incomingCommand -> fragmentsRemaining > 0 ||
           incomingCommand -> reliableSequenceNumber != (penet_uint16) (channel -> incomingReliableSequenceNumber + 1))
         break;

       channel -> incomingReliableSequenceNumber = incomingCommand -> reliableSequenceNumber;

       if (incomingCommand -> fragmentCount > 0)
         channel -> incomingReliableSequenceNumber += incomingCommand -> fragmentCount - 1;
    }

    if (currentCommand == penet_list_begin (& channel -> incomingReliableCommands))
      return;

    channel -> incomingUnreliableSequenceNumber = 0;

    penet_list_move (penet_list_end (& peer -> dispatchedCommands), penet_list_begin (& channel -> incomingReliableCommands), penet_list_previous (currentCommand));

    if (! peer -> needsDispatch)
    {
       penet_list_insert (penet_list_end (& peer -> host -> dispatchQueue), & peer -> dispatchList);

       peer -> needsDispatch = 1;
    }

    if (! penet_list_empty (& channel -> incomingUnreliableCommands))
       penet_peer_dispatch_incoming_unreliable_commands (peer, channel);
}

PENetIncomingCommand *
penet_peer_queue_incoming_command (PENetPeer * peer, const PENetProtocol * command, const void * data, size_t dataLength, penet_uint32 flags, penet_uint32 fragmentCount)
{
    static PENetIncomingCommand dummyCommand;

    PENetChannel * channel = & peer -> channels [command -> header.channelID];
    penet_uint32 unreliableSequenceNumber = 0, reliableSequenceNumber = 0;
    penet_uint16 reliableWindow, currentWindow;
    PENetIncomingCommand * incomingCommand;
    PENetListIterator currentCommand;
    PENetPacket * packet = NULL;

    if (peer -> state == PENET_PEER_STATE_DISCONNECT_LATER)
      goto discardCommand;

    if ((command -> header.command & PENET_PROTOCOL_COMMAND_MASK) != PENET_PROTOCOL_COMMAND_SEND_UNSEQUENCED)
    {
        reliableSequenceNumber = command -> header.reliableSequenceNumber;
        reliableWindow = reliableSequenceNumber / PENET_PEER_RELIABLE_WINDOW_SIZE;
        currentWindow = channel -> incomingReliableSequenceNumber / PENET_PEER_RELIABLE_WINDOW_SIZE;

        if (reliableSequenceNumber < channel -> incomingReliableSequenceNumber)
           reliableWindow += PENET_PEER_RELIABLE_WINDOWS;

        if (reliableWindow < currentWindow || reliableWindow >= currentWindow + PENET_PEER_FREE_RELIABLE_WINDOWS - 1)
          goto discardCommand;
    }

    switch (command -> header.command & PENET_PROTOCOL_COMMAND_MASK)
    {
    case PENET_PROTOCOL_COMMAND_SEND_FRAGMENT:
    case PENET_PROTOCOL_COMMAND_SEND_RELIABLE:
       if (reliableSequenceNumber == channel -> incomingReliableSequenceNumber)
         goto discardCommand;

       for (currentCommand = penet_list_previous (penet_list_end (& channel -> incomingReliableCommands));
            currentCommand != penet_list_end (& channel -> incomingReliableCommands);
            currentCommand = penet_list_previous (currentCommand))
       {
          incomingCommand = (PENetIncomingCommand *) currentCommand;

          if (reliableSequenceNumber >= channel -> incomingReliableSequenceNumber)
          {
             if (incomingCommand -> reliableSequenceNumber < channel -> incomingReliableSequenceNumber)
               continue;
          }
          else
          if (incomingCommand -> reliableSequenceNumber >= channel -> incomingReliableSequenceNumber)
            break;

          if (incomingCommand -> reliableSequenceNumber <= reliableSequenceNumber)
          {
             if (incomingCommand -> reliableSequenceNumber < reliableSequenceNumber)
               break;

             goto discardCommand;
          }
       }
       break;

    case PENET_PROTOCOL_COMMAND_SEND_UNRELIABLE:
    case PENET_PROTOCOL_COMMAND_SEND_UNRELIABLE_FRAGMENT:
       unreliableSequenceNumber = PENET_NET_TO_HOST_16 (command -> sendUnreliable.unreliableSequenceNumber);

       if (reliableSequenceNumber == channel -> incomingReliableSequenceNumber &&
           unreliableSequenceNumber <= channel -> incomingUnreliableSequenceNumber)
         goto discardCommand;

       for (currentCommand = penet_list_previous (penet_list_end (& channel -> incomingUnreliableCommands));
            currentCommand != penet_list_end (& channel -> incomingUnreliableCommands);
            currentCommand = penet_list_previous (currentCommand))
       {
          incomingCommand = (PENetIncomingCommand *) currentCommand;

          if ((command -> header.command & PENET_PROTOCOL_COMMAND_MASK) == PENET_PROTOCOL_COMMAND_SEND_UNSEQUENCED)
            continue;

          if (reliableSequenceNumber >= channel -> incomingReliableSequenceNumber)
          {
             if (incomingCommand -> reliableSequenceNumber < channel -> incomingReliableSequenceNumber)
               continue;
          }
          else
          if (incomingCommand -> reliableSequenceNumber >= channel -> incomingReliableSequenceNumber)
            break;

          if (incomingCommand -> reliableSequenceNumber < reliableSequenceNumber)
            break;

          if (incomingCommand -> reliableSequenceNumber > reliableSequenceNumber)
            continue;

          if (incomingCommand -> unreliableSequenceNumber <= unreliableSequenceNumber)
          {
             if (incomingCommand -> unreliableSequenceNumber < unreliableSequenceNumber)
               break;

             goto discardCommand;
          }
       }
       break;

    case PENET_PROTOCOL_COMMAND_SEND_UNSEQUENCED:
       currentCommand = penet_list_end (& channel -> incomingUnreliableCommands);
       break;

    default:
       goto discardCommand;
    }

    if (peer -> totalWaitingData >= peer -> host -> maximumWaitingData)
      goto notifyError;

    packet = penet_packet_create (data, dataLength, flags);
    if (packet == NULL)
      goto notifyError;

    incomingCommand = (PENetIncomingCommand *) penet_malloc (sizeof (PENetIncomingCommand));
    if (incomingCommand == NULL)
      goto notifyError;

    incomingCommand -> reliableSequenceNumber = command -> header.reliableSequenceNumber;
    incomingCommand -> unreliableSequenceNumber = unreliableSequenceNumber & 0xFFFF;
    incomingCommand -> command = * command;
    incomingCommand -> fragmentCount = fragmentCount;
    incomingCommand -> fragmentsRemaining = fragmentCount;
    incomingCommand -> packet = packet;
    incomingCommand -> fragments = NULL;

    if (fragmentCount > 0)
    {
       if (fragmentCount <= PENET_PROTOCOL_MAXIMUM_FRAGMENT_COUNT)
         incomingCommand -> fragments = (penet_uint32 *) penet_malloc ((fragmentCount + 31) / 32 * sizeof (penet_uint32));
       if (incomingCommand -> fragments == NULL)
       {
          penet_free (incomingCommand);

          goto notifyError;
       }
       memset (incomingCommand -> fragments, 0, (fragmentCount + 31) / 32 * sizeof (penet_uint32));
    }

    if (packet != NULL)
    {
       ++ packet -> referenceCount;

       peer -> totalWaitingData += packet -> dataLength;
    }

    penet_list_insert (penet_list_next (currentCommand), incomingCommand);

    switch (command -> header.command & PENET_PROTOCOL_COMMAND_MASK)
    {
    case PENET_PROTOCOL_COMMAND_SEND_FRAGMENT:
    case PENET_PROTOCOL_COMMAND_SEND_RELIABLE:
       penet_peer_dispatch_incoming_reliable_commands (peer, channel);
       break;

    default:
       penet_peer_dispatch_incoming_unreliable_commands (peer, channel);
       break;
    }

    return incomingCommand;

discardCommand:
    if (fragmentCount > 0)
      goto notifyError;

    if (packet != NULL && packet -> referenceCount == 0)
      penet_packet_destroy (packet);

    return & dummyCommand;

notifyError:
    if (packet != NULL && packet -> referenceCount == 0)
      penet_packet_destroy (packet);

    return NULL;
}

/** @} */
