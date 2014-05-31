extern std::string IniDir; // Win32\Config.cpp
extern HINSTANCE hInst;
std::string strMySystemFile;

static void GetIniFile(std::string &iniFile)
{
	iniFile.clear();
	if(!IniDir.length()) {
		char tmp[MAX_PATH] = {0};
		GetModuleFileName(GetModuleHandle((LPCSTR)hInst), tmp, MAX_PATH);

		std::string path(tmp);
		unsigned last = path.find_last_of("\\");
		iniFile = path.substr(0, last);
		iniFile.append("\\inis\\USBqemu-DIwheel.ini");
	} else {
		iniFile.append(IniDir);
		iniFile.append("USBqemu-DIwheel.ini");
	}
}

static int WriteToFile(char * strFileSection, char * strKey, LPCSTR strValue)
{
	//if ((strlen(strKey) > MAX_PATH) | (strlen(strValue) > MAX_PATH))
	//	return -1;

	return WritePrivateProfileString(strFileSection, strKey, strValue, strMySystemFile.c_str());
}

template <size_t _Size>
static int ReadFromFile(const char * strFileSection, const char * strKey, char (&strOut)[_Size])
{
	char strValue[255];
	int lngRtn=0;
	//memset(strOut, 0, _Size);

	lngRtn = GetPrivateProfileString(strFileSection, strKey, NULL, strValue, MAX_PATH, strMySystemFile.c_str());
	if (lngRtn > 0)
	{
		lngRtn = lngRtn > _Size - 1 ? _Size - 1 : lngRtn;
		memcpy(strOut, strValue, lngRtn);
		strOut[lngRtn] = 0;
		return lngRtn;
	}
	else
		strOut[0] = 0;
	return 0;
}
