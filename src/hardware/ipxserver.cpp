/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "dosbox.h"

#if C_IPX

#define WINDOWS_IGNORE_PACKING_MISMATCH

#include <cassert>
#include <cstdint>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <thread>

#include "ipx.h"
#include "ipxserver.h"
#include "timer.h"

ENetAddress ipxAddress = {{}, 0, 0};
//        struct in6_addr host;
//        enet_uint16 port;
//        enet_uint16 sin6_scope_id;

ENetHost *ipxServer = nullptr;

ENetEvent enetEvent = {ENET_EVENT_TYPE_NONE, nullptr, 0, 0, nullptr};
//        ENetEventType type;      /**< type of the event */
//        ENetPeer *    peer;      /**< peer that generated a connect, disconnect
//        or receive event */ enet_uint8    channelID; /**< channel on the peer
//        that generated the event, if appropriate */ enet_uint32   data; /**<
//        data associated with the event, if appropriate */ ENetPacket * packet;
//        /**< packet associated with the event, if appropriate */

bool serverRunning = false;

struct peerData {
	uint8_t ipxnode[6] = {};
};

TIMER_TickHandler* serverTimer;

Bit8u packetCRC(Bit8u *buffer, Bit16u bufSize) {
	Bit8u tmpCRC = 0;
	Bit16u i;
	for(i=0;i<bufSize;i++) {
		tmpCRC ^= *buffer;
		buffer++;
	}
	return tmpCRC;
}

const char *address_to_string(const ENetAddress &enet_addr)
{
	const uint8_t *v6 = enet_addr.host.s6_addr;
	const uint8_t *v4 = v6 + 12;

	static char str[65];
	sprintf(str, "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x | %u.%u.%u.%u:%u",
	        v6[0], v6[1], v6[2], v6[3], v6[4], v6[5], v6[6], v6[7], v6[8],
	        v6[9], v6[10], v6[11], v6[12], v6[13], v6[14], v6[15], v4[0],
	        v4[1], v4[2], v4[3], enet_addr.port);
	return str;
}

bool cmp_ipxnode(uint8_t *node1, uint8_t *node2)
{
	for (int i = 0; i < 6; i++) {
		if (node1[i] != node2[i]) {
			return false;
		}
	}
	return true;
}

static void sendIPXPacket(Bit8u *buffer, Bit16s bufSize) {
	uint16_t destport = 0;
	uint32_t desthost = 0;

	uint8_t *srcnode = nullptr;

	IPXHeader *tmpHeader = nullptr;
	tmpHeader = (IPXHeader *)buffer;

	desthost = tmpHeader->dest.addr.byIP.host;
	destport = tmpHeader->dest.addr.byIP.port;

	srcnode = tmpHeader->src.addr.byNode.node;
	uint8_t *destnode = tmpHeader->dest.addr.byNode.node;

	ENetPacket *packet = enet_packet_create(buffer, bufSize,
	                                        ENET_PACKET_FLAG_RELIABLE);

	if (desthost == 0xffffffff && destport == 0xffff) {
		// Broadcast
		// Can't use enet broadcast since it reflects to loopback
		LOG_MSG("Server IPX broadcast detected");

		ENetPeer *peer = nullptr;
		for (size_t i = 0; i < ipxServer->peerCount; i++) {
			peerData *pd;
			pd = (peerData *)ipxServer->peers[i].data;
			if (pd != nullptr && !cmp_ipxnode(pd->ipxnode, srcnode)) {
				// return;
				peer = &ipxServer->peers[i];
				if (peer != nullptr)
					enet_peer_send(peer, 0, packet);
			}
		}

	} else {
		// Specific address
		// Probably best to use a structure or std::unordered_map here
		// instead of a loop
		ENetPeer *peer = nullptr;
		for (size_t i = 0; i < ipxServer->peerCount; i++) {
			peerData *pd = (peerData *)ipxServer->peers[i].data;
			if (pd != nullptr && cmp_ipxnode(pd->ipxnode, destnode)) {
				peer = (ENetPeer *)&ipxServer->peers[i];
				if (peer != nullptr) {
					enet_peer_send(peer, 0, packet);
					enet_host_flush(ipxServer);
				}
			}
		}
	}
	enet_host_flush(ipxServer);
}

// Endian-safe conversion from 16-bit port to two 8-bit values
static void convert_port(const uint16_t port, uint8_t &multiplier, uint8_t &remainder)
{
	multiplier = port / 256;
	remainder = static_cast<uint8_t>(port - 256 * multiplier);
	// sanity check the conversion
	assert(multiplier * 256 + remainder == port);
}

