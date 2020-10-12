#pragma once
/*
 *   Copyright 2017-2020 by Thomas Early, N7TAE
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

#include <cstdint>

#include <arpa/inet.h>

// M17 Packet
//all structures must be big endian on the wire, so you'll want htonl (man byteorder 3) and such.
using SM17Lich = struct __attribute__((__packed__)) _LICH {
	uint8_t  addr_dst[6]; //48 bit int - you'll have to assemble it yourself unfortunately
	uint8_t  addr_src[6];
	uint16_t frametype; //frametype flag field per the M17 spec
	uint8_t  nonce[14]; //bytes for the nonce
}; // 6 + 6 + 2 + 14 = 28 bytes = 224 bits

// the one and only frame
using SM17Frame = struct __attribute__((__packed__)) _ip_frame {
	uint8_t  magic[4];
	uint16_t streamid;
	SM17Lich lich;
	uint16_t framenumber;
	uint8_t  payload[16];
	uint16_t  crc; 	//16 bit CRC
	uint16_t GetCRC        (void) const        { return ntohs(crc);            }
	uint16_t GetFrameType  (void) const        { return ntohs(lich.frametype); }
	uint16_t GetStreamID   (void) const        { return ntohs(streamid);       }
	uint16_t GetFrameNumber(void) const        { return ntohs(framenumber);    }
	void     SetCRC        (const uint16_t cr) { crc            = htons(cr);   }
	void     SetFrameType  (const uint16_t ft) { lich.frametype = htons(ft);   }
	void     SetFrameNumber(const uint16_t fn) { framenumber    = htons(fn);   }

}; // 4 + 2 + 28 + 2 + 16 + 2 = 54 bytes = 432 bits

// reflector packet for linking, unlinking, pinging, etc
using SM17RefPacket = struct __attribute__((__packed__)) reflector_tag {
	char magic[4];
	uint8_t cscode[6];
	char mod;
}; // 11 bytes (but sometimes 4 or 10 bytes)
