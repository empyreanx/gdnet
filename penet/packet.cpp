/** 
 @file  packet.c
 @brief PENet packet management functions
*/
#include <string.h>
#define PENET_BUILDING_LIB 1
#include "penet/penet.h"

/** @defgroup Packet PENet packet functions
    @{
*/

/** Creates a packet that may be sent to a peer.
    @param data         initial contents of the packet's data; the packet's data will remain uninitialized if data is NULL.
    @param dataLength   size of the data allocated for this packet
    @param flags        flags for this packet as described for the PENetPacket structure.
    @returns the packet on success, NULL on failure
*/
PENetPacket *
penet_packet_create (const void * data, size_t dataLength, penet_uint32 flags)
{
    PENetPacket * packet = (PENetPacket *) penet_malloc (sizeof (PENetPacket));
    if (packet == NULL)
      return NULL;

    if (flags & PENET_PACKET_FLAG_NO_ALLOCATE)
      packet -> data = (penet_uint8 *) data;
    else
    if (dataLength <= 0)
      packet -> data = NULL;
    else
    {
       packet -> data = (penet_uint8 *) penet_malloc (dataLength);
       if (packet -> data == NULL)
       {
          penet_free (packet);
          return NULL;
       }

       if (data != NULL)
         memcpy (packet -> data, data, dataLength);
    }

    packet -> referenceCount = 0;
    packet -> flags = flags;
    packet -> dataLength = dataLength;
    packet -> freeCallback = NULL;
    packet -> userData = NULL;

    return packet;
}

/** Destroys the packet and deallocates its data.
    @param packet packet to be destroyed
*/
void
penet_packet_destroy (PENetPacket * packet)
{
    if (packet == NULL)
      return;

    if (packet -> freeCallback != NULL)
      (* packet -> freeCallback) (packet);
    if (! (packet -> flags & PENET_PACKET_FLAG_NO_ALLOCATE) &&
        packet -> data != NULL)
      penet_free (packet -> data);
    penet_free (packet);
}

/** Attempts to resize the data in the packet to length specified in the
    dataLength parameter
    @param packet packet to resize
    @param dataLength new size for the packet data
    @returns 0 on success, < 0 on failure
*/
int
penet_packet_resize (PENetPacket * packet, size_t dataLength)
{
    penet_uint8 * newData;

    if (dataLength <= packet -> dataLength || (packet -> flags & PENET_PACKET_FLAG_NO_ALLOCATE))
    {
       packet -> dataLength = dataLength;

       return 0;
    }

    newData = (penet_uint8 *) penet_malloc (dataLength);
    if (newData == NULL)
      return -1;

    memcpy (newData, packet -> data, packet -> dataLength);
    penet_free (packet -> data);

    packet -> data = newData;
    packet -> dataLength = dataLength;

    return 0;
}

static int initializedCRC32 = 0;
static penet_uint32 crcTable [256];

static penet_uint32
reflect_crc (int val, int bits)
{
    int result = 0, bit;

    for (bit = 0; bit < bits; bit ++)
    {
        if(val & 1) result |= 1 << (bits - 1 - bit);
        val >>= 1;
    }

    return result;
}

static void
initialize_crc32 (void)
{
    int byte;

    for (byte = 0; byte < 256; ++ byte)
    {
        penet_uint32 crc = reflect_crc (byte, 8) << 24;
        int offset;

        for(offset = 0; offset < 8; ++ offset)
        {
            if (crc & 0x80000000)
                crc = (crc << 1) ^ 0x04c11db7;
            else
                crc <<= 1;
        }

        crcTable [byte] = reflect_crc (crc, 32);
    }

    initializedCRC32 = 1;
}

penet_uint32
penet_crc32 (const PENetBuffer * buffers, size_t bufferCount)
{
    penet_uint32 crc = 0xFFFFFFFF;

    if (! initializedCRC32) initialize_crc32 ();

    while (bufferCount -- > 0)
    {
        const penet_uint8 * data = (const penet_uint8 *) buffers -> data,
                         * dataEnd = & data [buffers -> dataLength];

        while (data < dataEnd)
        {
            crc = (crc >> 8) ^ crcTable [(crc & 0xFF) ^ *data++];
        }

        ++ buffers;
    }

    return PENET_HOST_TO_NET_32 (~ crc);
}

/** @} */
