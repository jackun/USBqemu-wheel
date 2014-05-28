USBqemu-wheel
=============

PCSX2 usb plugin based on gigaherz et al. usb plugin.

Started as a way to use Logitech MOMO wheel with Gran Turismo 3/4.
Probably works properly with Logitech wheels only currently.

GT Force buttons (as seen in GT4) (currently)
CROSS     - menu up
SQUARE    - menu down
CIRCLE    - backview/home / X
TRIANGLE  - cancel / Y
R1        - start / A?
L1        - reverse/select / B
Maybe 2 more buttons.

Mass storage device
=======

Now includes preliminary support for usb mass storage devices. Create a image file and format it.
http://www.fysnet.net/mtools.htm or http://www.ltr-data.se/opencode.html/ might be of some help to windows users.

A 256 MB image file is included in `Release` folder. 
You should be able to access files in image file with 7-zip ( http://7-zip.org/ ).

On linux:

	truncate -s 256M usb.img
	losetup -f usb.img
	mkfs.vfat -F 32 /dev/loopX
	mount /dev/loopX /mnt #or something

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
