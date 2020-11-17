/*
 *   Copyright (c) 2020 by Thomas A. Early N7TAE
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

#include <atomic>
#include <string>

#include "UnixDgramSocket.h"
#include "SockAddress.h"
#include "Configure.h"
#include "UDPSocket.h"
#include "Callsign.h"
#include "Timer.h"
#include "Packet.h"
#include "QnetDB.h"
#include "Base.h"
#include "CRC.h"

enum class ELinkState { unlinked, linking, linked };

using SM17Link = struct sm17link_tag
{
	SM17RefPacket pongPacket;
	CSockAddress addr;
	CCallsign cs;
	char from_mod;
	ELinkState state;
	CTimer receivePingTimer;
};

using SStream = struct stream_tag
{
	CTimer lastPacketTime;
	SM17Frame header;
};

class CM17Gateway : public CBase
{
public:
	CM17Gateway();
	~CM17Gateway();
	bool Init(const CFGDATA &cfgdata);
	void Process();
	void SetDestAddress(const std::string &address, uint16_t port);

	std::atomic<bool> keep_running;

private:
	CFGDATA cfg;
	CQnetDB qnDB;
	CCRC crc;
	CUnixDgramReader AM2M17;
	CUnixDgramWriter M172AM;
	CUDPSocket ipv4, ipv6;
	SM17Link mlink;
	CTimer linkingTime;
	SStream currentStream;
	std::string qnvoice_file;
	CSockAddress from17k, destination;

	void LinkCheck();
	void Write(const void *buf, const size_t size, const CSockAddress &addr) const;
	void PlayAudioMessage(const char *msg);
	void StreamTimeout();
	void PlayVoiceFile();
	void PlayAudioNotifyMessage(const char *msg);
	void Send(const void *buf, size_t size, const CSockAddress &addr) const;
	bool ProcessFrame(const uint8_t *buf);
	bool ProcessAM(const uint8_t *buf);
	void SendLinkRequest(const CCallsign &ref);
};
