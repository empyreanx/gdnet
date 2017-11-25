/** 
 @file  protocol.c
 @brief PENet protocol functions
*/
#include <stdio.h>
#include <string.h>
#define PENET_BUILDING_LIB 1
#include "penet/utility.h"
#include "penet/time.h"
#include "penet/penet.h"

static size_t commandSizes [PENET_PROTOCOL_COMMAND_COUNT] =
{
    0,
    sizeof (PENetProtocolAcknowledge),
    sizeof (PENetProtocolConnect),
    sizeof (PENetProtocolVerifyConnect),
    sizeof (PENetProtocolDisconnect),
    sizeof (PENetProtocolPing),
    sizeof (PENetProtocolSendReliable),
    sizeof (PENetProtocolSendUnreliable),
    sizeof (PENetProtocolSendFragment),
    sizeof (PENetProtocolSendUnsequenced),
    sizeof (PENetProtocolBandwidthLimit),
    sizeof (PENetProtocolThrottleConfigure),
    sizeof (PENetProtocolSendFragment)
};

size_t
penet_protocol_command_size (penet_uint8 commandNumber)
{
    return commandSizes [commandNumber & PENET_PROTOCOL_COMMAND_MASK];
}

static void
penet_protocol_change_state (PENetHost * host, PENetPeer * peer, PENetPeerState state)
{
    if (state == PENET_PEER_STATE_CONNECTED || state == PENET_PEER_STATE_DISCONNECT_LATER)
      penet_peer_on_connect (peer);
    else
      penet_peer_on_disconnect (peer);

    peer -> state = state;
}

static void
penet_protocol_dispatch_state (PENetHost * host, PENetPeer * peer, PENetPeerState state)
{
    penet_protocol_change_state (host, peer, state);

    if (! peer -> needsDispatch)
    {
       penet_list_insert (penet_list_end (& host -> dispatchQueue), & peer -> dispatchList);

       peer -> needsDispatch = 1;
    }
}

static int
penet_protocol_dispatch_incoming_commands (PENetHost * host, PENetEvent * event)
{
    while (! penet_list_empty (& host -> dispatchQueue))
    {
       PENetPeer * peer = (PENetPeer *) penet_list_remove (penet_list_begin (& host -> dispatchQueue));

       peer -> needsDispatch = 0;

       switch (peer -> state)
       {
       case PENET_PEER_STATE_CONNECTION_PENDING:
       case PENET_PEER_STATE_CONNECTION_SUCCEEDED:
           penet_protocol_change_state (host, peer, PENET_PEER_STATE_CONNECTED);

           event -> type = PENET_EVENT_TYPE_CONNECT;
           event -> peer = peer;
           event -> data = peer -> eventData;

           return 1;

       case PENET_PEER_STATE_ZOMBIE:
           host -> recalculateBandwidthLimits = 1;

           event -> type = PENET_EVENT_TYPE_DISCONNECT;
           event -> peer = peer;
           event -> data = peer -> eventData;

           penet_peer_reset (peer);

           return 1;

       case PENET_PEER_STATE_CONNECTED:
           if (penet_list_empty (& peer -> dispatchedCommands))
             continue;

           event -> packet = penet_peer_receive (peer, & event -> channelID);
           if (event -> packet == NULL)
             continue;

           event -> type = PENET_EVENT_TYPE_RECEIVE;
           event -> peer = peer;

           if (! penet_list_empty (& peer -> dispatchedCommands))
           {
              peer -> needsDispatch = 1;

              penet_list_insert (penet_list_end (& host -> dispatchQueue), & peer -> dispatchList);
           }

           return 1;

       default:
           break;
       }
    }

    return 0;
}

static void
penet_protocol_notify_connect (PENetHost * host, PENetPeer * peer, PENetEvent * event)
{
    host -> recalculateBandwidthLimits = 1;

    if (event != NULL)
    {
        penet_protocol_change_state (host, peer, PENET_PEER_STATE_CONNECTED);

        event -> type = PENET_EVENT_TYPE_CONNECT;
        event -> peer = peer;
        event -> data = peer -> eventData;
    }
    else
        penet_protocol_dispatch_state (host, peer, peer -> state == PENET_PEER_STATE_CONNECTING ? PENET_PEER_STATE_CONNECTION_SUCCEEDED : PENET_PEER_STATE_CONNECTION_PENDING);
}

static void
penet_protocol_notify_disconnect (PENetHost * host, PENetPeer * peer, PENetEvent * event)
{
    if (peer -> state >= PENET_PEER_STATE_CONNECTION_PENDING)
       host -> recalculateBandwidthLimits = 1;

    if (peer -> state != PENET_PEER_STATE_CONNECTING && peer -> state < PENET_PEER_STATE_CONNECTION_SUCCEEDED)
        penet_peer_reset (peer);
    else
    if (event != NULL)
    {
        event -> type = PENET_EVENT_TYPE_DISCONNECT;
        event -> peer = peer;
        event -> data = 0;

        penet_peer_reset (peer);
    }
    else
    {
        peer -> eventData = 0;

        penet_protocol_dispatch_state (host, peer, PENET_PEER_STATE_ZOMBIE);
    }
}

static void
penet_protocol_remove_sent_unreliable_commands (PENetPeer * peer)
{
    PENetOutgoingCommand * outgoingCommand;

    while (! penet_list_empty (& peer -> sentUnreliableCommands))
    {
        outgoingCommand = (PENetOutgoingCommand *) penet_list_front (& peer -> sentUnreliableCommands);

        penet_list_remove (& outgoingCommand -> outgoingCommandList);

        if (outgoingCommand -> packet != NULL)
        {
           -- outgoingCommand -> packet -> referenceCount;

           if (outgoingCommand -> packet -> referenceCount == 0)
           {
              outgoingCommand -> packet -> flags |= PENET_PACKET_FLAG_SENT;

              penet_packet_destroy (outgoingCommand -> packet);
           }
        }

        penet_free (outgoingCommand);
    }
}

static PENetProtocolCommand
penet_protocol_remove_sent_reliable_command (PENetPeer * peer, penet_uint16 reliableSequenceNumber, penet_uint8 channelID)
{
    PENetOutgoingCommand * outgoingCommand = NULL;
    PENetListIterator currentCommand;
    PENetProtocolCommand commandNumber;
    int wasSent = 1;

    for (currentCommand = penet_list_begin (& peer -> sentReliableCommands);
         currentCommand != penet_list_end (& peer -> sentReliableCommands);
         currentCommand = penet_list_next (currentCommand))
    {
       outgoingCommand = (PENetOutgoingCommand *) currentCommand;

       if (outgoingCommand -> reliableSequenceNumber == reliableSequenceNumber &&
           outgoingCommand -> command.header.channelID == channelID)
         break;
    }

    if (currentCommand == penet_list_end (& peer -> sentReliableCommands))
    {
       for (currentCommand = penet_list_begin (& peer -> outgoingReliableCommands);
            currentCommand != penet_list_end (& peer -> outgoingReliableCommands);
            currentCommand = penet_list_next (currentCommand))
       {
          outgoingCommand = (PENetOutgoingCommand *) currentCommand;

          if (outgoingCommand -> sendAttempts < 1) return PENET_PROTOCOL_COMMAND_NONE;

          if (outgoingCommand -> reliableSequenceNumber == reliableSequenceNumber &&
              outgoingCommand -> command.header.channelID == channelID)
            break;
       }

       if (currentCommand == penet_list_end (& peer -> outgoingReliableCommands))
         return PENET_PROTOCOL_COMMAND_NONE;

       wasSent = 0;
    }

    if (outgoingCommand == NULL)
      return PENET_PROTOCOL_COMMAND_NONE;

    if (channelID < peer -> channelCount)
    {
       PENetChannel * channel = & peer -> channels [channelID];
       penet_uint16 reliableWindow = reliableSequenceNumber / PENET_PEER_RELIABLE_WINDOW_SIZE;
       if (channel -> reliableWindows [reliableWindow] > 0)
       {
          -- channel -> reliableWindows [reliableWindow];
          if (! channel -> reliableWindows [reliableWindow])
            channel -> usedReliableWindows &= ~ (1 << reliableWindow);
       }
    }

    commandNumber = (PENetProtocolCommand) (outgoingCommand -> command.header.command & PENET_PROTOCOL_COMMAND_MASK);

    penet_list_remove (& outgoingCommand -> outgoingCommandList);

    if (outgoingCommand -> packet != NULL)
    {
       if (wasSent)
         peer -> reliableDataInTransit -= outgoingCommand -> fragmentLength;

       -- outgoingCommand -> packet -> referenceCount;

       if (outgoingCommand -> packet -> referenceCount == 0)
       {
          outgoingCommand -> packet -> flags |= PENET_PACKET_FLAG_SENT;

          penet_packet_destroy (outgoingCommand -> packet);
       }
    }

    penet_free (outgoingCommand);

    if (penet_list_empty (& peer -> sentReliableCommands))
      return commandNumber;

    outgoingCommand = (PENetOutgoingCommand *) penet_list_front (& peer -> sentReliableCommands);

    peer -> nextTimeout = outgoingCommand -> sentTime + outgoingCommand -> roundTripTimeout;

    return commandNumber;
}

