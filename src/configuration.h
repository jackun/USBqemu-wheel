#ifndef CONFIGURATION_H
#define CONFIGURATION_H
#include <vector>
#include <string>
#include <map>
#include "platcompat.h"

#define RESULT_CANCELED 0
#define RESULT_OK       1
#define RESULT_FAILED   2

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

extern std::map<std::pair<int, std::string>, std::string> changedAPIs;
std::string GetSelectedAPI(const std::pair<int, std::string>& pair);

enum CONFIG_VARIANT_TYPE
{
	CONFIG_TYPE_EMPTY,
	CONFIG_TYPE_INT,
	CONFIG_TYPE_DOUBLE,
	CONFIG_TYPE_CHAR,
	CONFIG_TYPE_WCHAR,
	CONFIG_TYPE_TCHAR,
	CONFIG_TYPE_BOOL,
	CONFIG_TYPE_PTR
};

struct CONFIGVARIANT
{
	const CONFIG_VARIANT_TYPE	type;
	const TCHAR*	desc;
	const TCHAR*	name;

	union
	{
		bool		boolValue;
		int32_t		intValue;
		double		doubleValue;
		//char*		charValue;
		//wchar_t*	wcharValue;
		void*		ptrValue;
	};

	//TODO Can't define as pointers and who would free them then?
	std::string strValue;
	std::wstring wstrValue;
	TSTDSTRING tstrValue;

	CONFIGVARIANT() : type(CONFIG_TYPE_EMPTY), ptrValue(0), desc(nullptr), name(nullptr) {}
	CONFIGVARIANT(const TCHAR* n, CONFIG_VARIANT_TYPE t) : name(n), type(t), ptrValue(0), desc(nullptr) {}
	CONFIGVARIANT(const TCHAR* d, const TCHAR* n, CONFIG_VARIANT_TYPE t)
		: desc(d), name(n), type(t), ptrValue(0) {}

	CONFIGVARIANT(const TCHAR* n, bool val) : CONFIGVARIANT(n, CONFIG_TYPE_BOOL)
	{ boolValue = val; }
	CONFIGVARIANT(const TCHAR* n, int32_t val) : CONFIGVARIANT(n, CONFIG_TYPE_INT)
	{ intValue = val; }
	CONFIGVARIANT(const TCHAR* n, std::string val) : CONFIGVARIANT(n, CONFIG_TYPE_CHAR)
	{ strValue = val; }
	CONFIGVARIANT(const TCHAR* n, std::wstring val) : CONFIGVARIANT(n, CONFIG_TYPE_WCHAR)
	{ wstrValue = val; }

	CONFIGVARIANT(const TCHAR* n, const char* val) : CONFIGVARIANT(n, CONFIG_TYPE_CHAR)
	{ strValue = val; }
	CONFIGVARIANT(const TCHAR* n, const wchar_t* val) : CONFIGVARIANT(n, CONFIG_TYPE_WCHAR)
	{ wstrValue = val; }
};

bool LoadSetting(int port, const std::string& key, CONFIGVARIANT& var);
bool SaveSetting(int port, const std::string& key, CONFIGVARIANT& var);

#endif
