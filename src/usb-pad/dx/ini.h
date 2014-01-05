
char strMySystemFile[255];

int WriteToFile(char * strFileSection, char * strKey, LPCSTR strValue)
{
	//if ((strlen(strKey) > MAX_PATH) | (strlen(strValue) > MAX_PATH))
	//	return -1;

	return WritePrivateProfileString(strFileSection, strKey, strValue, strMySystemFile);
}

char * ReadFromFile(char * strFileSection, char * strKey)
{
	char strValue[255];
	int lngRtn=0;

	lngRtn = GetPrivateProfileString(strFileSection, strKey, NULL, strValue, MAX_PATH, strMySystemFile);
	if (lngRtn > 0)
	{
		memcpy(strValue,strValue,lngRtn);
		return strValue;
	}
	else
		return 0;
}
