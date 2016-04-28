USBqemu-wheel
=============

PCSX2 usb plugin based on linuzappz, shadow, gigaherz et al. usb plugin using usb host emulation code from qemu.

USB packets with FFB commands are passed staight to the wheel when using raw input API mode.
As they are pretty much vendor specific, force feedback probably works with Logitech wheels only.

As such, DInput (DirectX) mode is recommended.

Linux: no built-in button/axis remapping yet.

Mass storage device
=======

Now includes preliminary support for usb mass storage devices. Create a image file and format it.
http://www.fysnet.net/mtools.htm or http://www.ltr-data.se/opencode.html/ might be of some help to windows users.

A 256MB and 4GB image file is included in `Release` folder. 
You should be able to access files in image file with 7-zip ( http://7-zip.org/ ).

On linux:

	truncate -s 256M usb.img
	mkfs.vfat -F 32 usb.img
	
Optionally mount image for file transfer:

	losetup -f usb.img
	mount /dev/loopX /mnt #or somewhere else

or let `mount` automagically set up a loopback device:

	mount usb.img /mnt
	
Of course, if a PS2 game/program itself can format a drive then you can just use some random file, heh.

Singstar
========

You can use 2 mono/stereo mics or one stereo mic with separate per-channel input (select same microphone for both players; left channel = P1, right channel = P2).

Windows: Uses Core Audio API. As such, it needs Vista or newer.

Linux: not supported yet, but one backend will be pulseaudio, atleast.


Building
==========

Basically:

	cd some/where/USBqemu-wheel
	mkdir build
	cd build
	cmake ..
	

CMake defines:

* `BUILD_RAW=[FALSE|TRUE]` for raw api
* `BUILD_DX=[FALSE|TRUE]` for dinput
* `BUILD_WITH_DXSDK=[FALSE|TRUE]` build with DX2010 SDK
* `BUILD_FIND_WINSDK=[FALSE|TRUE]` to find newest installed windows platform toolset/sdk. Probably unnecessery when building with Visual Studio.
	
DInput should be using Windows platform toolset/sdk now.

Optionally, you can still use 2010 DirectX SDK. CMake looks for %DXSDK_DIR% environment variable.

http://www.microsoft.com/en-us/download/details.aspx?id=6812

On linux, (limited) configuration dialog uses GTK+ as PCSX2 (still) uses wxGTK.

Forum
=========
http://forums.pcsx2.net/Thread-Qemu-USB-Wheel-mod

Credits
=========

DirectX version by Racer_S ( http://www.tocaedit.com/ )

Original by linuzappz, shadow, gigaherz, PCSX2 team.
