#include "common.h"
#include <stdexcept>

#if defined(BUILD_RAW)
#include "rawinput.h"
#endif

namespace common {

	void Initialize(void *ptr)
	{
		// Keeping it simple, for now
		#if defined(BUILD_RAW)
			if (!common::rawinput::Initialize(ptr))
				throw std::runtime_error("Failed to initialize raw input!");
		#endif
	}

	void Uninitialize(/*void *ptr*/)
	{
		#if defined(BUILD_RAW)
			common::rawinput::Uninitialize(/*ptr*/);
		#endif
	}

};