/* gdnet_peer.h */

#ifndef GDNET_PEER_H
#define GDNET_PEER_H

#include "error_macros.h"
#include "reference.h"
#include "variant.h"

#include "enet/enet.h"

#include "gdnet_address.h"
#include "gdnet_host.h"
#include "gdnet_message.h"

class GDNetHost;

class GDNetPeer : public Reference {
	
	OBJ_TYPE(GDNetPeer,Reference);
	
	GDNetHost* _host;
	ENetPeer* _peer;
	
protected:

	static void _bind_methods();
	
public:

	GDNetPeer(GDNetHost* host, ENetPeer* peer);
	~GDNetPeer();
	
	int get_peer_id();
	
	Ref<GDNetAddress> get_address();
	
	int get_avg_rtt();
	
	void ping();
	void reset();
	
	void disconnect(int data = 0);
	void disconnect_later(int data = 0);
	void disconnect_now(int data = 0);
	
	void send_packet(const ByteArray& packet, int channel_id = 0, int type = GDNetMessage::UNSEQUENCED);
	void send_var(const Variant& var, int channel_id = 0, int type = GDNetMessage::UNSEQUENCED);
	
	void set_timeout(int limit, int min_timeout, int max_timeout);
};

#endif
