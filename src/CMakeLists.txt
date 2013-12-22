cmake_minimum_required(VERSION 2.8.5)

SET(TargetName "USBqemu-wheel")
project(${TargetName})

set (PLUGIN_VERSION_MAJOR "0")
set (PLUGIN_VERSION_MINOR "1")
set (PLUGIN_VERSION_PATCH "1")
set (PLUGIN_VERSION "${PLUGIN_VERSION_MAJOR}.${PLUGIN_VERSION_MINOR}.${PLUGIN_VERSION_PATCH}")

#if(EXISTS "/usr/lib/i386-linux-gnu")
#    set(CMAKE_LIBRARY_ARCHITECTURE "i686-linux-gnu")
#endif()

set(HDRS
	./USB.h
	./PS2Etypes.h
	./PS2Edefs.h
	./qemu-usb/vl.h
	./qemu-usb/usb.h
	./usb-mic/usbreg.h
	./usb-mic/usbcore.h
	./usb-mic/audio.h
	./usb-mic/usbcfg.h
	./usb-mic/type.h
	./usb-mic/usbuser.h
	./usb-mic/usb.h
	./usb-mic/usbdesc.h
	./usb-mic/demo.h
	./usb-mic/usbhw.h
	./usb-mic/adcuser.h
	./usb-pad/usb-pad.h
	./usb-pad/config.h
)

set(SRCS
	./USB.cpp
	./qemu-usb/usb-hid.cpp
	./qemu-usb/usb-hub.cpp
	./qemu-usb/vl.cpp
	./qemu-usb/usb-base.cpp
	#./qemu-usb/usb-msd.cpp
	./qemu-usb/usb-ohci.cpp
	./usb-pad/bitjuggling.cpp
	./usb-pad/usb-pad.cpp
	#./qemu-usb/usb-kbd.cpp
	#./usb-mic/usb-mic-dummy.cpp
	#./usb-mic/usb-mic.cpp
	#./usb-eyetoy/usb-eyetoy.cpp
)

IF(WIN32)
	LIST(APPEND HDRS 
		./Win32/resource.h
		./Win32/Config.h
		./ddk/hidpi.h
		./ddk/setupapi.h
		./ddk/hidsdi.h
		./ddk/hidusage.h
	)
	LIST(APPEND SRCS 
		./usb-pad/usb-pad-win32.cpp
		./Win32/Config.cpp
		./Win32/Win32.cpp
	)
	file(GLOB RCS ./Win32/*.rc)
	file(GLOB DEF ./Win32/*.def)
	LIST(APPEND LIBS hid setupapi)
	#Less fcking around with platform sdks
	link_directories(${PROJECT_SOURCE_DIR}/ddk)
ENDIF(WIN32)

IF(UNIX)
	LIST(APPEND HDRS 
		./linux/ini.h
		./linux/actualfile.h
	)
	LIST(APPEND SRCS 
		./usb-pad/usb-pad-linux.cpp
		./linux/config.cpp
		./linux/ini.c
		./linux/actualfile.c
	)
ENDIF(UNIX)

IF(UNIX)
	add_definitions(-D_GNU_SOURCE -D_USE_LARGEFILE64 -D_FILE_OFFSET_BITS=64)
	find_package(GTK2 REQUIRED)
	LIST(APPEND LIBS ${GTK2_LIBRARIES})
	include_directories(${GTK2_INCLUDE_DIRS})
ENDIF()

add_library(${TargetName} SHARED ${SRCS} ${HDRS} ${RCS} ${DEF})
target_link_libraries(${TargetName} ${LIBS})
set_target_properties(${TargetName} PROPERTIES DEBUG_POSTFIX _d)
set_target_properties(${TargetName} PROPERTIES VERSION ${PLUGIN_VERSION} SOVERSION ${PLUGIN_VERSION_MAJOR})

# 64 bits specific configuration
if(CMAKE_SIZEOF_VOID_P MATCHES "8")
    # Do not search library in /usr/lib64
    SET_PROPERTY(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS OFF)
    # Probably useless but it will not harm
    SET_PROPERTY(GLOBAL PROPERTY COMPILE_DEFINITIONS "-m32")
    #add_definitions(-m32)
    set_target_properties(${TargetName} PROPERTIES COMPILE_FLAGS -m32 LINK_FLAGS -m32 )

    # Force the search on 32-bits path.
    #if(EXISTS "/usr/lib32")
    #    set(CMAKE_LIBRARY_ARCHITECTURE "../lib32")
    #endif()
endif(CMAKE_SIZEOF_VOID_P MATCHES "8")

#set_target_properties(PROPERTIES
#        INSTALL_RPATH ${CMAKE_INSTALL_PREFIX} )
#target_link_libraries(${TargetName} ${LIBS})

# post-build copy for win32
IF(WIN32 AND NOT MINGW)
    add_custom_command( TARGET ${TargetName} PRE_BUILD
        COMMAND if not exist .\\dist mkdir .\\dist )
    add_custom_command( TARGET ${TargetName} POST_BUILD
        COMMAND copy \"$(TargetPath)\" .\\dist )
ENDIF(WIN32 AND NOT MINGW)

# set the executable output path for UNIX and MinGW instead of post-build copy
IF(MINGW OR UNIX)
    set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/dist)
ENDIF(MINGW OR UNIX)

# if win32, add INSTALL project  -DCMAKE_INSTALL_PREFIX=...\pcsx2\plugins
IF(WIN32)
    install(TARGETS ${TargetName}
        RUNTIME DESTINATION .
        CONFIGURATIONS Release RelWithDebInfo MinSizeRel Debug) #All
ENDIF(WIN32)
