#ifndef AUDIOSOURCEPROXY_H
#define AUDIOSOURCEPROXY_H
#include "audiosrc.h"
#include <string>
#include <map>
#include <list>
#include <algorithm>

class AudioSourceProxyBase
{
	public:
	AudioSourceProxyBase(std::string name);
	virtual AudioSource* CreateObject() const = 0; //Can be generalized? Probably not
	virtual const wchar_t* GetName() const = 0;
};

template <class T>
class AudioSourceProxy : public AudioSourceProxyBase
{
	public:
	AudioSourceProxy(std::string name): AudioSourceProxyBase(name) {} //Why can't it automagically, ugh
	AudioSource* CreateObject() const { return new T; }
	virtual const wchar_t* GetName() const
	{
		return T::GetName();
	}
};

struct SelectKey {
	template <typename F, typename S>
	F operator()(const std::pair<const F, S> &x) const { return x.first; }
};

class RegisterAudioSource
{
	typedef std::map<std::string, AudioSourceProxyBase* > RegisterAudioSourceMap;

	public:
	static RegisterAudioSource& instance() {
		static RegisterAudioSource registerAudioSource;
		return registerAudioSource;
	}

	void Register(const std::string name, AudioSourceProxyBase* creator)
	{
		registerAudioSourceMap[name] = creator;
	}

	AudioSourceProxyBase* GetAudioSource(std::string name)
	{
		return registerAudioSourceMap[name];
	}
	
	std::list<std::string> names() const
	{
		std::list<std::string> nameList;  
		std::transform(
			registerAudioSourceMap.begin(), registerAudioSourceMap.end(), 
			back_inserter(nameList),
			SelectKey());
		return nameList;
	}
	
private:
	RegisterAudioSourceMap registerAudioSourceMap;
};

#define REGISTER_AUDIOSRC(name,cls) AudioSourceProxy<cls> g##cls##Proxy(#name)
#endif
