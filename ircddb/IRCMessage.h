#pragma once
#include <string>
#include <vector>

class IRCMessage
{
public:
	IRCMessage();
	IRCMessage(const std::string& toNick, const std::string& msg);
	IRCMessage(const std::string& command);
	~IRCMessage();

	std::string prefix;
	std::string command;
	std::vector<std::string> params;

	int numParams;

	std::string &getPrefixNick();
	std::string &getPrefixName();
	std::string &getPrefixHost();

	void composeMessage (std::string& output);

	void addParam(const std::string &p);

	std::string getCommand();

	std::string getParam(int pos);

	int getParamCount();

private:
	void parsePrefix();

	std::vector<std::string> prefixComponents;
	bool prefixParsed;
};
