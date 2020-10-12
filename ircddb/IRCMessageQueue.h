/*
CIRCDDB - ircDDB client library in C++

Based on original code by:
Copyright (C) 2010   Michael Dirska, DL1BFF (dl1bff@mdx.de)

Completely rewritten by:
Copyright (c) 2017 by Thomas A. Early N7TAE

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <mutex>
#include <queue>

#include "IRCMessage.h"

class IRCMessageQueue
{
public:
	IRCMessageQueue();
	~IRCMessageQueue();

	bool isEOF();
	void signalEOF();
	bool messageAvailable();
	IRCMessage *getMessage();
	IRCMessage *peekFirst();
	void putMessage(IRCMessage *m);

private:
	bool m_eof;
	std::mutex accessMutex;
	std::queue<IRCMessage *> m_queue;
};

