#include "padproxy.h"

usb_pad::PadProxyBase::PadProxyBase(const std::string& name)
{
	RegisterPad::instance().Add(name, this);
}