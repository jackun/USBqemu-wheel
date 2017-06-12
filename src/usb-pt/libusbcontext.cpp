// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+

#include "../USB.h"
#include <libusb-1.0/libusb.h>
#include <memory>
#include <mutex>

#include "libusbcontext.h"

namespace LibusbContext
{
static std::shared_ptr<libusb_context> s_libusb_context;
static std::once_flag s_context_initialized;

static libusb_context* Create()
{
	libusb_context* context;
	const int ret = libusb_init(&context);
	if (ret < LIBUSB_SUCCESS)
	{
		bool is_windows = false;
#ifdef _WIN32
		is_windows = true;
#endif
		if (is_windows && ret == LIBUSB_ERROR_NOT_FOUND)
			SysMessage(TEXT("Failed to initialize libusb because usbdk is not installed.\n"));
		else
			SysMessage(TEXT("Failed to initialize libusb: %" SFMTs "\n"), libusb_error_name(ret));
		return nullptr;
	}
	return context;
}

void Exit()
{
	s_libusb_context = nullptr;
}

std::shared_ptr<libusb_context> Get()
{
	std::call_once(s_context_initialized, []() {
		s_libusb_context.reset(Create(), [](libusb_context* context) {
			OSDebugOut_noprfx(TEXT("******** libusb_exit lambda ********\n"));
			if (context != nullptr)
				libusb_exit(context);
		});
	});
	return s_libusb_context;
}
}
