//  Copyright © 2022 Thomas A. Early, N7TAE
//
// ----------------------------------------------------------------------------
//    This file is part of M17Refd.
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
#ifndef TO_U_TYPE_DEF
#define TO_U_TYPE_DEF
template<typename E> constexpr auto toUType(E enumerator) noexcept
{
	return static_cast<std::underlying_type_t<E>>(enumerator);
} // Item #10 in "Effective Modern C++", by Scott Meyers, O'REILLY
#endif

enum class EMrefdValueID : uint64_t { Config=1, Peers=2, Clients=3, Users=4 };

/* PEERS */
using MrefdPeerTuple = std::tuple<std::string, std::string, std::time_t>;
enum class EMrefdPeerFields { Callsign, Modules, ConnectTime };
struct SMrefdPeers1
{
	std::time_t timestamp;
	unsigned int sequence;
	std::list<MrefdPeerTuple> list;

	MSGPACK_DEFINE(timestamp, sequence, list)
};

/* CLIENTS */
using MrefdClientTuple = std::tuple<std::string, std::string, char, std::time_t, std::time_t>;
enum class EMrefdClientFields { Callsign, Ip, Module, ConnectTime, LastHeardTime };
struct SMrefdClients1
{
	std::time_t timestamp;
	unsigned int sequence;
	std::list<MrefdClientTuple> list;

	MSGPACK_DEFINE(timestamp, sequence, list)
};

/* USERS */
using MrefdUserTuple = std::tuple<std::string, std::string, std::string, std::time_t>;
enum class EMrefdUserFields { Source, Destination, Reflector, LastHeardTime };
struct SMrefdUsers1
{
	std::time_t timestamp;
	unsigned int sequence;
	std::list<MrefdUserTuple> list;

	MSGPACK_DEFINE(timestamp, sequence, list)
};

/* CONFIGURATION */
struct SMrefdConfig1
{
	std::time_t timestamp;
	std::string cs, ipv4, ipv6, mods, emods, url, email, sponsor, country, version;
	uint16_t port;

	MSGPACK_DEFINE(timestamp, cs, ipv4, ipv6, mods, emods, url, email, sponsor, country, version, port)
};
