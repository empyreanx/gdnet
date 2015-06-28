/* gdnet_server_event.cpp */

#include "gdnet_event.h"

Variant GDNetEvent::get_var() {
	if (_packet.size() > 0) {
		ByteArray::Read r = _packet.read();
		
		Variant var;
		
		Error err = decode_variant(var, r.ptr(), _packet.size());
		
		ERR_FAIL_COND_V(err != OK, Variant());
		
		return var;
	}
	
	return Variant();
}

void GDNetEvent::_bind_methods() {
	BIND_CONSTANT(NONE);
	BIND_CONSTANT(CONNECT);
	BIND_CONSTANT(DISCONNECT);
	BIND_CONSTANT(RECEIVE);
	
	ObjectTypeDB::bind_method("get_event_type",&GDNetEvent::get_event_type);
	ObjectTypeDB::bind_method("get_time",&GDNetEvent::get_time);
	ObjectTypeDB::bind_method("get_peer_id",&GDNetEvent::get_peer_id);
	ObjectTypeDB::bind_method("get_channel_id",&GDNetEvent::get_channel_id);
	ObjectTypeDB::bind_method("get_packet",&GDNetEvent::get_packet);
	ObjectTypeDB::bind_method("get_var",&GDNetEvent::get_var);
	ObjectTypeDB::bind_method("get_data",&GDNetEvent::get_data);
}
