#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string>

#include "IRCDDBApp.h"
#include "IRCutils.h"

IRCDDBApp::IRCDDBApp(const std::string &u_chan, CCacheManager *cache) : numberOfTables(2)
{
	updateChannel = u_chan;
	this->cache = cache;
	maxTime = 950000000;	// Feb 2000
	wdTimer = -1;
	sendQ = NULL;
	initReady = false;
	state = 0;
	timer = 0;
	myNick = "none";


	terminateThread = false;
	tablePattern  = std::regex("^[0-9]$");
	datePattern   = std::regex("^20[0-9][0-9]-((1[0-2])|(0[1-9]))-((3[01])|([12][0-9])|(0[1-9]))$");
	timePattern   = std::regex("^((2[0-3])|([01][0-9])):[0-5][0-9]:[0-5][0-9]$");
	dbPattern     = std::regex("^[0-9A-Z_]{8}$");
	modulePattern = std::regex("^.*[ABCD]D?$");
}

IRCDDBApp::~IRCDDBApp()
{
	if (sendQ != NULL) {
		delete sendQ;
	}
}

void IRCDDBApp::rptrQTH(const std::string &rptrcall, double latitude, double longitude, const std::string &desc1, const std::string &desc2, const std::string &infoURL, const std::string &swVersion)
{
	char pos[32];
	snprintf(pos, 32, "%+09.5f %+010.5f", latitude, longitude);

	std::string d1 = desc1;
	std::string d2 = desc2;
	std::string rcall = rptrcall;

	d1.resize(20, '_');
	d2.resize(20, '_');

	std::regex nonValid("[^a-zA-Z0-9 +&(),./'-_]+");

	std::smatch sm;
	while (std::regex_search(d1, sm, nonValid))
		d1.erase(sm.position(0), sm.length());
	while (std::regex_search(d2, sm, nonValid))
		d2.erase(sm.position(0), sm.length());

	ReplaceChar(d1, ' ', '_');
	ReplaceChar(d2, ' ', '_');
	ReplaceChar(rcall, ' ', '_');
	std::string aspace(" ");
	std::string f = rcall + aspace + std::string(pos) + aspace + d1 + aspace + d2;

	locationMapMutex.lock();
	locationMap[rptrcall] = f;
	locationMapMutex.unlock();

	//printf("IRCDDB RPTRQTH: %s\n", f.c_str());

	std::regex urlNonValid("[^[:graph:]]+");

	std::string url = infoURL;
	while (std::regex_search(url, sm, nonValid))
		url.erase(sm.position(0), sm.length());

	std::string g = rcall + aspace + url;

	urlMapMutex.lock();
	urlMap[rptrcall] = g;
	urlMapMutex.unlock();

	//printf("IRCDDB RPTRURL: %s\n", g.c_str());

	std::string sw = swVersion;
	while (std::regex_search(sw, sm, nonValid))
		sw.erase(sm.position(0), sm.length());

	std::string h = rcall + std::string(" ") + sw;
	swMapMutex.lock();
	swMap[rptrcall] = h;
	swMapMutex.unlock();

	//printf("IRCDDB RPTRSW: %s\n", h.c_str());

	infoTimer = 5; // send info in 5 seconds
}

void IRCDDBApp::rptrQRG(const std::string &rptrcall, double txFrequency, double duplexShift, double range, double agl)
{

	if (std::regex_match(rptrcall, modulePattern)) {

		std::string c = rptrcall;
		ReplaceChar(c, ' ', '_');

		char f[48];
		snprintf(f, 48, "%011.5f %+010.5f %06.2f %06.1f", txFrequency, duplexShift, range / 1609.344, agl);
		std::string g = c + std::string(" ") + f;

		moduleMapMutex.lock();
		moduleMap[rptrcall] = g;
		moduleMapMutex.unlock();

		//printf("IRCDDB RPTRQRG: %s\n", g.c_str());

		infoTimer = 5; // send info in 5 seconds
	}
}

void IRCDDBApp::kickWatchdog(const std::string &s)
{
	if (s.length() > 0) {

		std::regex nonValid("[^[:graph:]]+");
		std::smatch sm;
		std::string u = s;
		while (std::regex_search(u, sm, nonValid))
			u.erase(sm.position(0), sm.length());
		wdInfo = u;

		if (u.length() > 0)
			wdTimer = 1;
	}
}

