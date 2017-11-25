/* gdnet_address.cpp */

#include "gdnet_address.h"

void GDNetAddress::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_host"),&GDNetAddress::set_host);
	ClassDB::bind_method(D_METHOD("get_host"),&GDNetAddress::get_host);

	ClassDB::bind_method(D_METHOD("set_port"),&GDNetAddress::set_port);
	ClassDB::bind_method(D_METHOD("get_port"),&GDNetAddress::get_port);
}
