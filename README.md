# GDNet

An [ENet](http://enet.bespin.org/) wrapper for Godot.

## About

[ENet](http://enet.bespin.org/) is a library that provides a number of features on top of UDP such as, connection handling, sequencing, reliability, channels, bandwidth throttling, packet fragmentation and reassembly. GDNet provides a (mostly) thin wrapper around ENet.

## Installation

Simply drop the `gdnet` directory in your `godot/modules` directory and build for the platfom of your choice. GDNet has been verified to build on Linux (64 bit), MacOS X (32/64 bit), and Windows (32/64 bit cross-compiled using MinGW).

## Example

```python
extends MainLoop

var client1 = null
var client2 = null
var peer1 = null
var peer2 = null
var server = null

func _init():
	var address = GDNetAddress.new()
	address.set_host("localhost")
	address.set_port(3000)

	server = GDNetHost.new()
	server.bind(address)

	client1 = GDNetHost.new()
	client1.bind()
	peer1 = client1.connect(address)

	client2 = GDNetHost.new()
	client2.bind()
	peer2 = client2.connect(address)

func _iteration(delta):
	if (client1.is_event_available()):
		var event = client1.get_event()
		
		if (event.get_event_type() == GDNetEvent.CONNECT):
			print("Client1 connected")
			peer1.send_var("Hello from client 1", 0)
			
		if (event.get_event_type() == GDNetEvent.RECEIVE):
			print(event.get_var())

	if (client2.is_event_available()):
		var event = client2.get_event()
		
		if (event.get_event_type() == GDNetEvent.CONNECT):
			print("Client2 connected")
			peer2.send_var("Hello from client 2", 0)
			
		if (event.get_event_type() == GDNetEvent.RECEIVE):
			print(event.get_var())

	if (server.is_event_available()):
		var event = server.get_event()
		
		if (event.get_event_type() == GDNetEvent.CONNECT):
			var peer = server.get_peer(event.get_peer_id())
			var address = peer.get_address();
			print("Peer connected from ", address.get_host(), ":", address.get_port())
			peer.send_var(str("Hello from server to peer ", event.get_peer_id()), 0)
			
		elif (event.get_event_type() == GDNetEvent.RECEIVE):
			print(event.get_var())
			server.broadcast_var("Server broadcast", 0)
```

**Sample Output:**
```
Client1 connected
Client2 connected
Peer connected from 127.0.0.1:56075
Peer connected from 127.0.0.1:42174
Hello from server to peer 0
Hello from client 2
Hello from server to peer 1
Server broadcast
Hello from client 1
Server broadcast
Server broadcast
Server broadcast
```

## API

#### GDNetAddress

- **set_port(port:Integer)**
- **get_port():Integer**
- **set_host(host:String)**
- **get_host():String**

#### GDNetEvent

- **get_event_type():Integer** - returns one of `GDNetEvent.CONNECT`, `GDNetEvent.DISCONNECT`, or `GDNetEvent.RECEIVE`
- **get_peer_id():Integer** - the peer associated with the event
- **get_channel_id():Integer** - only valid for `GDNetEvent.RECEIVE` events
- **get_packet():RawArray** - only valid for `GDNetEvent.RECEIVE` events
- **get_var():Variant** - only valid for `GDNetEvent.RECEIVE` events
- **get_data():Integer** - only valid for `GDNetEvent.CONNECT` and `GDNetEvent.DISCONNECT` events

#### GDNetHost

- **get_peer(id:Integer):GDNetPeer**
- **set_max_peers(max:Integer)** - must be called before `bind` (default: 32)
- **set_max_channels(max:Integer)** - must be called before `bind` (default: 1)
- **set_max_bandwidth_in(max:Integer)** - measured in bytes/sec, must be called before `bind` (default: unlimited)
- **set_max_bandwidth_out(max:Integer)** - measured in bytes/sec, must be called before `bind` (default: unlimited)
- **bind(addr:GDNetAddress)** - starts the host (the system determines the interface/port to bind if `addr` is empty)
- **unbind()** - stops the host
- **connect(addr:GDNetAddress, data:Integer):GDNetPeer** - attempt to connect to a remote host (data default: 0)
- **broadcast_packet(packet:RawArray, channel_id:Integer, type:Integer)** - type must be one of `GDNetMessage.UNSEQUENCED`, `GDNetMessage.SEQUENCED`, or `GDNetMessage.RELIABLE`
- **broadcast_var(var:Variant, channel_id:Integer, type:Integer)** - type must be one of `GDNetMessage.UNSEQUENCED`, `GDNetMessage.SEQUENCED`, or `GDNetMessage.RELIABLE`
- **is_event_available():Boolean** - returns `true` if there is an event in the queue
- **get_event_count():Integer** - returns the number of events in the queue
- **get_event():GDNetEvent** - return the next event in the queue

#### GDNetPeer

These methods should be called after a successful connection is established, that is, only after a `GDNetEvent.CONNECT` event is consumed.

- **get_peer_id():Integer**
- **get_address():GDNetAddress**
- **ping()** - sends a ping to the remote peer
- **get_avg_rtt** - Average Round Trip Time (RTT). Note, this value is initially 500 ms and will be adjusted by traffic or pings.
- **reset()** - forcefully disconnect a peer (foreign host is not notified)
- **disconnect(data:Integer)** - request a disconnection from a peer (data default: 0)
- **disconnect_later(data:Integer)** - request disconnection after all queued packets have been sent (data default: 0)
- **disconnect_now(data:Integer)** - forcefully disconnect peer (notification is sent, but not guaranteed to arrive) (data default: 0)
- **send_packet(packet:RawArray, channel_id:int, type:int)** - type must be one of `GDNetMessage.UNSEQUENCED`, `GDNetMessage.SEQUENCED`, or `GDNetMessage.RELIABLE`
- **send_var(var:Variant, channel_id:Integer, type:Integer)** - type must be one of `GDNetMessage.UNSEQUENCED`, `GDNetMessage.SEQUENCED`, or `GDNetMessage.RELIABLE`
- **set_timeout(limit:int, min_timeout:Integer, max_timeout:Integer)** 
	- **limit** - A factor that is multiplied with a value that based on the average round trip time to compute the timeout limit.
	- **min_timeout** - Timeout value, in milliseconds, that a reliable packet has to be acknowledged if the variable timeout limit was exceeded before dropping the peer.
	- **max_timeout** - Fixed timeout in milliseconds for which any packet has to be acknowledged before dropping the peer.

## License
Copyright (c) 2015 James McLean  
Licensed under the MIT license.
