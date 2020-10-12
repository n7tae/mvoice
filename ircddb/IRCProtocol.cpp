#include <string>
#include <vector>
#include <regex>
#include <mutex>

#include "IRCutils.h"
#include "IRCProtocol.h"
#include "IRCMessageQueue.h"
#include "IRCDDBApp.h"

#define CIRCDDB_VERSION	  "2.0.0"

void IRCProtocol::Init(IRCDDBApp *app, const std::string &callsign, const std::string &password, const std::string &channel, const std::string &versionInfo)
{
	srand(time(NULL));
	this->password = password;
	this->channel = channel;
	this->app = app;

	this->versionInfo = "CIRCDDB:";
	this->versionInfo.append(CIRCDDB_VERSION);

	if (versionInfo.length() > 0) {
		this->versionInfo.append(" ");
		this->versionInfo.append(versionInfo);
	}


	int hyphenPos = callsign.find('-');

	if (hyphenPos < 0) {
		std::string n;

		n = callsign + "-1";
		nicks.push_back(n);
		n = callsign + "-2";
		nicks.push_back(n);
		n = callsign + "-3";
		nicks.push_back(n);
		n = callsign + "-4";
		nicks.push_back(n);
	} else {
		nicks.push_back(callsign);
	}

	name = callsign;

	pingTimer = 60; // 30 seconds
	state = 0;
	timer = 0;


	chooseNewNick();
}

IRCProtocol::~IRCProtocol()
{
}

void IRCProtocol::chooseNewNick()
{
	int r = rand() % nicks.size();

	currentNick = nicks[r];
}

void IRCProtocol::setNetworkReady(bool b)
{
	if (b == true) {
		if (state != 0)
			printf("IRCProtocol::setNetworkReady: unexpected state");

		state = 1;
		chooseNewNick();
	} else {
		state = 0;
	}
}


