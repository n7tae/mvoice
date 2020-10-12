#pragma once

#include "IRCMessageQueue.h"
class IRCDDBApp;

class IRCProtocol
{
public:
	IRCProtocol() {}

	void Init(IRCDDBApp *app, const std::string &callsign, const std::string &password, const std::string &channel, const std::string &versionInfo);

	~IRCProtocol();

	void setNetworkReady(bool state);

	bool processQueues(IRCMessageQueue *recvQ, IRCMessageQueue *sendQ);

private:
	void chooseNewNick();

	std::vector<std::string> nicks;
	std::string password;
	std::string channel;
	std::string name;
	std::string currentNick;
	std::string versionInfo;

	int state;
	int timer;
	int pingTimer;

	std::string debugChannel;

	IRCDDBApp *app;
};
