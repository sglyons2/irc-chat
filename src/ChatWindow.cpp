#include "include/ChatWindow.hpp"
#include <cctype>

enum CONN_STATUS { NO_CONN=1, IS_CONN=2 };

ChatWindow::ChatWindow()
{
	cursor_y = 0;
	cursor_x = 0;
	// Will be drawing line by line, so no need to create a window
	// Can consider showing a cursor and keeping track of it
	// to save, modify, then restore afterwards
	// Create socket? Get ready?
	socket = new IRCSocket();
	// Other colors picked to avoid a bit of typical customization like
	// solarized colors changing the colors from what is expected.
	// 197 = Red
	// 35 = Green
	init_pair(NO_CONN, -1, 197);
	init_pair(IS_CONN, -1, 35);
}

ChatWindow::~ChatWindow()
{
	// erase all chat info
	delete socket;
}

void ChatWindow::draw(Window *parent)
{
	drawMessages(parent);
	drawStatusBar(parent);
	drawInput(parent);
	// refresh is parent's responsibility after draw
}

void ChatWindow::drawMessages(Window *parent)
{
	clearRows(parent, 0, parent->height-2);

	saveCursor(parent);
	// draw last parent->rows-2 lines of messages
	// Convert messages to lines until fill up last parent->rows-2 lines
	// Draw lines
	if (!messages.empty()) {
		// translate to lines
		// then work with last parents->height-2 lines

		if ((int) messages.size() > parent->height-2) {
			for (int i = 0; i <= parent->height-2; i++) {
				mvwprintw(parent->window, parent->height-2 + parent->begin_y - i, 0, messages[messages.size()-i].getMessage().c_str());
			}
		} else {
			for (size_t i = 0; i < messages.size(); i++) {
				mvwprintw(parent->window, parent->begin_y + i, 0, messages[i].getMessage().c_str());
			}
		}
	}

	restoreCursor(parent);
}

void ChatWindow::drawStatusBar(Window *parent)
{
	clearRows(parent, parent->height-2, 1);

	saveCursor(parent);
	time_t cur_time = time(NULL);
	struct tm *tm = localtime(&cur_time);
	// Use attributes for color~
	// limit info based on cols/width
	// Priority:
	//  1) Channel
	//  2) Time
	//  3) Nickname
	//  4) Server
	//  Channel, Server, Nickname can be taken from IRCSocket?
	
	// TODO: cleanup
	std::string channel = socket->channel;
	std::string nickname = socket->nickname;
	std::string server = socket->server;
	int y = parent->height-2;
	int x = 0;
	wattr_on(parent->window, A_STANDOUT, NULL);
	for(int i = 0; i < parent->width; i++) {
		mvwaddch(parent->window, y, i, ' ');
	}
	if (!socket->nickname.empty()) {
		mvwprintw(parent->window, y, x, nickname.c_str());
		x += nickname.length();
		mvwprintw(parent->window, y, x, "@");
		x++;
	}
	if (!socket->server.empty()) {
		int pair = socket->isConnected() ? IS_CONN : NO_CONN;
		wattr_on(parent->window, COLOR_PAIR(pair) | A_BOLD, NULL);
		mvwprintw(parent->window, y, x, server.c_str());
		wattr_off(parent->window, COLOR_PAIR(pair) | A_BOLD, NULL);
		x += server.length();
	}
	if (!socket->nickname.empty()) {
		mvwprintw(parent->window, y, x, "/");
		x++;
		mvwprintw(parent->window, y, x, channel.c_str());
	}
	mvwprintw(parent->window, parent->height-2, parent->width-8,
	          "%02u:%02u:%02u", tm->tm_hour, tm->tm_min, tm->tm_sec);
	wattr_off(parent->window, A_STANDOUT, NULL);
	restoreCursor(parent);
}

void ChatWindow::drawInput(Window *parent)
{
	clearRows(parent, parent->height-1, 1);

	// Use horizontal scrolling to have it consistent and usable
	std::string visible;

       	if ((int) input.length() < parent->width-5) {
		visible = input;
	} else {
		// TODO: support left<->right scrolling.
		// gotta introduce cursor watching stuff for that.
		visible = input.substr(input.length()-(parent->width-5));
	}

	mvwprintw(parent->window, parent->height-1, 0, visible.c_str());
	cursor_y = parent->height-1;
	cursor_x = visible.length();
	wmove(parent->window, cursor_y, cursor_x);
}

void ChatWindow::saveCursor(Window *parent)
{
	getyx(parent->window, cursor_y, cursor_x);
}

void ChatWindow::restoreCursor(Window *parent)
{
	wmove(parent->window, cursor_y, cursor_x);
}

void ChatWindow::clearRows(Window *parent, int begin_y, int num_lines)
{
	saveCursor(parent);
	for (int i = 0; i < num_lines; i++) {
		wmove(parent->window, begin_y+i, 0);
		wclrtoeol(parent->window);
	}
	restoreCursor(parent);
}

void ChatWindow::refresh(Window *parent)
{
	if (cursor_y != parent->height) {
		cursor_y = parent->height;
		wmove(parent->window, cursor_y, cursor_x);
	}
	// technically is parent's refresh, but we can also do work here~
	// do a lot of the work here
	drawStatusBar(parent); // update time and status every time
	socket->send();
	std::string recv = socket->recv();
	if (!recv.empty()) {
		addMessage(recv);
		drawMessages(parent);
	}
}

// Working with Input! Parent takes care of some, but not all! Passes the rest here!
// Submit input for processing
void ChatWindow::handleInput(Window *parent, int ch)
{
	switch(ch) {
	case KEY_ENTER:
	case 10u:
		submitInput(parent);
		break;
	case KEY_BACKSPACE:
	case 7u:
		if (!input.empty()) {
			input.pop_back();
			drawInput(parent);
		}
		break;
	default:
		if (isalnum(ch) || ispunct(ch) || ch == ' ') {
			input += ch;
			drawInput(parent);
		}
	}
}

void ChatWindow::submitInput(Window *parent)
{
	// check for commands
	if (input.find("/connect ") == 0) {
		socket->connect(input.substr(9), "#hello", "nick");
	} else {
		// translate into Message for messages
		// translate into raw message for sending to server
		addMessage(input);
		socket->send(input);
	}
	input.clear();
	draw(parent);
}

// Adding Messages!
void ChatWindow::addMessage(std::string raw_msg)
{
	// refine this with IRC specifics or run it through something like IRCFormatter
	Message tmp(time(NULL), "anon", raw_msg);
	messages.push_back(tmp);
	// TODO: change 100 to a max (not going to just remove what no longer fits,
	//       as window resizing it supported).
	if (messages.size() > 100) {
		messages.erase(messages.begin(), messages.end()-99);
	}
}

void ChatWindow::processSend(std::string raw_msg)
{
	// if starts with /
		// tokenize
	// else make it into a privmsg
}

void ChatWindow::processRecv(std::string raw_msg)
{
	// tokenize
	// response response_number more
}