int IRCDDBApp::getConnectionState()
{
	return state;
}

IRCDDB_RESPONSE_TYPE IRCDDBApp::getReplyMessageType()
{
	IRCMessage * m = replyQ.peekFirst();
	if (m == NULL) {
		return IDRT_NONE;
	}

	std::string msgType = m->getCommand();

	if (msgType == std::string("IDRT_PING")) {
		return IDRT_PING;
	}

	printf("IRCDDBApp::getMessageType: unknown msg type: %s\n", msgType.c_str());

	return IDRT_NONE;
}

IRCMessage *IRCDDBApp::getReplyMessage()
{
	return replyQ.getMessage();
}

void IRCDDBApp::putReplyMessage(IRCMessage *m)
{
	replyQ.putMessage(m);
}

bool IRCDDBApp::startWork()
{
	terminateThread = false;
	worker_thread = std::async(std::launch::async, &IRCDDBApp::Entry, this);
	return true;
}

void IRCDDBApp::stopWork()
{
	terminateThread = true;
	worker_thread.get();
}

void IRCDDBApp::userJoin(const std::string &nick, const std::string &name, const std::string &addr)
{
	if (0 == nick.compare(0, 2, "u-")) {
		return;
	}
	std::string gate(name);
	ToUpper(gate);
	gate.resize(7, ' ');
	gate.push_back('G');
	cache->updateName(name, nick);
	cache->updateGate(gate, addr);
}

void IRCDDBApp::userLeave(const std::string &nick)
{
	if (0 == nick.compare(0, 2, "s-")) {
		currentServer.clear();
		state = 2;
		timer = 200;
		initReady = false;
		return;
	}
	std::string name(nick);
	name.pop_back();
	if ('-' == name.back()) {
		name.pop_back();
		cache->eraseName(name);
		ToUpper(name);
		name.resize(7, ' ');
		name.push_back('G');
		cache->eraseGate(name);
	}
}

void IRCDDBApp::userListReset()
{
	cache->clearGate();	// clears the NameNick as well
}

void IRCDDBApp::setCurrentNick(const std::string &nick)
{
	myNick.assign(nick);
	printf("IRCDDBApp::setCurrentNick %s\n", nick.c_str());
}

void IRCDDBApp::setBestServer(const std::string &ircUser)
{
	bestServer.assign(ircUser);
	printf("IRCDDBApp::setBestServer %s\n", ircUser.c_str());
}

bool IRCDDBApp::findServerUser()
{
	std::string suser(cache->findServerUser());
	if (suser.empty())
		return false;
	currentServer.assign(suser);
	return true;
}

// to is the gateway to which we are sending the message, (the gateway last used by URCall)
// from is the repeater we are expecting the receive the the ping
// this will open the routing port on the target
void IRCDDBApp::sendPing(const std::string &to, const std::string &from)
{
	std::string name = to.substr(0, 7);

	while (isspace(name[name.length()-1]))
		name.pop_back();
	ToLower(name);

	auto nick = cache->findNameNick(name);

	if (! nick.empty()) {
		std::string rptr(from);
		ReplaceChar(rptr, ' ', '_');
		IRCMessage *m = new IRCMessage(nick, "IDRT_PING");
		m->addParam(rptr);
		sendQ->putMessage(m);
	}
}

