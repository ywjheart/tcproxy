#include "stdafx.h"
#include "connection.hpp"

#define BUF_SIZE	(4*1024)
#define MAX_IDLE	(4*60)

int connection::total_seq_ = 0;

connection::connection(boost::asio::io_service& io_service,
					   endpoint_collection_shared remote_ep)
	: strand_(io_service),remote_ep_(remote_ep),
	socket_client_(io_service),socket_server_(io_service),timer_(io_service),
	connected_(false),
	sending_to_client_(false),sending_to_server_(false),
	seq_(++total_seq_),
	client_failure_(false),server_failure_(false)
{
}

connection::~connection()
{

}

boost::asio::ip::tcp::socket& connection::socket()
{
	return socket_client_;
}

void connection::start(uint32_t start_index)
{
	cur_index_ = start_index;
	retried_count_ = 0;
	{
		last_tick_ = 0;
		boost::system::error_code ignored_ec;
		timer_.expires_at(boost::posix_time::microsec_clock::universal_time() + boost::posix_time::seconds(MAX_IDLE), ignored_ec);
		timer_.async_wait(
			strand_.wrap(
			boost::bind(&connection::handle_timeout, shared_from_this(),
			boost::asio::placeholders::error,last_tick_)));
	}

	// wait for client's data
	read_from_client();

	// begin connecting to the remote server
	socket_server_.async_connect((*remote_ep_)[cur_index_], 
		strand_.wrap(
		boost::bind(&connection::handle_connect_to_server, shared_from_this(),
		boost::asio::placeholders::error)));
}

void connection::handle_read_from_client(const boost::system::error_code& e,
	std::size_t bytes_transferred,
	boost::shared_ptr<boost::asio::streambuf> buf)
{
	if (!e)
	{
		last_tick_++;
		buf->commit(bytes_transferred);
		buffer_cs_.push_back(buf);
		read_from_client();

		if (connected_ && !sending_to_server_ && !server_failure_)
		{// connected, send the current buffer
			sending_to_server_ = true;
			write_to_server();
		}
		else
		{
			// wait for connection, the data will be sent when connected
		}
	}
	else
	{
		boost::system::error_code ignored_ec;
		socket_client_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
		client_failure_ = true;
	}
}

void connection::handle_read_from_server(const boost::system::error_code& e,
							 std::size_t bytes_transferred,
							 boost::shared_ptr<boost::asio::streambuf> buf)
{
	if (!e)
	{
		last_tick_++;
		buf->commit(bytes_transferred);
		buffer_sc_.push_back(buf);
		read_from_server();

		if (!sending_to_client_ && !client_failure_)
		{
			sending_to_client_ = true;
			write_to_client();
		}
	}
	else
	{
		boost::system::error_code ignored_ec;
		socket_server_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
		server_failure_ = true;
	}
}

void connection::handle_connect_to_server(const boost::system::error_code& e)
{
	if (!e)
	{
		last_tick_++;
		connected_ = true;
		remote_ep_.reset();// recyle, no longer need them
		read_from_server();

		// send the pending data to the server if it exists
		if (buffer_cs_.size() && !sending_to_server_ && !server_failure_)
		{
			sending_to_server_ = true;
			write_to_server();
		}
	}
	else
	{
		retried_count_++;
		if (retried_count_==remote_ep_->size())
		{
			// tried all end point,give up
			boost::system::error_code ignored_ec;
			socket_server_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
			server_failure_ = true;
		}
		else
		{
			last_tick_++;

			cur_index_ = (cur_index_+1)%remote_ep_->size();
			socket_server_.async_connect((*remote_ep_)[cur_index_], 
				strand_.wrap(
				boost::bind(&connection::handle_connect_to_server, shared_from_this(),
				boost::asio::placeholders::error)));
		}
	}
}

void connection::handle_write_to_client(const boost::system::error_code& e,
										boost::shared_ptr<boost::asio::streambuf> buf)
{
	if (!e)
	{
		last_tick_++;
		if (buffer_sc_.size())
		{
			write_to_client();
		}
		else
		{
			sending_to_client_ = false;
		}
	}
	else
	{
		boost::system::error_code ignored_ec;
		socket_client_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
		client_failure_ = true;
	}
}

void connection::handle_write_to_server(const boost::system::error_code& e,
										boost::shared_ptr<boost::asio::streambuf> buf)
{
	if (!e)
	{
		last_tick_++;
		if (buffer_cs_.size())
			write_to_server();
		else
			sending_to_server_ = false;
	}
	else
	{
		boost::system::error_code ignored_ec;
		socket_server_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
		server_failure_ = true;
	}
}

void connection::write_to_client()
{
	boost::shared_ptr<boost::asio::streambuf> buf = buffer_sc_.front();
	buffer_sc_.pop_front();
	boost::asio::async_write(socket_client_, buf->data(),
		strand_.wrap(
		boost::bind(&connection::handle_write_to_client, shared_from_this(),
		boost::asio::placeholders::error,buf)));
}

void connection::write_to_server()
{
	boost::shared_ptr<boost::asio::streambuf> buf = buffer_cs_.front();
	buffer_cs_.pop_front();
	boost::asio::async_write(socket_server_, buf->data(),
			strand_.wrap(
			boost::bind(&connection::handle_write_to_server, shared_from_this(),
			boost::asio::placeholders::error,buf)));
}

void connection::read_from_client()
{
	if (server_failure_ && buffer_sc_.size()==0)
	{
		// 1. server side has failed
		// 2. no more pending data from server to client
		return;
	}

	boost::shared_ptr<boost::asio::streambuf> buf;
	buf.reset(new boost::asio::streambuf);
	socket_client_.async_read_some(buf->prepare(BUF_SIZE),
		strand_.wrap(
		boost::bind(&connection::handle_read_from_client, shared_from_this(),
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred,buf)));
}

void connection::read_from_server()
{
	if (client_failure_ && buffer_cs_.size()==0)
	{
		// 1. client side has failed
		// 2. no more pending data from client to server
		return;
	}

	boost::shared_ptr<boost::asio::streambuf> buf;
	buf.reset(new boost::asio::streambuf);
	socket_server_.async_read_some(buf->prepare(BUF_SIZE),
		strand_.wrap(
		boost::bind(&connection::handle_read_from_server, shared_from_this(),
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred,buf)));
}

void connection::handle_timeout(const boost::system::error_code& e,long tick)
{
	if (tick==last_tick_)
	{
		boost::system::error_code ignored_ec;
		if (!server_failure_) socket_server_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
		if (!client_failure_) socket_client_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
	}
	else if(!server_failure_ || !client_failure_)
	{
		boost::system::error_code ignored_ec;
		timer_.expires_at(boost::posix_time::microsec_clock::universal_time() + boost::posix_time::seconds(MAX_IDLE), ignored_ec);
		timer_.async_wait(
			strand_.wrap(
			boost::bind(&connection::handle_timeout, shared_from_this(),
			boost::asio::placeholders::error,last_tick_)));
	}
}
