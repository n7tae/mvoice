
#include <sys/types.h>
#include <sys/socket.h>

#include "IRCutils.h"
#include "IRCMessage.h"
#include "IRCReceiver.h"

void IRCReceiver::Init(CTCPReaderWriterClient *sock, IRCMessageQueue *q)
{
	ircSock = sock;
	recvQ = q;
}

IRCReceiver::~IRCReceiver()
{
}

bool IRCReceiver::startWork()
{
	terminateThread = false;
	rec_thread = std::async(std::launch::async, &IRCReceiver::Entry, this);
	return true;
}

void IRCReceiver::stopWork()
{
	terminateThread = true;
	rec_thread.get();
}

int IRCReceiver::doRead(CTCPReaderWriterClient *ircSock, char *buf, int buf_size)
{
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	fd_set rdset;
	fd_set errset;

	int fd = ircSock->GetFD();
	FD_ZERO(&rdset);
	FD_ZERO(&errset);
	FD_SET(fd, &rdset);
	FD_SET(fd, &errset);

	int res;

	res = select(fd+1, &rdset, NULL, &errset, &tv);

	if ( res < 0 ) {
		printf("IRCReceiver::doread: select() error.\n");
		return -1;
	} else if ( res > 0 ) {
		if (FD_ISSET(fd, &errset)) {
			printf("IRCReceiver::doRead: FD_ISSET error\n");
			return -1;
		}

		if (FD_ISSET(fd, &rdset)) {
			res = ircSock->Read((unsigned char *)buf, buf_size);

			if (res < 0) {
				printf("IRCReceiver::doRead: recv error\n");
				return -1;
			} else if (res == 0) {
				printf("IRCReceiver::doRead: EOF read==0\n");
				return -1;
			} else
				return res;
		}

	}

	return 0;
}

void IRCReceiver::Entry()
{
	IRCMessage *m = new IRCMessage();

	int i;
	int state = 0;

	while (!terminateThread) {
		char buf[200];
		int r = doRead(ircSock, buf, sizeof buf);

		if (r < 0) {
			recvQ->signalEOF();
			delete m;  // delete unfinished IRCMessage
			break;
		}

		for (i=0; i<r; i++) {
			char b = buf[i];

			if (b > 0) {
				if (b == '\n') {
					recvQ->putMessage(m);
					m = new IRCMessage();
					state = 0;
				} else if (b == '\r') {
					// do nothing
				} else switch (state) {
					case 0:
						if (b == ':') {
							state = 1; // prefix
						} else if (b == ' ') {
							// do nothing
						} else {
							m->command.push_back(b);
							state = 2; // command
						}
						break;

					case 1:
						if (b == ' ') {
							state = 2; // command is next
						} else {
							m->prefix.push_back(b);
						}
						break;

					case 2:
						if (b == ' ') {
							state = 3; // params
							m->numParams = 1;
							m->params.push_back("");
						} else {
							m->command.push_back(b);
						}
						break;

					case 3:
						if (b == ' ') {
							m->numParams++;
							if (m->numParams >= 15) {
								state = 5; // ignore the rest
							}

							m->params.push_back("");
						} else if ((b == ':') && (m->params[m->numParams - 1].length() == 0)) {
							state = 4; // rest of line is this param
						} else {
							m->params[m->numParams - 1].push_back(b);
						}
						break;

					case 4:
						m->params[m->numParams - 1].push_back(b);
						break;
					} // switch
			} // if
		} // for
	} // while

	return;
}
