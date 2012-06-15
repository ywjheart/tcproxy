// tcproxy.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "server.hpp"

#include <tchar.h>
#include <iostream>

class ioservice
{
public:
	boost::asio::io_service io_service_;
	boost::asio::signal_set signals_;
	ioservice():signals_(io_service_)
	{
		// Register to handle the signals that indicate when the server should exit.
		// It is safe to register for the same signal multiple times in a program,
		// provided all registration for the specified signal is made through Asio.
		signals_.add(SIGINT);
		signals_.add(SIGTERM);
#if defined(SIGQUIT)
		signals_.add(SIGQUIT);
#endif // defined(SIGQUIT)
		signals_.async_wait(boost::bind(&ioservice::handle_stop, this));
	}

	void handle_stop()
	{
		io_service_.stop();
	}

	boost::asio::io_service& get()
	{
		return io_service_;
	}
	void run()
	{
		// Create a pool of threads to run all of the io_services.
		std::vector<boost::shared_ptr<boost::thread> > threads;
		std::size_t thread_pool_size = configmanager::GetInstance()->threads_;
		for (std::size_t i = 0; i < thread_pool_size; ++i)
		{
			boost::shared_ptr<boost::thread> thread(new boost::thread(
				boost::bind(&boost::asio::io_service::run, &io_service_)));
			threads.push_back(thread);
		}

		// Wait for all threads in the pool to exit.
		for (std::size_t i = 0; i < threads.size(); ++i)
			threads[i]->join();
	}
};

// because a domain must be in ansi
std::string Utf16ToAnsi(std::wstring src)
{
	std::string ret;
	for (size_t i=0;i<src.size();i++)
	{
		ret.push_back((char)src[i]);
	}
	return ret;
}

#define ARGV(i) Utf16ToAnsi(argv[i])

int _tmain(int argc, TCHAR* argv[])
{
	if (argc<3)
	{
		std::cerr << "Usage:" << ARGV(0) << " remote_address remote_port [local_address] [local_port]" << std::endl; 
		return -1;
	}
	std::string remote_address = ARGV(1);
	std::string remote_port = ARGV(2);
	std::cout << "Remote: " << remote_address << ":" << remote_port << std::endl; 

	std::string local_address = (argc>=4)?ARGV(3):"0.0.0.0";
	std::string local_port = (argc>=5)?ARGV(4):remote_port;
	std::cout << "Local: " << local_address << ":" << local_port << std::endl; 

	configmanager::GetInstance()->remote_.address_ = remote_address;
	configmanager::GetInstance()->remote_.port_ = remote_port;
	configmanager::GetInstance()->local_.address_ = local_address;
	configmanager::GetInstance()->local_.port_ = local_port;
	if (!configmanager::GetInstance()->init())
	{
		std::cerr << "Unable to init configuration" << std::endl;
		return -1;
	}

	std::cout << "Thread: " << configmanager::GetInstance()->threads_ << std::endl; 

	try
	{
		ioservice ios;
		server svr(ios.get());
		ios.get().run();
	}
	catch(std::exception e)
	{
		return -1;
	}
	return 0;
}
