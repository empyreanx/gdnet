/* gdnet_commands.h */

#ifndef GDNET_COMMANDS_H
#define GDNET_COMMANDS_H

#include "enet/enet.h"

class GDNetCommand {
public:
	virtual void execute() = 0;
};

class GDNetPeerCommand : public GDNetCommand {
public:
	void set_peer(ENetPeer* peer) { _peer = peer; }

protected:
	ENetPeer* _peer;
};

class GDNetPingCommand : public GDNetPeerCommand {
public:
	void execute();
};

class GDNetResetCommand : public GDNetPeerCommand {
public:
	void execute();
};

class GDNetDisconnectCommand : public GDNetPeerCommand {
public:
	void set_params(int data);
	void execute();

private:
	int _data;
};

class GDNetDisconnectLaterCommand : public GDNetPeerCommand {
public:
	void set_params(int data);
	void execute();

private:
	int _data;
};

class GDNetDisconnectNowCommand : public GDNetPeerCommand {
public:
	void set_params(int data);
	void execute();

private:
	int _data;
};

class GDNetSetTimeoutCommand : public GDNetPeerCommand {
public:
	void set_params(int limit, int min_timeout, int max_timeout);
	void execute();

private:
	int _limit;
	int _min_timeout;
	int _max_timeout;
};

class GDNetHostCommand : public GDNetCommand {
public:
	void set_host(ENetHost* host) { _host = host; }

protected:
	ENetHost* _host;
};

class GDNetBandwidthLimitCommand : public GDNetHostCommand {
public:
	void set_params(int incomingBandwidth, int outgoingBandwith);
	void execute();

private:
	int _incomingBandwith;
	int _outgoingBandwith;
};

#endif
