#pragma once

#ifndef CONFIGURATION_H
#define CONFIGURATION_H
#include <vector>
#include <string>
#include <map>
#include "platcompat.h"

#define RESULT_CANCELED 0
#define RESULT_OK       1
#define RESULT_FAILED   2

// freeze modes:
#define FREEZE_LOAD			0
#define FREEZE_SAVE			1
#define FREEZE_SIZE			2

// Device-level config related defines
#define S_DEVICE_API	TEXT("Device API")
#define S_WHEEL_TYPE	TEXT("Wheel type")
#define S_DEVICE_PORT0	TEXT("Port 0")
#define S_DEVICE_PORT1	TEXT("Port 1")

#define N_DEVICE_API	TEXT("device_api")
#define N_DEVICES		TEXT("devices")
#define N_WHEEL_PT		TEXT("wheel_pt")
#define N_DEVICE_PORT0	TEXT("port_0")
#define N_DEVICE_PORT1	TEXT("port_1")
#define N_WHEEL_TYPE0	TEXT("wheel_type_0")
#define N_WHEEL_TYPE1	TEXT("wheel_type_1")

#define PLAYER_TWO_PORT 0
#define PLAYER_ONE_PORT 1
#define USB_PORT PLAYER_ONE_PORT

typedef struct _Config {
  int Log;
  std::string Port[2];
  int DFPPass; //[2]; //TODO per player
  int WheelType[2];

  _Config();
} Config;

extern Config conf;
void SaveConfig();
void LoadConfig();

extern std::map<std::pair<int /*port*/, std::string /*devname*/>, std::string> changedAPIs;
std::string GetSelectedAPI(const std::pair<int /*port*/, std::string /*devname*/>& pair);

#ifdef _WIN32
#include "Win32/Config.h"
#else
#ifdef __POSIX__
#include "linux/config.h"
#else
#error No configuration methods implemented for current platform.
#endif
#endif

#endif
