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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "UnixDgramSocket.h"

CUnixDgramReader::CUnixDgramReader() : fd(-1) {}

CUnixDgramReader::~CUnixDgramReader()
{
	Close();
}

bool CUnixDgramReader::Open(const char *path)	// returns true on failure
{
	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) {
		fprintf(stderr, "CUnixDgramReader::Open: socket() failed: %s\n", strerror(errno));
		return true;
	}
	//fcntl(fd, F_SETFL, O_NONBLOCK);

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path+1, path, sizeof(addr.sun_path)-2);

	int rval = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
	if (rval < 0) {
		fprintf(stderr, "CUnixDgramReader::Open: bind() failed: %s\n", strerror(errno));
		close(fd);
		fd = -1;
		return true;
	}
	return false;
}

ssize_t CUnixDgramReader::Read(void *buf, size_t size)
{
	if (fd < 0)
		return -1;
	ssize_t len = read(fd, buf, size);
	if (len < 1)
		fprintf(stderr, "CUnixDgramReader::Read read() returned %d: %s\n", int(len), strerror(errno));
	return len;
}

void CUnixDgramReader::Close()
{
	if (fd >= 0)
		close(fd);
	fd = -1;
}

int CUnixDgramReader::GetFD()
{
	return fd;
}

CUnixDgramWriter::CUnixDgramWriter() {}

CUnixDgramWriter::~CUnixDgramWriter() {}

void CUnixDgramWriter::SetUp(const char *path)	// returns true on failure
{
	// setup the socket address
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path+1, path, sizeof(addr.sun_path)-2);
}

ssize_t CUnixDgramWriter::Write(const void *buf, size_t size)
{
	// open the socket
	int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) {
		fprintf(stderr, "Failed to open socket %s : %s\n", addr.sun_path+1, strerror(errno));
		return -1;
	}
	// connect to the receiver
	int rval = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	if (rval < 0) {
		fprintf(stderr, "Failed to connect to socket %s : %s\n", addr.sun_path+1, strerror(errno));
		close(fd);
		return -1;
	}

	ssize_t written = 0;
	int count = 0;
	while (written <= 0) {
		written = write(fd, buf, size);
		if (written == (ssize_t)size)
			break;
		else if (written < 0)
			fprintf(stderr, "ERROR: faied to write to %s : %s\n", addr.sun_path+1, strerror(errno));
		else if (written == 0)
			fprintf(stderr, "Warning: zero bytes written to %s\n", addr.sun_path+1);
		else if (written != (ssize_t)size) {
			fprintf(stderr, "ERROR: only %d of %d bytes written to %s\n", (int)written, (int)size, addr.sun_path+1);
			break;
		}
		if (++count >= 100) {
			fprintf(stderr, "ERROR: Write failed after %d attempts\n", count-1);
			break;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(5));
	}

	close(fd);
	return written;
}
