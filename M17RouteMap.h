/*
 *   Copyright (c) 2020 by Thomas A. Early N7TAE
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

#pragma once

#include <mutex>
#include <string>
#include <memory>
#include <map>
#include <list>

#include "SockAddress.h"

using SHost = struct host_tag
{
	std::string url, ip4addr, ip6addr;
	uint16_t port;
};

class CM17RouteMap
{
public:
	CM17RouteMap() {}
	~CM17RouteMap();
	const std::shared_ptr<SHost> Find(const std::string &cs) const;
	void Update(const std::string &cs, const std::string &url, const std::string &ip4addr, const std::string &ip6addr, const uint16_t port = 17000);
	void Save() const;
	void ReadAll();
	const std::list<std::string> GetKeys() const;
	void Erase(const std::string &cs);
	size_t Size() const;

private:
	void Read(const char *filename);
	void ReadJson(const char *filename);
	std::map<std::string, std::shared_ptr<SHost>> baseMap;
	mutable std::mutex mux;
};
