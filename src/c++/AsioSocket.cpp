#include "include/AsioSocket.hpp"
#include <iostream>

namespace educhat {

	AsioSocket::AsioSocket()
		: socket(io_service)
	{
		connected = false;
		connect_in_progress = false;
		send_in_progress = false;
		recv_in_progress = false;
		recv_msg[0] = '\0';
		recv_msg[RECVMSG_MAXLENGTH-1] = '\0';
	}

	AsioSocket::~AsioSocket()
	{
		std::cout << "destruction\n";
		io_service.post([this]() { socket.close(); });
		t->join();
		delete t;
	}

	void AsioSocket::connect(const std::string addr, const std::string port)
	{
		if (connect_in_progress || connected)
			return;

		connect_in_progress = true;

		boost::asio::ip::tcp::resolver resolver(io_service);
		auto endpoint_iterator = resolver.resolve({ addr, port });

		boost::asio::async_connect(socket, endpoint_iterator,
			[this](boost::system::error_code ec, boost::asio::ip::tcp::resolver::iterator)
			{
				std::cout << "handler called!\n";
				connect_in_progress = false;
				if (!ec) {
					connected = true;
					std::cout << "connected\n";
					doRecv();
				} else {
					std::cout << "connect failed\n";
					exit(1);
				}
			});

		t = new std::thread([this](){ io_service.run(); });
	}

	bool AsioSocket::isConnected() const
	{
		return connected;
	}

	void AsioSocket::send(const std::string msg)
	{
		if (!msg.empty()) {
			to_send.push_back(msg);
			std::cout << "message queued: " << msg.substr(0, msg.length()-2) << '\n';
		}

		if (to_send.empty() || send_in_progress)
			return;

		doSend();
	}


	void AsioSocket::doSend()
	{
		send_in_progress = true;

		std::cout << "sending: " << to_send.front().substr(0, to_send.front().length()-2) << "\n";

		boost::asio::async_write(socket,
			boost::asio::buffer(to_send.front().c_str(),
				to_send.front().length()),
			[this](boost::system::error_code ec, std::size_t)
			{
				if (!ec) {
					std::cout << "sent: " << to_send.front();
					to_send.pop_front();
					send_in_progress = false;
				} else {
					std::cout << "send err\n";
					socket.close();
					exit(1);
				}
			});
	}

	std::string AsioSocket::recv()
	{
		std::string msg = "";

		if (!to_recv.empty()) {
			msg = to_recv.front();
			to_recv.pop_front();
		}

		return msg;
	}

	void AsioSocket::doRecv()
	{
		if (recv_in_progress)
			return;

		recv_in_progress = true;

		socket.async_read_some(//socket,
			boost::asio::buffer(recv_msg, RECVMSG_MAXLENGTH-1),
			[this](boost::system::error_code ec, std::size_t len)
			{
				if (!ec) {
					for (std::size_t i = len; i < RECVMSG_MAXLENGTH; ++i) {
						recv_msg[i] = '\0';
					}

					to_recv.push_back(recv_msg);
					
					recv_in_progress = false;
					doRecv();
				} else {
					std::cout << "recv failed " << ec << "\n";
					socket.close();
					exit(1);
				}
			});
	}

} // namespace educhat