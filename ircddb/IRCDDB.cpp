#include "IRCDDB.h"

#include "IRCClient.h"
#include "IRCDDBApp.h"
#include "IRCutils.h"

CIRCDDB::CIRCDDB(const std::string &hostName, unsigned int port, const std::string &callsign, const std::string &password, const std::string &versionInfo)

{
	const std::string update_channel("#dstar");

	app = new IRCDDBApp(update_channel, &cache);
	client = new IRCClient(app, update_channel, hostName, port, callsign, password, versionInfo);
}

CIRCDDB::~CIRCDDB()
{
	delete client;
	delete app;
}

int CIRCDDB::GetFamily()
{
	return client->GetFamily();
}


// A false return implies a network error, or unable to log in
bool CIRCDDB::open()
{
	printf("starting CIRCDDB\n");
	return client->startWork() && app->startWork();
}


int CIRCDDB::getConnectionState()
{
	return app->getConnectionState();
}


void CIRCDDB::rptrQTH(const std::string &rptrcall, double latitude, double longitude, const std::string &desc1, const std::string &desc2, const std::string &infoURL, const std::string &swVersion)
{
	app->rptrQTH(rptrcall, latitude, longitude, desc1, desc2, infoURL, swVersion);
}


void CIRCDDB::rptrQRG(const std::string &rptrcall, double txFrequency, double duplexShift, double range, double agl)
{
	app->rptrQRG(rptrcall, txFrequency, duplexShift, range, agl);
}


void CIRCDDB::kickWatchdog(const std::string &wdInfo)
{
	app->kickWatchdog(wdInfo);
}

// Send heard data, a false return implies a network error
bool CIRCDDB::sendHeard(const std::string &myCall, const std::string &myCallExt, const std::string &yourCall, const std::string &rpt1, const std::string &rpt2, unsigned char flag1, unsigned char flag2, unsigned char flag3)
{
	if (myCall.size() != 8) {
		printf("CIRCDDB::sendHeard:myCall: len != 8\n");
		return false;
	}

	if (myCallExt.size() != 4) {
		printf("CIRCDDB::sendHeard:myCallExt: len != 4\n");
		return false;
	}

	if (yourCall.size() != 8) {
		printf("CIRCDDB::sendHeard:yourCall: len != 8\n");
		return false;
	}

	if (rpt1.size() != 8) {
		printf("CIRCDDB::sendHeard:rpt1: len != 8\n");
		return false;
	}

	if (rpt2.size() != 8) {
		printf("CIRCDDB::sendHeard:rpt2: len != 8\n");
		return false;
	}

	return app->sendHeard( myCall, myCallExt, yourCall, rpt1, rpt2, flag1, flag2, flag3, "        ", "", "");
}

// Send heard data, a false return implies a network error
bool CIRCDDB::sendHeardWithTXMsg(const std::string &myCall, const std::string &myCallExt, const std::string &yourCall, const std::string &rpt1, const std::string &rpt2, unsigned char flag1, unsigned char flag2, unsigned char flag3, const std::string &network_destination, const std::string &tx_message)
{
	if (myCall.size() != 8) {
		printf("CIRCDDB::sendHeard:myCall: len != 8\n");
		return false;
	}

	if (myCallExt.size() != 4) {
		printf("CIRCDDB::sendHeard:myCallExt: len != 4\n");
		return false;
	}

	if (yourCall.size() != 8) {
		printf("CIRCDDB::sendHeard:yourCall: len != 8\n");
		return false;
	}

	if (rpt1.size() != 8) {
		printf("CIRCDDB::sendHeard:rpt1: len != 8\n");
		return false;
	}

	if (rpt2.size() != 8) {
		printf("CIRCDDB::sendHeard:rpt2: len != 8\n");
		return false;
	}

	std::string dest = network_destination;

	if (dest.size() == 0)
		dest = "        ";

	if (dest.size() != 8) {
		printf("CIRCDDB::sendHeard:network_destination: len != 8\n");
		return false;
	}

	std::string msg;

	if (tx_message.length() == 20) {
		for (unsigned int i=0; i < tx_message.size(); i++) {
			char ch = tx_message.at(i);

			if (ch>32 && ch<127) {
				msg.push_back(ch);
			} else {
				msg.push_back('_');
			}
		}
	}

	return app->sendHeard( myCall, myCallExt, yourCall, rpt1, rpt2, flag1, flag2, flag3, dest, msg, "");
}

