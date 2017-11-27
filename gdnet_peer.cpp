/* gdnet_peer.cpp */

#include "gdnet_peer.h"

GDNetPeer::GDNetPeer(GDNetHost* host, PENetPeer* peer) : _host(host), _peer(peer) {
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
	penet_address_get_host_ip(&_peer->address, ip, 64);
	address->set_host(ip);

	return address;
}

int GDNetPeer::get_avg_rtt() {
	ERR_FAIL_COND_V(_host->_host == NULL, -1);
	return _peer->roundTripTime;
}

void GDNetPeer::ping() {
	ERR_FAIL_COND(_host->_host == NULL);

	_host->acquireMutex();

	penet_peer_ping(_peer);

	_host->releaseMutex();
}

void GDNetPeer::set_ping_interval(int pingInterval) {
    ERR_FAIL_COND(_host->_host == NULL);
 
    _host->acquireMutex();
 
    penet_peer_ping_interval(_peer, pingInterval);
 
    _host->releaseMutex();
}

void GDNetPeer::reset() {
	ERR_FAIL_COND(_host->_host == NULL);

	_host->acquireMutex();

	penet_peer_reset(_peer);

	_host->releaseMutex();
}

void GDNetPeer::peer_disconnect(int data) {
	ERR_FAIL_COND(_host->_host == NULL);

	_host->acquireMutex();

	penet_peer_disconnect(_peer, data);

	_host->releaseMutex();
}

void GDNetPeer::disconnect_later(int data) {
	ERR_FAIL_COND(_host->_host == NULL);

	_host->acquireMutex();

	penet_peer_disconnect_later(_peer, data);

	_host->releaseMutex();
}

void GDNetPeer::disconnect_now(int data) {
	ERR_FAIL_COND(_host->_host == NULL);

	_host->acquireMutex();

	penet_peer_disconnect_now(_peer, data);

	_host->releaseMutex();
}

void GDNetPeer::send_packet(const PoolByteArray& packet, int channel_id, int type) {
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

	PoolByteArray packet;
	packet.resize(len);

	PoolByteArray::Write w = packet.write();
	err = encode_variant(var, &w[0], len);

	ERR_FAIL_COND(err != OK);

	message->set_packet(packet);

	_host->_message_queue.push(message);
}

void GDNetPeer::set_timeout(int limit, int min_timeout, int max_timeout) {
	ERR_FAIL_COND(_host->_host == NULL);

	_host->acquireMutex();

	penet_peer_timeout(_peer, limit, min_timeout, max_timeout);

	_host->releaseMutex();
}

void GDNetPeer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_peer_id"), &GDNetPeer::get_peer_id);
	ClassDB::bind_method(D_METHOD("get_address"), &GDNetPeer::get_address);
	ClassDB::bind_method(D_METHOD("get_avg_rtt"), &GDNetPeer::get_avg_rtt);
	ClassDB::bind_method(D_METHOD("ping"), &GDNetPeer::ping);
	ClassDB::bind_method(D_METHOD("set_ping_interval"), &GDNetPeer::set_ping_interval,DEFVAL(0));
	ClassDB::bind_method(D_METHOD("reset"), &GDNetPeer::reset);
	ClassDB::bind_method(D_METHOD("peer_disconnect"), &GDNetPeer::peer_disconnect,DEFVAL(0));
	ClassDB::bind_method(D_METHOD("disconnect_later"), &GDNetPeer::disconnect_later,DEFVAL(0));
	ClassDB::bind_method(D_METHOD("disconnect_now"), &GDNetPeer::disconnect_now,DEFVAL(0));
	ClassDB::bind_method(D_METHOD("send_packet"), &GDNetPeer::send_packet,DEFVAL(0),DEFVAL(GDNetMessage::UNSEQUENCED));
	ClassDB::bind_method(D_METHOD("send_var"), &GDNetPeer::send_var,DEFVAL(0),DEFVAL(GDNetMessage::UNSEQUENCED));
	ClassDB::bind_method(D_METHOD("set_timeout"), &GDNetPeer::set_timeout);
}
