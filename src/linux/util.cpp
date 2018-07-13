#include "util.h"
#include <dirent.h>
#include <stdio.h>

bool file_exists(std::string path)
{
	FILE *i = fopen(path.c_str(), "r");

	if (i == NULL)
		return false;

	fclose(i);
	return true;
}

bool dir_exists(std::string path)
{
	DIR *i = opendir(path.c_str());

	if (i == NULL)
		return false;

	closedir(i);
	return true;
}
