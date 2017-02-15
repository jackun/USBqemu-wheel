#!/bin/sh

set -ex

linux_32_before_install() {
	# Build worker is 64-bit only by default it seems.
	sudo dpkg --add-architecture i386

	sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test

	# Compilers
	if [ "${CXX}" = "g++" ]; then
		COMPILER_PACKAGE="g++-${VERSION}-multilib"
	fi

	# apt-get update fails because Chrome is 64-bit only.
	sudo rm -f /etc/apt/sources.list.d/google-chrome.list

	sudo apt-get -qq update

	# The 64-bit versions of the first 7 dependencies are part of the initial
	# build image. libgtk2.0-dev:i386 requires the 32-bit
	# versions of the dependencies, and the 2 versions conflict. So those
	# dependencies must be explicitly installed.
	sudo apt-get -y install \
		gir1.2-freedesktop:i386 \
		gir1.2-gdkpixbuf-2.0:i386 \
		gir1.2-glib-2.0:i386 \
		libgirepository-1.0-1:i386 \
		libglib2.0-dev:i386 \
		libgtk2.0-dev:i386 \
		libpulse-dev:i386 \
		${COMPILER_PACKAGE}
}

linux_32_script() {
	mkdir build
	cd build

	export CC=${CC}-${VERSION} CXX=${CXX}-${VERSION}
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=cmake/linux-compiler-i386-multilib.cmake \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=/usr/lib/i386-linux-gnu/pcsx2 \
		..

	# Documentation says 1.5 cores, so 2 or 3 threads should work ok.
	make -j3 install
}


linux_64_before_install() {
	# Compilers
	if [ "${CXX}" = "g++" ]; then
		sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
		COMPILER_PACKAGE="g++-${VERSION}"
	fi

	sudo apt-get -qq update

	sudo apt-get -y install \
		libgtk2.0-dev \
		libpulse-dev \
		${COMPILER_PACKAGE}
}


linux_64_script() {
	mkdir build
	cd build

	# Prevents warning spam
	export CC=${CC}-${VERSION} CXX=${CXX}-${VERSION}
	cmake \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=/usr/lib/i386-linux-gnu/pcsx2 \
		..

	# Documentation says 1.5 cores, so 2 or 3 threads should work ok.
	make -j3 install
}

linux_after_success() {
	ccache -s
}

# Just in case I do manual testing and accidentally insert "rm -rf /"
case "${1}" in
before_install|script)
	${TRAVIS_OS_NAME}_${BITS}_${1}
	;;
before_script)
    ;;
after_success)
	${TRAVIS_OS_NAME}_${1}
	;;
*)
	echo "Unknown command" && false
	;;
esac
