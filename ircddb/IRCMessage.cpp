//#include <string>
//#include <list>

#include "IRCMessage.h"

IRCMessage::IRCMessage()
{
	numParams = 0;
	prefixParsed = false;
}

IRCMessage::IRCMessage(const std::string &toNick, const std::string &msg)
{
	command = "PRIVMSG";
	numParams = 2;
	params.push_back(toNick);
	params.push_back(msg);
	prefixParsed = false;
}

IRCMessage::IRCMessage(const std::string &cmd)
{
	command = cmd;
	numParams = 0;
	prefixParsed = false;
}

IRCMessage::~IRCMessage()
{
}


void IRCMessage::addParam(const std::string &p)
{
	params.push_back(p);
	numParams = params.size();
}

int IRCMessage::getParamCount()
{
	return params.size();
}

std::string IRCMessage::getParam(int pos)
{
	return params[pos];
}

std::string IRCMessage::getCommand()
{
	return command;
}


void IRCMessage::parsePrefix()
{
	unsigned int i;

	for (i=0; i < 3; i++) {
		prefixComponents.push_back("");
	}

	int state = 0;

	for (i=0; i < prefix.length(); i++) {
		char c = prefix.at(i);

		switch (c) {
		case '!':
			state = 1; // next is name
			break;

		case '@':
			state = 2; // next is host
			break;

		default:
			prefixComponents[state].append(1, c);
			break;
		}
	}

	prefixParsed = true;
}

std::string &IRCMessage::getPrefixNick()
{
	if (!prefixParsed) {
		parsePrefix();
	}

	return prefixComponents[0];
}

std::string &IRCMessage::getPrefixName()
{
	if (!prefixParsed) {
		parsePrefix();
	}

	return prefixComponents[1];
}

std::string &IRCMessage::getPrefixHost()
{
	if (!prefixParsed) {
		parsePrefix();
	}

	return prefixComponents[2];
}

void IRCMessage::composeMessage(std::string &output)
{
	std::string o;

	if (prefix.length() > 0) {
		o = std::string(":") + prefix + ' ';
	}

	o += command;

	for (int i=0; i < numParams; i++) {
		if (i == (numParams - 1)) {
			o += (std::string(" :") + params[i]);
		} else {
			o += (std::string(" ") + params[i]);
		}
	}

	o += std::string("\r\n");

	output = o;
}
