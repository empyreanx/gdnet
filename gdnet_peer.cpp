/* gdnet_peer.cpp */

#include "gdnet_peer.h"

GDNetPeer::GDNetPeer(GDNetHost* host, ENetPeer* peer) : _host(host), _peer(peer) {
	_host->reference();
}

GDNetPeer::~GDNetPeer() {
	_host->unreference();
}

int GDNetPeer::get_peer_id() {
	ERR_FAIL_COND_V(_host->_host == NULL, -1);
	return (int)(_peer - _host->_host->peers);
}

Ref<GDNetAddress> GDNetPeer::get_address() {
	Ref<GDNetAddress> address = memnew(GDNetAddress);
	address->set_port(_peer->address.port);
	
	char ip[64];
	enet_address_get_host_ip(&_peer->address, ip, 64);
	address->set_host(ip);
	
	return address;
}

int GDNetPeer::get_avg_rtt() {
	ERR_FAIL_COND_V(_host->_host == NULL, -1);
	return _peer->roundTripTime;
}

void GDNetPeer::ping() {
	ERR_FAIL_COND(_host->_host == NULL);
	
	while (true) {
		if (_host->_mutex->try_lock() == 0) {
			enet_peer_ping(_peer);
			_host->_mutex->unlock();
			break;
		}
	}	
}

void GDNetPeer::reset() {
	ERR_FAIL_COND(_host->_host == NULL);
	
	while (true) {
		if (_host->_mutex->try_lock() == 0) {
			enet_peer_reset(_peer);
			_host->_mutex->unlock();
			break;
		}
	}
}

void GDNetPeer::disconnect(int data) {
	ERR_FAIL_COND(_host->_host == NULL);
	
	while (true) {
		if (_host->_mutex->try_lock() == 0) {
			enet_peer_disconnect(_peer, data);
			_host->_mutex->unlock();
			break;
		}
	}	
}

void GDNetPeer::disconnect_later(int data) {
	ERR_FAIL_COND(_host->_host == NULL);
	
	while (true) {
		if (_host->_mutex->try_lock() == 0) {
			enet_peer_disconnect_later(_peer, data);
			_host->_mutex->unlock();
			break;
		}
	}
}

void GDNetPeer::disconnect_now(int data) {
	ERR_FAIL_COND(_host->_host == NULL);
	
	while (true) {
		if (_host->_mutex->try_lock() == 0) {
			enet_peer_disconnect_now(_peer, data);
			_host->_mutex->unlock();
			break;
		}
	}	
}

void GDNetPeer::send_packet(const ByteArray& packet, int channel_id, int type) {
	ERR_FAIL_COND(_host->_host == NULL);
	
	GDNetMessage* message = memnew(GDNetMessage((GDNetMessage::Type)type));
	message->set_peer_id(get_peer_id());
	message->set_channel_id(channel_id);
	message->set_packet(packet);
	_host->_message_queue.push(message);
}

void GDNetPeer::send_var(const Variant& var, int channel_id, int type) {
	ERR_FAIL_COND(_host->_host == NULL);
	
	int len;
	
	Error err = encode_variant(var, NULL, len);
	
	ERR_FAIL_COND(err != OK || len == 0);
	
	GDNetMessage* message = memnew(GDNetMessage((GDNetMessage::Type)type));
	message->set_peer_id(get_peer_id());
	message->set_channel_id(channel_id);
	
	ByteArray packet;
	packet.resize(len);
	
	ByteArray::Write w = packet.write();
	err = encode_variant(var, w.ptr(), len);
	
	ERR_FAIL_COND(err != OK);
	
	message->set_packet(packet);
	
	_host->_message_queue.push(message);
}

void GDNetPeer::set_timeout(int limit, int min_timeout, int max_timeout) {
	ERR_FAIL_COND(_host->_host == NULL);
	
	while (true) {
		if (_host->_mutex->try_lock() == 0) {
			enet_peer_timeout(_peer, limit, min_timeout, max_timeout);
			_host->_mutex->unlock();
			break;
		}
	}
}

void GDNetPeer::_bind_methods() {
	ObjectTypeDB::bind_method("get_peer_id", &GDNetPeer::get_peer_id);
	ObjectTypeDB::bind_method("get_address", &GDNetPeer::get_address);
	ObjectTypeDB::bind_method("get_avg_rtt", &GDNetPeer::get_avg_rtt);
	ObjectTypeDB::bind_method("ping", &GDNetPeer::ping);
	ObjectTypeDB::bind_method("reset", &GDNetPeer::reset);
	ObjectTypeDB::bind_method("disconnect", &GDNetPeer::disconnect,DEFVAL(0));
	ObjectTypeDB::bind_method("disconnect_later", &GDNetPeer::disconnect_later,DEFVAL(0));
	ObjectTypeDB::bind_method("disconnect_now", &GDNetPeer::disconnect_now,DEFVAL(0));
	ObjectTypeDB::bind_method("send_packet", &GDNetPeer::send_packet,DEFVAL(0),DEFVAL(GDNetMessage::UNSEQUENCED));
	ObjectTypeDB::bind_method("send_var", &GDNetPeer::send_var,DEFVAL(0),DEFVAL(GDNetMessage::UNSEQUENCED));
	ObjectTypeDB::bind_method("set_timeout", &GDNetPeer::set_timeout);
}
