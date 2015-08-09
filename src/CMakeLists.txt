cmake_minimum_required(VERSION 2.8.5)
macro(getenv_path VAR)
   set(ENV_${VAR} $ENV{${VAR}})
   # replace won't work if var is blank
   if (ENV_${VAR})
     string( REGEX REPLACE "\\\\" "/" ENV_${VAR} ${ENV_${VAR}} )
   endif ()
endmacro(getenv_path)

SET(TargetName "USBqemu-wheel")
project(${TargetName})

set (PLUGIN_VERSION_MAJOR "0")
set (PLUGIN_VERSION_MINOR "5")
set (PLUGIN_VERSION_PATCH "1")
set (PLUGIN_VERSION "${PLUGIN_VERSION_MAJOR}.${PLUGIN_VERSION_MINOR}.${PLUGIN_VERSION_PATCH}")
IF(WIN32)
OPTION (BUILD_RAW "Build with raw input api" TRUE)
OPTION (BUILD_DX "Build with DX input" TRUE)
ELSE(WIN32)
IF(_DEBUG)
add_definitions(-D_DEBUG=1)
ELSE(_DEBUG)
MESSAGE("Define _DEBUG for debug print.")
ENDIF(_DEBUG)

ENDIF(WIN32)

#if(EXISTS "/usr/lib/i386-linux-gnu")
#    set(CMAKE_LIBRARY_ARCHITECTURE "i686-linux-gnu")
#endif()

set(HDRS_SAMPLERATE
	./libsamplerate/common.h
	./libsamplerate/config.h
	./libsamplerate/fastest_coeffs.h
	./libsamplerate/float_cast.h
	./libsamplerate/high_qual_coeffs.h
	./libsamplerate/mid_qual_coeffs.h
	./libsamplerate/samplerate.h
)

set(SRCS_SAMPLERATE
	./libsamplerate/samplerate.c
	./libsamplerate/src_linear.c
	./libsamplerate/src_sinc.c
	./libsamplerate/src_zoh.c
)

# Some unneeded headers too
set(HDRS_MIC
	#./usb-mic/usbreg.h
	#./usb-mic/usbcore.h
	#./usb-mic/usbcfg.h
	#./usb-mic/type.h
	#./usb-mic/usbuser.h
	#./usb-mic/usbdesc.h
	#./usb-mic/demo.h
	#./usb-mic/usbhw.h
	#./usb-mic/adcuser.h
	./usb-mic/audio.h
	./usb-mic/usb.h
	./usb-mic/mic-audiodefs.h
)

set(HDRS_QEMU
	./qemu-usb/vl.h
	./qemu-usb/usb.h
)

set(HDRS_PAD
	./usb-pad/usb-pad.h
	./usb-pad/config.h
)

set(HDRS_PLG
	./USB.h
	./PS2Etypes.h
	./PS2Edefs.h
)

set(SRCS_PLG
	./USB.cpp
)

set(SRCS_QEMU
	./qemu-usb/usb-hid.cpp
	./qemu-usb/usb-hub.cpp
	./qemu-usb/vl.cpp
	./qemu-usb/usb-base.cpp
	./qemu-usb/usb-msd.cpp
	./qemu-usb/usb-ohci.cpp
)

set(SRCS_PAD
	./usb-pad/bitjuggling.cpp
	./usb-pad/usb-pad.cpp
)

set(SRCS_MIC
	./usb-mic/usb-mic-singstar.cpp
	#./usb-mic/usb-mic-dummy.cpp
	#./usb-mic/usb-mic.cpp
	#./usb-eyetoy/usb-eyetoy.cpp
)

add_definitions(-DLIBSRC_ONLY_FAST)

