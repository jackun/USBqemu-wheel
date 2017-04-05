# - Find libusb for portable USB support
# This module will find libusb as published by
#  http://libusb.sf.net and
#  http://libusb-win32.sf.net
#
# It will use PkgConfig if present and supported, else search
# it on its own. If the LibUSB_ROOT_DIR environment variable
# is defined, it will be used as base path.
# The following standard variables get defined:
#  LibUSB_FOUND:        true if LibUSB was found
#  LibUSB_INCLUDE_DIRS: the directory that contains the include file
#  LibUSB_LIBRARIES:    the library

include ( CheckLibraryExists )
include ( CheckIncludeFile )

OPTION(BUILD_TARGET64 "Build for 64 bit target if set to ON or 32 bit if set to OFF." OFF)

find_package ( PkgConfig )
if ( PKG_CONFIG_FOUND )
  pkg_check_modules ( PKGCONFIG_LIBUSB libusb-1.0>=1.0 )
  if ( NOT PKGCONFIG_LIBUSB_FOUND )
    pkg_check_modules ( PKGCONFIG_LIBUSB libusb>=1.0 )
  endif ( NOT PKGCONFIG_LIBUSB_FOUND )
endif ( PKG_CONFIG_FOUND )

if ( PKGCONFIG_LIBUSB_FOUND )
    set ( LibUSB_FOUND ${PKGCONFIG_LIBUSB_FOUND} )
    set ( LibUSB_INCLUDE_DIRS ${PKGCONFIG_LIBUSB_INCLUDEDIR}/libusb-1.0 )
    foreach ( i ${PKGCONFIG_LIBUSB_LIBRARIES} )
    find_library ( ${i}_LIBRARY
      NAMES ${i}
      PATHS ${PKGCONFIG_LIBUSB_LIBDIR}
    )
    if ( ${i}_LIBRARY )
        list ( APPEND LibUSB_LIBRARIES ${${i}_LIBRARY} )
    endif ( ${i}_LIBRARY )
    mark_as_advanced ( ${i}_LIBRARY )
    endforeach ( i )

else ( PKGCONFIG_LIBUSB_FOUND )

    find_path ( LibUSB_DIR
    NAMES
      libusb.h
    PATHS
      $ENV{ProgramFiles}/LibUSB-Win32
      $ENV{LibUSB_ROOT_DIR}
      ${LibUSB_DIR}
      "../libusb"
    PATH_SUFFIXES
      include
      include/libusb-1.0
    DOC "root directory of LibUSB"
    )

    find_path ( LibUSB_INCLUDE_DIRS
    NAMES
      libusb-1.0/libusb.h
    PATHS
      $ENV{ProgramFiles}/LibUSB-Win32
      $ENV{LibUSB_ROOT_DIR}
      ${LibUSB_DIR}
	  "../libusb"
    PATH_SUFFIXES
      include
      #include/libusb-1.0
    )

    mark_as_advanced ( LibUSB_INCLUDE_DIRS )
    #  message ( STATUS "LibUSB include dir: ${LibUSB_ROOT_DIR}" )

    if ( ${CMAKE_SYSTEM_NAME} STREQUAL "Windows" )
        # LibUSB-Win32 binary distribution contains several libs.
        # Use the lib that got compiled with the same compiler.
        if ( MSVC )
            if (BUILD_TARGET64)
                set ( LibUSB_LIBRARY_PATH_SUFFIX MS64/dll )
            else (BUILD_TARGET64)
                set ( LibUSB_LIBRARY_PATH_SUFFIX MS32/dll )
            endif (BUILD_TARGET64)
        elseif ( BORLAND )
            set ( LibUSB_LIBRARY_PATH_SUFFIX lib/bcc )
        elseif ( CMAKE_COMPILER_IS_GNUCC )
            set ( LibUSB_LIBRARY_PATH_SUFFIX lib/gcc )
        endif ( MSVC )
    endif ( ${CMAKE_SYSTEM_NAME} STREQUAL "Windows" )

    find_library ( LibUSB_LIBRARY
    NAMES
        libusb usb libusb-1.0
    PATHS
        $ENV{ProgramFiles}/LibUSB-Win32
        $ENV{LibUSB_ROOT_DIR}
        ${LibUSB_DIR}
		"../libusb"
    PATH_SUFFIXES
        ${LibUSB_LIBRARY_PATH_SUFFIX}
    )
    mark_as_advanced ( LibUSB_LIBRARY )
    if ( LibUSB_LIBRARY )
        set ( LibUSB_LIBRARIES ${LibUSB_LIBRARY} )
    endif ( LibUSB_LIBRARY )

    if ( LibUSB_INCLUDE_DIRS AND LibUSB_LIBRARIES )
        set ( LibUSB_FOUND true )
    endif ( LibUSB_INCLUDE_DIRS AND LibUSB_LIBRARIES )
endif ( PKGCONFIG_LIBUSB_FOUND )

if ( LibUSB_FOUND )
    set ( CMAKE_REQUIRED_INCLUDES "${LibUSB_INCLUDE_DIRS}" )
    check_include_file ( usb.h LibUSB_FOUND )
#    message ( STATUS "LibUSB: usb.h is usable: ${LibUSB_FOUND}" )
endif ( LibUSB_FOUND )
if ( LibUSB_FOUND )
    check_library_exists ( "${LibUSB_LIBRARIES}" usb_open "" LibUSB_FOUND )
#    message ( STATUS "LibUSB: library is usable: ${LibUSB_FOUND}" )
endif ( LibUSB_FOUND )

if ( NOT LibUSB_FOUND )
    if ( NOT LibUSB_FIND_QUIETLY )
      message ( STATUS "LibUSB not found, try setting LibUSB_ROOT_DIR environment variable." )
    endif ( NOT LibUSB_FIND_QUIETLY )
    if ( LibUSB_FIND_REQUIRED )
      message ( FATAL_ERROR "" )
    endif ( LibUSB_FIND_REQUIRED )
endif ( NOT LibUSB_FOUND )
#  message ( STATUS "LibUSB: ${LibUSB_FOUND}" )
