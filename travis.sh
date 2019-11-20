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
	# build image. libgtk2.0-dev:i386 requires the 32-bit versions of
	# the dependencies, and the 2 versions conflict. So those
	# dependencies must be explicitly installed.
	sudo apt-get -y install \
		gir1.2-freedesktop:i386 \
		gir1.2-gdkpixbuf-2.0:i386 \
		gir1.2-glib-2.0:i386 \
		libcairo2-dev:i386 \
		libfontconfig1-dev:i386 \
		libfreetype6-dev:i386 \
		libpng-dev:i386 \
		libgdk-pixbuf2.0-dev:i386 \
		libpng12-dev:i386 \
		libgirepository-1.0-1:i386 \
		libglib2.0-dev:i386 \
		libgtk2.0-dev:i386 \
		libpango1.0-dev:i386 \
		libxft-dev:i386 \
		libpulse-dev:i386 \
		git-buildpackage \
		python-dateutil \
		python-pkg-resources \
		dh-make \
		build-essential \
		fakeroot \
		${COMPILER_PACKAGE}
}

linux_32_script() {
	# TODO where are they?
	BUILD_TMP=$(mktemp -d)
	for i in objcopy strip objdump; do
		ln -sv /usr/bin/$i ${BUILD_TMP}/i686-linux-gnu-$i
	done
	export PATH="${BUILD_TMP}:${PATH}"

	export CC=${CC}-${VERSION} CXX=${CXX}-${VERSION}
	if [ "x${TRAVIS_TAG}" != "x" ]; then
		# generate changelog since last time it was touched
		git checkout -b master
		gbp dch --spawn-editor=never -R -U low -a --upstream-tag=${TRAVIS_TAG} -N ${TRAVIS_TAG}
	fi
	dpkg-buildpackage -d -b -us -uc -ai386
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
		..

	# Documentation says 1.5 cores, so 2 or 3 threads should work ok.
	make -j3 install
}

linux_after_success() {
	for file in ../*.deb; do
		mv -v "${file}" "${file%%.deb}-${CC}-${VERSION}-${TARGET_DISTRIB}.deb"
	done
	ccache -s
}

# Just in case I do manual testing and accidentally insert "rm -rf /"
# Run manually with:
# TARGET_DISTRIB=[ubuntu|debian] TRAVIS_OS_NAME=linux BITS=32 VERSION=8 CC=gcc CXX=g++ ./travis.sh [before_script | script]
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
