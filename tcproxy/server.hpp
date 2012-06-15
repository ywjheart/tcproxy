#ifndef HTTP_SERVER3_SERVER_HPP
#define HTTP_SERVER3_SERVER_HPP

#include "connection.hpp"
#include "config.h"

/// The top-level class of the server.
class server
	: private boost::noncopyable
{
public:
	/// Construct the server to listen on the specified TCP address and port, and
	/// serve up files from the given directory.
	explicit server(boost::asio::io_service& io_service);

protected:
	/// Initiate an asynchronous accept operation.
	void start_accept();

	/// Handle completion of an asynchronous accept operation.
	void handle_accept(const boost::system::error_code& e);

	/// The io_service used to perform asynchronous operations.
	boost::asio::io_service& io_service_;

	/// Acceptor used to listen for incoming connections.
	boost::asio::ip::tcp::acceptor acceptor_;

	/// The next connection to be accepted.
	connection_ptr new_connection_;

	endpoint_collection_shared remote_ep_;
};

#endif // HTTP_SERVER3_SERVER_HPP