#pragma once

struct addressinfo 
{
	std::string address_;
	std::string port_;
	addressinfo& operator=(const addressinfo& src)
	{
		address_ = src.address_;
		port_ = src.port_;
		return *this;
	}
	addressinfo(const addressinfo& src)
	{
		*this = src;
	}
	addressinfo()
	{

	}
};

struct config
{
	addressinfo local_;
	addressinfo remote_;
	uint32_t threads_;
};

class configmanager:
	public config,public CSingleTon_t<configmanager>
{
public:
	bool init();
};