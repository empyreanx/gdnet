/* gdnet_message.cpp */

#include "gdnet_message.h"

GDNetMessage::GDNetMessage(Type type) :
	_type(type),
	_broadcast(false),
	_peer_id(0),
	_channel_id(0) {
}

void GDNetMessage::_bind_methods() {
	BIND_ENUM_CONSTANT(UNSEQUENCED);
	BIND_ENUM_CONSTANT(SEQUENCED);
	BIND_ENUM_CONSTANT(RELIABLE);
}
