#pragma once
//TODO Maybe too much inheritance?
class ProxyBase
{
	public:
	virtual ~ProxyBase() {}
	virtual const TCHAR* Name() const = 0;
	virtual int Configure(int port, void *data) = 0;
	virtual std::vector<CONFIGVARIANT> GetSettings() = 0;
};