bool IRCDDBApp::sendHeard(const std::string &myCall, const std::string &myCallExt, const std::string &yourCall, const std::string &rpt1, const std::string &rpt2, unsigned char flag1, unsigned char flag2, unsigned char flag3, const std::string &destination, const std::string &tx_msg, const std::string &tx_stats)
{

	std::string my = myCall;
	std::string myext = myCallExt;
	std::string ur = yourCall;
	std::string r1 = rpt1;
	std::string r2 = rpt2;
	std::string dest = destination;

	std::regex nonValid("[^A-Z0-9/_]");
	char underScore = '_';
	std::smatch sm;
	while (std::regex_search(my, sm, nonValid))
		my[sm.position(0)] = underScore;
	while (std::regex_search(myext, sm, nonValid))
		myext[sm.position(0)] = underScore;
	while (std::regex_search(ur, sm, nonValid))
		ur[sm.position(0)] = underScore;
	while (std::regex_search(r1, sm, nonValid))
		r1[sm.position(0)] = underScore;
	while (std::regex_search(r2, sm, nonValid))
		r2[sm.position(0)] = underScore;
	while (std::regex_search(dest, sm, nonValid))
		dest[sm.position(0)] = underScore;

	bool statsMsg = (tx_stats.length() > 0);

	std::string srv = currentServer;
	IRCMessageQueue *q = getSendQ();

	if ((srv.length() > 0) && (state >= 6) && (q != NULL)) {
		std::string cmd("UPDATE ");

		cmd.append(getCurrentTime());

		cmd.append(" ");

		cmd.append(my);
		cmd.append(" ");
		cmd.append(r1);
		cmd.append(" ");
		if (!statsMsg) {
			cmd.append("0 ");
		}
		cmd.append(r2);
		cmd.append(" ");
		cmd.append(ur);
		cmd.append(" ");

		char flags[16];
		snprintf(flags, 16, "%02X %02X %02X", flag1, flag2, flag3);

		cmd.append(flags);
		cmd.append(" ");
		cmd.append(myext);

		if (statsMsg) {
			cmd.append(" # ");
			cmd.append(tx_stats);
		} else {
			cmd.append(" 00 ");
			cmd.append(dest);

			if (tx_msg.length() == 20) {
				cmd.append(" ");
				cmd.append(tx_msg);
			}
		}


		IRCMessage *m = new IRCMessage(srv, cmd);

		q->putMessage(m);
		return true;
	} else
		return false;
}

bool IRCDDBApp::findUser(const std::string &usrCall)
{
	std::string srv = currentServer;
	IRCMessageQueue *q = getSendQ();

	if ((srv.length() > 0) && (state >= 6) && (q != NULL)) {
		std::string usr = usrCall;

		ReplaceChar(usr, ' ', '_');

		IRCMessage *m = new IRCMessage(srv, std::string("FIND ") + usr );

		q->putMessage(m);
	}

	return true;
}

void IRCDDBApp::msgChannel(IRCMessage *m)
{
	if (0==m->getPrefixNick().compare(0, 2, "s-") && (m->numParams >= 2)) { // server msg
		doUpdate(m->params[1]);
	}
}

void IRCDDBApp::doNotFound(std::string &msg, std::string &retval)
{
	int tableID = 0;

	std::vector<std::string> tkz = stringTokenizer(msg);

	if (0u == tkz.size())
		return;  // no text in message

	std::string tk = tkz.front();
	tkz.erase(tkz.begin());


	if (std::regex_match(tk, tablePattern)) {
		long tableID = std::stol(tk);

		if ((tableID < 0) || (tableID >= numberOfTables)) {
			printf("invalid table ID %ld", tableID);
			return;
		}

		if (0u == tkz.size())
			return;  // received nothing but the tableID

		tk = tkz.front();
		tk.erase(tk.begin());
	}

	if (tableID == 0) {
		if (! std::regex_match(tk, dbPattern))
			return; // no valid key

		retval = tk;
	}
}

void IRCDDBApp::doUpdate(std::string &msg)
{
	int tableID = 0;

	std::vector<std::string> tkz = stringTokenizer(msg);

	if (0u == tkz.size())
		return;  // no text in message

	std::string tk = tkz.front();
	tkz.erase(tkz.begin());

	if (std::regex_match(tk, tablePattern)) {
		tableID = stol(tk);
		if ((tableID < 0) || (tableID >= numberOfTables)) {
			printf("invalid table ID %d", tableID);
			return;
		}

		if (0 == tkz.size())
			return;  // received nothing but the tableID

		tk = tkz.front();
		tkz.erase(tkz.begin());
	}

	if (std::regex_match(tk, datePattern)) {
		if (0 == tkz.size())
			return;  // nothing after date string

		std::string timeToken = tkz.front();
		tkz.erase(tkz.begin());

		if (! std::regex_match(timeToken, timePattern))
			return; // no time string after date string

		std::string tstr(std::string(tk + " " + timeToken));	// used to update user time
		auto rtime = parseTime(tstr);							// used to update maxTime for sendlist

		if ((tableID == 0) || (tableID == 1)) {
			if (0 == tkz.size())
				return;  // nothing after time string

			std::string key = tkz.front();
			tkz.erase(tkz.begin());

			if (! std::regex_match(key, dbPattern))
				return; // no valid key

			if (0 == tkz.size())
				return;  // nothing after time string

			std::string value = tkz.front();
			tkz.erase(tkz.begin());

			if (! std::regex_match(value, dbPattern))
				return; // no valid key

			//printf("TABLE %d %s %s\n", tableID, key.c_str(), value.c_str());

			if (tableID == 1) {

				if (initReady && key.compare(0,6, value, 0, 6)) {
					std::string rptr(key);
					std::string gate(value);

					ReplaceChar(rptr, '_', ' ');
					ReplaceChar(gate, '_', ' ');
					gate[7] = 'G';
					cache->updateRptr(rptr, gate, "");
					if (rtime > maxTime)
						maxTime = rtime;
				}
			} else if ((tableID == 0) && initReady) {
				std::string user(key);
				std::string rptr(value);

				ReplaceChar(user, '_', ' ');
				ReplaceChar(rptr, '_', ' ');

				cache->updateUser(user, rptr, "", "", tstr);

			}
		}
	}
}

