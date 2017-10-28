#include <algorithm>
#include <cctype>

#include "include/IrcHandler.hpp"

namespace educhat {

	IrcHandler::IrcHandler(std::shared_ptr<Socket> sock)
	{
		socket = sock;
	}

	IrcHandler::~IrcHandler()
	{
		socket.reset();
	}

	void IrcHandler::handleCommand(const std::string command)
	{
		// split to get first "word"?
		std::vector<std::string> words;

		split(command, words);

		// match first "word" to known command
		// run command as appropriate
		// refer to RFC 2812
		// TODO: actually fix. keep for quick implementation testing
		if (words.empty()) {
			return;
		}

		std::string first = words[0];
		std::transform(first.begin(), first.end(), first.begin(), ::tolower);

		if (first == "/connect" || first == "/c") {
			if (words.size() > 2) {
				socket->connect(words[1], words[2]);
			}
		} else if (first == "/join" || first == "/j") {
			if (words.size() > 1) {
				channel = words[1];
				socket->send("JOIN " + channel + "\r\n");
			}
		} else if (first == "/nick" || first == "/n") {
			if (words.size() > 1) {
				nick = words[1];
				socket->send("NICK " + nick + "\r\n");
			}
		} else if (first == "/user" || first == "/u") {
			if (words.size() > 4) {
				user = words[1];
				std::string irc_user_command = "USER " + user + " " +
				                               words[2] + " " +
						     	       words[3] + " " +
						     	       words[4] + " :";
				for (std::size_t i = 5; i < words.size()-1; ++i) {
						irc_user_command += words[i] + " ";
				}
				irc_user_command += words[words.size()-1] + "\r\n";
				socket->send(irc_user_command);
			}
		} else {
			socket->send("PRIVMSG " + channel + " :" + command + "\r\n");
		}
	}

	bool IrcHandler::isConnected() const
	{
		return socket->isConnected();
	}

	std::shared_ptr<message> IrcHandler::getUpdate()
	{
		// recv from socket TODO consider try/catch
		std::string received = socket->recv();
		if (received.empty()) {
			return nullptr;
		}

		if (received.substr(0, 4) == "PING") {
			socket->send("PONG\r\n");
			return nullptr;
		}

		std::shared_ptr<message> msg(new message);

		// handle message from socket
		// refer to RFC 2812
		// TODO: actually fix. keeping for quick implementation testing
		msg->type = log_msg;
		msg->text = received;
		msg->owner = "channel";
		time(&msg->timestamp);

		// return struct message pointer
		return msg;
	}

} // namespace educhat
