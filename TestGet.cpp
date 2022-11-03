/*
 *   Copyright (c) 2022 by Thomas A. Early N7TAE
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <opendht.h>

struct SReflectorData
{
	std::string cs;
	std::string ipv4;
	std::string ipv6;
	std::string modules;
	std::string url;
	std::string email;
	uint16_t port;
	std::vector<std::pair<std::string, std::string>> peers;
	MSGPACK_DEFINE(cs, ipv4, ipv6, modules, url, email, port, peers);
};

int main(int argc, char *argv[])
{
	if (3 != argc)
	{
		std::cout << "usage: " << argv[0] << " bootstrap key" << std::endl;
		return EXIT_FAILURE;
	}

	const std::string key(argv[2]);

	std::string name("TestGet");
	name += std::to_string(getpid());
	dht::DhtRunner node;
	node.run(17171, dht::crypto::generateIdentity(name));
	node.bootstrap(argv[1], "17171");
	std::cout << "Joined the DHT at " << argv[1] << " using an id of " << name << std::endl;
	std::cout << "Getting Data for " << key << " ..." << std::endl;
	node.get(
		dht::InfoHash::get(key),
		[](const std::shared_ptr<dht::Value> &v) {
			if (v->checkSignature())
			{
				std::cout << "Value is signed" << std::endl;
				std::cout << "Version: " << v->user_type << std::endl;
				auto rdat = dht::Value::unpack<SReflectorData>(*v);
				std::cout << "Callsign: '" << rdat.cs << "'" << std::endl;
				std::cout << "IPv4 Address: '" << rdat.ipv4 << "'" << std::endl;
				std::cout << "IPv6 Address: '" << rdat.ipv6 << "'" << std::endl;
				std::cout << "Port: " << rdat.port << std::endl;
				std::cout << "Modules: '" << rdat.modules << "'" << std::endl;
				std::cout << "URL: '" << rdat.url << "'" << std::endl;
				std::cout << "Email Address: '" << rdat.email << "'" << std::endl;
				if (rdat.peers.size())
				{
					std::cout << "Peer\tModule(s):" << std::endl;
					for (const auto &p : rdat.peers)
					{
						std::cout << p.first << '\t' << p.second << std::endl;
					}
				}
				else
				{
					std::cout << "Peers: none" << std::endl;
				}
			}
			else
				std::cout << "Value signature FAILED" << std::endl;
			return true;
		},
		[](bool success) {
			if (! success)
				std::cerr << "get() FAILED!" << std::endl;
		}
	);

	std::this_thread::sleep_for(std::chrono::seconds(2));

	node.join();

	return EXIT_SUCCESS;
}
