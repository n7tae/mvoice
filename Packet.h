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

#pragma once

#include <cstdint>
#include <string.h>
#include <memory>

#include "Callsign.h"

using SM17RefPacket = struct __attribute__((__packed__)) reflector_tag {
	char magic[4];
	uint8_t cscode[6];
	char mod;
}; // 11 bytes (but sometimes 4 or 10 bytes)

#define MAX_PACKET_SIZE 859

class CPacket
{
public:
	CPacket();
	// get pointer to different parts
	      uint8_t *GetData()        { return data; }
	const uint8_t *GetCData() const { return data; }
		  uint8_t *GetDstAddress();
	const uint8_t *GetCDstAddress() const;
	      uint8_t *GetSrcAddress();
	const uint8_t *GetCSrcAddress() const;
	      uint8_t *GetMetaData();
	const uint8_t *GetCMetaData() const;
          uint8_t *GetVoiceData(bool firsthalf = true);
	const uint8_t *GetCVoiceData(bool firsthalf = true) const;

	// get various 16 bit value in host byte order
	uint16_t GetStreamId()    const;
	uint16_t GetFrameType()   const;
	uint16_t GetFrameNumber() const;
	uint16_t GetCRC(bool first = true) const;

	// set 16 bit values in network byte order
	void SetStreamId(uint16_t sid);
	void SetFrameType(uint16_t ft);
	void SetFrameNumber(uint16_t fn);

	// get the state data
	size_t          GetSize() const { return size; }
	bool       IsStreamData() const { return isstream; }
	bool       IsPacketData() const { return not isstream; }
	bool       IsLastPacket() const;

	// set state data 
	void SetSize(size_t n) { size = n; }
	void SetType(bool iss) { isstream = iss; }
	void Initialize(size_t n, bool iss);

	// calculate and set CRC value(s)
	void CalcCRC();

private:
	uint16_t Get16At(size_t pos) const;
	void Set16At(size_t pos, uint16_t val);
	bool isstream;
	size_t size;
	uint8_t data[MAX_PACKET_SIZE+1];
};
