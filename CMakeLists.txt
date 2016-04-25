CMAKE_MINIMUM_REQUIRED(VERSION 2.8.5)

MACRO(getenv_path VAR)
	SET(ENV_${VAR} $ENV{${VAR}})
	# replace won't work if var is blank
	IF (ENV_${VAR})
		STRING( REGEX REPLACE "\\\\" "/" ENV_${VAR} ${ENV_${VAR}} )
	ENDIF ()
ENDMACRO(getenv_path)

SET(TargetName "USBqemu-wheel")
PROJECT(${TargetName})

set(CMAKE_MODULE_PATH
  "${${TargetName}_SOURCE_DIR}/cmake"
)

SET (PLUGIN_VERSION_MAJOR "0")
SET (PLUGIN_VERSION_MINOR "5")
SET (PLUGIN_VERSION_PATCH "1")
SET (PLUGIN_VERSION "${PLUGIN_VERSION_MAJOR}.${PLUGIN_VERSION_MINOR}.${PLUGIN_VERSION_PATCH}")

IF(WIN32)
	OPTION (BUILD_RAW "Build with raw input api" TRUE)
	OPTION (BUILD_DX "Build with DX input" TRUE)
	OPTION (BUILD_WITH_DXSDK "Build with DX 2010 SDK" FALSE)
ELSE(WIN32)
	IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
		MESSAGE("debug mode")
	ENDIF()
	IF(_DEBUG)
		ADD_DEFINITIONS(-D_DEBUG=1)
	ELSE(_DEBUG)
		MESSAGE("Define _DEBUG for debug print.")
	ENDIF(_DEBUG)
ENDIF(WIN32)

# 64 bits specific configuration
IF(CMAKE_SIZEOF_VOID_P MATCHES "8")
	MESSAGE("Building as x86 on x86_64.")
	# Do not search library in /usr/lib64
	SET_PROPERTY(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS OFF)
	# Probably useless but it will not harm
	SET_PROPERTY(GLOBAL PROPERTY COMPILE_DEFINITIONS "-m32")
	# Force the search on 32-bits path.
	IF(EXISTS "/usr/lib32")
		SET(CMAKE_LIBRARY_ARCHITECTURE "../lib32")
	ENDIF()
ENDIF(CMAKE_SIZEOF_VOID_P MATCHES "8")

#IF(EXISTS "/usr/lib/i386-linux-gnu")
#	SET(CMAKE_LIBRARY_ARCHITECTURE "i686-linux-gnu")
#ENDIF()

SET(HDRS_SAMPLERATE
	./src/libsamplerate/common.h
	./src/libsamplerate/config.h
	./src/libsamplerate/fastest_coeffs.h
	./src/libsamplerate/float_cast.h
	./src/libsamplerate/high_qual_coeffs.h
	./src/libsamplerate/mid_qual_coeffs.h
	./src/libsamplerate/samplerate.h
)

SET(SRCS_SAMPLERATE
	./src/libsamplerate/samplerate.c
	./src/libsamplerate/src_linear.c
	./src/libsamplerate/src_sinc.c
	./src/libsamplerate/src_zoh.c
)

# Some unneeded headers too
SET(HDRS_MIC
	#./src/usb-mic/usbreg.h
	#./src/usb-mic/usbcore.h
	#./src/usb-mic/usbcfg.h
	#./src/usb-mic/type.h
	#./src/usb-mic/usbuser.h
	#./src/usb-mic/usbdesc.h
	#./src/usb-mic/demo.h
	#./src/usb-mic/usbhw.h
	#./src/usb-mic/adcuser.h
	./src/usb-mic/audio.h
	./src/usb-mic/usb.h
	./src/usb-mic/mic-audiodefs.h
)

SET(HDRS_QEMU
	./src/qemu-usb/vl.h
	./src/qemu-usb/usb.h
)

SET(HDRS_PAD
	./src/usb-pad/usb-pad.h
	./src/usb-pad/config.h
)

