/*
 *   Copyright (C) 2019 by Thomas A. Early N7TAE
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

#include <ctime>
#include <chrono>

class CTimer
{
public:
	CTimer() { start(); }
	~CTimer() {}
	void start() {
		starttime = std::chrono::steady_clock::now();
	}
	double time() {
		std::chrono::duration<double> elapsed(std::chrono::steady_clock::now() - starttime);
		return elapsed.count();
	}
private:
	std::chrono::steady_clock::time_point starttime;
};
