#pragma once
#include <future>
#include "IRCMessageQueue.h"
#include "../TCPReaderWriterClient.h"

class IRCReceiver
{
public:
	IRCReceiver() {}
	void Init(CTCPReaderWriterClient *ircSock, IRCMessageQueue *q);
	virtual ~IRCReceiver();
	bool startWork();
	void stopWork();

protected:
	virtual void Entry();

private:
	static int doRead(CTCPReaderWriterClient *ircSock, char *buf, int buf_size);

	CTCPReaderWriterClient *ircSock;
	bool terminateThread;
	int sock;
	IRCMessageQueue *recvQ;
    std::future<void> rec_thread;
};
