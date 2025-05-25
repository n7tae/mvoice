//
//  Copyright © 2020-2025 Thomas A. Early, N7TAE
//
// ----------------------------------------------------------------------------
//
//    m17ref is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    m17ref is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    with this software.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

#include <arpa/inet.h>

#include "Packet.h"
#include "CRC.h"

static const CCRC CRC;

CPacket::CPacket()
{
	memset(data, 0, MAX_PACKET_SIZE + 1);
	size = 0;
}

void CPacket::Initialize(size_t n, bool bis)
{
	memcpy(data, bis ? "M17 " : "M17P", 4);
	size = n;
	isstream = bis;
}

const uint8_t *CPacket::GetCDstAddress() const
{
	return data + (isstream ? 6u : 4u);
}

uint8_t *CPacket::GetDstAddress()
{
	return data + (isstream ? 6u : 4u);
}

const uint8_t *CPacket::GetCSrcAddress() const
{
	return data + (isstream ? 12u : 10u);
}

uint8_t *CPacket::GetSrcAddress()
{
	return data + (isstream ? 12u : 10u);
}

// returns the StreamID in host byte order
uint16_t CPacket::GetStreamId() const
{
	return isstream ? 0x100u * data[4] + data[5] : 0u;
}

void CPacket::SetStreamID(uint16_t sid)
{
	if (isstream)
	{
		data[4] = 0xffu & (sid >> 8);
		data[5] = 0xffu & sid;
	}
}

// returns LSD:TYPE in host byte order
uint16_t CPacket::GetFrameType() const
{
	if (isstream)
		return 0x100u * data[18] + data[19];

	return 0x100u * data[16] + data[17];
}

void CPacket::SetFrameType(uint16_t ft)
{
	unsigned offset = isstream ? 18 : 16;
	data[offset]   = 0xffu & (ft >> 8);
	data[offset+1] = 0xffu & ft;
}

uint16_t CPacket::GetFrameNumber() const
{
	if (isstream)
		return 0x100u * data[34] + data[35];
	else
		return 0u;
}

uint16_t CPacket::GetCRC(bool first) const
{
	uint16_t rval;
	if (isstream)
	{
		rval =  0x100u * data[52] + data[53];
	}
	else
	{
		if (first)
			rval = 0x100u * data[32] + data[33];
		else
			rval = 0x100u * data[size - 2] + data[size - 1];
	}
	return rval;
}

void CPacket::SetFrameNumber(uint16_t fn)
{
	if (isstream)
	{
		data[34] = 0xffu & (fn >> 8);
		data[35] = 0xffu & fn;
	}
}

const uint8_t *CPacket::GetCVoiceData(bool firsthalf) const
{
	if (isstream)
		return data + (firsthalf ? 36u : 44u);
	else
		return 0u;
}

uint8_t *CPacket::GetVoiceData(bool firsthalf)
{
	if (isstream)
		return data + (firsthalf ? 36u : 44u);
	else
		return 0u;
}

bool CPacket::IsLastPacket() const
{
	if (isstream and size)
		return 0x8000u == (0x8000u & GetFrameNumber());
	return false;
}

void CPacket::CalcCRC()
{
	if (isstream)
	{
		auto crc = CRC.CalcCRC(data, 52);
		data[52] = 0xffu & (crc >> 8);
		data[53] = 0xffu & crc;
	}
	else
	{	// set the CRC for the LSF
		auto crc = CRC.CalcCRC(data+4, 28);
		data[32] = 0xffu & (crc >> 8);
		data[33] = 0xffu & crc;
		// now for the payload
		crc = CRC.CalcCRC(data+34, size-36);
		data[size-2] = 0xffu & (crc >> 8);
		data[size-1] = 0xffu & crc;
	}
}
