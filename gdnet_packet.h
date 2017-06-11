/* gdnet_packet.h */

#ifndef GDNET_PACKET_H
#define GDNET_PACKET_H

#include "object.h"
#include "typedefs.h"
#include "variant.h"

#include "enet/enet.h"

#include <string.h>

class GDNetPacket : public Object {

	OBJ_TYPE(GDNetPacket, Object);

private:

	void put_back(void *data, int size);
	void get_front(void *data, int size);

	bool _littleEndian;
	ByteArray _data;
	int _pos;

protected:

	static void _bind_methods();

public:
	GDNetPacket();

	void reset_pos();

	int size();

	void push_int8(int8_t value);
	void push_int16(int16_t value);
	void push_int32(int32_t value);
	void push_int64(int64_t value);

	void push_uint8(uint8_t value);
	void push_uint16(uint16_t value);
	void push_uint32(uint32_t value);
	void push_uint64(uint64_t value);

	int8_t pop_int8();
	int16_t pop_int16();
	int32_t pop_int32();
	int64_t pop_int64();

	uint8_t pop_uint8();
	uint16_t pop_uint16();
	uint32_t pop_uint32();
	uint64_t pop_uint64();
};

#endif
