#pragma once

/*
 *   Copyright (C) 2019-2020 by Thomas Early N7TAE
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

#include <iostream>
#include <cstring>

#include <strings.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

class CSockAddress
{
public:
	CSockAddress()
	{
		Clear();
	}

	CSockAddress(const char *address, uint16_t port, int type = SOCK_DGRAM)
	{
		Clear();
		struct addrinfo hints, *result;
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = type;
		if (0 == getaddrinfo(address, (port ? std::to_string(port).c_str() : nullptr), &hints, &result))
		{
			memcpy(&addr, result->ai_addr, result->ai_addrlen);
			addr.ss_family = result->ai_family;
			freeaddrinfo(result);
		}
		else
		{
			std::cerr << "ERROR: Couldn't find IP Address for '" << address << "'" << std::endl;
			addr.ss_family = AF_INET;
		}
		SetPort(port);
	}

	CSockAddress(const int family, const unsigned short port = 0, const char *address = NULL)
	{
		Initialize(family, port, address);
	}

	~CSockAddress() {}

	void Initialize(const int family, const uint16_t port = 0U, const char *address = NULL)
	{
		Clear();
		addr.ss_family = family;
		if (AF_INET == family) {
			auto addr4 = (struct sockaddr_in *)&addr;
			addr4->sin_port = htons(port);
			if (address) {
				if (0 == strncasecmp(address, "loc", 3))
					inet_pton(AF_INET, "127.0.0.1", &(addr4->sin_addr));
				else if (0 == strncasecmp(address, "any", 3))
					inet_pton(AF_INET, "0.0.0.0", &(addr4->sin_addr));
				else if (address) {
					if (1 > inet_pton(AF_INET, address, &(addr4->sin_addr)))
						std::cerr << "Address Initialization Error: '" << address << "' is not a valdid IPV4 address!" << std::endl;
				}
			}
		} else if (AF_INET6 == family) {
			auto addr6 = (struct sockaddr_in6 *)&addr;
			addr6->sin6_port = htons(port);
			if (address) {
				if (0 == strncasecmp(address, "loc", 3))
					inet_pton(AF_INET6, "::1", &(addr6->sin6_addr));
				else if (0 == strncasecmp(address, "any", 3))
					inet_pton(AF_INET6, "::", &(addr6->sin6_addr));
				else if (address) {
					if (1 > inet_pton(AF_INET6, address, &(addr6->sin6_addr)))
						std::cerr << "Address Initialization Error: '" << address << "' is not a valid IPV6 address!" << std::endl;
				}
			}
		} else {
			addr.ss_family = AF_INET;
			std::cerr << "Error: Wrong address family type:" << family << " for [" << (address ? address : "NULL") << "]:" << port << std::endl;
		}
	}

	CSockAddress &operator=(const CSockAddress &from)
	{
		Clear();
		if (AF_INET == from.addr.ss_family)
			memcpy(&addr, &from.addr, sizeof(struct sockaddr_in));
		else
			memcpy(&addr, &from.addr, sizeof(struct sockaddr_in6));
		strcpy(straddr, from.straddr);
		return *this;
	}

	bool operator==(const CSockAddress &rhs) const	// doesn't compare ports, only addresses and families
	{
		if (addr.ss_family != rhs.addr.ss_family)
			return false;
		if (AF_INET == addr.ss_family) {
			auto l = (struct sockaddr_in *)&addr;
			auto r = (struct sockaddr_in *)&rhs.addr;
			return (l->sin_addr.s_addr == r->sin_addr.s_addr);
		} else if (AF_INET6 == addr.ss_family) {
			auto l = (struct sockaddr_in6 *)&addr;
			auto r = (struct sockaddr_in6 *)&rhs.addr;
			return (0 == memcmp(&(l->sin6_addr), &(r->sin6_addr), sizeof(struct in6_addr)));
		}
		return false;
	}

	bool operator!=(const CSockAddress &rhs) const	// doesn't compare ports, only addresses and families
	{
		if (addr.ss_family != rhs.addr.ss_family)
			return true;
		if (AF_INET == addr.ss_family) {
			auto l = (struct sockaddr_in *)&addr;
			auto r = (struct sockaddr_in *)&rhs.addr;
			return (l->sin_addr.s_addr != r->sin_addr.s_addr);
		} else if (AF_INET6 == addr.ss_family) {
			auto l = (struct sockaddr_in6 *)&addr;
			auto r = (struct sockaddr_in6 *)&rhs.addr;
			return (0 != memcmp(&(l->sin6_addr), &(r->sin6_addr), sizeof(struct in6_addr)));
		}
		return true;
	}

	operator const char *() const { return GetAddress(); }

	friend std::ostream &operator<<(std::ostream &stream, const CSockAddress &addr)
	{
		const char *sz = addr;
		if (AF_INET6 == addr.GetFamily())
			stream << "[" << sz << "]";
		else
			stream << sz;
		const uint16_t port = addr.GetPort();
		if (port)
			stream << ":" << port;
		return stream;
	}

	bool AddressIsZero() const
	{
		if (AF_INET == addr.ss_family) {
			 auto addr4 = (struct sockaddr_in *)&addr;
			return (addr4->sin_addr.s_addr == 0U);
		} else {
			auto addr6 = (struct sockaddr_in6 *)&addr;
			for (unsigned int i=0; i<16; i++) {
				if (addr6->sin6_addr.s6_addr[i])
					return false;
			}
			return true;
		}
	}

	void ClearAddress()
	{
		if (AF_INET == addr.ss_family) {
			auto addr4 = (struct sockaddr_in *)&addr;
			addr4->sin_addr.s_addr = 0U;
			strcpy(straddr, "0.0.0.0");
		} else {
			auto addr6 = (struct sockaddr_in6 *)&addr;
			memset(&(addr6->sin6_addr.s6_addr), 0, 16);
			strcpy(straddr, "::");
		}
	}

	const char *GetAddress() const
	{
		if (straddr[0])
			return straddr;
		if (AF_INET == addr.ss_family) {
			auto addr4 = (struct sockaddr_in *)&addr;
			inet_ntop(AF_INET, &(addr4->sin_addr), straddr, INET6_ADDRSTRLEN);
		} else if (AF_INET6 == addr.ss_family) {
			auto addr6 = (struct sockaddr_in6 *)&addr;
			inet_ntop(AF_INET6, &(addr6->sin6_addr), straddr, INET6_ADDRSTRLEN);
		} else {
			std::cerr << "Unknown socket family: " << addr.ss_family << std::endl;
		}
		return straddr;
	}

    int GetFamily() const
    {
        return addr.ss_family;
    }

	unsigned short GetPort() const
	{
		if (AF_INET == addr.ss_family) {
			auto addr4 = (struct sockaddr_in *)&addr;
			return ntohs(addr4->sin_port);
		} else if (AF_INET6 == addr.ss_family) {
			auto addr6 = (struct sockaddr_in6 *)&addr;
			return ntohs(addr6->sin6_port);
		} else
			return 0;
	}

	void SetPort(const uint16_t newport)
	{
		if (AF_INET == addr.ss_family) {
			auto addr4 = (struct sockaddr_in *)&addr;
			addr4->sin_port = htons(newport);
		} else if (AF_INET6 == addr.ss_family) {
			auto addr6 = (struct sockaddr_in6 *)&addr;
			addr6->sin6_port = htons(newport);
		}
	}

	struct sockaddr *GetPointer()
	{
		memset(straddr, 0, INET6_ADDRSTRLEN);	// things might change
		return (struct sockaddr *)&addr;
	}

	const struct sockaddr *GetCPointer() const
	{
		return (const struct sockaddr *)&addr;
	}

	size_t GetSize() const
	{
		if (AF_INET == addr.ss_family)
			return sizeof(struct sockaddr_in);
		else
			return sizeof(struct sockaddr_in6);
	}

	void Clear()
	{
		memset(&addr, 0, sizeof(struct sockaddr_storage));
		memset(straddr, 0, INET6_ADDRSTRLEN);
	}

private:
	struct sockaddr_storage addr;
	mutable char straddr[INET6_ADDRSTRLEN];
};