SET(HDRS_PLG
	./src/USB.h
	./src/PS2Etypes.h
	./src/PS2Edefs.h
)

SET(SRCS_PLG
	./src/USB.cpp
)

SET(SRCS_QEMU
	./src/qemu-usb/usb-hid.cpp
	#./src/qemu-usb/usb-hub.cpp
	./src/qemu-usb/vl.cpp
	./src/qemu-usb/usb-base.cpp
	./src/qemu-usb/usb-msd.cpp
	./src/qemu-usb/usb-ohci.cpp
)

SET(SRCS_PAD
	./src/usb-pad/bitjuggling.cpp
	./src/usb-pad/usb-pad.cpp
)

SET(SRCS_MIC
	./src/usb-mic/usb-mic-singstar.cpp
	#./src/usb-mic/usb-mic-dummy.cpp
	#./src/usb-mic/usb-mic.cpp
	#./src/usb-eyetoy/usb-eyetoy.cpp
)

ADD_DEFINITIONS(-DLIBSRC_ONLY_FAST)

IF(WIN32)
	ADD_DEFINITIONS(-DUNICODE)
	ADD_DEFINITIONS(-D_UNICODE)
	LIST(APPEND HDRS_WIN32
		./src/Win32/resource.h
		./src/Win32/Config.h
	)
	
	LIST(APPEND SRCS_WIN32
		./src/Win32/Config.cpp
		./src/Win32/Win32.cpp
	)
	
	LIST(APPEND SRCS_MIC
		./src/usb-mic/audiosrc-win32.cpp
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
	ELSE(NOT BUILD_DX)
		ADD_DEFINITIONS(-DBUILD_DX=1)
		
		IF(BUILD_WITH_DXSDK)
			getenv_path(DXSDK_DIR)
			SET (BUILD_DX_INCLUDE "${ENV_DXSDK_DIR}/Include" CACHE PATH "DirectX include dir")
			SET (BUILD_DX_LIB "${ENV_DXSDK_DIR}/Lib/x86" CACHE PATH "DirectX library dir")
		ELSE(BUILD_WITH_DXSDK)
			include(FindWindowsSDK)
			get_windowssdk_include_dirs(${WINDOWSSDK_PREFERRED_DIR} BUILD_DX_INCLUDE)
			get_windowssdk_library_dirs(${WINDOWSSDK_PREFERRED_DIR} BUILD_DX_LIB)
		ENDIF(BUILD_WITH_DXSDK)
		
		INCLUDE_DIRECTORIES(${BUILD_DX_INCLUDE})
		LINK_DIRECTORIES(${BUILD_DX_LIB})
		LIST(APPEND HDRS_DX
			./src/usb-pad/dx/di.h
			./src/usb-pad/dx/dialog.h
			./src/usb-pad/dx/global.h
			./src/usb-pad/dx/ini.h
			./src/usb-pad/dx/versionproxy.h
		)
		LIST(APPEND SRCS_DX
			./src/usb-pad/dx/usb-pad-dx.cpp
		)
		SOURCE_GROUP("Header Files\\Dx" FILES ${HDRS_DX})
		SOURCE_GROUP("Source Files\\Dx" FILES ${SRCS_DX})
		LIST(APPEND HDRS ${HDRS_DX})
		LIST(APPEND SRCS ${SRCS_DX})
		LIST(APPEND LIBS dxguid dinput8 Comctl32)
		FILE(GLOB DX_RCS ./src/usb-pad/dx/*.rc)
	ENDIF(NOT BUILD_DX)

	IF(BUILD_RAW)
		ADD_DEFINITIONS(-DBUILD_RAW=1)
		LIST(APPEND HDRS_RAW
			./src/usb-pad/raw/hid.h
			./src/usb-pad/raw/raw-config.h
			./src/usb-pad/raw/raw-config-res.h
		)
		LIST(APPEND SRCS_RAW
			./src/usb-pad/raw/usb-pad-raw.cpp
			./src/usb-pad/raw/raw-config.cpp
			./src/usb-pad/raw/hid.cpp
		)
		SOURCE_GROUP("Header Files\\Raw" FILES ${HDRS_RAW})
		SOURCE_GROUP("Source Files\\Raw" FILES ${SRCS_RAW})
		
		LIST(APPEND HDRS ${HDRS_RAW})
		LIST(APPEND SRCS ${SRCS_RAW})
		LIST(APPEND LIBS setupapi)
		FILE(GLOB RAW_RCS ./src/usb-pad/raw/*.rc)
	ENDIF(BUILD_RAW)
	
	FILE(GLOB RCS ./src/Win32/*.rc)
	FILE(GLOB DEF ./src/Win32/*.def)
ENDIF(WIN32)

IF(UNIX)
	LIST(APPEND HDRS 
		./src/linux/ini.h
		./src/linux/actualfile.h
	)
	LIST(APPEND SRCS 
		./src/usb-pad/usb-pad-linux.cpp
		./src/usb-mic/audio-linux.cpp
		./src/linux/config.cpp
		./src/linux/ini.c
		./src/linux/actualfile.c
	)
	
	ADD_DEFINITIONS(-D_GNU_SOURCE -D_USE_LARGEFILE64 -D_FILE_OFFSET_BITS=64)
	FIND_PACKAGE(GTK2 REQUIRED)
	LIST(APPEND LIBS ${GTK2_LIBRARIES})
	INCLUDE_DIRECTORIES(${GTK2_INCLUDE_DIRS})

	#INCLUDE(FindPkgConfig)
	#PKG_CHECK_MODULES(GTK3 gtk+-3.0)
	#LIST(APPEND LIBS ${GTK3_LIBRARIES})
	#INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS})
ENDIF()

LIST(APPEND HDRS ${HDRS_PLG} ${HDRS_QEMU} ${HDRS_MIC} ${HDRS_PAD} ${HDRS_SAMPLERATE})
LIST(APPEND SRCS ${SRCS_PLG} ${SRCS_QEMU} ${SRCS_MIC} ${SRCS_PAD} ${SRCS_SAMPLERATE})

ADD_LIBRARY(${TargetName} SHARED ${SRCS} ${HDRS} ${RAW_RCS} ${DX_RCS} ${RCS} ${DEF})
TARGET_LINK_LIBRARIES(${TargetName} ${LIBS})
SET_TARGET_PROPERTIES(${TargetName} PROPERTIES DEBUG_POSTFIX _d)
SET_TARGET_PROPERTIES(${TargetName} PROPERTIES VERSION ${PLUGIN_VERSION} SOVERSION ${PLUGIN_VERSION_MAJOR})

# 64 bits specific configuration
IF(CMAKE_SIZEOF_VOID_P MATCHES "8")
	#ADD_DEFINITIONS(-m32)
	SET_TARGET_PROPERTIES(${TargetName} PROPERTIES COMPILE_FLAGS -m32 LINK_FLAGS -m32 )
ENDIF(CMAKE_SIZEOF_VOID_P MATCHES "8")

# post-build copy for win32
IF(WIN32 AND NOT MINGW)
	ADD_CUSTOM_COMMAND( TARGET ${TargetName} PRE_BUILD
		COMMAND if not exist \"${PROJECT_SOURCE_DIR}\"\\Release mkdir \"${PROJECT_SOURCE_DIR}\"\\Release )
	ADD_CUSTOM_COMMAND( TARGET ${TargetName} POST_BUILD
		COMMAND copy \"$(TargetPath)\" \"${PROJECT_SOURCE_DIR}\"\\Release )
ENDIF(WIN32 AND NOT MINGW)

# set the executable output path for UNIX and MinGW instead of post-build copy
IF(MINGW OR UNIX)
	SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/dist)
ENDIF(MINGW OR UNIX)

# if win32, add INSTALL project  -DCMAKE_INSTALL_PREFIX=...\pcsx2\plugins
IF(WIN32)
	INSTALL(TARGETS ${TargetName}
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
