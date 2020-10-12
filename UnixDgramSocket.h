/*
 *   Copyright (C) 2019 by Thomas Early N7TAE
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

#include <string>
#include <stdlib.h>
#include <sys/un.h>

class CUnixDgramReader
{
public:
	CUnixDgramReader();
	~CUnixDgramReader();
	bool Open(const char *path);
	ssize_t Read(void *buf, size_t size);
	void Close();
	int GetFD();
private:
	int fd;
};

class CUnixDgramWriter
{
public:
	CUnixDgramWriter();
	~CUnixDgramWriter();
	void SetUp(const char *path);
	ssize_t Write(const void *buf, size_t size);
	ssize_t Write(const std::string &s) { return Write(s.c_str(), s.size()); }
private:
	struct sockaddr_un addr;
};
