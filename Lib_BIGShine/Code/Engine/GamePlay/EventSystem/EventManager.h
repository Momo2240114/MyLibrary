#pragma once

#include<string>
#include<map>
#include <functional>
class EventManager
{
public:
	static void Subscribe(std::string event, std::function<void()> func)
	{
		m_Events[event].emplace_back(func);
	}

	static void Trigger(std::string event)
	{
		for (std::function<void()>& func : m_Events[event])
		{
			func();
		}
	}

	static void Clear()
	{
		m_Events.clear();
	}
private:

	//複数定義扱いにならないようするためにinlineを設定する
	inline static std::unordered_map<std::string, std::vector<std::function<void()>>> m_Events;
};