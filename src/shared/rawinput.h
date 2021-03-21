#pragma once
#include <windows.h>
#include "hidapi.h"
#include <setupapi.h>

namespace shared
{
	namespace rawinput
	{

		class ParseRawInputCB
		{
		public:
			virtual void ParseRawInput(PRAWINPUT pRawInput) = 0;
		};

		int Initialize(void* hWnd);
		void Uninitialize();

		void RegisterCallback(ParseRawInputCB* cb);
		void UnregisterCallback(ParseRawInputCB* cb);
	} // namespace rawinput
} // namespace shared