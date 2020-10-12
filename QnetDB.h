#pragma once
/*
 *   Copyright (C) 2020 by Thomas Early N7TAE
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

#include <stdio.h>
#include <sqlite3.h>
#include <string>
#include <list>

#include "HostQueue.h"

class CLink {
public:
	CLink(const std::string &call, const unsigned char *addr, time_t ltime) : callsign(call) , address((const char *)addr) , linked_time(ltime) {}

	CLink(const CLink &from)
	{
		callsign.assign(from.callsign);
		address.assign(from.address),
		linked_time=from.linked_time;
	}

	CLink &operator=(const CLink &from)
	{
		callsign.assign(from.callsign);
		address.assign(from.address),
		linked_time=from.linked_time;
		return *this;
	}

	~CLink() {}

	std::string callsign, address;
	time_t linked_time;
};

class CQnetDB {
public:
	CQnetDB() : db(NULL) {}
	~CQnetDB() { if (db) sqlite3_close(db); }
	bool Open(const char *name);
	bool UpdateLH(const char *callsign, const char *sfx, const char module, const char *reflector);
	bool UpdateLS(const char *address, const char from_mod, const char *to_callsign, const char to_mod, time_t connect_time);
	bool UpdateGW(CHostQueue &);
	bool DeleteLS(const char *address);
	bool FindLS(const char mod, std::list<CLink> &linklist);
	bool FindGW(const char *name, std::string &address, unsigned short &port);
	bool FindGW(const char *name);
	void ClearLH();
	void ClearLS();
	void ClearGW();
	int Count(const char *table);

private:
	bool Init();
	bool UpdateGW(const char *name, const char *address, unsigned short port);
	sqlite3 *db;
};