static PENetPeer *
penet_protocol_handle_connect (PENetHost * host, PENetProtocolHeader * header, PENetProtocol * command)
{
    penet_uint8 incomingSessionID, outgoingSessionID;
    penet_uint32 mtu, windowSize;
    PENetChannel * channel;
    size_t channelCount, duplicatePeers = 0;
    PENetPeer * currentPeer, * peer = NULL;
    PENetProtocol verifyCommand;

    channelCount = PENET_NET_TO_HOST_32 (command -> connect.channelCount);

    if (channelCount < PENET_PROTOCOL_MINIMUM_CHANNEL_COUNT ||
        channelCount > PENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT)
      return NULL;

    for (currentPeer = host -> peers;
         currentPeer < & host -> peers [host -> peerCount];
         ++ currentPeer)
    {
        if (currentPeer -> state == PENET_PEER_STATE_DISCONNECTED)
        {
            if (peer == NULL)
              peer = currentPeer;
        }
        else
        if (currentPeer -> state != PENET_PEER_STATE_CONNECTING &&
            currentPeer -> address.host == host -> receivedAddress.host)
        {
            if (currentPeer -> address.port == host -> receivedAddress.port &&
                currentPeer -> connectID == command -> connect.connectID)
              return NULL;

            ++ duplicatePeers;
        }
    }

    if (peer == NULL || duplicatePeers >= host -> duplicatePeers)
      return NULL;

    if (channelCount > host -> channelLimit)
      channelCount = host -> channelLimit;
    peer -> channels = (PENetChannel *) penet_malloc (channelCount * sizeof (PENetChannel));
    if (peer -> channels == NULL)
      return NULL;
    peer -> channelCount = channelCount;
    peer -> state = PENET_PEER_STATE_ACKNOWLEDGING_CONNECT;
    peer -> connectID = command -> connect.connectID;
    peer -> address = host -> receivedAddress;
    peer -> outgoingPeerID = PENET_NET_TO_HOST_16 (command -> connect.outgoingPeerID);
    peer -> incomingBandwidth = PENET_NET_TO_HOST_32 (command -> connect.incomingBandwidth);
    peer -> outgoingBandwidth = PENET_NET_TO_HOST_32 (command -> connect.outgoingBandwidth);
    peer -> packetThrottleInterval = PENET_NET_TO_HOST_32 (command -> connect.packetThrottleInterval);
    peer -> packetThrottleAcceleration = PENET_NET_TO_HOST_32 (command -> connect.packetThrottleAcceleration);
    peer -> packetThrottleDeceleration = PENET_NET_TO_HOST_32 (command -> connect.packetThrottleDeceleration);
    peer -> eventData = PENET_NET_TO_HOST_32 (command -> connect.data);

    incomingSessionID = command -> connect.incomingSessionID == 0xFF ? peer -> outgoingSessionID : command -> connect.incomingSessionID;
    incomingSessionID = (incomingSessionID + 1) & (PENET_PROTOCOL_HEADER_SESSION_MASK >> PENET_PROTOCOL_HEADER_SESSION_SHIFT);
    if (incomingSessionID == peer -> outgoingSessionID)
      incomingSessionID = (incomingSessionID + 1) & (PENET_PROTOCOL_HEADER_SESSION_MASK >> PENET_PROTOCOL_HEADER_SESSION_SHIFT);
    peer -> outgoingSessionID = incomingSessionID;

    outgoingSessionID = command -> connect.outgoingSessionID == 0xFF ? peer -> incomingSessionID : command -> connect.outgoingSessionID;
    outgoingSessionID = (outgoingSessionID + 1) & (PENET_PROTOCOL_HEADER_SESSION_MASK >> PENET_PROTOCOL_HEADER_SESSION_SHIFT);
    if (outgoingSessionID == peer -> incomingSessionID)
      outgoingSessionID = (outgoingSessionID + 1) & (PENET_PROTOCOL_HEADER_SESSION_MASK >> PENET_PROTOCOL_HEADER_SESSION_SHIFT);
    peer -> incomingSessionID = outgoingSessionID;

    for (channel = peer -> channels;
         channel < & peer -> channels [channelCount];
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

    mtu = PENET_NET_TO_HOST_32 (command -> connect.mtu);

    if (mtu < PENET_PROTOCOL_MINIMUM_MTU)
      mtu = PENET_PROTOCOL_MINIMUM_MTU;
    else
    if (mtu > PENET_PROTOCOL_MAXIMUM_MTU)
      mtu = PENET_PROTOCOL_MAXIMUM_MTU;

    peer -> mtu = mtu;

    if (host -> outgoingBandwidth == 0 &&
        peer -> incomingBandwidth == 0)
      peer -> windowSize = PENET_PROTOCOL_MAXIMUM_WINDOW_SIZE;
    else
    if (host -> outgoingBandwidth == 0 ||
        peer -> incomingBandwidth == 0)
      peer -> windowSize = (PENET_MAX (host -> outgoingBandwidth, peer -> incomingBandwidth) /
                                    PENET_PEER_WINDOW_SIZE_SCALE) *
                                      PENET_PROTOCOL_MINIMUM_WINDOW_SIZE;
    else
      peer -> windowSize = (PENET_MIN (host -> outgoingBandwidth, peer -> incomingBandwidth) /
                                    PENET_PEER_WINDOW_SIZE_SCALE) *
                                      PENET_PROTOCOL_MINIMUM_WINDOW_SIZE;

    if (peer -> windowSize < PENET_PROTOCOL_MINIMUM_WINDOW_SIZE)
      peer -> windowSize = PENET_PROTOCOL_MINIMUM_WINDOW_SIZE;
    else
    if (peer -> windowSize > PENET_PROTOCOL_MAXIMUM_WINDOW_SIZE)
      peer -> windowSize = PENET_PROTOCOL_MAXIMUM_WINDOW_SIZE;

    if (host -> incomingBandwidth == 0)
      windowSize = PENET_PROTOCOL_MAXIMUM_WINDOW_SIZE;
    else
      windowSize = (host -> incomingBandwidth / PENET_PEER_WINDOW_SIZE_SCALE) *
                     PENET_PROTOCOL_MINIMUM_WINDOW_SIZE;

    if (windowSize > PENET_NET_TO_HOST_32 (command -> connect.windowSize))
      windowSize = PENET_NET_TO_HOST_32 (command -> connect.windowSize);

    if (windowSize < PENET_PROTOCOL_MINIMUM_WINDOW_SIZE)
      windowSize = PENET_PROTOCOL_MINIMUM_WINDOW_SIZE;
    else
    if (windowSize > PENET_PROTOCOL_MAXIMUM_WINDOW_SIZE)
      windowSize = PENET_PROTOCOL_MAXIMUM_WINDOW_SIZE;

    verifyCommand.header.command = PENET_PROTOCOL_COMMAND_VERIFY_CONNECT | PENET_PROTOCOL_COMMAND_FLAG_ACKNOWLEDGE;
    verifyCommand.header.channelID = 0xFF;
    verifyCommand.verifyConnect.outgoingPeerID = PENET_HOST_TO_NET_16 (peer -> incomingPeerID);
    verifyCommand.verifyConnect.incomingSessionID = incomingSessionID;
    verifyCommand.verifyConnect.outgoingSessionID = outgoingSessionID;
    verifyCommand.verifyConnect.mtu = PENET_HOST_TO_NET_32 (peer -> mtu);
    verifyCommand.verifyConnect.windowSize = PENET_HOST_TO_NET_32 (windowSize);
    verifyCommand.verifyConnect.channelCount = PENET_HOST_TO_NET_32 (channelCount);
    verifyCommand.verifyConnect.incomingBandwidth = PENET_HOST_TO_NET_32 (host -> incomingBandwidth);
    verifyCommand.verifyConnect.outgoingBandwidth = PENET_HOST_TO_NET_32 (host -> outgoingBandwidth);
    verifyCommand.verifyConnect.packetThrottleInterval = PENET_HOST_TO_NET_32 (peer -> packetThrottleInterval);
    verifyCommand.verifyConnect.packetThrottleAcceleration = PENET_HOST_TO_NET_32 (peer -> packetThrottleAcceleration);
    verifyCommand.verifyConnect.packetThrottleDeceleration = PENET_HOST_TO_NET_32 (peer -> packetThrottleDeceleration);
    verifyCommand.verifyConnect.connectID = peer -> connectID;

    penet_peer_queue_outgoing_command (peer, & verifyCommand, NULL, 0, 0);

    return peer;
}

static int
penet_protocol_handle_send_reliable (PENetHost * host, PENetPeer * peer, const PENetProtocol * command, penet_uint8 ** currentData)
{
    size_t dataLength;

    if (command -> header.channelID >= peer -> channelCount ||
        (peer -> state != PENET_PEER_STATE_CONNECTED && peer -> state != PENET_PEER_STATE_DISCONNECT_LATER))
      return -1;

    dataLength = PENET_NET_TO_HOST_16 (command -> sendReliable.dataLength);
    * currentData += dataLength;
    if (dataLength > host -> maximumPacketSize ||
        * currentData < host -> receivedData ||
        * currentData > & host -> receivedData [host -> receivedDataLength])
      return -1;

    if (penet_peer_queue_incoming_command (peer, command, (const penet_uint8 *) command + sizeof (PENetProtocolSendReliable), dataLength, PENET_PACKET_FLAG_RELIABLE, 0) == NULL)
      return -1;

    return 0;
}

static int
penet_protocol_handle_send_unsequenced (PENetHost * host, PENetPeer * peer, const PENetProtocol * command, penet_uint8 ** currentData)
{
    penet_uint32 unsequencedGroup, index;
    size_t dataLength;

    if (command -> header.channelID >= peer -> channelCount ||
        (peer -> state != PENET_PEER_STATE_CONNECTED && peer -> state != PENET_PEER_STATE_DISCONNECT_LATER))
      return -1;

    dataLength = PENET_NET_TO_HOST_16 (command -> sendUnsequenced.dataLength);
    * currentData += dataLength;
    if (dataLength > host -> maximumPacketSize ||
        * currentData < host -> receivedData ||
        * currentData > & host -> receivedData [host -> receivedDataLength])
      return -1;

    unsequencedGroup = PENET_NET_TO_HOST_16 (command -> sendUnsequenced.unsequencedGroup);
    index = unsequencedGroup % PENET_PEER_UNSEQUENCED_WINDOW_SIZE;

    if (unsequencedGroup < peer -> incomingUnsequencedGroup)
      unsequencedGroup += 0x10000;

    if (unsequencedGroup >= (penet_uint32) peer -> incomingUnsequencedGroup + PENET_PEER_FREE_UNSEQUENCED_WINDOWS * PENET_PEER_UNSEQUENCED_WINDOW_SIZE)
      return 0;

    unsequencedGroup &= 0xFFFF;

    if (unsequencedGroup - index != peer -> incomingUnsequencedGroup)
    {
        peer -> incomingUnsequencedGroup = unsequencedGroup - index;

        memset (peer -> unsequencedWindow, 0, sizeof (peer -> unsequencedWindow));
    }
    else
    if (peer -> unsequencedWindow [index / 32] & (1 << (index % 32)))
      return 0;

    if (penet_peer_queue_incoming_command (peer, command, (const penet_uint8 *) command + sizeof (PENetProtocolSendUnsequenced), dataLength, PENET_PACKET_FLAG_UNSEQUENCED, 0) == NULL)
      return -1;

    peer -> unsequencedWindow [index / 32] |= 1 << (index % 32);

    return 0;
}

static int
penet_protocol_handle_send_unreliable (PENetHost * host, PENetPeer * peer, const PENetProtocol * command, penet_uint8 ** currentData)
{
    size_t dataLength;

    if (command -> header.channelID >= peer -> channelCount ||
        (peer -> state != PENET_PEER_STATE_CONNECTED && peer -> state != PENET_PEER_STATE_DISCONNECT_LATER))
      return -1;

    dataLength = PENET_NET_TO_HOST_16 (command -> sendUnreliable.dataLength);
    * currentData += dataLength;
    if (dataLength > host -> maximumPacketSize ||
        * currentData < host -> receivedData ||
        * currentData > & host -> receivedData [host -> receivedDataLength])
      return -1;

    if (penet_peer_queue_incoming_command (peer, command, (const penet_uint8 *) command + sizeof (PENetProtocolSendUnreliable), dataLength, 0, 0) == NULL)
      return -1;

    return 0;
}

static int
penet_protocol_handle_send_fragment (PENetHost * host, PENetPeer * peer, const PENetProtocol * command, penet_uint8 ** currentData)
{
    penet_uint32 fragmentNumber,
           fragmentCount,
           fragmentOffset,
           fragmentLength,
           startSequenceNumber,
           totalLength;
    PENetChannel * channel;
    penet_uint16 startWindow, currentWindow;
    PENetListIterator currentCommand;
    PENetIncomingCommand * startCommand = NULL;

    if (command -> header.channelID >= peer -> channelCount ||
        (peer -> state != PENET_PEER_STATE_CONNECTED && peer -> state != PENET_PEER_STATE_DISCONNECT_LATER))
      return -1;

    fragmentLength = PENET_NET_TO_HOST_16 (command -> sendFragment.dataLength);
    * currentData += fragmentLength;
    if (fragmentLength > host -> maximumPacketSize ||
        * currentData < host -> receivedData ||
        * currentData > & host -> receivedData [host -> receivedDataLength])
      return -1;

    channel = & peer -> channels [command -> header.channelID];
    startSequenceNumber = PENET_NET_TO_HOST_16 (command -> sendFragment.startSequenceNumber);
    startWindow = startSequenceNumber / PENET_PEER_RELIABLE_WINDOW_SIZE;
    currentWindow = channel -> incomingReliableSequenceNumber / PENET_PEER_RELIABLE_WINDOW_SIZE;

    if (startSequenceNumber < channel -> incomingReliableSequenceNumber)
      startWindow += PENET_PEER_RELIABLE_WINDOWS;

    if (startWindow < currentWindow || startWindow >= currentWindow + PENET_PEER_FREE_RELIABLE_WINDOWS - 1)
      return 0;

    fragmentNumber = PENET_NET_TO_HOST_32 (command -> sendFragment.fragmentNumber);
    fragmentCount = PENET_NET_TO_HOST_32 (command -> sendFragment.fragmentCount);
    fragmentOffset = PENET_NET_TO_HOST_32 (command -> sendFragment.fragmentOffset);
    totalLength = PENET_NET_TO_HOST_32 (command -> sendFragment.totalLength);

    if (fragmentCount > PENET_PROTOCOL_MAXIMUM_FRAGMENT_COUNT ||
        fragmentNumber >= fragmentCount ||
        totalLength > host -> maximumPacketSize ||
        fragmentOffset >= totalLength ||
        fragmentLength > totalLength - fragmentOffset)
      return -1;

    for (currentCommand = penet_list_previous (penet_list_end (& channel -> incomingReliableCommands));
         currentCommand != penet_list_end (& channel -> incomingReliableCommands);
         currentCommand = penet_list_previous (currentCommand))
    {
       PENetIncomingCommand * incomingCommand = (PENetIncomingCommand *) currentCommand;

       if (startSequenceNumber >= channel -> incomingReliableSequenceNumber)
       {
          if (incomingCommand -> reliableSequenceNumber < channel -> incomingReliableSequenceNumber)
            continue;
       }
       else
       if (incomingCommand -> reliableSequenceNumber >= channel -> incomingReliableSequenceNumber)
         break;

       if (incomingCommand -> reliableSequenceNumber <= startSequenceNumber)
       {
          if (incomingCommand -> reliableSequenceNumber < startSequenceNumber)
            break;

          if ((incomingCommand -> command.header.command & PENET_PROTOCOL_COMMAND_MASK) != PENET_PROTOCOL_COMMAND_SEND_FRAGMENT ||
              totalLength != incomingCommand -> packet -> dataLength ||
              fragmentCount != incomingCommand -> fragmentCount)
            return -1;

          startCommand = incomingCommand;
          break;
       }
    }

    if (startCommand == NULL)
    {
       PENetProtocol hostCommand = * command;

       hostCommand.header.reliableSequenceNumber = startSequenceNumber;

       startCommand = penet_peer_queue_incoming_command (peer, & hostCommand, NULL, totalLength, PENET_PACKET_FLAG_RELIABLE, fragmentCount);
       if (startCommand == NULL)
         return -1;
    }

    if ((startCommand -> fragments [fragmentNumber / 32] & (1 << (fragmentNumber % 32))) == 0)
    {
       -- startCommand -> fragmentsRemaining;

       startCommand -> fragments [fragmentNumber / 32] |= (1 << (fragmentNumber % 32));

       if (fragmentOffset + fragmentLength > startCommand -> packet -> dataLength)
         fragmentLength = startCommand -> packet -> dataLength - fragmentOffset;

       memcpy (startCommand -> packet -> data + fragmentOffset,
               (penet_uint8 *) command + sizeof (PENetProtocolSendFragment),
               fragmentLength);

        if (startCommand -> fragmentsRemaining <= 0)
          penet_peer_dispatch_incoming_reliable_commands (peer, channel);
    }

    return 0;
}

static int
penet_protocol_handle_send_unreliable_fragment (PENetHost * host, PENetPeer * peer, const PENetProtocol * command, penet_uint8 ** currentData)
{
    penet_uint32 fragmentNumber,
           fragmentCount,
           fragmentOffset,
           fragmentLength,
           reliableSequenceNumber,
           startSequenceNumber,
           totalLength;
    penet_uint16 reliableWindow, currentWindow;
    PENetChannel * channel;
    PENetListIterator currentCommand;
    PENetIncomingCommand * startCommand = NULL;

    if (command -> header.channelID >= peer -> channelCount ||
        (peer -> state != PENET_PEER_STATE_CONNECTED && peer -> state != PENET_PEER_STATE_DISCONNECT_LATER))
      return -1;

    fragmentLength = PENET_NET_TO_HOST_16 (command -> sendFragment.dataLength);
    * currentData += fragmentLength;
    if (fragmentLength > host -> maximumPacketSize ||
        * currentData < host -> receivedData ||
        * currentData > & host -> receivedData [host -> receivedDataLength])
      return -1;

    channel = & peer -> channels [command -> header.channelID];
    reliableSequenceNumber = command -> header.reliableSequenceNumber;
    startSequenceNumber = PENET_NET_TO_HOST_16 (command -> sendFragment.startSequenceNumber);

    reliableWindow = reliableSequenceNumber / PENET_PEER_RELIABLE_WINDOW_SIZE;
    currentWindow = channel -> incomingReliableSequenceNumber / PENET_PEER_RELIABLE_WINDOW_SIZE;

    if (reliableSequenceNumber < channel -> incomingReliableSequenceNumber)
      reliableWindow += PENET_PEER_RELIABLE_WINDOWS;

    if (reliableWindow < currentWindow || reliableWindow >= currentWindow + PENET_PEER_FREE_RELIABLE_WINDOWS - 1)
      return 0;

    if (reliableSequenceNumber == channel -> incomingReliableSequenceNumber &&
        startSequenceNumber <= channel -> incomingUnreliableSequenceNumber)
      return 0;

    fragmentNumber = PENET_NET_TO_HOST_32 (command -> sendFragment.fragmentNumber);
    fragmentCount = PENET_NET_TO_HOST_32 (command -> sendFragment.fragmentCount);
    fragmentOffset = PENET_NET_TO_HOST_32 (command -> sendFragment.fragmentOffset);
    totalLength = PENET_NET_TO_HOST_32 (command -> sendFragment.totalLength);

    if (fragmentCount > PENET_PROTOCOL_MAXIMUM_FRAGMENT_COUNT ||
        fragmentNumber >= fragmentCount ||
        totalLength > host -> maximumPacketSize ||
        fragmentOffset >= totalLength ||
        fragmentLength > totalLength - fragmentOffset)
      return -1;

    for (currentCommand = penet_list_previous (penet_list_end (& channel -> incomingUnreliableCommands));
         currentCommand != penet_list_end (& channel -> incomingUnreliableCommands);
         currentCommand = penet_list_previous (currentCommand))
    {
       PENetIncomingCommand * incomingCommand = (PENetIncomingCommand *) currentCommand;

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

       if (incomingCommand -> unreliableSequenceNumber <= startSequenceNumber)
       {
          if (incomingCommand -> unreliableSequenceNumber < startSequenceNumber)
            break;

          if ((incomingCommand -> command.header.command & PENET_PROTOCOL_COMMAND_MASK) != PENET_PROTOCOL_COMMAND_SEND_UNRELIABLE_FRAGMENT ||
              totalLength != incomingCommand -> packet -> dataLength ||
              fragmentCount != incomingCommand -> fragmentCount)
            return -1;

          startCommand = incomingCommand;
          break;
       }
    }

    if (startCommand == NULL)
    {
       startCommand = penet_peer_queue_incoming_command (peer, command, NULL, totalLength, PENET_PACKET_FLAG_UNRELIABLE_FRAGMENT, fragmentCount);
       if (startCommand == NULL)
         return -1;
    }

    if ((startCommand -> fragments [fragmentNumber / 32] & (1 << (fragmentNumber % 32))) == 0)
    {
       -- startCommand -> fragmentsRemaining;

       startCommand -> fragments [fragmentNumber / 32] |= (1 << (fragmentNumber % 32));

       if (fragmentOffset + fragmentLength > startCommand -> packet -> dataLength)
         fragmentLength = startCommand -> packet -> dataLength - fragmentOffset;

       memcpy (startCommand -> packet -> data + fragmentOffset,
               (penet_uint8 *) command + sizeof (PENetProtocolSendFragment),
               fragmentLength);

        if (startCommand -> fragmentsRemaining <= 0)
          penet_peer_dispatch_incoming_unreliable_commands (peer, channel);
    }

    return 0;
}

static int
penet_protocol_handle_ping (PENetHost * host, PENetPeer * peer, const PENetProtocol * command)
{
    if (peer -> state != PENET_PEER_STATE_CONNECTED && peer -> state != PENET_PEER_STATE_DISCONNECT_LATER)
      return -1;

    return 0;
}

static int
penet_protocol_handle_bandwidth_limit (PENetHost * host, PENetPeer * peer, const PENetProtocol * command)
{
    if (peer -> state != PENET_PEER_STATE_CONNECTED && peer -> state != PENET_PEER_STATE_DISCONNECT_LATER)
      return -1;

    if (peer -> incomingBandwidth != 0)
      -- host -> bandwidthLimitedPeers;

    peer -> incomingBandwidth = PENET_NET_TO_HOST_32 (command -> bandwidthLimit.incomingBandwidth);
    peer -> outgoingBandwidth = PENET_NET_TO_HOST_32 (command -> bandwidthLimit.outgoingBandwidth);

    if (peer -> incomingBandwidth != 0)
      ++ host -> bandwidthLimitedPeers;

    if (peer -> incomingBandwidth == 0 && host -> outgoingBandwidth == 0)
      peer -> windowSize = PENET_PROTOCOL_MAXIMUM_WINDOW_SIZE;
    else
    if (peer -> incomingBandwidth == 0 || host -> outgoingBandwidth == 0)
      peer -> windowSize = (PENET_MAX (peer -> incomingBandwidth, host -> outgoingBandwidth) /
                             PENET_PEER_WINDOW_SIZE_SCALE) * PENET_PROTOCOL_MINIMUM_WINDOW_SIZE;
    else
      peer -> windowSize = (PENET_MIN (peer -> incomingBandwidth, host -> outgoingBandwidth) /
                             PENET_PEER_WINDOW_SIZE_SCALE) * PENET_PROTOCOL_MINIMUM_WINDOW_SIZE;

    if (peer -> windowSize < PENET_PROTOCOL_MINIMUM_WINDOW_SIZE)
      peer -> windowSize = PENET_PROTOCOL_MINIMUM_WINDOW_SIZE;
    else
    if (peer -> windowSize > PENET_PROTOCOL_MAXIMUM_WINDOW_SIZE)
      peer -> windowSize = PENET_PROTOCOL_MAXIMUM_WINDOW_SIZE;

    return 0;
}

static int
penet_protocol_handle_throttle_configure (PENetHost * host, PENetPeer * peer, const PENetProtocol * command)
{
    if (peer -> state != PENET_PEER_STATE_CONNECTED && peer -> state != PENET_PEER_STATE_DISCONNECT_LATER)
      return -1;

    peer -> packetThrottleInterval = PENET_NET_TO_HOST_32 (command -> throttleConfigure.packetThrottleInterval);
    peer -> packetThrottleAcceleration = PENET_NET_TO_HOST_32 (command -> throttleConfigure.packetThrottleAcceleration);
    peer -> packetThrottleDeceleration = PENET_NET_TO_HOST_32 (command -> throttleConfigure.packetThrottleDeceleration);

    return 0;
}

static int
penet_protocol_handle_disconnect (PENetHost * host, PENetPeer * peer, const PENetProtocol * command)
{
    if (peer -> state == PENET_PEER_STATE_DISCONNECTED || peer -> state == PENET_PEER_STATE_ZOMBIE || peer -> state == PENET_PEER_STATE_ACKNOWLEDGING_DISCONNECT)
      return 0;

    penet_peer_reset_queues (peer);

    if (peer -> state == PENET_PEER_STATE_CONNECTION_SUCCEEDED || peer -> state == PENET_PEER_STATE_DISCONNECTING || peer -> state == PENET_PEER_STATE_CONNECTING)
        penet_protocol_dispatch_state (host, peer, PENET_PEER_STATE_ZOMBIE);
    else
    if (peer -> state != PENET_PEER_STATE_CONNECTED && peer -> state != PENET_PEER_STATE_DISCONNECT_LATER)
    {
        if (peer -> state == PENET_PEER_STATE_CONNECTION_PENDING) host -> recalculateBandwidthLimits = 1;

        penet_peer_reset (peer);
    }
    else
    if (command -> header.command & PENET_PROTOCOL_COMMAND_FLAG_ACKNOWLEDGE)
      penet_protocol_change_state (host, peer, PENET_PEER_STATE_ACKNOWLEDGING_DISCONNECT);
    else
      penet_protocol_dispatch_state (host, peer, PENET_PEER_STATE_ZOMBIE);

    if (peer -> state != PENET_PEER_STATE_DISCONNECTED)
      peer -> eventData = PENET_NET_TO_HOST_32 (command -> disconnect.data);

    return 0;
}

static int
penet_protocol_handle_acknowledge (PENetHost * host, PENetEvent * event, PENetPeer * peer, const PENetProtocol * command)
{
    penet_uint32 roundTripTime,
           receivedSentTime,
           receivedReliableSequenceNumber;
    PENetProtocolCommand commandNumber;

    if (peer -> state == PENET_PEER_STATE_DISCONNECTED || peer -> state == PENET_PEER_STATE_ZOMBIE)
      return 0;

    receivedSentTime = PENET_NET_TO_HOST_16 (command -> acknowledge.receivedSentTime);
    receivedSentTime |= host -> serviceTime & 0xFFFF0000;
    if ((receivedSentTime & 0x8000) > (host -> serviceTime & 0x8000))
        receivedSentTime -= 0x10000;

    if (PENET_TIME_LESS (host -> serviceTime, receivedSentTime))
      return 0;

    peer -> lastReceiveTime = host -> serviceTime;
    peer -> earliestTimeout = 0;

    roundTripTime = PENET_TIME_DIFFERENCE (host -> serviceTime, receivedSentTime);

    penet_peer_throttle (peer, roundTripTime);

    peer -> roundTripTimeVariance -= peer -> roundTripTimeVariance / 4;

    if (roundTripTime >= peer -> roundTripTime)
    {
       peer -> roundTripTime += (roundTripTime - peer -> roundTripTime) / 8;
       peer -> roundTripTimeVariance += (roundTripTime - peer -> roundTripTime) / 4;
    }
    else
    {
       peer -> roundTripTime -= (peer -> roundTripTime - roundTripTime) / 8;
       peer -> roundTripTimeVariance += (peer -> roundTripTime - roundTripTime) / 4;
    }

    if (peer -> roundTripTime < peer -> lowestRoundTripTime)
      peer -> lowestRoundTripTime = peer -> roundTripTime;

    if (peer -> roundTripTimeVariance > peer -> highestRoundTripTimeVariance)
      peer -> highestRoundTripTimeVariance = peer -> roundTripTimeVariance;

    if (peer -> packetThrottleEpoch == 0 ||
        PENET_TIME_DIFFERENCE (host -> serviceTime, peer -> packetThrottleEpoch) >= peer -> packetThrottleInterval)
    {
        peer -> lastRoundTripTime = peer -> lowestRoundTripTime;
        peer -> lastRoundTripTimeVariance = peer -> highestRoundTripTimeVariance;
        peer -> lowestRoundTripTime = peer -> roundTripTime;
        peer -> highestRoundTripTimeVariance = peer -> roundTripTimeVariance;
        peer -> packetThrottleEpoch = host -> serviceTime;
    }

    receivedReliableSequenceNumber = PENET_NET_TO_HOST_16 (command -> acknowledge.receivedReliableSequenceNumber);

    commandNumber = penet_protocol_remove_sent_reliable_command (peer, receivedReliableSequenceNumber, command -> header.channelID);

    switch (peer -> state)
    {
    case PENET_PEER_STATE_ACKNOWLEDGING_CONNECT:
       if (commandNumber != PENET_PROTOCOL_COMMAND_VERIFY_CONNECT)
         return -1;

       penet_protocol_notify_connect (host, peer, event);
       break;

    case PENET_PEER_STATE_DISCONNECTING:
       if (commandNumber != PENET_PROTOCOL_COMMAND_DISCONNECT)
         return -1;

       penet_protocol_notify_disconnect (host, peer, event);
       break;

    case PENET_PEER_STATE_DISCONNECT_LATER:
       if (penet_list_empty (& peer -> outgoingReliableCommands) &&
           penet_list_empty (& peer -> outgoingUnreliableCommands) &&
           penet_list_empty (& peer -> sentReliableCommands))
         penet_peer_disconnect (peer, peer -> eventData);
       break;

    default:
       break;
    }

    return 0;
}

static int
penet_protocol_handle_verify_connect (PENetHost * host, PENetEvent * event, PENetPeer * peer, const PENetProtocol * command)
{
    penet_uint32 mtu, windowSize;
    size_t channelCount;

    if (peer -> state != PENET_PEER_STATE_CONNECTING)
      return 0;

    channelCount = PENET_NET_TO_HOST_32 (command -> verifyConnect.channelCount);

    if (channelCount < PENET_PROTOCOL_MINIMUM_CHANNEL_COUNT || channelCount > PENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT ||
        PENET_NET_TO_HOST_32 (command -> verifyConnect.packetThrottleInterval) != peer -> packetThrottleInterval ||
        PENET_NET_TO_HOST_32 (command -> verifyConnect.packetThrottleAcceleration) != peer -> packetThrottleAcceleration ||
        PENET_NET_TO_HOST_32 (command -> verifyConnect.packetThrottleDeceleration) != peer -> packetThrottleDeceleration ||
        command -> verifyConnect.connectID != peer -> connectID)
    {
        peer -> eventData = 0;

        penet_protocol_dispatch_state (host, peer, PENET_PEER_STATE_ZOMBIE);

        return -1;
    }

    penet_protocol_remove_sent_reliable_command (peer, 1, 0xFF);

    if (channelCount < peer -> channelCount)
      peer -> channelCount = channelCount;

    peer -> outgoingPeerID = PENET_NET_TO_HOST_16 (command -> verifyConnect.outgoingPeerID);
    peer -> incomingSessionID = command -> verifyConnect.incomingSessionID;
    peer -> outgoingSessionID = command -> verifyConnect.outgoingSessionID;

    mtu = PENET_NET_TO_HOST_32 (command -> verifyConnect.mtu);

    if (mtu < PENET_PROTOCOL_MINIMUM_MTU)
      mtu = PENET_PROTOCOL_MINIMUM_MTU;
    else
    if (mtu > PENET_PROTOCOL_MAXIMUM_MTU)
      mtu = PENET_PROTOCOL_MAXIMUM_MTU;

    if (mtu < peer -> mtu)
      peer -> mtu = mtu;

    windowSize = PENET_NET_TO_HOST_32 (command -> verifyConnect.windowSize);

    if (windowSize < PENET_PROTOCOL_MINIMUM_WINDOW_SIZE)
      windowSize = PENET_PROTOCOL_MINIMUM_WINDOW_SIZE;

    if (windowSize > PENET_PROTOCOL_MAXIMUM_WINDOW_SIZE)
      windowSize = PENET_PROTOCOL_MAXIMUM_WINDOW_SIZE;

    if (windowSize < peer -> windowSize)
      peer -> windowSize = windowSize;

    peer -> incomingBandwidth = PENET_NET_TO_HOST_32 (command -> verifyConnect.incomingBandwidth);
    peer -> outgoingBandwidth = PENET_NET_TO_HOST_32 (command -> verifyConnect.outgoingBandwidth);

    penet_protocol_notify_connect (host, peer, event);
    return 0;
}

static int
penet_protocol_handle_incoming_commands (PENetHost * host, PENetEvent * event)
{
    PENetProtocolHeader * header;
    PENetProtocol * command;
    PENetPeer * peer;
    penet_uint8 * currentData;
    size_t headerSize;
    penet_uint16 peerID, flags;
    penet_uint8 sessionID;

    if (host -> receivedDataLength < (size_t) & ((PENetProtocolHeader *) 0) -> sentTime)
      return 0;

    header = (PENetProtocolHeader *) host -> receivedData;

    peerID = PENET_NET_TO_HOST_16 (header -> peerID);
    sessionID = (peerID & PENET_PROTOCOL_HEADER_SESSION_MASK) >> PENET_PROTOCOL_HEADER_SESSION_SHIFT;
    flags = peerID & PENET_PROTOCOL_HEADER_FLAG_MASK;
    peerID &= ~ (PENET_PROTOCOL_HEADER_FLAG_MASK | PENET_PROTOCOL_HEADER_SESSION_MASK);

    headerSize = (flags & PENET_PROTOCOL_HEADER_FLAG_SENT_TIME ? sizeof (PENetProtocolHeader) : (size_t) & ((PENetProtocolHeader *) 0) -> sentTime);
    if (host -> checksum != NULL)
      headerSize += sizeof (penet_uint32);

    if (peerID == PENET_PROTOCOL_MAXIMUM_PEER_ID)
      peer = NULL;
    else
    if (peerID >= host -> peerCount)
      return 0;
    else
    {
       peer = & host -> peers [peerID];

       if (peer -> state == PENET_PEER_STATE_DISCONNECTED ||
           peer -> state == PENET_PEER_STATE_ZOMBIE ||
           ((host -> receivedAddress.host != peer -> address.host ||
             host -> receivedAddress.port != peer -> address.port) &&
             peer -> address.host != PENET_HOST_BROADCAST) ||
           (peer -> outgoingPeerID < PENET_PROTOCOL_MAXIMUM_PEER_ID &&
            sessionID != peer -> incomingSessionID))
         return 0;
    }

    if (flags & PENET_PROTOCOL_HEADER_FLAG_COMPRESSED)
    {
        size_t originalSize;
        if (host -> compressor.context == NULL || host -> compressor.decompress == NULL)
          return 0;

        originalSize = host -> compressor.decompress (host -> compressor.context,
                                    host -> receivedData + headerSize,
                                    host -> receivedDataLength - headerSize,
                                    host -> packetData [1] + headerSize,
                                    sizeof (host -> packetData [1]) - headerSize);
        if (originalSize <= 0 || originalSize > sizeof (host -> packetData [1]) - headerSize)
          return 0;

        memcpy (host -> packetData [1], header, headerSize);
        host -> receivedData = host -> packetData [1];
        host -> receivedDataLength = headerSize + originalSize;
    }

    if (host -> checksum != NULL)
    {
        penet_uint32 * checksum = (penet_uint32 *) & host -> receivedData [headerSize - sizeof (penet_uint32)],
                    desiredChecksum = * checksum;
        PENetBuffer buffer;

        * checksum = peer != NULL ? peer -> connectID : 0;

        buffer.data = host -> receivedData;
        buffer.dataLength = host -> receivedDataLength;

        if (host -> checksum (& buffer, 1) != desiredChecksum)
          return 0;
    }

    if (peer != NULL)
    {
       peer -> address.host = host -> receivedAddress.host;
       peer -> address.port = host -> receivedAddress.port;
       peer -> incomingDataTotal += host -> receivedDataLength;
    }

    currentData = host -> receivedData + headerSize;

    while (currentData < & host -> receivedData [host -> receivedDataLength])
    {
       penet_uint8 commandNumber;
       size_t commandSize;

       command = (PENetProtocol *) currentData;

       if (currentData + sizeof (PENetProtocolCommandHeader) > & host -> receivedData [host -> receivedDataLength])
         break;

       commandNumber = command -> header.command & PENET_PROTOCOL_COMMAND_MASK;
       if (commandNumber >= PENET_PROTOCOL_COMMAND_COUNT)
         break;

       commandSize = commandSizes [commandNumber];
       if (commandSize == 0 || currentData + commandSize > & host -> receivedData [host -> receivedDataLength])
         break;

       currentData += commandSize;

       if (peer == NULL && commandNumber != PENET_PROTOCOL_COMMAND_CONNECT)
         break;

       command -> header.reliableSequenceNumber = PENET_NET_TO_HOST_16 (command -> header.reliableSequenceNumber);

       switch (commandNumber)
       {
       case PENET_PROTOCOL_COMMAND_ACKNOWLEDGE:
          if (penet_protocol_handle_acknowledge (host, event, peer, command))
            goto commandError;
          break;

       case PENET_PROTOCOL_COMMAND_CONNECT:
          if (peer != NULL)
            goto commandError;
          peer = penet_protocol_handle_connect (host, header, command);
          if (peer == NULL)
            goto commandError;
          break;

       case PENET_PROTOCOL_COMMAND_VERIFY_CONNECT:
          if (penet_protocol_handle_verify_connect (host, event, peer, command))
            goto commandError;
          break;

       case PENET_PROTOCOL_COMMAND_DISCONNECT:
          if (penet_protocol_handle_disconnect (host, peer, command))
            goto commandError;
          break;

       case PENET_PROTOCOL_COMMAND_PING:
          if (penet_protocol_handle_ping (host, peer, command))
            goto commandError;
          break;

       case PENET_PROTOCOL_COMMAND_SEND_RELIABLE:
          if (penet_protocol_handle_send_reliable (host, peer, command, & currentData))
            goto commandError;
          break;

       case PENET_PROTOCOL_COMMAND_SEND_UNRELIABLE:
          if (penet_protocol_handle_send_unreliable (host, peer, command, & currentData))
            goto commandError;
          break;

       case PENET_PROTOCOL_COMMAND_SEND_UNSEQUENCED:
          if (penet_protocol_handle_send_unsequenced (host, peer, command, & currentData))
            goto commandError;
          break;

       case PENET_PROTOCOL_COMMAND_SEND_FRAGMENT:
          if (penet_protocol_handle_send_fragment (host, peer, command, & currentData))
            goto commandError;
          break;

       case PENET_PROTOCOL_COMMAND_BANDWIDTH_LIMIT:
          if (penet_protocol_handle_bandwidth_limit (host, peer, command))
            goto commandError;
          break;

       case PENET_PROTOCOL_COMMAND_THROTTLE_CONFIGURE:
          if (penet_protocol_handle_throttle_configure (host, peer, command))
            goto commandError;
          break;

       case PENET_PROTOCOL_COMMAND_SEND_UNRELIABLE_FRAGMENT:
          if (penet_protocol_handle_send_unreliable_fragment (host, peer, command, & currentData))
            goto commandError;
          break;

       default:
          goto commandError;
       }

       if (peer != NULL &&
           (command -> header.command & PENET_PROTOCOL_COMMAND_FLAG_ACKNOWLEDGE) != 0)
       {
           penet_uint16 sentTime;

           if (! (flags & PENET_PROTOCOL_HEADER_FLAG_SENT_TIME))
             break;

           sentTime = PENET_NET_TO_HOST_16 (header -> sentTime);

           switch (peer -> state)
           {
           case PENET_PEER_STATE_DISCONNECTING:
           case PENET_PEER_STATE_ACKNOWLEDGING_CONNECT:
           case PENET_PEER_STATE_DISCONNECTED:
           case PENET_PEER_STATE_ZOMBIE:
              break;

           case PENET_PEER_STATE_ACKNOWLEDGING_DISCONNECT:
              if ((command -> header.command & PENET_PROTOCOL_COMMAND_MASK) == PENET_PROTOCOL_COMMAND_DISCONNECT)
                penet_peer_queue_acknowledgement (peer, command, sentTime);
              break;

           default:
              penet_peer_queue_acknowledgement (peer, command, sentTime);
              break;
           }
       }
    }

commandError:
    if (event != NULL && event -> type != PENET_EVENT_TYPE_NONE)
      return 1;

    return 0;
}

static int
penet_protocol_receive_incoming_commands (PENetHost * host, PENetEvent * event)
{
    int packets;

    for (packets = 0; packets < 256; ++ packets)
    {
       int receivedLength;
       PENetBuffer buffer;

       buffer.data = host -> packetData [0];
       buffer.dataLength = sizeof (host -> packetData [0]);

       receivedLength = penet_socket_receive (host -> socket,
                                             & host -> receivedAddress,
                                             & buffer,
                                             1);

       if (receivedLength < 0)
         return -1;

       if (receivedLength == 0)
         return 0;

       host -> receivedData = host -> packetData [0];
       host -> receivedDataLength = receivedLength;

       host -> totalReceivedData += receivedLength;
       host -> totalReceivedPackets ++;

       if (host -> intercept != NULL)
       {
          switch (host -> intercept (host, event))
          {
          case 1:
             if (event != NULL && event -> type != PENET_EVENT_TYPE_NONE)
               return 1;

             continue;

          case -1:
             return -1;

          default:
             break;
          }
       }

       switch (penet_protocol_handle_incoming_commands (host, event))
       {
       case 1:
          return 1;

       case -1:
          return -1;

       default:
          break;
       }
    }

    return -1;
}

static void
penet_protocol_send_acknowledgements (PENetHost * host, PENetPeer * peer)
{
    PENetProtocol * command = & host -> commands [host -> commandCount];
    PENetBuffer * buffer = & host -> buffers [host -> bufferCount];
    PENetAcknowledgement * acknowledgement;
    PENetListIterator currentAcknowledgement;
    penet_uint16 reliableSequenceNumber;

    currentAcknowledgement = penet_list_begin (& peer -> acknowledgements);

    while (currentAcknowledgement != penet_list_end (& peer -> acknowledgements))
    {
       if (command >= & host -> commands [sizeof (host -> commands) / sizeof (PENetProtocol)] ||
           buffer >= & host -> buffers [sizeof (host -> buffers) / sizeof (PENetBuffer)] ||
           peer -> mtu - host -> packetSize < sizeof (PENetProtocolAcknowledge))
       {
          host -> continueSending = 1;

          break;
       }

       acknowledgement = (PENetAcknowledgement *) currentAcknowledgement;

       currentAcknowledgement = penet_list_next (currentAcknowledgement);

       buffer -> data = command;
       buffer -> dataLength = sizeof (PENetProtocolAcknowledge);

       host -> packetSize += buffer -> dataLength;

       reliableSequenceNumber = PENET_HOST_TO_NET_16 (acknowledgement -> command.header.reliableSequenceNumber);

       command -> header.command = PENET_PROTOCOL_COMMAND_ACKNOWLEDGE;
       command -> header.channelID = acknowledgement -> command.header.channelID;
       command -> header.reliableSequenceNumber = reliableSequenceNumber;
       command -> acknowledge.receivedReliableSequenceNumber = reliableSequenceNumber;
       command -> acknowledge.receivedSentTime = PENET_HOST_TO_NET_16 (acknowledgement -> sentTime);

       if ((acknowledgement -> command.header.command & PENET_PROTOCOL_COMMAND_MASK) == PENET_PROTOCOL_COMMAND_DISCONNECT)
         penet_protocol_dispatch_state (host, peer, PENET_PEER_STATE_ZOMBIE);

       penet_list_remove (& acknowledgement -> acknowledgementList);
       penet_free (acknowledgement);

       ++ command;
       ++ buffer;
    }

    host -> commandCount = command - host -> commands;
    host -> bufferCount = buffer - host -> buffers;
}

static void
penet_protocol_send_unreliable_outgoing_commands (PENetHost * host, PENetPeer * peer)
{
    PENetProtocol * command = & host -> commands [host -> commandCount];
    PENetBuffer * buffer = & host -> buffers [host -> bufferCount];
    PENetOutgoingCommand * outgoingCommand;
    PENetListIterator currentCommand;

    currentCommand = penet_list_begin (& peer -> outgoingUnreliableCommands);

    while (currentCommand != penet_list_end (& peer -> outgoingUnreliableCommands))
    {
       size_t commandSize;

       outgoingCommand = (PENetOutgoingCommand *) currentCommand;
       commandSize = commandSizes [outgoingCommand -> command.header.command & PENET_PROTOCOL_COMMAND_MASK];

       if (command >= & host -> commands [sizeof (host -> commands) / sizeof (PENetProtocol)] ||
           buffer + 1 >= & host -> buffers [sizeof (host -> buffers) / sizeof (PENetBuffer)] ||
           peer -> mtu - host -> packetSize < commandSize ||
           (outgoingCommand -> packet != NULL &&
             peer -> mtu - host -> packetSize < commandSize + outgoingCommand -> fragmentLength))
       {
          host -> continueSending = 1;

          break;
       }

       currentCommand = penet_list_next (currentCommand);

       if (outgoingCommand -> packet != NULL && outgoingCommand -> fragmentOffset == 0)
       {
          peer -> packetThrottleCounter += PENET_PEER_PACKET_THROTTLE_COUNTER;
          peer -> packetThrottleCounter %= PENET_PEER_PACKET_THROTTLE_SCALE;

          if (peer -> packetThrottleCounter > peer -> packetThrottle)
          {
             penet_uint16 reliableSequenceNumber = outgoingCommand -> reliableSequenceNumber,
                         unreliableSequenceNumber = outgoingCommand -> unreliableSequenceNumber;
             for (;;)
             {
                -- outgoingCommand -> packet -> referenceCount;

                if (outgoingCommand -> packet -> referenceCount == 0)
                  penet_packet_destroy (outgoingCommand -> packet);

                penet_list_remove (& outgoingCommand -> outgoingCommandList);
                penet_free (outgoingCommand);

                if (currentCommand == penet_list_end (& peer -> outgoingUnreliableCommands))
                  break;

                outgoingCommand = (PENetOutgoingCommand *) currentCommand;
                if (outgoingCommand -> reliableSequenceNumber != reliableSequenceNumber ||
                    outgoingCommand -> unreliableSequenceNumber != unreliableSequenceNumber)
                  break;

                currentCommand = penet_list_next (currentCommand);
             }

             continue;
          }
       }

       buffer -> data = command;
       buffer -> dataLength = commandSize;

       host -> packetSize += buffer -> dataLength;

       * command = outgoingCommand -> command;

       penet_list_remove (& outgoingCommand -> outgoingCommandList);

       if (outgoingCommand -> packet != NULL)
       {
          ++ buffer;

          buffer -> data = outgoingCommand -> packet -> data + outgoingCommand -> fragmentOffset;
          buffer -> dataLength = outgoingCommand -> fragmentLength;

          host -> packetSize += buffer -> dataLength;

          penet_list_insert (penet_list_end (& peer -> sentUnreliableCommands), outgoingCommand);
       }
       else
         penet_free (outgoingCommand);

       ++ command;
       ++ buffer;
    }

    host -> commandCount = command - host -> commands;
    host -> bufferCount = buffer - host -> buffers;

    if (peer -> state == PENET_PEER_STATE_DISCONNECT_LATER &&
        penet_list_empty (& peer -> outgoingReliableCommands) &&
        penet_list_empty (& peer -> outgoingUnreliableCommands) &&
        penet_list_empty (& peer -> sentReliableCommands))
      penet_peer_disconnect (peer, peer -> eventData);
}

static int
penet_protocol_check_timeouts (PENetHost * host, PENetPeer * peer, PENetEvent * event)
{
    PENetOutgoingCommand * outgoingCommand;
    PENetListIterator currentCommand, insertPosition;

    currentCommand = penet_list_begin (& peer -> sentReliableCommands);
    insertPosition = penet_list_begin (& peer -> outgoingReliableCommands);

    while (currentCommand != penet_list_end (& peer -> sentReliableCommands))
    {
       outgoingCommand = (PENetOutgoingCommand *) currentCommand;

       currentCommand = penet_list_next (currentCommand);

       if (PENET_TIME_DIFFERENCE (host -> serviceTime, outgoingCommand -> sentTime) < outgoingCommand -> roundTripTimeout)
         continue;

       if (peer -> earliestTimeout == 0 ||
           PENET_TIME_LESS (outgoingCommand -> sentTime, peer -> earliestTimeout))
         peer -> earliestTimeout = outgoingCommand -> sentTime;

       if (peer -> earliestTimeout != 0 &&
             (PENET_TIME_DIFFERENCE (host -> serviceTime, peer -> earliestTimeout) >= peer -> timeoutMaximum ||
               (outgoingCommand -> roundTripTimeout >= outgoingCommand -> roundTripTimeoutLimit &&
                 PENET_TIME_DIFFERENCE (host -> serviceTime, peer -> earliestTimeout) >= peer -> timeoutMinimum)))
       {
          penet_protocol_notify_disconnect (host, peer, event);

          return 1;
       }

       if (outgoingCommand -> packet != NULL)
         peer -> reliableDataInTransit -= outgoingCommand -> fragmentLength;

       ++ peer -> packetsLost;

       outgoingCommand -> roundTripTimeout *= 2;

       penet_list_insert (insertPosition, penet_list_remove (& outgoingCommand -> outgoingCommandList));

       if (currentCommand == penet_list_begin (& peer -> sentReliableCommands) &&
           ! penet_list_empty (& peer -> sentReliableCommands))
       {
          outgoingCommand = (PENetOutgoingCommand *) currentCommand;

          peer -> nextTimeout = outgoingCommand -> sentTime + outgoingCommand -> roundTripTimeout;
       }
    }

    return 0;
}

static int
penet_protocol_send_reliable_outgoing_commands (PENetHost * host, PENetPeer * peer)
{
    PENetProtocol * command = & host -> commands [host -> commandCount];
    PENetBuffer * buffer = & host -> buffers [host -> bufferCount];
    PENetOutgoingCommand * outgoingCommand;
    PENetListIterator currentCommand;
    PENetChannel *channel;
    penet_uint16 reliableWindow;
    size_t commandSize;
    int windowExceeded = 0, windowWrap = 0, canPing = 1;

    currentCommand = penet_list_begin (& peer -> outgoingReliableCommands);

    while (currentCommand != penet_list_end (& peer -> outgoingReliableCommands))
    {
       outgoingCommand = (PENetOutgoingCommand *) currentCommand;

       channel = outgoingCommand -> command.header.channelID < peer -> channelCount ? & peer -> channels [outgoingCommand -> command.header.channelID] : NULL;
       reliableWindow = outgoingCommand -> reliableSequenceNumber / PENET_PEER_RELIABLE_WINDOW_SIZE;
       if (channel != NULL)
       {
           if (! windowWrap &&
               outgoingCommand -> sendAttempts < 1 &&
               ! (outgoingCommand -> reliableSequenceNumber % PENET_PEER_RELIABLE_WINDOW_SIZE) &&
               (channel -> reliableWindows [(reliableWindow + PENET_PEER_RELIABLE_WINDOWS - 1) % PENET_PEER_RELIABLE_WINDOWS] >= PENET_PEER_RELIABLE_WINDOW_SIZE ||
                 channel -> usedReliableWindows & ((((1 << PENET_PEER_FREE_RELIABLE_WINDOWS) - 1) << reliableWindow) |
                   (((1 << PENET_PEER_FREE_RELIABLE_WINDOWS) - 1) >> (PENET_PEER_RELIABLE_WINDOWS - reliableWindow)))))
             windowWrap = 1;
          if (windowWrap)
          {
             currentCommand = penet_list_next (currentCommand);

             continue;
          }
       }

       if (outgoingCommand -> packet != NULL)
       {
          if (! windowExceeded)
          {
             penet_uint32 windowSize = (peer -> packetThrottle * peer -> windowSize) / PENET_PEER_PACKET_THROTTLE_SCALE;

             if (peer -> reliableDataInTransit + outgoingCommand -> fragmentLength > PENET_MAX (windowSize, peer -> mtu))
               windowExceeded = 1;
          }
          if (windowExceeded)
          {
             currentCommand = penet_list_next (currentCommand);

             continue;
          }
       }

       canPing = 0;

       commandSize = commandSizes [outgoingCommand -> command.header.command & PENET_PROTOCOL_COMMAND_MASK];
       if (command >= & host -> commands [sizeof (host -> commands) / sizeof (PENetProtocol)] ||
           buffer + 1 >= & host -> buffers [sizeof (host -> buffers) / sizeof (PENetBuffer)] ||
           peer -> mtu - host -> packetSize < commandSize ||
           (outgoingCommand -> packet != NULL &&
             (penet_uint16) (peer -> mtu - host -> packetSize) < (penet_uint16) (commandSize + outgoingCommand -> fragmentLength)))
       {
          host -> continueSending = 1;

          break;
       }

       currentCommand = penet_list_next (currentCommand);

       if (channel != NULL && outgoingCommand -> sendAttempts < 1)
       {
          channel -> usedReliableWindows |= 1 << reliableWindow;
          ++ channel -> reliableWindows [reliableWindow];
       }

       ++ outgoingCommand -> sendAttempts;

       if (outgoingCommand -> roundTripTimeout == 0)
       {
          outgoingCommand -> roundTripTimeout = peer -> roundTripTime + 4 * peer -> roundTripTimeVariance;
          outgoingCommand -> roundTripTimeoutLimit = peer -> timeoutLimit * outgoingCommand -> roundTripTimeout;
       }

       if (penet_list_empty (& peer -> sentReliableCommands))
         peer -> nextTimeout = host -> serviceTime + outgoingCommand -> roundTripTimeout;

       penet_list_insert (penet_list_end (& peer -> sentReliableCommands),
                         penet_list_remove (& outgoingCommand -> outgoingCommandList));

       outgoingCommand -> sentTime = host -> serviceTime;

       buffer -> data = command;
       buffer -> dataLength = commandSize;

       host -> packetSize += buffer -> dataLength;
       host -> headerFlags |= PENET_PROTOCOL_HEADER_FLAG_SENT_TIME;

       * command = outgoingCommand -> command;

       if (outgoingCommand -> packet != NULL)
       {
          ++ buffer;

          buffer -> data = outgoingCommand -> packet -> data + outgoingCommand -> fragmentOffset;
          buffer -> dataLength = outgoingCommand -> fragmentLength;

          host -> packetSize += outgoingCommand -> fragmentLength;

          peer -> reliableDataInTransit += outgoingCommand -> fragmentLength;
       }

       ++ peer -> packetsSent;

       ++ command;
       ++ buffer;
    }

    host -> commandCount = command - host -> commands;
    host -> bufferCount = buffer - host -> buffers;

    return canPing;
}

static int
penet_protocol_send_outgoing_commands (PENetHost * host, PENetEvent * event, int checkForTimeouts)
{
    penet_uint8 headerData [sizeof (PENetProtocolHeader) + sizeof (penet_uint32)];
    PENetProtocolHeader * header = (PENetProtocolHeader *) headerData;
    PENetPeer * currentPeer;
    int sentLength;
    size_t shouldCompress = 0;

    host -> continueSending = 1;

    while (host -> continueSending)
    for (host -> continueSending = 0,
           currentPeer = host -> peers;
         currentPeer < & host -> peers [host -> peerCount];
         ++ currentPeer)
    {
        if (currentPeer -> state == PENET_PEER_STATE_DISCONNECTED ||
            currentPeer -> state == PENET_PEER_STATE_ZOMBIE)
          continue;

        host -> headerFlags = 0;
        host -> commandCount = 0;
        host -> bufferCount = 1;
        host -> packetSize = sizeof (PENetProtocolHeader);

        if (! penet_list_empty (& currentPeer -> acknowledgements))
          penet_protocol_send_acknowledgements (host, currentPeer);

        if (checkForTimeouts != 0 &&
            ! penet_list_empty (& currentPeer -> sentReliableCommands) &&
            PENET_TIME_GREATER_EQUAL (host -> serviceTime, currentPeer -> nextTimeout) &&
            penet_protocol_check_timeouts (host, currentPeer, event) == 1)
        {
            if (event != NULL && event -> type != PENET_EVENT_TYPE_NONE)
              return 1;
            else
              continue;
        }

        if ((penet_list_empty (& currentPeer -> outgoingReliableCommands) ||
              penet_protocol_send_reliable_outgoing_commands (host, currentPeer)) &&
            penet_list_empty (& currentPeer -> sentReliableCommands) &&
            PENET_TIME_DIFFERENCE (host -> serviceTime, currentPeer -> lastReceiveTime) >= currentPeer -> pingInterval &&
            currentPeer -> mtu - host -> packetSize >= sizeof (PENetProtocolPing))
        {
            penet_peer_ping (currentPeer);
            penet_protocol_send_reliable_outgoing_commands (host, currentPeer);
        }

        if (! penet_list_empty (& currentPeer -> outgoingUnreliableCommands))
          penet_protocol_send_unreliable_outgoing_commands (host, currentPeer);

        if (host -> commandCount == 0)
          continue;

        if (currentPeer -> packetLossEpoch == 0)
          currentPeer -> packetLossEpoch = host -> serviceTime;
        else
        if (PENET_TIME_DIFFERENCE (host -> serviceTime, currentPeer -> packetLossEpoch) >= PENET_PEER_PACKET_LOSS_INTERVAL &&
            currentPeer -> packetsSent > 0)
        {
           penet_uint32 packetLoss = currentPeer -> packetsLost * PENET_PEER_PACKET_LOSS_SCALE / currentPeer -> packetsSent;

#ifdef PENET_DEBUG
           printf ("peer %u: %f%%+-%f%% packet loss, %u+-%u ms round trip time, %f%% throttle, %u/%u outgoing, %u/%u incoming\n", currentPeer -> incomingPeerID, currentPeer -> packetLoss / (float) PENET_PEER_PACKET_LOSS_SCALE, currentPeer -> packetLossVariance / (float) PENET_PEER_PACKET_LOSS_SCALE, currentPeer -> roundTripTime, currentPeer -> roundTripTimeVariance, currentPeer -> packetThrottle / (float) PENET_PEER_PACKET_THROTTLE_SCALE, penet_list_size (& currentPeer -> outgoingReliableCommands), penet_list_size (& currentPeer -> outgoingUnreliableCommands), currentPeer -> channels != NULL ? penet_list_size (& currentPeer -> channels -> incomingReliableCommands) : 0, currentPeer -> channels != NULL ? penet_list_size (& currentPeer -> channels -> incomingUnreliableCommands) : 0);
#endif

           currentPeer -> packetLossVariance -= currentPeer -> packetLossVariance / 4;

           if (packetLoss >= currentPeer -> packetLoss)
           {
              currentPeer -> packetLoss += (packetLoss - currentPeer -> packetLoss) / 8;
              currentPeer -> packetLossVariance += (packetLoss - currentPeer -> packetLoss) / 4;
           }
           else
           {
              currentPeer -> packetLoss -= (currentPeer -> packetLoss - packetLoss) / 8;
              currentPeer -> packetLossVariance += (currentPeer -> packetLoss - packetLoss) / 4;
           }

           currentPeer -> packetLossEpoch = host -> serviceTime;
           currentPeer -> packetsSent = 0;
           currentPeer -> packetsLost = 0;
        }

        host -> buffers -> data = headerData;
        if (host -> headerFlags & PENET_PROTOCOL_HEADER_FLAG_SENT_TIME)
        {
            header -> sentTime = PENET_HOST_TO_NET_16 (host -> serviceTime & 0xFFFF);

            host -> buffers -> dataLength = sizeof (PENetProtocolHeader);
        }
        else
          host -> buffers -> dataLength = (size_t) & ((PENetProtocolHeader *) 0) -> sentTime;

        shouldCompress = 0;
        if (host -> compressor.context != NULL && host -> compressor.compress != NULL)
        {
            size_t originalSize = host -> packetSize - sizeof(PENetProtocolHeader),
                   compressedSize = host -> compressor.compress (host -> compressor.context,
                                        & host -> buffers [1], host -> bufferCount - 1,
                                        originalSize,
                                        host -> packetData [1],
                                        originalSize);
            if (compressedSize > 0 && compressedSize < originalSize)
            {
                host -> headerFlags |= PENET_PROTOCOL_HEADER_FLAG_COMPRESSED;
                shouldCompress = compressedSize;
#ifdef PENET_DEBUG_COMPRESS
                printf ("peer %u: compressed %u -> %u (%u%%)\n", currentPeer -> incomingPeerID, originalSize, compressedSize, (compressedSize * 100) / originalSize);
#endif
            }
        }

        if (currentPeer -> outgoingPeerID < PENET_PROTOCOL_MAXIMUM_PEER_ID)
          host -> headerFlags |= currentPeer -> outgoingSessionID << PENET_PROTOCOL_HEADER_SESSION_SHIFT;
        header -> peerID = PENET_HOST_TO_NET_16 (currentPeer -> outgoingPeerID | host -> headerFlags);
        if (host -> checksum != NULL)
        {
            penet_uint32 * checksum = (penet_uint32 *) & headerData [host -> buffers -> dataLength];
            * checksum = currentPeer -> outgoingPeerID < PENET_PROTOCOL_MAXIMUM_PEER_ID ? currentPeer -> connectID : 0;
            host -> buffers -> dataLength += sizeof (penet_uint32);
            * checksum = host -> checksum (host -> buffers, host -> bufferCount);
        }

        if (shouldCompress > 0)
        {
            host -> buffers [1].data = host -> packetData [1];
            host -> buffers [1].dataLength = shouldCompress;
            host -> bufferCount = 2;
        }

        currentPeer -> lastSendTime = host -> serviceTime;

        sentLength = penet_socket_send (host -> socket, & currentPeer -> address, host -> buffers, host -> bufferCount);

        penet_protocol_remove_sent_unreliable_commands (currentPeer);

        if (sentLength < 0)
          return -1;

        host -> totalSentData += sentLength;
        host -> totalSentPackets ++;
    }

    return 0;
}

/** Sends any queued packets on the host specified to its designated peers.

    @param host   host to flush
    @remarks this function need only be used in circumstances where one wishes to send queued packets earlier than in a call to penet_host_service().
    @ingroup host
*/
void
penet_host_flush (PENetHost * host)
{
    host -> serviceTime = penet_time_get ();

    penet_protocol_send_outgoing_commands (host, NULL, 0);
}

/** Checks for any queued events on the host and dispatches one if available.

    @param host    host to check for events
    @param event   an event structure where event details will be placed if available
    @retval > 0 if an event was dispatched
    @retval 0 if no events are available
    @retval < 0 on failure
    @ingroup host
*/
int
penet_host_check_events (PENetHost * host, PENetEvent * event)
{
    if (event == NULL) return -1;

    event -> type = PENET_EVENT_TYPE_NONE;
    event -> peer = NULL;
    event -> packet = NULL;

    return penet_protocol_dispatch_incoming_commands (host, event);
}

/** Waits for events on the host specified and shuttles packets between
    the host and its peers.

    @param host    host to service
    @param event   an event structure where event details will be placed if one occurs
                   if event == NULL then no events will be delivered
    @param timeout number of milliseconds that PENet should wait for events
    @retval > 0 if an event occurred within the specified time limit
    @retval 0 if no event occurred
    @retval < 0 on failure
    @remarks penet_host_service should be called fairly regularly for adequate performance
    @ingroup host
*/
int
penet_host_service (PENetHost * host, PENetEvent * event, penet_uint32 timeout)
{
    penet_uint32 waitCondition;

    if (event != NULL)
    {
        event -> type = PENET_EVENT_TYPE_NONE;
        event -> peer = NULL;
        event -> packet = NULL;

        switch (penet_protocol_dispatch_incoming_commands (host, event))
        {
        case 1:
            return 1;

        case -1:
#ifdef PENET_DEBUG
            perror ("Error dispatching incoming packets");
#endif

            return -1;

        default:
            break;
        }
    }

    host -> serviceTime = penet_time_get ();

    timeout += host -> serviceTime;

    do
    {
       if (PENET_TIME_DIFFERENCE (host -> serviceTime, host -> bandwidthThrottleEpoch) >= PENET_HOST_BANDWIDTH_THROTTLE_INTERVAL)
         penet_host_bandwidth_throttle (host);

       switch (penet_protocol_send_outgoing_commands (host, event, 1))
       {
       case 1:
          return 1;

       case -1:
#ifdef PENET_DEBUG
          perror ("Error sending outgoing packets");
#endif

          return -1;

       default:
          break;
       }

       switch (penet_protocol_receive_incoming_commands (host, event))
       {
       case 1:
          return 1;

       case -1:
#ifdef PENET_DEBUG
          perror ("Error receiving incoming packets");
#endif

          return -1;

       default:
          break;
       }

       switch (penet_protocol_send_outgoing_commands (host, event, 1))
       {
       case 1:
          return 1;

       case -1:
#ifdef PENET_DEBUG
          perror ("Error sending outgoing packets");
#endif

          return -1;

       default:
          break;
       }

       if (event != NULL)
       {
          switch (penet_protocol_dispatch_incoming_commands (host, event))
          {
          case 1:
             return 1;

          case -1:
#ifdef PENET_DEBUG
             perror ("Error dispatching incoming packets");
#endif

             return -1;

          default:
             break;
          }
       }

       if (PENET_TIME_GREATER_EQUAL (host -> serviceTime, timeout))
         return 0;

       do
       {
          host -> serviceTime = penet_time_get ();

          if (PENET_TIME_GREATER_EQUAL (host -> serviceTime, timeout))
            return 0;

          waitCondition = PENET_SOCKET_WAIT_RECEIVE | PENET_SOCKET_WAIT_INTERRUPT;

          if (penet_socket_wait (host -> socket, & waitCondition, PENET_TIME_DIFFERENCE (timeout, host -> serviceTime)) != 0)
            return -1;
       }
       while (waitCondition & PENET_SOCKET_WAIT_INTERRUPT);

       host -> serviceTime = penet_time_get ();
    } while (waitCondition & PENET_SOCKET_WAIT_RECEIVE);

    return 0;
}
