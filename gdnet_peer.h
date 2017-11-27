/* gdnet_peer.h */

#ifndef GDNET_PEER_H
#define GDNET_PEER_H

#include "error_macros.h"
#include "reference.h"
#include "variant.h"

#include "penet/penet.h"

#include "gdnet_address.h"
#include "gdnet_host.h"
#include "gdnet_message.h"

class GDNetHost;

class GDNetPeer : public Reference {

	GDCLASS(GDNetPeer,Reference);

	GDNetHost* _host;
	PENetPeer* _peer;

protected:

	static void _bind_methods();

public:

	GDNetPeer(GDNetHost* host, PENetPeer* peer);
	~GDNetPeer();

	int get_peer_id();

	Ref<GDNetAddress> get_address();

	int get_avg_rtt();

	void ping();
	void set_ping_interval(int pingInterval = 0);
	void reset();

	void gdnet_disconnect(int data = 0);
	void disconnect_later(int data = 0);
	void disconnect_now(int data = 0);

	void send_packet(const PoolByteArray& packet, int channel_id = 0, int type = GDNetMessage::UNSEQUENCED);
	void send_var(const Variant& var, int channel_id = 0, int type = GDNetMessage::UNSEQUENCED);

	void set_timeout(int limit, int min_timeout, int max_timeout);
};

#endif
