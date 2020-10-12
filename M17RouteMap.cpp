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

#include <fstream>

#include "M17RouteMap.h"

#ifndef CFG_DIR
#define CFG_DIR "/tmp/"
#endif

CM17RouteMap::~CM17RouteMap()
{
	mux.lock();
	baseMap.clear();
	cs2baseMap.clear();
	mux.unlock();
}

const std::shared_ptr<CSockAddress> CM17RouteMap::Find(const std::string &callsign) const
{
	std::shared_ptr<CSockAddress> rval = nullptr;
	mux.lock();
	auto cit = cs2baseMap.find(callsign);
	if (cs2baseMap.end() != cit) {
		auto bit = baseMap.find(cit->second);
		if (bit != baseMap.end())
			rval = bit->second;
	}
	mux.unlock();
	return rval;
}

const std::shared_ptr<CSockAddress> CM17RouteMap::FindBase(const std::string &call) const
{
	std::shared_ptr<CSockAddress> rval = nullptr;
	auto pos = call.find_first_of(" ./");
	if (pos < 3)
		return rval;
	const std::string b(call, 0, pos);
	mux.lock();
	const auto it = baseMap.find(b);
	if (it != baseMap.end())
		rval = it->second;
	mux.unlock();
	return rval;
}

void CM17RouteMap::Update(const std::string &callsign, const std::string &address)
{
	std::string base;
	auto pos = callsign.find_first_of(" /.");
	if (pos < 3)
		return;
	mux.lock();
	auto cit = cs2baseMap.find(callsign);
	if (cs2baseMap.end() == cit) {
		base.assign(callsign, 0, pos);
		cs2baseMap[callsign] = base;
	} else {
		base.assign(cit->second);
	}
	baseMap[base] = std::make_shared<CSockAddress>(address.c_str(), 17000);
	mux.unlock();
}

void CM17RouteMap::Open()
{
	std::string path(CFG_DIR);
	path.append("M17-destinations.cfg");
	std::ifstream file(path, std::ifstream::in);
	if (file.is_open()) {
		char line[128];
		while (file.getline(line, 128)) {
			const char *key = strtok(line, "=");
			if ((! key) || (*key == '#') | (0==strlen(key))) continue;
			const char *val = strtok(NULL, " \t\r\n");
			if (! val) continue;
			Update(key, val);
		}
		file.close();
	}
}

void CM17RouteMap::Save() const
{
	std::string path(CFG_DIR);
	path.append("M17-destinations.cfg");
	std::ofstream file(path.c_str(), std::ofstream::out | std::ofstream::trunc);
	if (file.is_open()) {
		mux.lock();
		for (const auto &pair : cs2baseMap) {
			auto bit = baseMap.find(pair.second);
			if (baseMap.end() != bit)
				file << pair.first << '=' << bit->second->GetAddress() << std::endl;
		}
		file.close();
		mux.unlock();
	}
}

const std::list<std::string> CM17RouteMap::GetKeys() const
{
	std::list<std::string> keys;
	mux.lock();
	for (const auto &pair : cs2baseMap)
		keys.push_back(pair.first);
	mux.unlock();
	return keys;
}

void CM17RouteMap::Erase(const std::string &cs)
{
	mux.lock();
	auto it = cs2baseMap.find(cs);
	if (it != cs2baseMap.end())
		cs2baseMap.erase(it);
	mux.unlock();
}

size_t CM17RouteMap::Size() const
{
	return cs2baseMap.size();
}