static void ackClient(ENetPeer *epeer)
{
	IPXHeader regHeader;

	// Create the IPX server node
	uint8_t s_ipxnode[6];
	for (int i = 0; i < 4; i++) {
		s_ipxnode[i] = ipxServer->address.host.s6_addr[i + 12];
	}
	convert_port(ipxServer->address.port, s_ipxnode[4], s_ipxnode[5]);

	peerData *pd = static_cast<peerData *>(epeer->data);

	SDLNet_Write16(0xffff, regHeader.checkSum);
	SDLNet_Write16(sizeof(regHeader), regHeader.length);

	SDLNet_Write32(0, regHeader.dest.network);

	for (int i = 0; i < 6; i++) {
		regHeader.dest.addr.byNode.node[i] = pd->ipxnode[i];
	}

	SDLNet_Write16(0x2, regHeader.dest.socket);
	SDLNet_Write32(1, regHeader.src.network);

	for (int i = 0; i < 6; i++) {
		regHeader.src.addr.byNode.node[i] = s_ipxnode[i];
	}

	SDLNet_Write16(0x2, regHeader.src.socket);
	regHeader.transControl = 0;

	// Create packet
	ENetPacket *packet = enet_packet_create(&regHeader, sizeof(regHeader),
	                                        ENET_PACKET_FLAG_RELIABLE);

	// Send registration string to client.  If client doesn't get this,
	// client will not be registered
	enet_peer_send(epeer, 0, packet);
	enet_host_flush(ipxServer);
}

static void IPX_ServerLoop() {
	while (serverRunning) {
		if (enet_host_service(ipxServer, &enetEvent, 1) > 0) {
			LOG_MSG("Server loop host_service");
			peerData *pd = nullptr;
			switch (enetEvent.type) {
			case ENET_EVENT_TYPE_CONNECT:
				LOG_MSG("IPXSERVER: Connect from %s",
				        address_to_string(enetEvent.peer->address));
				pd = new peerData();
				for (int i = 0; i < 4; i++) {
					pd->ipxnode[i] = enetEvent.peer->address
					                         .host.s6_addr[i + 12];
					;
				}
				convert_port(enetEvent.peer->address.port,
				             pd->ipxnode[4], pd->ipxnode[5]);
				// Store struct into peer data
				enetEvent.peer->data = pd;
				break;
			case ENET_EVENT_TYPE_RECEIVE:
				// Check to see if incoming packet is a
				// registration packet For this, I just spoofed
				// the echo protocol packet designation 0x02
				IPXHeader *tmpHeader;
				tmpHeader = (IPXHeader *)enetEvent.packet->data;
				// Null destination node means its a server
				// registration packet
				if (SDLNet_Read16(tmpHeader->dest.socket) == 0x2 &&
				    SDLNet_Read32(tmpHeader->dest.addr.byNode.node) ==
				            (uint32_t)0x0) {
					// Send peer with generated IPX node in
					// data struct to complete registration
					// on client side
					ackClient(enetEvent.peer);
					// LOG_MSG("IPXSERVER: Connect from
					// %d.%d.%d.%d", CONVIP(host));
					break;
				} else {
					const auto data_length =
					        enetEvent.packet->dataLength;
					// sendIPXPacket can only handle up to 32KB of data
					assert(data_length <= INT16_MAX);
					sendIPXPacket(enetEvent.packet->data,
					              static_cast<int16_t>(data_length));
				}

				/* Clean up the packet now that we're done using
				 * it. */
				// enet_packet_destroy(enetEvent.packet);
				break;
			case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
			case ENET_EVENT_TYPE_DISCONNECT:
				delete (peerData *)enetEvent.peer->data;
				break;
			case ENET_EVENT_TYPE_NONE: break;
			}
		}
	}
}

void IPX_StopServer() {
	serverRunning = false;
	enet_host_destroy(ipxServer);
}

bool IPX_StartServer(uint16_t portnum)
{
	ipxAddress.host = ENET_HOST_ANY;
	ipxAddress.port = portnum;
	ipxServer = enet_host_create(&ipxAddress, SOCKETTABLESIZE, 1, 0, 0);
	if (ipxServer == NULL)
		return false;

	serverRunning = true;
	std::thread(IPX_ServerLoop).detach();
	return true;
}
#endif