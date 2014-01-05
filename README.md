USBqemu-wheel
=============

PCSX2 usb plugin based on gigaherz et al. usb plugin.

Started as a way to use Logitech MOMO wheel with Gran Turismo 3/4.
Probably works properly with Logitech wheels only currently.

Building
==========

Basically:

	cd some/where/USBqemu-wheel
	mkdir build
	cd build
	cmake ../src
	
CMake configures for 2010 DirectX SDK. It looks for %DXSDK_DIR% environment variable.
May need to tweak it for Windows 8+ platform SDK

http://www.microsoft.com/en-us/download/details.aspx?id=6812

Forum
=========
http://forums.pcsx2.net/Thread-Qemu-USB-Wheel-mod

Credits
=========

DirectX version by Racer_S ( http://www.tocaedit.com/ )

Original by linuzappz, shadow, gigaherz, PCSX2 team.