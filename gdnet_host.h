/* gdnet_host.h */

#ifndef GDNET_HOST_H
#define GDNET_HOST_H

#include <string.h>

#include "os/thread.h"
#include "reference.h"

#include "enet/enet.h"

#include "gdnet_address.h"
#include "gdnet_event.h"
#include "gdnet_message.h"
#include "gdnet_peer.h"
#include "gdnet_queue.h"

class GDNetEvent;
class GDNetPeer;

class GDNetHost : public Reference {

	OBJ_TYPE(GDNetHost,Reference);

	friend class GDNetPeer;

	enum {
		THREAD_LOOP_WAIT = 1,
		DEFAULT_MAX_PEERS = 32
	};
	
	ENetHost* _host;
	volatile bool _running;
	Thread* _thread;
	
	int _max_peers;
	int _max_channels;
	int _max_bandwidth_in;
	int _max_bandwidth_out;	
	
	GDNetQueue<GDNetEvent> _event_queue;
	GDNetQueue<GDNetMessage> _message_queue;
	
	void send_messages();
	void poll_events();
	
	static void thread_callback(void *instance);
	void thread_start();
	void thread_loop();
	void thread_stop();

protected:

	static void _bind_methods();
	
public:

	GDNetHost();

	Ref<GDNetPeer> get_peer(int id);

	void set_max_peers(int max) { _max_peers = max; }
	void set_max_channels(int max) { _max_channels = max; }
	void set_max_bandwidth_in(int max) { _max_bandwidth_in = max; }
	void set_max_bandwidth_out(int max) { _max_bandwidth_out = max; }

	Error bind(Ref<GDNetAddress> addr);
	void unbind();

	Ref<GDNetPeer> connect(Ref<GDNetAddress> addr);

	void broadcast_packet(const ByteArray& packet, int channel_id, int type = GDNetMessage::UNSEQUENCED);
	void broadcast_var(const Variant& var, int channel_id, int type = GDNetMessage::UNSEQUENCED);

	bool is_event_available();
	Ref<GDNetEvent> get_event();
};

#endif
