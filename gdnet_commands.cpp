/* gdnet_commands.cpp */

#include "gdnet_commands.h"

void GDNetPingCommand::execute() {
	enet_peer_ping(_peer);
}

void GDNetResetCommand::execute() {
	enet_peer_reset(_peer);
}

void GDNetDisconnectCommand::set_params(int data) {
	_data = data;
}

void GDNetDisconnectCommand::execute() {
	enet_peer_disconnect(_peer, _data);
}

void GDNetDisconnectLaterCommand::set_params(int data) {
	_data = data;
}

void GDNetDisconnectLaterCommand::execute() {
	enet_peer_disconnect_later(_peer, _data);
}

void GDNetDisconnectNowCommand::set_params(int data) {
	_data = data;
}

void GDNetDisconnectNowCommand::execute() {
	enet_peer_disconnect_now(_peer, _data);
}

void GDNetSetTimeoutCommand::set_params(int limit, int min_timeout, int max_timeout) {
	_limit = limit;
	_min_timeout = min_timeout;
	_max_timeout = max_timeout;
}

void GDNetSetTimeoutCommand::execute() {
	enet_peer_timeout(_peer, _limit, _min_timeout, _max_timeout);
}

void GDNetBandwidthLimitCommand::set_params(int incomingBandwidth, int outgoingBandwith) {
	_incomingBandwith = incomingBandwidth;
	_outgoingBandwith = outgoingBandwith;
}

void GDNetBandwidthLimitCommand::execute() {
	enet_host_bandwidth_limit(_host, _incomingBandwith, _outgoingBandwith);
}
