#pragma once

#include <future>
#include <atomic>
#include "../TCPReaderWriterClient.h"

#include "IRCReceiver.h"
#include "IRCMessageQueue.h"
#include "IRCProtocol.h"
#include "IRCReceiver.h"

class IRCDDBApp;

class IRCClient
{
public:
	IRCClient(IRCDDBApp *app, const std::string &update_channel, const std::string &hostName, unsigned int port, const std::string &callsign, const std::string &password, const std::string &versionInfo);

	virtual ~IRCClient();
	bool startWork();
	void stopWork();
	int GetFamily();

protected:
	virtual void Entry();

private:
	std::atomic<int> family;
	std::future<void> client_thread;
	char host_name[100];
	char local_addr[100];
	unsigned int port;
	std::string callsign;
	std::string password;

	bool terminateThread;

	IRCReceiver receiver;
	IRCMessageQueue *recvQ;
	IRCMessageQueue *sendQ;
	IRCProtocol proto;
	IRCDDBApp *app;

};
