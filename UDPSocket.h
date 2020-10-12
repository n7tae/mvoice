#pragma once
//
//  Copyright © 2020 Thomas A. Early, N7TAE
//
// ----------------------------------------------------------------------------
//
//    xlxd is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    xlxd is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

#include "SockAddress.h"

#define UDP_BUFFER_LENMAX 1024

class CUDPSocket
{
public:
	CUDPSocket();

	~CUDPSocket();

	bool Open(const CSockAddress &addr);
	void Close(void);

	int GetSocket(void) const { return m_fd; }
	unsigned short GetPort() const { return m_addr.GetPort(); }

	size_t Read(unsigned char *buf, const size_t size, CSockAddress &addr);
	void Write(const void *buf, const size_t size, const CSockAddress &addr) const;

protected:
	int m_fd;
	CSockAddress m_addr;
};
