#include "padproxy.h"

PadProxyBase::PadProxyBase(const std::string& name)
{
	RegisterPad::instance().Add(name, this);
}
