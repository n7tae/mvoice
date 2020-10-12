/*
 *   Copyright (c) 2019 by Thomas A. Early N7TAE
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

#include <queue>
#include <string>

class CHost {
public:
	CHost(const CHost &from)
	{
		name.assign(from.name);
		addr.assign(from.addr);
		port = from.port;
	}

	CHost(const std::string n, const std::string a, unsigned short p)
	{
		name.assign(n);
		addr.assign(a);
		port = p;
	}

	CHost &operator=(const CHost &from)
	{
		name.assign(from.name);
		addr.assign(from.addr);
		port = from.port;
		return *this;
	}

	std::string name, addr;
	unsigned short port;
};

template <class T> class CTQueue
{
public:
	~CTQueue()
	{
		Clear();
	}

	void Push(T item)
	{
		queue.push(item);
	}

	T Pop()
	{
		T item = queue.front();
		queue.pop();
		return item;
	}

	bool Empty()
	{
		return queue.empty();
	}

	void Clear()
	{
		while (queue.size())
			queue.pop();
	}

private:
	std::queue<T> queue;
};

using CHostQueue = CTQueue<CHost>;
