#include "configuration.h"
std::map<std::pair<int, std::string>, std::string> changedAPIs;
std::string GetSelectedAPI(const std::pair<int, std::string>& pair)
{
	auto it = changedAPIs.find(pair);
	if (it != changedAPIs.end())
		return it->second;
	return std::string();
}