bool CIRCDDB::sendHeardWithTXStats(const std::string &myCall, const std::string &myCallExt, const std::string &yourCall, const std::string &rpt1, const std::string &rpt2, unsigned char flag1,
                        unsigned char flag2, unsigned char flag3, int num_dv_frames, int num_dv_silent_frames, int num_bit_errors)
{
	if (num_dv_frames<= 0 || num_dv_frames>65535) {
		printf("CIRCDDB::sendHeard:num_dv_frames not in range 1-65535\n");
		return false;
	}

	if (num_dv_silent_frames > num_dv_frames) {
		printf("CIRCDDB::sendHeard:num_dv_silent_frames > num_dv_frames\n");
		return false;
	}

	if (num_bit_errors > 4*num_dv_frames) { // max 4 bit errors per frame
		printf("CIRCDDB::sendHeard:num_bit_errors > (4*num_dv_frames)\n");
		return false;
	}

	if (myCall.size() != 8) {
		printf("CIRCDDB::sendHeard:myCall: len != 8\n");
		return false;
	}

	if (myCallExt.size() != 4) {
		printf("CIRCDDB::sendHeard:myCallExt: len != 4\n");
		return false;
	}

	if (yourCall.size() != 8) {
		printf("CIRCDDB::sendHeard:yourCall: len != 8\n");
		return false;
	}

	if (rpt1.size() != 8) {
		printf("CIRCDDB::sendHeard:rpt1: len != 8\n");
		return false;
	}

	if (rpt2.size() != 8) {
		printf("CIRCDDB::sendHeard:rpt2: len != 8\n");
		return false;
	}

	char buf[16];
	snprintf(buf, 16, "%04x", num_dv_frames);
	std::string stats = buf;

	if (num_dv_silent_frames >= 0) {
		snprintf(buf, 16, "%02x", num_dv_silent_frames * 100 / num_dv_frames);
		stats.append(buf);

		if (num_bit_errors >= 0) {
			snprintf(buf,16, "%02x", num_bit_errors * 125 / (num_dv_frames * 3));
			stats.append(buf);
		} else {
			stats.append("__");
		}
	} else {
		stats.append("____");
	}

	stats.append("____________");  // stats string should have 20 chars

	return app->sendHeard( myCall, myCallExt, yourCall, rpt1, rpt2, flag1, flag2, flag3, "        ", "", stats);
}

// Send query for a user, a false return implies a network error
bool CIRCDDB::findUser(const std::string &userCallsign)
{
	if (userCallsign.size() != 8) {
		printf("CIRCDDB::findUser: len != 8\n");
		return false;
	}
	std::string ucs = userCallsign;
	ToUpper(ucs);
	return app->findUser(ucs);
}

// The following functions are for processing received messages

// Get the waiting message type
IRCDDB_RESPONSE_TYPE CIRCDDB::getMessageType()
{
	return app->getReplyMessageType();
}

bool CIRCDDB::receivePing(std::string &repeaterCallsign)
{
	IRCDDB_RESPONSE_TYPE rt = app->getReplyMessageType();

	if (rt != IDRT_PING) {
		printf("CIRCDDB::receivePing: unexpected response type\n");
		return false;
	}

	IRCMessage *m = app->getReplyMessage();

	if (NULL == m) {
		printf("CIRCDDB::receivePing: no message\n");
		return false;
	}

	if (m->getCommand().compare("IDRT_PING")) {
		printf("CIRCDDB::receivePing: wrong messsage type\n");
		return false;
	}

	if (1 != m->getParamCount()) {
		printf("CIRCDDB::receivePing: unexpected number of message parameters\n");
		return false;
	}

	repeaterCallsign = m->getParam(0);

	delete m;

	return true;
}

void CIRCDDB::sendPing(const std::string &to, const std::string &from)
{
	app->sendPing(to, from);
}

void CIRCDDB::close()		// Implictely kills any threads in the IRC code
{
	client->stopWork();
	app->stopWork();
}
