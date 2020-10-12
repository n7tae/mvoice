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

#include "HostQueue.h"

template <class T, class U, int N> class CTFrame
{
public:
	CTFrame()
	{
		memset(data, 0, N * sizeof(T));
		flag = 0;
	}

	CTFrame(const T *from)
	{
		memcpy(data, from, N * sizeof(T));
		flag = 0;
	}

	CTFrame(const CTFrame<T, U, N> &from)
	{
		memcpy(data, from.GetData(), N *sizeof(T));
		flag = from.GetFlag();
	}

	CTFrame<T, U, N> &operator=(const CTFrame<T, U, N> &from)
	{
		memcpy(data, from.GetData(), N * sizeof(T));
		flag = from.GetFlag();
		return *this;
	}

	const T *GetData() const
	{
		return data;
	}

	U GetFlag() const
	{
		return flag;
	}

	unsigned int Size() const
	{
		return N;
	}

	void SetFlag(U s)
	{
		flag = s;
	}

private:
	T data[N];
	U flag;
};

// audio
using CAudioFrame = CTFrame<short int, bool, 160>;
using CAudioQueue = CTQueue<CAudioFrame>;

// M17
using CC2DataFrame = CTFrame<unsigned char, bool, 8>;
using CC2DataQueue = CTQueue<CC2DataFrame>;
