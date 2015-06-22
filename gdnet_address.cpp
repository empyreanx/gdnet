/* gdnet_address.cpp */

#include "gdnet_address.h"

void GDNetAddress::_bind_methods() {
	ObjectTypeDB::bind_method("set_host",&GDNetAddress::set_host);
	ObjectTypeDB::bind_method("get_host",&GDNetAddress::get_host);
	
	ObjectTypeDB::bind_method("set_port",&GDNetAddress::set_port);
	ObjectTypeDB::bind_method("get_port",&GDNetAddress::get_port);
}
