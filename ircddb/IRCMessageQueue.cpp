/*
CIRCDDB - ircDDB client library in C++

Based on code by:
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

#include "IRCMessageQueue.h"

IRCMessageQueue::IRCMessageQueue()
{
	m_eof = false;
}

IRCMessageQueue::~IRCMessageQueue()
{
	accessMutex.lock();
	while (! m_queue.empty()) {
		delete m_queue.front();
		m_queue.pop();
	}
	accessMutex.unlock();
}

bool IRCMessageQueue::isEOF()
{
	return m_eof;
}

void IRCMessageQueue::signalEOF()
{
	m_eof = true;
}

bool IRCMessageQueue::messageAvailable()
{
  accessMutex.lock();
  bool retv = ! m_queue.empty();
  accessMutex.unlock();
  return retv;
}

IRCMessage *IRCMessageQueue::peekFirst()
{
	accessMutex.lock();
	IRCMessage *msg = m_queue.empty() ? NULL : m_queue.front();
	accessMutex.unlock();
	return msg;
}

IRCMessage *IRCMessageQueue::getMessage()
{
	accessMutex.lock();
	IRCMessage *msg = m_queue.empty() ? NULL : m_queue.front();
	if (msg)
		m_queue.pop();
	accessMutex.unlock();
	return msg;
}

void IRCMessageQueue::putMessage(IRCMessage *m)
{
	accessMutex.lock();
	m_queue.push(m);
	accessMutex.unlock();
}




