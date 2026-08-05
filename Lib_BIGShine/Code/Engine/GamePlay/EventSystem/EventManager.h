#pragma once

#include<string>
#include<map>
#include <functional>
class EventManager
{
public:
	static void Subscribe(std::string event, std::function<void()> func)
	{
		// Event名ごとにCallbackを積み、同じEventに複数の処理を紐づけられるようにする。
		m_Events[event].emplace_back(func);
	}

	static void Trigger(std::string event)
	{
		// 登録済みCallbackを登録順に実行する。
		// 未登録Eventの場合は空の配列が作られ、何も実行されない。
		for (std::function<void()>& func : m_Events[event])
		{
			func();
		}
	}

	static void Clear()
	{
		// Scene切り替えなどで古いCallbackが残らないよう、Event登録を全て破棄する。
		m_Events.clear();
	}
private:

	// Header内に実体を置いても複数定義扱いにならないよう、inline staticで共有する。
	inline static std::unordered_map<std::string, std::vector<std::function<void()>>> m_Events;
};
