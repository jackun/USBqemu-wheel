extern std::wstring IniDir; // Win32\Config.cpp
extern HINSTANCE hInst;

std::wstring strMySystemFile;

static void GetIniFile(std::wstring &iniFile)
{
	iniFile.clear();
	if(!IniDir.length()) {
		TCHAR tmp[MAX_PATH] = {0};
		GetModuleFileName(GetModuleHandle((LPWSTR)hInst), tmp, MAX_PATH);

		std::wstring path(tmp);
		unsigned last = path.find_last_of(L"\\");
		iniFile = path.substr(0, last);
		iniFile.append(L"\\inis\\USBqemu-DIwheel.ini");
	} else {
		iniFile.append(IniDir);
		iniFile.append(L"USBqemu-DIwheel.ini");
	}
}

static int WriteToFile(TCHAR * strFileSection, TCHAR * strKey, TCHAR* strValue)
{
	//if ((strlen(strKey) > MAX_PATH) | (strlen(strValue) > MAX_PATH))
	//	return -1;

	return WritePrivateProfileString(strFileSection, strKey, strValue, strMySystemFile.c_str());
}

template <size_t _Size>
static int ReadFromFile(const TCHAR * strFileSection, const TCHAR * strKey, TCHAR (&strOut)[_Size])
{
	TCHAR strValue[255];
	int lngRtn=0;
	//memset(strOut, 0, _Size);

	lngRtn = GetPrivateProfileString(strFileSection, strKey, NULL, strValue, MAX_PATH, strMySystemFile.c_str());
	if (lngRtn > 0)
	{
		lngRtn = lngRtn > _Size - 1 ? _Size - 1 : lngRtn;
		memcpy(strOut, strValue, lngRtn * sizeof(*strValue));
		strOut[lngRtn] = 0;
		return lngRtn;
	}
	else
		strOut[0] = 0;
	return 0;
}
