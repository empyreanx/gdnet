/* gdnet_address.h */

#ifndef GDNET_ADDRESS_H
#define GDNET_ADDRESS_H

#include "core/reference.h"
#include "core/ustring.h"

class GDNetAddress : public Reference {

	GDCLASS(GDNetAddress,Reference);

	String _host;
	int _port;

protected:

	static void _bind_methods();

public:

	GDNetAddress() : _port(0) { }

	String get_host() const { return _host; }
	void set_host(const String& host) { _host = host; }

	int get_port() const { return _port; }
	void set_port(int port) { _port = port; }

};

#endif
