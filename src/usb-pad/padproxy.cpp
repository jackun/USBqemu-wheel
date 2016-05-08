#include "padproxy.h"

PadProxyBase::PadProxyBase(std::string name)
{
	RegisterPad::instance().Add(name, this);
}
