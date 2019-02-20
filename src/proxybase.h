#pragma once
//TODO Maybe too much inheritance?
class ProxyBase
{
	public:
	virtual ~ProxyBase() {}
	virtual const TCHAR* Name() const = 0;
	virtual int Configure(int port, const char* dev_type, void *data) = 0;
};