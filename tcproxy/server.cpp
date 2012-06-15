#include "stdafx.h"
#include "server.hpp"

server::server(boost::asio::io_service& io_service)
	: acceptor_(io_service),io_service_(io_service),new_connection_()
{
	boost::system::error_code ignored_ec;
	// Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
	std::string& address = configmanager::GetInstance()->local_.address_;
	std::string& port = configmanager::GetInstance()->local_.port_;
	boost::asio::ip::tcp::resolver resolver(io_service_);
	boost::asio::ip::tcp::resolver::query query(address, port);
	boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query,ignored_ec);
	std::cout << "Local: " << address << " -> " << endpoint.address().to_string() << std::endl; 
	acceptor_.open(endpoint.protocol());
	acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
	acceptor_.bind(endpoint);
	acceptor_.listen();

	// solve remote address
	remote_ep_.reset(new endpoint_collection);
	{
		address = configmanager::GetInstance()->remote_.address_;
		port = configmanager::GetInstance()->remote_.port_;

		boost::asio::ip::tcp::resolver::query query(address, port);
		for (boost::asio::ip::tcp::resolver::iterator it=resolver.resolve(query,ignored_ec);
			it!=boost::asio::ip::tcp::resolver::iterator();it++)
		{
			endpoint = *it;
			std::cout << "Remote: " << address << " -> " << endpoint.address().to_string() << std::endl; 
			remote_ep_->push_back(endpoint);
		}
		if (remote_ep_->size()==0) throw std::exception("");
	}

	start_accept();
}

void server::start_accept()
{
	new_connection_.reset(new connection(io_service_,remote_ep_));
	acceptor_.async_accept(new_connection_->socket(),
		boost::bind(&server::handle_accept, this,
		boost::asio::placeholders::error));
}

void server::handle_accept(const boost::system::error_code& e)
{
	if (!e)
	{
		uint32_t index= new_connection_->seq()%remote_ep_->size();
		new_connection_->start(index);
	}

	start_accept();
}
