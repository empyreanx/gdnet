/* register_types.cpp */

#include "error_macros.h"
#include "object_type_db.h"
#include "register_types.h"

#include "enet/enet.h"

#include "gdnet_address.h"
#include "gdnet_event.h"
#include "gdnet_message.h"
#include "gdnet_peer.h"

void register_gdnet_types() {
	ObjectTypeDB::register_virtual_type<GDNetPeer>();
	ObjectTypeDB::register_virtual_type<GDNetEvent>();
	ObjectTypeDB::register_virtual_type<GDNetMessage>();
	ObjectTypeDB::register_type<GDNetHost>();
	ObjectTypeDB::register_type<GDNetAddress>();
	
	if (enet_initialize() != 0)
		ERR_EXPLAIN("Unable to initialize ENet");
}

void unregister_gdnet_types() {
	enet_deinitialize();
}
