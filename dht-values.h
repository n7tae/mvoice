//  Copyright © 2022 Thomas A. Early, N7TAE
//
// ----------------------------------------------------------------------------
//    This is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    with this software.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

#pragma once

#include <opendht.h>

#define USE_MREFD_VALUES
#define USE_URFD_VALUES

/* HELPERS */
template<typename E> constexpr auto toUType(E enumerator) noexcept
{
	return static_cast<std::underlying_type_t<E>>(enumerator);
} // Item #10 in "Effective Modern C++", by Scott Meyers, O'REILLY

#ifdef USE_MREFD_VALUES

// user_type for mrefd values
#define MREFD_USERS_1   "mrefd-users-1"
#define MREFD_PEERS_1   "mrefd-peers-1"
#define MREFD_CONFIG_1  "mrefd-config-1"
#define MREFD_CLIENTS_1 "mrefd-clients-1"

// dht::Value ids of the different parts of the document
// can be assigned any unsigned value except 0
enum class EMrefdValueID : uint64_t { Config=1, Peers=2, Clients=3, Users=4 };

using MrefdPeerTuple = std::tuple<std::string, std::string, std::time_t>;
enum class EMrefdPeerFields { Callsign, Modules, ConnectTime };
struct SMrefdPeers1
{
	std::time_t timestamp;
	unsigned int sequence;
	std::list<MrefdPeerTuple> list;

	MSGPACK_DEFINE(timestamp, sequence, list)
};

using MrefdClientTuple = std::tuple<std::string, std::string, char, std::time_t, std::time_t>;
enum class EMrefdClientFields { Callsign, Ip, Module, ConnectTime, LastHeardTime };
struct SMrefdClients1
{
	std::time_t timestamp;
	unsigned int sequence;
	std::list<MrefdClientTuple> list;

	MSGPACK_DEFINE(timestamp, sequence, list)
};

using MrefdUserTuple = std::tuple<std::string, std::string, std::string, std::time_t>;
enum class EMrefdUserFields { Source, Destination, Reflector, LastHeardTime };
struct SMrefdUsers1
{
	std::time_t timestamp;
	unsigned int sequence;
	std::list<MrefdUserTuple> list;

	MSGPACK_DEFINE(timestamp, sequence, list)
};

struct SMrefdConfig1
{
	std::time_t timestamp;
	std::string callsign, ipv4addr, ipv6addr, modules, encryptedmods, url, email, sponsor, country, version;
	uint16_t port;

	MSGPACK_DEFINE(timestamp, callsign, ipv4addr, ipv6addr, modules, encryptedmods, url, email, sponsor, country, version, port)
};

#endif

#ifdef USE_URFD_VALUES

#define URFD_PEERS_1   "urfd-peers-1"
#define URFD_USERS_1   "urfd-users-1"
#define URFD_CONFIG_1  "urfd-config-1"
#define URFD_CLIENTS_1 "urfd-clients-1"

enum class EUrfdValueID : uint64_t { Config=1, Peers=2, Clients=3, Users=4 };

using UrfdPeerTuple = std::tuple<std::string, std::string, std::time_t>;
enum class EUrfdPeerFields { Callsign, Modules, ConnectTime };
struct SUrfdPeers1
{
	std::time_t timestamp;
	unsigned int sequence;
	std::list<UrfdPeerTuple> list;

	MSGPACK_DEFINE(timestamp, sequence, list)
};

using UrfdClientTuple = std::tuple<std::string, std::string, char, std::time_t, std::time_t>;
enum class EUrfdClientFields { Callsign, Ip, Module, ConnectTime, LastHeardTime };
struct SUrfdClients1
{
	std::time_t timestamp;
	unsigned int sequence;
	std::list<UrfdClientTuple> list;

	MSGPACK_DEFINE(timestamp, sequence, list)
};

using UrfdUserTuple = std::tuple<std::string, std::string, char, std::string, std::time_t>;
enum class EUrfdUserFields { Callsign, ViaNode, OnModule, ViaPeer, LastHeardTime };
struct SUrfdUsers1
{
	std::time_t timestamp;
	unsigned int sequence;
	std::list<UrfdUserTuple> list;

	MSGPACK_DEFINE(timestamp, sequence, list)
};

// 'SIZE' has to be last value for these scoped enums
enum class EUrfdPorts : unsigned { dcs, dextra, dmrplus, dplus, m17, mmdvm, nxdn, p25, urf, ysf, SIZE };
enum class EUrfdAlMod : unsigned { nxdn, p25, ysf, SIZE };
enum class EUrfdTxRx  : unsigned { rx, tx, SIZE };
enum class EUrfdRefId : unsigned { nxdn, p25, SIZE };
struct SUrfdConfig1
{
	std::time_t timestamp;
	std::string callsign, ipv4addr, ipv6addr, modules, transcodedmods, url, email, sponsor, country, version;
	std::array<uint16_t, toUType(EUrfdPorts::SIZE)> port;
	std::array<char, toUType(EUrfdAlMod::SIZE)> almod;
	std::array<unsigned long, toUType(EUrfdTxRx::SIZE)> ysffreq;
	std::array<unsigned, toUType(EUrfdRefId::SIZE)> refid;
	std::unordered_map<char, std::string> description;
	bool g3enabled;

	MSGPACK_DEFINE(timestamp, callsign, ipv4addr, ipv6addr, modules, transcodedmods, url, email, sponsor, country, version, almod, ysffreq, refid, g3enabled, port, description)
};

#endif
