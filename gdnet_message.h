/* gdnet_packet.h */

#ifndef GDNET_MESSAGE_H
#define GDNET_MESSAGE_H

#include <string.h>

#include "core/int_types.h"
#include "core/os/memory.h"
#include "core/object.h"
#include "core/variant.h"

class GDNetMessage : public Object {

	GDCLASS(GDNetMessage,Object);

public:

	enum Type {
		UNSEQUENCED,
		SEQUENCED,
		RELIABLE
	};

private:

	Type _type;
	bool _broadcast;
	int _peer_id;
	int _channel_id;
	PoolByteArray _packet;

protected:

	static void _bind_methods();

public:

	GDNetMessage(Type type);

	Type get_type() { return _type; }

	int get_peer_id() { return _peer_id; }
	void set_peer_id(int peer_id) { _peer_id = peer_id; }

	int get_channel_id() { return _channel_id; }
	void set_channel_id(int channel_id) { _channel_id = channel_id; }

	void set_broadcast(bool broadcast) { _broadcast = broadcast; }
	bool is_broadcast() { return _broadcast; }

	PoolByteArray& get_packet() { return _packet; }
	void set_packet(const PoolByteArray& packet) { _packet = packet; }
};

VARIANT_ENUM_CAST(GDNetMessage::Type);

#endif
