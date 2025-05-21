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
#include <mutex>
#include <condition_variable>
#include <string>

template <class T> class CTQueue
{
public:
	CTQueue() : q(), m(), c() {}

	~CTQueue()
	{
		Clear();
	}

	void Push(T item)
	{
		std::lock_guard<std::mutex> lock(m);
		q.push(std::move(item));
		c.notify_one();
	}

	T WaitPop()
	{
		std::unique_lock<std::mutex> lock(m);
		while (q.empty())
		{
			c.wait(lock);
		}
		T item = std::move(q.front());
		q.pop();
		return item;
	}

	bool IsEmpty() const
	{
		std::unique_lock<std::mutex> lock(m);
		return q.empty();
	}

	void Clear()
	{
		std::unique_lock<std::mutex> lock(m);
		while (!q.empty())
			q.pop();
	}

private:
	std::queue<T> q;
	mutable std::mutex m;
	std::condition_variable c;
};

template <class T, int N> class CTFrame
{
public:
	CTFrame()
	{
		memset(data, 0, N * sizeof(T));
		flag = false;
	}

	CTFrame(const T *from)
	{
		memcpy(data, from, N * sizeof(T));
		flag = false;
	}

	CTFrame(const CTFrame<T, N> &from)
	{
		memcpy(data, from.GetData(), N *sizeof(T));
		flag = from.GetFlag();
	}

	CTFrame<T, N> &operator=(const CTFrame<T, N> &from)
	{
		memcpy(data, from.GetData(), N * sizeof(T));
		flag = from.GetFlag();
		return *this;
	}

	const T *GetData() const
	{
		return data;
	}

	bool GetFlag() const
	{
		return flag;
	}

	unsigned int Size() const
	{
		return N;
	}

	void SetFlag(bool s)
	{
		flag = s;
	}

private:
	T data[N];
	bool flag;
};

// audio
using CAudioFrame = CTFrame<short int, 160>;
using CAudioQueue = CTQueue<CAudioFrame>;

// M17
using CC2DataFrame = CTFrame<unsigned char, 8>;
using CC2DataQueue = CTQueue<CC2DataFrame>;