bool IRCProtocol::processQueues(IRCMessageQueue *recvQ, IRCMessageQueue *sendQ)
{
	if (timer > 0) {
		timer--;
	}

	while (recvQ->messageAvailable()) {
		IRCMessage *m = recvQ->getMessage();

#if defined(DEBUG_IRC)
		std::string d = std::string("R [") + m->prefix + std::string("] [") + m->command + std::string("]");
		for (int i=0; i < m->numParams; i++) {
			d.append(std::string(" [") + m->params[i] + std::string("]") );
		}
		// d.Replace(std::string("%"), std::string("%%"), true);
		// d.Replace(std::string("\\"), std::string("\\\\"), true);
		printf("%s\n", d.c_str());
#endif

		if (0 == m->command.compare("004")) {
			if (state == 4) {
				if (m->params.size() > 1) {
					std::regex serverNamePattern("^grp[1-9]s[1-9].ircDDB$");

					if (std::regex_match(m->params[1], serverNamePattern)) {
						app->setBestServer(std::string("s-") + m->params[1].substr(0,6));
					}
				}
				state = 5;  // next: JOIN
				app->setCurrentNick(currentNick);
			}
		} else if (0 == m->command.compare("PING")) {
			IRCMessage *m2 = new IRCMessage();
			m2->command = "PONG";
			if (m->params.size() > 0) {
				m2->numParams = 1;
				m2->params.push_back( m->params[0] );
			}
			sendQ -> putMessage(m2);
		} else if (0 == m->command.compare("JOIN")) {
			if ((m->numParams >= 1) && 0==m->params[0].compare(channel)) {
				if (0==m->getPrefixNick().compare(currentNick) && (state == 6)) {
					if (debugChannel.length() > 0) {
						state = 7;  // next: join debug_channel
					} else {
						state = 10; // next: WHO *
					}
				} else if (app != NULL) {
					app->userJoin( m->getPrefixNick(), m->getPrefixName(), m->getPrefixHost());
				}
			}

			if ((m->numParams >= 1) && 0==m->params[0].compare(debugChannel)) {
				if (0==m->getPrefixNick().compare(currentNick) && (state == 8)) {
					state = 10; // next: WHO *
				}
			}
		} else if (0 == m->command.compare("PONG")) {
			if (state == 12) {
				timer = pingTimer;
				state = 11;
			}
		} else if (0 == m->command.compare("PART")) {
			if ((m->numParams >= 1) && 0==m->params[0].compare(channel)) {
				if (app != NULL) {
					app->userLeave( m->getPrefixNick() );
				}
			}
		} else if (0 == m->command.compare("KICK")) {
			if ((m->numParams >= 2) && 0==m->params[0].compare(channel)) {
				if (0 == m->params[1].compare(currentNick)) {
					// i was kicked!!
					delete m;
					return false;
				} else if (app != NULL) {
					app->userLeave( m->params[1] );
				}
			}
		} else if (0 == m->command.compare("QUIT")) {
			if (app != NULL) {
				app->userLeave( m->getPrefixNick() );
			}
		} else if (0 == m->command.compare("PRIVMSG")) {
			if (app) {
				// std::string out;
				// m->composeMessage(out);
				// out.pop_back(); out.pop_back();
				if (2 == m->numParams) {
					if (0 == m->params[0].compare(channel)) {
						app->msgChannel(m);
					} else if (0 == m->params[0].compare(currentNick)) {
						if (0 == m->params[1].find("IDRT_PING")) {
							std::string from = m->params[1].substr(10);
							IRCMessage *rm = new IRCMessage("IDRT_PING");
							rm->addParam(from);
							app->putReplyMessage(rm);
						} else
							app->msgQuery(m);
					}
				}
			}
		} else if (0 == m->command.compare("352")) { // WHO list
			if ((m->numParams >= 7) && 0==m->params[0].compare(currentNick) && 0==m->params[1].compare(channel)) {
				if (app != NULL) {
					app->userJoin(m->params[5], m->params[2], m->params[3]);
				}
			}
		} else if (0 == m->command.compare("433")) { // nick collision
			if (state == 2) {
				state = 3;  // nick collision, choose new nick
				timer = 10; // wait 5 seconds..
			}
		}

		delete m;
	}

	IRCMessage * m;

	switch (state) {
	case 1:
		m = new IRCMessage();
		m->command = "PASS";
		m->numParams = 1;
		m->params.push_back(password);
		sendQ->putMessage(m);

		m = new IRCMessage();
		m->command = "NICK";
		m->numParams = 1;
		m->params.push_back(currentNick);
		sendQ->putMessage(m);

		timer = 10;  // wait for possible nick collision message
		state = 2;
		break;

	case 2:
		if (timer == 0) {
			m = new IRCMessage();
			m->command = "USER";
			m->numParams = 4;
			m->params.push_back(name);
			m->params.push_back("0");
			m->params.push_back("*");
			m->params.push_back(versionInfo);
			sendQ->putMessage(m);

			timer = 30;
			state = 4; // wait for login message
		}
		break;

	case 3:
		if (timer == 0) {
			chooseNewNick();
			m = new IRCMessage();
			m->command = "NICK";
			m->numParams = 1;
			m->params.push_back(currentNick);
			sendQ->putMessage(m);

			timer = 10;  // wait for possible nick collision message
			state = 2;
		}
		break;

	case 4:
		if (timer == 0) {
			// no login message received -> disconnect
			return false;
		}
		break;

	case 5:
		m = new IRCMessage();
		m->command = "JOIN";
		m->numParams = 1;
		m->params.push_back(channel);
		sendQ->putMessage(m);

		timer = 30;
		state = 6; // wait for join message
		break;

	case 6:
		if (timer == 0) {
			// no join message received -> disconnect
			return false;
		}
		break;

	case 7:
		if (debugChannel.length() == 0) {
			return false; // this state cannot be processed if there is no debug_channel
		}

		m = new IRCMessage();
		m->command = "JOIN";
		m->numParams = 1;
		m->params.push_back(debugChannel);
		sendQ->putMessage(m);

		timer = 30;
		state = 8; // wait for join message
		break;

	case 8:
		if (timer == 0) {
			// no join message received -> disconnect
			return false;
		}
		break;

	case 10:
		m = new IRCMessage();
		m->command = "WHO";
		m->numParams = 2;
		m->params.push_back(channel);
		m->params.push_back("*");
		sendQ->putMessage(m);

		timer = pingTimer;
		state = 11; // wait for timer and then send ping

		if (app != NULL) {
			app->setSendQ(sendQ);  // this switches the application on
		}
		break;

	case 11:
		if (timer == 0) {
			m = new IRCMessage();
			m->command = "PING";
			m->numParams = 1;
			m->params.push_back(currentNick);
			sendQ->putMessage(m);

			timer = pingTimer;
			state = 12; // wait for pong
		}
		break;

	case 12:
		if (timer == 0) {
			// no pong message received -> disconnect
			return false;
		}
		break;
	}

	return true;
}
