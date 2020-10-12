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

class CM17RouteMap
{
public:
	CM17RouteMap() {}
	~CM17RouteMap();
	const std::shared_ptr<CSockAddress> Find(const std::string &cs) const;
	const std::shared_ptr<CSockAddress> FindBase(const std::string &call) const;
	void Update(const std::string &cs, const std::string &addr);
	void Open();
	void Save() const;
	const std::list<std::string> GetKeys() const;
	void Erase(const std::string &cs);
	size_t Size() const;

private:
	std::map<std::string, std::shared_ptr<CSockAddress>> baseMap;
	std::map<std::string, std::string> cs2baseMap;
	mutable std::mutex mux;
};
