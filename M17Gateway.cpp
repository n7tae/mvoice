/*
 *   Copyright (c) 2020-2021,2025 by Thomas A. Early N7TAE
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

#include <sys/select.h>

#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring>

#include "M17Gateway.h"

CM17Gateway::CM17Gateway() : CBase()
{
	keep_running = false; // not running initially. this will be set to true in CMainWindow
}

CM17Gateway::~CM17Gateway()
{
	AM2M17.Close();
	ipv4.Close();
	ipv6.Close();
}

bool CM17Gateway::TryLock()
{
	return streamLock.try_lock();
}

void CM17Gateway::ReleaseLock()
{
	streamLock.unlock();
}

bool CM17Gateway::Init(const CFGDATA &cfgdata)
{
	currentStream.header.Initialize(54u, true);
	mlink.state = ELinkState::unlinked;
	if (AM2M17.Open("am2m17"))
		return true;
	M172AM.SetUp("m172am");
	if (cfgdata.eNetType != EInternetType::ipv6only)
	{
		if (ipv4.Open(CSockAddress(AF_INET, 0, "any")))  // use ephemeral port
			return true;
	}
	if (cfgdata.eNetType != EInternetType::ipv4only)
	{
		if (ipv6.Open(CSockAddress(AF_INET6, 0, "any"))) // use ephemeral port
			return true;
	}
	keep_running = true;
	CConfigure config;
	config.CopyFrom(cfgdata);
	config.CopyTo(cfg);
	return false;
}

void CM17Gateway::LinkCheck()
{
	if (mlink.receivePingTimer.time() > 30) // is the reflector okay?
	{
		// looks like we lost contact
		SendLog("Disconnected from %s, TIMEOUT...\n", mlink.cs.GetCS().c_str());
		mlink.state = ELinkState::unlinked;
		mlink.addr.Clear();
	}
}

void CM17Gateway::StreamTimeout()
{
	// set the frame number
	uint16_t fn = (currentStream.header.GetFrameNumber() + 1) % 0x8000u;
	currentStream.header.SetFrameNumber(fn | 0x8000u);
	// fill in a silent codec2
	switch (currentStream.header.GetFrameType() & 0x6u) {
	case 0x4u:
		{ //3200
			uint8_t silent[] = { 0x01u, 0x00u, 0x09u, 0x43u, 0x9cu, 0xe4u, 0x21u, 0x08u };
			memcpy(currentStream.header.GetVoiceData(),   silent, 8);
			memcpy(currentStream.header.GetVoiceData(false), silent, 8);
		}
		break;
	case 0x6u:
		{ // 1600
			uint8_t silent[] = { 0x01u, 0x00u, 0x04u, 0x00u, 0x25u, 0x75u, 0xddu, 0xf2u };
			memcpy(currentStream.header.GetVoiceData(), silent, 8);
		}
		break;
	default:
		break;
	}
	// calculate the crc
	currentStream.header.CalcCRC();
	// send the packet
	M172AM.Write(currentStream.header.GetCData(), currentStream.header.GetSize());
	// close the stream;
	currentStream.header.SetStreamID(0u);
	streamLock.unlock();
}

void CM17Gateway::SendMessage(const CPacket &pack)
{
	Write(pack.GetCData(), pack.GetSize(), destination);
}

void CM17Gateway::Process()
{
	fd_set fdset;
	timeval tv;
	int max_nfds = 0;
	const auto ip4fd = ipv4.GetSocket();
	const auto ip6fd = ipv6.GetSocket();
	const auto amfd = AM2M17.GetFD();
	if ((EInternetType::ipv6only != cfg.eNetType) && (ip4fd > max_nfds))
		max_nfds = ip4fd;
	if ((EInternetType::ipv4only != cfg.eNetType) && (ip6fd > max_nfds))
		max_nfds = ip6fd;
	if (amfd > max_nfds)
		max_nfds = amfd;
	while (keep_running)
	{
		if (ELinkState::linked == mlink.state)
		{
			LinkCheck();
		}
		else if (ELinkState::linking == mlink.state)
		{
			if (linkingTime.time() >= 5.0)
			{
				SendLog("Link request to %s timeout.\n", mlink.cs.GetCS().c_str());
				mlink.state = ELinkState::unlinked;
			}
		}

		if (currentStream.header.GetStreamId() && currentStream.lastPacketTime.time() >= 2.0)
		{
			StreamTimeout(); // current stream has timed out
		}

		FD_ZERO(&fdset);
		if (EInternetType::ipv6only != cfg.eNetType)
			FD_SET(ip4fd, &fdset);
		if (EInternetType::ipv4only != cfg.eNetType)
			FD_SET(ip6fd, &fdset);
		FD_SET(amfd, &fdset);
		tv.tv_sec = 0;
		tv.tv_usec = 40000;	// wait up to 40 ms for something to happen
		auto rval = select(max_nfds + 1, &fdset, 0, 0, &tv);
		if (0 > rval)
		{
			std::cerr << "select() error: " << strerror(errno) << std::endl;
			return;
		}

		bool is_packet = false;
		CPacket pack;
		socklen_t fromlen = sizeof(struct sockaddr_storage);
		int length;

		if (keep_running && (ip4fd >= 0) && FD_ISSET(ip4fd, &fdset))
		{
			length = recvfrom(ip4fd, pack.GetData(), MAX_PACKET_SIZE, 0, from17k.GetPointer(), &fromlen);
			is_packet = true;
			FD_CLR(ip4fd, &fdset);
		}

		if (keep_running && (ip6fd >= 0) && FD_ISSET(ip6fd, &fdset))
		{
			length = recvfrom(ip6fd, pack.GetData(), MAX_PACKET_SIZE, 0, from17k.GetPointer(), &fromlen);
			is_packet = true;
			FD_CLR(ip6fd, &fdset);
		}

		if (keep_running && is_packet)
		{
			switch (length)
			{
			case 4:  				// DISC, ACKN or NACK
				if ((ELinkState::unlinked != mlink.state) && (from17k == mlink.addr))
				{
					if (0 == memcmp(pack.GetCData(), "ACKN", 4))
					{
						mlink.state = ELinkState::linked;
						SendLog("Connected to %s\n", mlink.cs.GetCS().c_str());
						mlink.receivePingTimer.start();
					}
					else if (0 == memcmp(pack.GetCData(), "NACK", 4))
					{
						mlink.state = ELinkState::unlinked;
						SendLog("Link request refused from %s\n", mlink.cs.GetCS().c_str());
						mlink.state = ELinkState::unlinked;
					}
					else if (0 == memcmp(pack.GetCData(), "DISC", 4))
					{
						SendLog("Disconnected from %s\n", mlink.cs.GetCS().c_str());
						mlink.state = ELinkState::unlinked;
					}
					else
					{
						Dump("unexpected packet while not unlinked", pack.GetCData(), length);
					}
				}
				else
				{
					Dump("unexpected packet while unlinked", pack.GetCData(), length);
				}
				break;
			case 10: 				// PING or DISC
				if ((ELinkState::linked == mlink.state) && (from17k == mlink.addr))
				{
					if (0 == memcmp(pack.GetCData(), "PING", 4))
					{
						Send(mlink.pongPacket.magic, 10, mlink.addr);
						mlink.receivePingTimer.start();
					}
					else if (0 == memcmp(pack.GetCData(), "DISC", 4))
					{
						mlink.state = ELinkState::unlinked;
					}
					else
					{
						Dump("unexpected packet", pack.GetCData(), length);
					}
				}
				break;
			default:	// An M17 frame
				if (length > 37 && 0==memcmp(pack.GetCData(), "M17P", 4))
				{
					pack.Initialize(length, false);
					ProcessPacket(pack);
				}
				else if (54u==length && 0==memcmp(pack.GetCData(), "M17 ", 4))
				{
					pack.Initialize(length, true);
					ProcessFrame(pack);
				}
				else
					Dump("unknown packet", pack.GetCData(), length);
				break;
			}
		}

		if (keep_running && FD_ISSET(amfd, &fdset))
		{
			CPacket pack;
			pack.Initialize(54, true);
			length = AM2M17.Read(pack.GetData(), pack.GetSize());
			const CCallsign dest(pack.GetCDstAddress());
			//printf("DEST=%s=0x%02x%02x%02x%02x%02x%02x\n", dest.GetCS().c_str(), pack.GetCDstAddress()[0], pack.GetCDstAddress()[1], pack.GetCDstAddress()[2], pack.GetCDstAddress()[3], pack.GetCDstAddress()[4], pack.GetCDstAddress()[5]);
			//std::cout << "Read " << length << " bytes with dest='" << dest.GetCS() << "'" << std::endl;
			if (0==dest.GetCS(3).compare("M17") || 0==dest.GetCS(3).compare("URF")) // Linking a reflector
			{
				switch (mlink.state)
				{
				case ELinkState::linked:
					if (mlink.cs == dest) // this is heading in to the correct desination
					{
						Write(pack.GetCData(), length, mlink.addr);
					}
					break;
				case ELinkState::unlinked:
					if ('L' == dest.GetCS().at(7))
					{
						std::string ref(dest.GetCS(7));
						ref.resize(8, ' ');
						ref.resize(9, dest.GetModule());
						const CCallsign d(ref);
						SendLinkRequest(d);
					}
					break;
				default:
					break;
				}
			}
			else if (0 == dest.GetCS().compare("U"))
			{
				SM17RefPacket disc;
				memcpy(disc.magic, "DISC", 4);
				std::string s(cfg.sM17SourceCallsign);
				s.resize(8, ' ');
				s.append(1, cfg.cModule);
				CCallsign call(s);
				call.CodeOut(disc.cscode);
				Write(disc.magic, 10, mlink.addr);
			} else {
				Write(pack.GetCData(), length, destination);
			}
			FD_CLR(amfd, &fdset);
		}
	}
	AM2M17.Close();
	ipv4.Close();
	ipv6.Close();
}

void CM17Gateway::SetDestAddress(const std::string &address, uint16_t port)
{
	if (std::string::npos == address.find(':'))
		destination.Initialize(AF_INET, port, address.c_str());
	else
		destination.Initialize(AF_INET6, port, address.c_str());
}

void CM17Gateway::SendLinkRequest(const CCallsign &ref)
{
	mlink.addr = destination;
	mlink.cs = ref;
	mlink.from_mod = cfg.cModule;

	// make a CONN packet
	SM17RefPacket conn;
	memcpy(conn.magic, "CONN", 4);
	std::string source(cfg.sM17SourceCallsign);
	source.resize(8, ' ');
	source.append(1, cfg.cModule);
	const CCallsign from(source);
	from.CodeOut(conn.cscode);
	conn.mod = ref.GetModule();
	Write(conn.magic, 11, mlink.addr);	// send the link request
	// go ahead and make the pong packet
	memcpy(mlink.pongPacket.magic, "PONG", 4);
	from.CodeOut(mlink.pongPacket.cscode);

	// finish up
	mlink.state = ELinkState::linking;
	linkingTime.start();
}

bool CM17Gateway::ProcessPacket(const CPacket &pack)
{
	// check the lsf crc
	auto crccalc = crc.CalcCRC(pack.GetCData()+4, 28);
	//std::cout << "LSF:: " << std::hex << std::showbase << "calc crc: " << crccalc << " received crc: " << frame.GetCRC() << std::noshowbase << std::dec << std::endl;
	if (pack.GetCRC() == crccalc) {
		const CCallsign dst(pack.GetCDstAddress());
		const CCallsign src(pack.GetCSrcAddress());
		SendLog("PM Packet to %s from %s:\n", dst.GetCS().c_str(), src.GetCS().c_str());
		auto payloadlength = unsigned(pack.GetSize() - 36);
		auto datacrccalc = crc.CalcCRC(pack.GetCData()+34, payloadlength);
		uint16_t datacrc = pack.GetCRC(false);
		//std::cout << "Payload:: length: " << payloadlength << std::hex << std::showbase << " calc crc: " << datacrccalc << " received crc: " << datacrc << std::noshowbase << std::dec << std::endl;

		if (datacrc == datacrccalc)
		{
			if (0x05u == pack.GetCData()[34])
			{
				if (payloadlength > (strnlen((const char *)pack.GetCData()+34, payloadlength)))
				{
					SendLog("Message: %s\n", (const char *)pack.GetCData()+35);
				}
				else
				{
					SendLog("Could not find a null byte in the data!\n");
					Dump("Packet Mode data:", pack.GetCData(), pack.GetSize());
					return true;
				}
			}
			else
			{
				SendLog("Is not an SMS type data field, but type is %u\n", pack.GetCData()[34]);
				Dump("Packet Mode data:", pack.GetCData(), pack.GetSize());
				return true;
			}
		}
		else
		{
			SendLog("Has invalid data CRC\n");
			Dump("Invalid CRC in PM data:", pack.GetCData(), pack.GetSize());
			return false;
		}
	}
	else
	{
		SendLog("PM packet has bad LSF crc!\n");
		return false;
	}
	return true;
}

bool CM17Gateway::ProcessFrame(const CPacket &pack)
{
	if (currentStream.header.GetStreamId())
	{
		if (currentStream.header.GetStreamId() == pack.GetStreamId())
		{
			M172AM.Write(pack.GetCData(), pack.GetSize());
			currentStream.header.SetFrameNumber(pack.GetFrameNumber());
			uint16_t fn = pack.GetFrameNumber();
			if (fn & 0x8000u)
			{
				SendLog("Close stream id=0x%04x, duration=%.2f sec\n", pack.GetStreamId(), 0.04f * (0x7fffu & fn));
				currentStream.header.SetFrameNumber(0); // close the stream
				currentStream.header.SetStreamID(0u);
				streamLock.unlock();
			}
			else
			{
				currentStream.lastPacketTime.start();
			}
		}
		else
		{
			return false;
		}
	}
	else
	{
		// here comes a first packet, so try to lock it
		if (streamLock.try_lock())
		{
			// then init the currentStream
			auto check = crc.CalcCRC(pack.GetCData(), 52u);
			if (pack.GetCRC() != check)
				std::cout << Now() << "Header Packet crc=0x" << std::hex << pack.GetCRC() << " calculate=0x" << std::hex << check << std::endl;
			memcpy(currentStream.header.GetData(), pack.GetCData(), 54u);
			M172AM.Write(pack.GetCData(), 54u);
			const CCallsign src(pack.GetCSrcAddress());
			SendLog("Open stream id=0x%04x from %s at %s\n", pack.GetStreamId(), src.GetCS().c_str(), from17k.GetAddress());
			currentStream.lastPacketTime.start();
		}
		else
		{
			return false;
		}
	}
	return true;
}

void CM17Gateway::Write(const void *buf, const size_t size, const CSockAddress &addr) const
{
	if (AF_INET6 == addr.GetFamily())
		ipv6.Write(buf, size, addr);
	else
		ipv4.Write(buf, size, addr);
}

void CM17Gateway::Send(const void *buf, size_t size, const CSockAddress &addr) const
{
	if (AF_INET ==  addr.GetFamily())
		ipv4.Write(buf, size, addr);
	else
		ipv6.Write(buf, size, addr);
}
