/* gdnet_server_event.cpp */

#include "gdnet_event.h"

Variant GDNetEvent::get_var() {
	if (_packet.size() > 0) {
		PoolVector<uint8_t>::Read r = _packet.read();
   		int len = _packet.size();
		Variant var;

		Error err = decode_variant(var, &r[0], len);

		ERR_FAIL_COND_V(err != OK, Variant());

		return var;
	}

	return Variant();
}

void GDNetEvent::_bind_methods() {
	BIND_ENUM_CONSTANT(NONE);
	BIND_ENUM_CONSTANT(CONNECT);
	BIND_ENUM_CONSTANT(DISCONNECT);
	BIND_ENUM_CONSTANT(RECEIVE);

	ClassDB::bind_method(D_METHOD("get_event_type"),&GDNetEvent::get_event_type);
	ClassDB::bind_method(D_METHOD("get_time"),&GDNetEvent::get_time);
	ClassDB::bind_method(D_METHOD("get_peer_id"),&GDNetEvent::get_peer_id);
	ClassDB::bind_method(D_METHOD("get_channel_id"),&GDNetEvent::get_channel_id);
	ClassDB::bind_method(D_METHOD("get_packet"),&GDNetEvent::get_packet);
	ClassDB::bind_method(D_METHOD("get_var"),&GDNetEvent::get_var);
	ClassDB::bind_method(D_METHOD("get_data"),&GDNetEvent::get_data);
}