std::string IRCDDBApp::getTableIDString(int tableID, bool spaceBeforeNumber)
{
	if (tableID == 0) {
		return std::string("");
	} else if ((tableID > 0) && (tableID < numberOfTables)) {
		if (spaceBeforeNumber) {
			return std::string(" ") + std::to_string(tableID);
		} else {
			return std::to_string(tableID) + std::string(" ");
		}
	} else {
		return std::string(" TABLE_ID_OUT_OF_RANGE ");
	}
}

void IRCDDBApp::msgQuery(IRCMessage *m)
{

	if (0 == strcmp(m->getPrefixNick().substr(0,2).c_str(), "s-") && (m->numParams >= 2)) { // server msg
		std::string msg = m->params[1];
		std::vector<std::string> tkz = stringTokenizer(msg);

		if (0 == tkz.size())
			return;  // no text in message

		std::string cmd = tkz.front();
		tkz.erase(tkz.begin());

		if (cmd == std::string("UPDATE")) {
			std::string restOfLine;
			while (tkz.size()) {
				restOfLine += tkz.front();
				tkz.erase(tkz.begin());
				if (tkz.size())
					restOfLine += " ";
			}
			doUpdate(restOfLine);
		} else if (cmd == std::string("LIST_END")) {
			if (state == 5) { // if in sendlist processing state
				state = 3;  // get next table
			}
		} else if (cmd == std::string("LIST_MORE")) {
			if (state == 5) { // if in sendlist processing state
				state = 4;  // send next SENDLIST
			}
		} else if (cmd == std::string("NOT_FOUND")) {
			std::string callsign;
			std::string restOfLine;
			while (tkz.size()) {
				restOfLine += tkz.front();
				tkz.erase(tkz.begin());
				if (tkz.size())
					restOfLine += " ";
			}
			doNotFound(restOfLine, callsign);

			if (callsign.length() > 0) {
				ReplaceChar(callsign, '_', ' ');

				IRCMessage *m2 = new IRCMessage("IDRT_USER");
				m2->addParam(callsign);
				m2->addParam("");
				m2->addParam("");
				m2->addParam("");
				m2->addParam("");
				replyQ.putMessage(m2);
			}
		}
	}
}

void IRCDDBApp::setSendQ(IRCMessageQueue *s)
{
	sendQ = s;
}

IRCMessageQueue *IRCDDBApp::getSendQ()
{
	return sendQ;
}

std::string IRCDDBApp::getLastEntryTime(int tableID)
{
	if (tableID == 1) {
		struct tm *ptm = gmtime(&maxTime);
		char tstr[80];
		strftime(tstr, 80, "%Y-%m-%d %H:%M:%S", ptm);
		std::string max = tstr;
		return max;
	}

	return "DBERROR";
}

