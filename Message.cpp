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

#include <stdio.h>

#include "Message.h"

void CMessage::Init()
{
	ctl = 0;
	memset(msg, 0, 53);
}

void CMessage::MakeBlocks(const std::string &msg, std::vector<SMeta> &blks)
{
	blks.clear();
	unsigned size = msg.size();
	unsigned count = size / 13u;
	if (count > 4u)
		count = 4;
	else if ((count < 4u) and (size % 13u))
		count++;
	if (0u == count)
		return;
	uint8_t ctrl;
	switch (count)
	{
	case 1u:
		ctrl = 0x1u;
		break;
	case 2u:
		ctrl = 0x3u;
		break;
	case 3u:
		ctrl = 0x7u;
		break;
	default:
		ctrl = 0xfu;
		break;
	}
	SMeta meta;
	for (unsigned i=0; i<count; i++)
	{
		meta.data[0] = (0x1u << (4+i)) | ctrl;
		for (unsigned j=0; j<13; j++)
		{
			auto o = 13u * i +j;
			if (o < size)
				meta.data[j+1] = uint8_t(msg.at(0));
			else
				break;
		}
		blks.push_back(meta);
	}
}

bool CMessage::GetBlock(const uint8_t *data)
{
	if (data[0] == 0u)
		return false;
	switch (data[0] >> 4)
	{
		case 0x1u:
		case 0x3u:
		case 0x7u:
		case 0xfu:
			switch(data[0] & 0xfu)
			{
				case 0x1u:
					Clean(msg, data+1);
					break;
				case 0x2u:
					Clean(msg+13, data+1);
					break;
				case 0x4u:
					Clean(msg+26, data+1);
					break;
				case 0x8u:
					Clean(msg+39, data+1);
					break;
				default:
					Init();
					return false;
			}
			ctl |= data[0];
			break;
		default:
			Init();
			return false;
	}
	if ((ctl & 0xfu) == (ctl >> 4))
	{
		Trim();
		return true;
	}
	return false;
}

void CMessage::Clean(char *to, const uint8_t *from)
{
	for (unsigned i=0; i<13u; i++)
	{
		auto c = char(from[i]);
		if (not isgraph(c))
			c = ' ';
		to[i] = c;
	}
}

void CMessage::Trim()
{
	str.assign(msg);
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](int ch) { return not std::isspace(ch); }));
	str.erase(std::find_if(str.rbegin(), str.rend(), [](int ch) { return not std::isspace(ch); }).base(), str.end());
}

const std::string &CMessage::GetMessage()
{
	return str;
}
