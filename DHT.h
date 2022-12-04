//  Copyright © 2022 Thomas A. Early, N7TAE
//
// ----------------------------------------------------------------------------
//    This file is part of MVoice.
//
//    M17Refd is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    M17Refd is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    with this software.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

#pragma once

#include <opendht.h>

/* HELPERS */
template<typename E> constexpr auto toUType(E enumerator) noexcept
{
	return static_cast<std::underlying_type_t<E>>(enumerator);
} // Item #10 in "Effective Modern C++", by Scott Meyers, O'REILLY

enum class EMrefdValueID : uint64_t { Config=1, Peers=2, Clients=3 };

/* PEERS */
using PeerTuple = std::tuple<std::string, std::string, std::time_t, std::time_t>;
enum class EMrefdPeerFields { Callsign, Modules, ConnectTime, LastHeardTime };
struct SMrefdPeers0
{
	std::list<PeerTuple> peers;

	MSGPACK_DEFINE(peers)
};

/* Clients */
using ClientTuple = std::tuple<std::string, std::string, char, std::time_t, std::time_t>;
enum class EMrefdClientFields { Callsign, Ip, Module, ConnectTime, LastHeardTime };
struct SMrefdClients0
{
	std::list<ClientTuple> clients;

	MSGPACK_DEFINE(clients)
};

struct SMrefdConfig0
{
	std::string cs, ipv4, ipv6, mods, emods, url, email, sponsor, country;
	uint16_t port;

	MSGPACK_DEFINE(cs, ipv4, ipv6, mods, emods, url, email, sponsor, country, port)
};