void IRCDDBApp::Entry()
{
	int sendlistTableID = 0;

	while (!terminateThread) {

		if (timer > 0) {
			timer--;
		}

		switch(state) {
		case 0:  // wait for network to start

			if (getSendQ() != NULL) {
				state = 1;
			}
			break;

		case 1:
			// connect to db
			state = 2;
			timer = 200;
			break;

		case 2:   // choose server
			printf("IRCDDBApp: state=2 choose new 's-'-user\n");
			if (getSendQ() == NULL) {
				state = 10;
			} else {
				if (findServerUser()) {
					sendlistTableID = numberOfTables;

					state = 3; // next: send "SENDLIST"
				} else if (timer == 0) {
					state = 10;
					IRCMessage *m = new IRCMessage("QUIT");

					m->addParam("no op user with 's-' found.");

					IRCMessageQueue * q = getSendQ();
					if (q != NULL) {
						q->putMessage(m);
					}
				}
			}
			break;

		case 3:
			if (getSendQ() == NULL) {
				state = 10; // disconnect DB
			} else {
				sendlistTableID --;
				if (sendlistTableID < 0) {
					state = 6; // end of sendlist
				} else {
					printf("IRCDDBApp: state=3 tableID=%d\n", sendlistTableID);
					state = 4; // send "SENDLIST"
					timer = 900; // 15 minutes max for update
				}
			}
			break;

		case 4:
			if (getSendQ() == NULL) {
				state = 10; // disconnect DB
			} else {
				if (1 == sendlistTableID) {
					IRCMessage *m = new IRCMessage(currentServer, std::string("SENDLIST") + getTableIDString(sendlistTableID, true)
					                                + std::string(" ") + getLastEntryTime(sendlistTableID));

					IRCMessageQueue *q = getSendQ();
					if (q != NULL)
						q->putMessage(m);

					state = 5; // wait for answers
				} else
					state = 3; // don't send SENDLIST for this table (tableID 0), go to next table
			}
			break;

		case 5: // sendlist processing
			if (getSendQ() == NULL) {
				state = 10; // disconnect DB
			} else if (timer == 0) {
				state = 10; // disconnect DB
				IRCMessage *m = new IRCMessage("QUIT");

				m->addParam("timeout SENDLIST");

				IRCMessageQueue *q = getSendQ();
				if (q != NULL) {
					q->putMessage(m);
				}

			}
			break;

		case 6:
			if (getSendQ() == NULL) {
				state = 10; // disconnect DB
			} else {
				printf("IRCDDBApp: state=6 initialization completed\n");

				infoTimer = 2;

				initReady = true;
				state = 7;
			}
			break;


		case 7: // standby state after initialization
			if (getSendQ() == NULL)
				state = 10; // disconnect DB

			if (infoTimer > 0) {
				infoTimer--;

				if (infoTimer == 0) {
					moduleMapMutex.lock();

					for (auto itl = locationMap.begin(); itl != locationMap.end(); itl++) {
						std::string value = itl->second;
						IRCMessage *m = new IRCMessage(currentServer, std::string("IRCDDB RPTRQTH: ") + value);

						IRCMessageQueue * q = getSendQ();
						if (q != NULL) {
							q->putMessage(m);
						}
					}

					for (auto itu = urlMap.begin(); itu != urlMap.end(); itu++) {
						std::string value = itu->second;
						IRCMessage * m = new IRCMessage(currentServer, std::string("IRCDDB RPTRURL: ") + value);

						IRCMessageQueue * q = getSendQ();
						if (q != NULL) {
							q->putMessage(m);
						}
					}

					for(auto itm = moduleMap.begin(); itm != moduleMap.end(); itm++) {
						std::string value = itm->second;
						IRCMessage * m = new IRCMessage(currentServer, std::string("IRCDDB RPTRQRG: ") + value);

						IRCMessageQueue *q = getSendQ();
						if (q != NULL) {
							q->putMessage(m);
						}
					}

					for(auto its = swMap.begin(); its != swMap.end(); its++) {
						std::string value = its->second;
						IRCMessage * m = new IRCMessage(currentServer, std::string("IRCDDB RPTRSW: ") + value);

						IRCMessageQueue *q = getSendQ();
						if (q != NULL) {
							q->putMessage(m);
						}
					}

					moduleMapMutex.unlock();
				}
			}

			if (wdTimer > 0) {
				wdTimer--;
				if (wdTimer <= 0) {
					wdTimer = 900;  // 15 minutes

					IRCMessage *m = new IRCMessage(currentServer, std::string("IRCDDB WATCHDOG: ") +
							getCurrentTime() + std::string(" ") + wdInfo + std::string(" 1"));

					IRCMessageQueue *q = getSendQ();
					if (q != NULL)
						q->putMessage(m);
					else
						delete m;
				}
			}
			break;

		case 10:
			// disconnect db
			state = 0;
			timer = 0;
			initReady = false;
			break;

		}

		sleep(1);

	} // while

	return;
} // Entry()
