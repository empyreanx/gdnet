/* gdnet_packet.cpp */

#include "gdnet_packet.h"

#include <stdio.h>

GDNetPacket::GDNetPacket() : _pos(0) {
	int n = 1;
	_littleEndian = (*reinterpret_cast<char*>(&n) == 1);
}

void GDNetPacket::reset_pos() {
	_pos = 0;
}

int GDNetPacket::size() {
	return _data.size();
}

void GDNetPacket::put_back(void *data, int size) {
	_data.resize(_data.size() + size);

	ByteArray::Write w = _data.write();
	memcpy(w.ptr() + _data.size() - size, data, size);
}

void GDNetPacket::get_front(void *data, int size) {
	ERR_FAIL_COND(_data.size() < size);

	ByteArray::Read r = _data.read();
	memcpy(data, r.ptr() + _pos, size);

	_pos += size;
}

void GDNetPacket::push_int8(int8_t value) {
	put_back(&value, sizeof(value));
}

void GDNetPacket::push_int16(int16_t value) {
	int16_t data = ENET_HOST_TO_NET_16(value);
	put_back(&data, sizeof(value));
}

void GDNetPacket::push_int32(int32_t value) {
	int32_t data = ENET_HOST_TO_NET_32(value);
	put_back(&data, sizeof(int32_t));
}

void GDNetPacket::push_int64(int64_t value) {
	if (_littleEndian) {
		uint8_t data[] = {
			static_cast<uint8_t>((value >> 56) & 0xFF),
			static_cast<uint8_t>((value >> 48) & 0xFF),
			static_cast<uint8_t>((value >> 40) & 0xFF),
			static_cast<uint8_t>((value >> 32) & 0xFF),
			static_cast<uint8_t>((value >> 24) & 0xFF),
			static_cast<uint8_t>((value >> 16) & 0xFF),
			static_cast<uint8_t>((value >>  8) & 0xFF),
			static_cast<uint8_t>((value      ) & 0xFF)
		};

		put_back(data, sizeof(data));
	} else {
		put_back(&value, sizeof(value));
	}
}

void GDNetPacket::push_uint8(uint8_t value) {
	put_back(&value, sizeof(value));
}

void GDNetPacket::push_uint16(uint16_t value) {
	uint16_t data = ENET_HOST_TO_NET_16(value);
	put_back(&data, sizeof(value));
}

void GDNetPacket::push_uint32(uint32_t value) {
	int32_t data = ENET_HOST_TO_NET_32(value);
	put_back(&data, sizeof(int32_t));
}

void GDNetPacket::push_uint64(uint64_t value) {
	if (_littleEndian) {
		uint8_t data[] = {
			static_cast<uint8_t>((value >> 56) & 0xFF),
			static_cast<uint8_t>((value >> 48) & 0xFF),
			static_cast<uint8_t>((value >> 40) & 0xFF),
			static_cast<uint8_t>((value >> 32) & 0xFF),
			static_cast<uint8_t>((value >> 24) & 0xFF),
			static_cast<uint8_t>((value >> 16) & 0xFF),
			static_cast<uint8_t>((value >>  8) & 0xFF),
			static_cast<uint8_t>((value      ) & 0xFF)
		};

		put_back(data, sizeof(data));
	} else {
		put_back(&value, sizeof(value));
	}
}

int8_t GDNetPacket::pop_int8() {
	int8_t value;
	get_front(&value, sizeof(value));
	return value;
}

int16_t GDNetPacket::pop_int16() {
	int16_t value;
	get_front(&value, sizeof(value));
	return ENET_NET_TO_HOST_16(value);
}

int32_t GDNetPacket::pop_int32() {
	int32_t value;
	get_front(&value, sizeof(value));
	return ENET_NET_TO_HOST_32(value);
}

int64_t GDNetPacket::pop_int64() {
	int64_t value;
	get_front(&value, sizeof(value));

	if (_littleEndian) {
		uint8_t* bytes = reinterpret_cast<uint8_t*>(&value);

		value = (static_cast<int64_t>(bytes[0]) << 56) |
				(static_cast<int64_t>(bytes[1]) << 48) |
				(static_cast<int64_t>(bytes[2]) << 40) |
				(static_cast<int64_t>(bytes[3]) << 32) |
				(static_cast<int64_t>(bytes[4]) << 24) |
				(static_cast<int64_t>(bytes[5]) << 16) |
				(static_cast<int64_t>(bytes[6]) <<  8) |
				(static_cast<int64_t>(bytes[7])      );
	}

	return value;
}

uint8_t GDNetPacket::pop_uint8() {
	uint8_t value;
	get_front(&value, sizeof(value));
	return value;
}

uint16_t GDNetPacket::pop_uint16() {
	uint16_t value;
	get_front(&value, sizeof(value));
	return ENET_NET_TO_HOST_16(value);
}

uint32_t GDNetPacket::pop_uint32() {
	uint32_t value;
	get_front(&value, sizeof(value));
	return ENET_NET_TO_HOST_32(value);
}

uint64_t GDNetPacket::pop_uint64() {
	uint64_t value;
	get_front(&value, sizeof(value));

	if (_littleEndian) {
		uint8_t* bytes = reinterpret_cast<uint8_t*>(&value);

		value = (static_cast<uint64_t>(bytes[0]) << 56) |
				(static_cast<uint64_t>(bytes[1]) << 48) |
				(static_cast<uint64_t>(bytes[2]) << 40) |
				(static_cast<uint64_t>(bytes[3]) << 32) |
				(static_cast<uint64_t>(bytes[4]) << 24) |
				(static_cast<uint64_t>(bytes[5]) << 16) |
				(static_cast<uint64_t>(bytes[6]) <<  8) |
				(static_cast<uint64_t>(bytes[7])      );
	}

	return value;
}

void GDNetPacket::_bind_methods() {
	ObjectTypeDB::bind_method("push_int8", &GDNetPacket::push_int8);
	ObjectTypeDB::bind_method("push_int16", &GDNetPacket::push_int16);
	ObjectTypeDB::bind_method("push_int32", &GDNetPacket::push_int32);
	ObjectTypeDB::bind_method("push_int64", &GDNetPacket::push_int64);

	ObjectTypeDB::bind_method("push_uint8", &GDNetPacket::push_uint8);
	ObjectTypeDB::bind_method("push_uint16", &GDNetPacket::push_uint16);
	ObjectTypeDB::bind_method("push_uint32", &GDNetPacket::push_uint32);
	ObjectTypeDB::bind_method("push_uint64", &GDNetPacket::push_uint64);

	ObjectTypeDB::bind_method("pop_int8", &GDNetPacket::pop_int8);
	ObjectTypeDB::bind_method("pop_int16", &GDNetPacket::pop_int16);
	ObjectTypeDB::bind_method("pop_int32", &GDNetPacket::pop_int32);
	ObjectTypeDB::bind_method("pop_int64", &GDNetPacket::pop_int64);

	ObjectTypeDB::bind_method("pop_uint8", &GDNetPacket::pop_uint8);
	ObjectTypeDB::bind_method("pop_uint16", &GDNetPacket::pop_uint16);
	ObjectTypeDB::bind_method("pop_uint32", &GDNetPacket::pop_uint32);
	ObjectTypeDB::bind_method("pop_uint64", &GDNetPacket::pop_uint64);
}
