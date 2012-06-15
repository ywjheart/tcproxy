#include "stdafx.h"
#include "config.h"

bool configmanager::init()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	threads_ = std::max((int)si.dwNumberOfProcessors,2);

	return true;
}