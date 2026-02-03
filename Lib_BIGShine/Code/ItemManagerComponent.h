#pragma once

#include"ComponentBase.h"

//アイテムのデータ
struct ItemInfo
{
	Vector3 position = { 0,0,0 };
	Vector3 m_Angle = { 0,0,0 };
	Vector3 m_Scale = {0.1f,0.1f,0.1f};
};

class ItemManagerComponent :public Component
{
public:
	const std::string& GetName() const override
	{
		static std::string name = "ItemManagerComponent";
		return name;
	}

	void Start();

	void Update(float et);

	void Gui();

	int GetActualSpawnedCount() {
		return m_actualSpawnedCount;
	};

	//Jsonに保存
	void Serialize(nlohmann::json& j) const override
	{
		j["ItemSpawnCount"] = m_itemSpawnCount;

		// "ItemInfos" というキーで配列を作成
		nlohmann::json itemArray = nlohmann::json::array();

		for (const ItemInfo& item : m_ItemInfos)
		{
			nlohmann::json itemJson;
			// Vector3 を配列として保存 [x, y, z]
			itemJson["Pos"] = { item.position.x,item.position.y,item.position.z };
			itemJson["Angle"] = { item.m_Angle.x,item.m_Angle.y,item.m_Angle.z };
			itemJson["Scale"] = { item.m_Scale.x,item.m_Scale.y,item.m_Scale.z };
			itemArray.push_back(itemJson);
		}
		j["ItemInfos"] = itemArray;
	}

	//Jsonからデータを取得
	void Deserialize(const nlohmann::json& j) override 
	{
		// --- 追加: スポーン設定の復元 ---
		if (j.contains("ItemSpawnCount"))
		{
			m_itemSpawnCount = j["ItemSpawnCount"].get<int>();
		}

		// 既存のデータをクリア
		m_ItemInfos.clear();

		if (j.contains("ItemInfos") && j["ItemInfos"].is_array())
		{
			for (const auto& itemJson : j["ItemInfos"])
			{
				ItemInfo info;

				if (itemJson.contains("Pos"))
				{
					const auto& pos = itemJson["Pos"];
					info.position.x = pos[0];
					info.position.y = pos[1];
					info.position.z = pos[2];
				}
				if (itemJson.contains("Angle"))
				{
					const auto& angle = itemJson["Angle"];
					info.m_Angle.x = angle[0];
					info.m_Angle.y = angle[1];
					info.m_Angle.z = angle[2];
				}
				if (itemJson.contains("Scale"))
				{
					const auto& scale = itemJson["Scale"];
					info.m_Scale.x = scale[0];
					info.m_Scale.y = scale[1];
					info.m_Scale.z = scale[2];
				}


				m_ItemInfos.push_back(info);
			}
		}
	}

private:
	int m_itemSpawnCount = 10; // ITEMCOUNTの代わりに変数化
	int m_actualSpawnedCount = 0; // 実際に生成された数（これがゴール条件になる）
private:

	std::vector<ItemInfo> m_ItemInfos;

	std::vector<std::weak_ptr<GameObject>>m_pItemObjects;

	int m_MaxItem = 1;
};