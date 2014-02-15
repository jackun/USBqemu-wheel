extern std::string szIniDir; // Win32\Config.cpp
extern HINSTANCE hInst;
std::string strMySystemFile;

static void GetIniFile(std::string &iniFile)
{
	if(!szIniDir.length()) {
		char *szTemp = NULL, tmp[MAX_PATH];
		GetModuleFileName(GetModuleHandle((LPCSTR)hInst), tmp, MAX_PATH);
		szTemp = strrchr(tmp, '\\');
		if(!szTemp) return;
		strcpy(szTemp, "\\inis\\USBqemu-DIwheel.ini");
		iniFile.append(tmp);
	} else {
		iniFile.append(szIniDir);
		iniFile.append("USBqemu-DIwheel.ini");
	}
}

static int WriteToFile(char * strFileSection, char * strKey, LPCSTR strValue)
{
	//if ((strlen(strKey) > MAX_PATH) | (strlen(strValue) > MAX_PATH))
	//	return -1;

	return WritePrivateProfileString(strFileSection, strKey, strValue, strMySystemFile.c_str());
}

static char * ReadFromFile(char * strFileSection, char * strKey)
{
	char strValue[255];
	int lngRtn=0;

	lngRtn = GetPrivateProfileString(strFileSection, strKey, NULL, strValue, MAX_PATH, strMySystemFile.c_str());
	if (lngRtn > 0)
	{
		memcpy(strValue,strValue,lngRtn);
		return strValue;
	}
	else
		return 0;
}
