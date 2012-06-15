#ifndef HTTP_SERVER3_CONNECTION_HPP
#define HTTP_SERVER3_CONNECTION_HPP

typedef std::vector<boost::asio::ip::tcp::endpoint> endpoint_collection;
typedef boost::shared_ptr< endpoint_collection > endpoint_collection_shared;

/// Represents a single connection from a client.
class connection
	: public boost::enable_shared_from_this<connection>,
	private boost::noncopyable
{
public:
	/// Construct a connection with the given io_service.
	explicit connection(boost::asio::io_service& io_service,
		endpoint_collection_shared remote_ep);
	~connection();

	/// Get the socket associated with the connection.
	boost::asio::ip::tcp::socket& socket();

	int seq(){return seq_;}

	/// Start the first asynchronous operation for the connection.
	void start(uint32_t start_index);

private:
	/// Handle completion of a read operation.
	void handle_read_from_client(const boost::system::error_code& e,
		std::size_t bytes_transferred,
		boost::shared_ptr<boost::asio::streambuf> buf);
	void handle_read_from_server(const boost::system::error_code& e,
		std::size_t bytes_transferred,
		boost::shared_ptr<boost::asio::streambuf> buf);

	/// Handle completion of a write operation.
	void handle_write_to_client(const boost::system::error_code& e,
		boost::shared_ptr<boost::asio::streambuf> buf);
	void handle_write_to_server(const boost::system::error_code& e,
		boost::shared_ptr<boost::asio::streambuf> buf);

	void handle_connect_to_server(const boost::system::error_code& e);
	void handle_timeout(const boost::system::error_code& e,long tick);

	void write_to_client();
	void write_to_server();
	void read_from_client();
	void read_from_server();

	/// Strand to ensure the connection's handlers are not called concurrently.
	boost::asio::io_service::strand strand_;

	/// Socket for the connection.
	boost::asio::ip::tcp::socket socket_client_,socket_server_;

	/// Buffer for incoming data.
	std::list< boost::shared_ptr<boost::asio::streambuf> >  buffer_cs_,buffer_sc_;

	bool connected_,sending_to_client_,sending_to_server_;
	bool client_failure_,server_failure_;

	boost::asio::deadline_timer timer_;
	long last_tick_;

	uint32_t cur_index_,retried_count_;
	endpoint_collection_shared remote_ep_;
protected:
	static int total_seq_;
	int seq_;
};

typedef boost::shared_ptr<connection> connection_ptr;

#endif // HTTP_SERVER3_CONNECTION_HPP