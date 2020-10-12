#pragma once

#include <string>

#include "../CacheManager.h"

enum IRCDDB_RESPONSE_TYPE {
	IDRT_NONE,
	IDRT_PING
};

enum DSTAR_PROTOCOL {
	DP_UNKNOWN,
	DP_DEXTRA,
	DP_DPLUS
};

class IRCDDBApp;
class IRCClient;

class CIRCDDB
{
public:
	CIRCDDB(const std::string &hostName, unsigned int port, const std::string &callsign, const std::string &password, const std::string &versionInfo);
	~CIRCDDB();

	// returns the socket family type
	int GetFamily();

	// A false return implies a network error, or unable to log in
	bool open();

	// rptrQTH can be called multiple times if necessary
	//   rptrcall     callsign of the repeater
	//   latitude     WGS84 position of antenna in degrees, positive value -> NORTH
	//   longitude    WGS84 position of antenna in degrees, positive value -> EAST
	//   desc1, desc2   20-character description of QTH

	void rptrQTH(const std::string &rptrcall, double latitude, double longitude, const std::string &desc1, const std::string &desc2, const std::string &infoURL, const std::string &swVersion);

	// rptrQRG can be called multiple times if necessary
	//  module      letter of the module, valid values: "A", "B", "C", "D", "AD", "BD", "CD", "DD"
	//  txFrequency   repeater TX frequency in MHz
	//  duplexShift   duplex shift in MHz (positive or negative value):  RX_freq = txFrequency + duplexShift
	//  range       range of the repeater in meters (meters = miles * 1609.344)
	//  agl         height of the antenna above ground in meters (meters = feet * 0.3048)

	void rptrQRG(const std::string &rptrcall, double txFrequency, double duplexShift, double range, double agl);

	// If you call this method once, watchdog messages will be sent to the
	// to the ircDDB network every 15 minutes. Invoke this method every 1-2 minutes to indicate
	// that the gateway is working properly. After activating the watchdog, a red LED will be displayed
	// on the ircDDB web page if this method is not called within a period of about 30 minutes.
	// The string wdInfo should contain information about the source of the alive messages, e.g.,
	// version of the RF decoding software. For example, the ircDDB java software sets this
	// to "rpm_ircddbmhd-x.z-z".  The string wdInfo must contain at least one non-space character.

	void kickWatchdog(const std::string &wdInfo);

	// get internal network status
	int getConnectionState();
	// one of these values is returned:
	//  0  = not (yet) connected to the IRC server
	//  1-6  = a new connection was established, download of repeater info etc. is
	//         in progress
	//  7 = the ircDDB connection is fully operational
	//  10 = some network error occured, next state is "0" (new connection attempt)

	// Send heard data, a false return implies a network error
	bool sendHeard(const std::string &myCall, const std::string &myCallExt, const std::string &yourCall, const std::string &rpt1, const std::string &rpt2, unsigned char flag1, unsigned char flag2, unsigned char flag3);

	// same as sendHeard with two new fields:
	//   network_destination:  empty string or 8-char call sign of the repeater
	//	    or reflector, where this transmission is relayed to.
	//   tx_message:  20-char TX message or empty string, if the user did not
	//       send a TX message
	bool sendHeardWithTXMsg(const std::string &myCall, const std::string &myCallExt, const std::string &yourCall, const std::string &rpt1, const std::string &rpt2, unsigned char flag1, unsigned char flag2, unsigned char flag3, const std::string &network_destination, const std::string &tx_message);

	// this method should be called at the end of a transmission
	//  num_dv_frames: number of DV frames sent out (96 bit frames, 20ms)
	//  num_dv_silent_frames: number of DV silence frames sent out in the
	//	last transmission, or -1 if the information is not available
	//  num_bit_errors: number of bit errors of the received data. This should
	//      be the derived from the first Golay block of the voice data. This
	//      error correction code only looks at 24 bits of the 96 bit frame.
	//      So, the overall bit error rate is calculated like this:
	//      BER = num_bit_errors / (num_dv_frames * 24)
	//      Set num_bit_errors = -1, if the error information is not available.
	bool sendHeardWithTXStats(const std::string &myCall, const std::string &myCallExt, const std::string &yourCall, const std::string &rpt1, const std::string &rpt2, unsigned char flag1, unsigned char flag2, unsigned char flag3, int num_dv_frames, int num_dv_silent_frames, int num_bit_errors);

	// The following three functions don't block waiting for a reply, they just send the data

	// Send query for a user, a false return implies a network error
	bool findUser(const std::string &userCallsign);

	// The following functions are for processing received messages

	// Get the waiting message type
	IRCDDB_RESPONSE_TYPE getMessageType();

	// Get a gateway message, as a result of IDRT_REPEATER returned from getMessageType()
	// A false return implies a network error
	bool receivePing(std::string &repeaterCallsign);

	void sendPing(const std::string &to, const std::string &from);

	void close();		// Implictely kills any threads in the IRC code

	CCacheManager cache;

private:
	IRCDDBApp *app;
	IRCClient *client;
};
