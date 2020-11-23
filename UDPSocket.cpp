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

#include <string.h>

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include "UDPSocket.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CUDPSocket::CUDPSocket() : m_fd(-1) {}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CUDPSocket::~CUDPSocket()
{
	Close();
}

////////////////////////////////////////////////////////////////////////////////////////
// open & close

bool CUDPSocket::Open(const CSockAddress &addr)
{
	// create socket
	m_fd = socket(addr.GetFamily(), SOCK_DGRAM, 0);
	if (0 > m_fd)
	{
		std::cerr << "Cannot create socket on " << addr << ", " << strerror(errno) << std::endl;
		return true;
	}

	if (0 > fcntl(m_fd, F_SETFL, O_NONBLOCK))
	{
		std::cerr << "cannot set socket " << addr << " to non-blocking: " << strerror(errno) << std::endl;
		close(m_fd);
		m_fd = -1;
		return true;
	}

	const int reuse = 1;
	if (0 > setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)))
	{
		std::cerr << "Cannot set the UDP socket option on " << m_addr << ", err: " << strerror(errno) << std::endl;
		close(m_fd);
		m_fd = -1;
		return true;
	}

	// initialize sockaddr struct
	m_addr = addr;

	if (0 != bind(m_fd, m_addr.GetCPointer(), m_addr.GetSize()))
	{
		std::cerr << "bind failed on " << m_addr << ", " << strerror(errno) << std::endl;
		close(m_fd);
		m_fd = -1;
		return true;
	}

	if (0 == m_addr.GetPort()) {	// get the assigned port for an ephemeral port request
		CSockAddress a;
		socklen_t len = sizeof(struct sockaddr_storage);
		if (getsockname(m_fd, a.GetPointer(), &len)) {
			std::cerr << "getsockname error " << m_addr << ", " << strerror(errno) << std::endl;
			Close();
			return false;
		}
		if (a != m_addr)
			std::cout << "getsockname didn't return the same address as set: returned " << a.GetAddress() << ", should have been " << m_addr.GetAddress() << std::endl;

		m_addr.SetPort(a.GetPort());
	}

	return false;
}

void CUDPSocket::Close(void)
{
	if ( m_fd >= 0 )
	{
		std::cout << "Closing socket " << m_fd << " on " << m_addr << std::endl;
		close(m_fd);
		m_fd = -1;
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// read

size_t CUDPSocket::Read(unsigned char *buf, const size_t size, CSockAddress &Ip)
{
	if ( 0 > m_fd )
		return 0;

	unsigned int len = sizeof(struct sockaddr_storage);
	auto rval = recvfrom(m_fd, buf, size, 0, Ip.GetPointer(), &len);
	if (0 > rval)
		std::cerr << "Read error on port " << m_addr << ": " << strerror(errno) << std::endl;

	return rval;
}

void CUDPSocket::Write(const void *Buffer, const size_t size, const CSockAddress &Ip) const
{
	//std::cout << "Sent " << size << " bytes to " << Ip << std::endl;
	auto rval = sendto(m_fd, Buffer, size, 0, Ip.GetCPointer(), Ip.GetSize());
	if (0 > rval)
		std::cerr << "Write error to " << Ip << ", " << strerror(errno) << std::endl;
	else if ((size_t)rval != size)
		std::cerr << "Short write, " << rval << "<" << size << " to " << Ip << std::endl;
}