IF(WIN32)
	add_definitions(-DUNICODE)
	LIST(APPEND HDRS_WIN32
		./Win32/resource.h
		./Win32/Config.h
	)
	
	LIST(APPEND SRCS_WIN32
		./Win32/Config.cpp
		./Win32/Win32.cpp
	)
	
	LIST(APPEND SRCS_MIC
		./usb-mic/audiosrc-win32.cpp
	)
	
	# These two eventually contain only code that manages raw/dx configurations
	LIST(APPEND HDRS ${HDRS_WIN32})
	LIST(APPEND SRCS ${SRCS_WIN32})
	
	SOURCE_GROUP("Header Files\\Win32" FILES ${HDRS_WIN32})
	SOURCE_GROUP("Header Files" FILES ${HDRS_PLG})
	SOURCE_GROUP("Header Files\\qemu-usb" FILES ${HDRS_QEMU})
	SOURCE_GROUP("Header Files\\usb-mic" FILES ${HDRS_MIC})
	SOURCE_GROUP("Header Files\\usb-pad" FILES ${HDRS_PAD})
	SOURCE_GROUP("Header Files\\libsamplerate" FILES ${HDRS_SAMPLERATE})

	SOURCE_GROUP("Source Files" FILES ${SRCS_PLG})
	SOURCE_GROUP("Source Files\\Win32" FILES ${SRCS_WIN32})
	SOURCE_GROUP("Source Files\\qemu-usb" FILES ${SRCS_QEMU})
	SOURCE_GROUP("Source Files\\usb-mic" FILES ${SRCS_MIC})
	SOURCE_GROUP("Source Files\\usb-pad" FILES ${SRCS_PAD})
	SOURCE_GROUP("Source Files\\libsamplerate" FILES ${SRCS_SAMPLERATE})
	
	IF(NOT BUILD_DX)
		SET(BUILD_RAW TRUE)
	ELSE()
		add_definitions(-DBUILD_DX=1)
		getenv_path(DXSDK_DIR)
		SET (BUILD_DX_INCLUDE "${ENV_DXSDK_DIR}/Include" CACHE PATH "DirectX include dir")
		SET (BUILD_DX_LIB "${ENV_DXSDK_DIR}/Lib/x86" CACHE PATH "DirectX library dir")
		include_directories(${BUILD_DX_INCLUDE})
		link_directories(${BUILD_DX_LIB})
		LIST(APPEND HDRS_DX
			./usb-pad/dx/di.h
			./usb-pad/dx/dialog.h
			./usb-pad/dx/global.h
			./usb-pad/dx/ini.h
			./usb-pad/dx/versionproxy.h
		)
		LIST(APPEND SRCS_DX
			./usb-pad/dx/usb-pad-dx.cpp
		)
		SOURCE_GROUP("Header Files\\Dx" FILES ${HDRS_DX})
		SOURCE_GROUP("Source Files\\Dx" FILES ${SRCS_DX})
		LIST(APPEND HDRS ${HDRS_DX})
		LIST(APPEND SRCS ${SRCS_DX})
		LIST(APPEND LIBS d3dx9 d3d9 dxguid dinput8 Comctl32)
		file(GLOB DX_RCS ./usb-pad/dx/*.rc)
	ENDIF()

	IF(BUILD_RAW)
		add_definitions(-DBUILD_RAW=1)
		LIST(APPEND HDRS_RAW
			./usb-pad/raw/hid.h
			./usb-pad/raw/raw-config.h
			./usb-pad/raw/raw-config-res.h
		)
		LIST(APPEND SRCS_RAW
			./usb-pad/raw/usb-pad-raw.cpp
			./usb-pad/raw/raw-config.cpp
			./usb-pad/raw/hid.cpp
		)
		SOURCE_GROUP("Header Files\\Raw" FILES ${HDRS_RAW})
		SOURCE_GROUP("Source Files\\Raw" FILES ${SRCS_RAW})
		
		LIST(APPEND HDRS ${HDRS_RAW})
		LIST(APPEND SRCS ${SRCS_RAW})
		LIST(APPEND LIBS setupapi)
		file(GLOB RAW_RCS ./usb-pad/raw/*.rc)
	ENDIF(BUILD_RAW)
	
	file(GLOB RCS ./Win32/*.rc)
	file(GLOB DEF ./Win32/*.def)
ENDIF(WIN32)

IF(UNIX)
	LIST(APPEND HDRS 
		./linux/ini.h
		./linux/actualfile.h
	)
	LIST(APPEND SRCS 
		./usb-pad/usb-pad-linux.cpp
		./usb-mic/audio-linux.cpp
		./linux/config.cpp
		./linux/ini.c
		./linux/actualfile.c
	)
	
	add_definitions(-D_GNU_SOURCE -D_USE_LARGEFILE64 -D_FILE_OFFSET_BITS=64)
	find_package(GTK2 REQUIRED)
	LIST(APPEND LIBS ${GTK2_LIBRARIES})
	include_directories(${GTK2_INCLUDE_DIRS})
ENDIF()

LIST(APPEND HDRS ${HDRS_PLG} ${HDRS_QEMU} ${HDRS_MIC} ${HDRS_PAD} ${HDRS_SAMPLERATE})
LIST(APPEND SRCS ${SRCS_PLG} ${SRCS_QEMU} ${SRCS_MIC} ${SRCS_PAD} ${SRCS_SAMPLERATE})

add_library(${TargetName} SHARED ${SRCS} ${HDRS} ${RAW_RCS} ${DX_RCS} ${RCS} ${DEF})
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
        COMMAND if not exist \"${PROJECT_SOURCE_DIR}\"\\..\\Release mkdir \"${PROJECT_SOURCE_DIR}\"\\..\\Release )
    add_custom_command( TARGET ${TargetName} POST_BUILD
        COMMAND copy \"$(TargetPath)\" \"${PROJECT_SOURCE_DIR}\"\\..\\Release )
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
ELSE(WIN32)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    INSTALL(
        TARGETS ${TargetName}
        LIBRARY DESTINATION .
        COMPONENT library
    )
ENDIF(WIN32)
