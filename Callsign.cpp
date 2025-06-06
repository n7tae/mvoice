/*
 *   Copyright (c) 2025 by Thomas A. Early N7TAE
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

#include "Callsign.h"

#define M17CHARACTERS " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/."

CCallsign::CCallsign()
{
	memset(cs, 0, sizeof(cs));
	coded = 0;
}

CCallsign::CCallsign(const std::string &callsign)
{
	CSIn(callsign);
}

CCallsign::CCallsign(const uint8_t *in)
{
	CodeIn(in);
}

void CCallsign::CSIn(const std::string &callsign)
{
	memset(cs, 0, sizeof(cs));
	if (0 == callsign.compare("@ALL"))
	{
		strncpy(cs, "@ALL", sizeof(cs));
		coded = 0xffffffffffffu;
		return;
	}
	const std::string m17_alphabet(M17CHARACTERS);
	memcpy(cs, callsign.c_str(), (callsign.size()<10) ? callsign.size() : 9);	// no more than 9 chars
	coded = 0;
	for( int i=int(callsign.size()-1); i>=0; i-- ) {
		auto pos = m17_alphabet.find(cs[i]);
		if (pos == std::string::npos) {
			if ('#' == cs[i] and 0 == i) {
				coded += 0xee6b28000000u;
				break;
			}
			pos = 0;
		}
		cs[i] = m17_alphabet[pos];	// replace with valid character
		coded *= 40;
		coded += pos;
	}
	// strip trailing spaces (just to be nice!)
	auto len = strlen(cs);
	while (len && (' ' == cs[len-1]))
		cs[--len] = 0;
}

const std::string CCallsign::GetCS(unsigned len) const
{
	if (len > 9)
		len = 9;
	std::string rval(cs);
	if (len)
		rval.resize(len, ' ');
	return rval;
}

void CCallsign::CodeIn(const uint8_t *in)
{
	const std::string m17_alphabet(M17CHARACTERS);
	memset(cs, 0, 10);
	coded = in[0];
	for (int i=1; i<6; i++)
		coded = (coded << 8) | in[i];
	if (coded == 0xffffffffffffu) {
		strncpy(cs, "@ALL", sizeof(cs));
		return;
	}
	if (coded > 0xf46108ffffffu) {
		strncpy(cs, "@INVALID", sizeof(cs));
		return;
	}
	int i = 0;
	auto c = coded;
	if (coded > 0xee6b27ffffffu) {
		cs[i++] = '#';
		coded -= 0xee6b28000000u;
	}
	while (c) {
		cs[i++] = m17_alphabet[c % 40];
		c /= 40;
	}
}

void CCallsign::CodeOut(uint8_t *out) const
{
	memset(out, 0, 6);
	auto c = coded;
	auto pout = out+5;
	while (c)
	{
		*pout-- = c % 0x100u;
		c /= 0x100u;
	}
}

char CCallsign::GetModule() const
{
	if (cs[8])
		return cs[8];
	else
		return ' ';
}

bool CCallsign::operator==(const CCallsign &rhs) const
{
	return coded == rhs.coded;
}

bool CCallsign::operator!=(const CCallsign &rhs) const
{
	return coded != rhs.coded;
}

void CCallsign::SetModule(char m)
{
	std::string call(cs);
	call.resize(8, ' ');
	call.append(1, m);
	CSIn(call);
}

std::ostream &operator<<(std::ostream &stream, const CCallsign &call)
{
	stream << call.cs;
	return stream;
}
