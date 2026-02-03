

#include"ItemManagerComponent.h"
#include"GraphicsDeviceManager.h"

#include"GameObjectManager.h"
#include"BoxCollider.h"
#include"GraphicsDeviceManager.h"

//#include"GlTFModelComponent.h"
#include"ItemComponent.h"
#include"RandomHelper.h"

//#define ITEMCOUNT 10

void ItemManagerComponent::Gui()
{
    ImGui::DragInt("Max Spawn Count", &m_itemSpawnCount, 1, 0, 100);
    ImGui::Text("Actual Spawned: %d", m_actualSpawnedCount);

    if (ImGui::Button("AddItem"))
    {
        m_ItemInfos.emplace_back();
    }
    ImGui::SameLine(); // ボタンを横に並べる
    if (ImGui::Button("Delete Last"))
    {
        if (!m_ItemInfos.empty()) m_ItemInfos.pop_back();
    }

    ImGui::Separator(); // 区切り線

    int itemSize = m_ItemInfos.size();
    ImGui::InputInt("Size", &itemSize);

    for (int i = 0; i < (int)m_ItemInfos.size(); i++)
    {
        // 1. ループのインデックス（i）をIDとしてスタックに積む
        // これにより、この中の "position" などのラベルがすべてユニーク（固有）になります
        ImGui::PushID(i);

        // 2. 見出しを表示（どのアイテムをいじっているか分かりやすくするため）
        if (ImGui::TreeNode("Item", "Item [%d]", i))
        {
            ImGui::DragFloat3("position", &m_ItemInfos[i].position.x, 0.1f, -50.0f, 50.0f);
            ImGui::DragFloat3("angle", &m_ItemInfos[i].m_Angle.x, 0.1f, 0.0f, 360.0f);
            ImGui::DragFloat3("scale", &m_ItemInfos[i].m_Scale.x, 0.1f, 0.01f, 10.0f);

            // 3. 個別の削除ボタン（特定の番号を消せるようにする）
            if (ImGui::Button("Delete This"))
            {
                m_ItemInfos.erase(m_ItemInfos.begin() + i);
                // 要素を消した時は、このフレームの処理を中断して抜けるのが安全
                ImGui::TreePop();
                ImGui::PopID();
                break;
            }

            ImGui::TreePop();
        }

        // 4. 積んだIDを必ず降ろす
        ImGui::PopID();
    }
}

void ItemManagerComponent::Start()
{

     //  元のデータをローカル変数にコピーする
    std::vector<ItemInfo> tempItemInfos = m_ItemInfos;

    // 変数化した m_itemSpawnCount を使用
    int loopCount = (std::min)(m_itemSpawnCount, (int)tempItemInfos.size());

    // 実際に生成した数を保存
    m_actualSpawnedCount = loopCount;

    for (int i = 0; i < loopCount; ++i)
    {
        // 2. コピーしたリスト（tempItemInfos）のサイズからランダムに選ぶ
        int randomIndex = RandomRangeCreat(0, (int)tempItemInfos.size() - 1);

        // アイテムを生成
        std::shared_ptr<GameObject> itemPtr = GameObjectManager::Instance().Create("Item");
        if (!itemPtr) continue;

        // 各種設定
        itemPtr->SetNoSaveObject();
        itemPtr->SetLayer(GameObject::LayerType::Item);

        // 管理用リストに追加
        m_pItemObjects.emplace_back(itemPtr);

        // 3. コピーしたリストから座標を取得
        itemPtr->transform->SetLocalPosition(tempItemInfos[randomIndex].position);

        // コンポーネント追加
        auto collider = itemPtr->AddComponent<BoxCollider>();
        if (collider) {
            collider->isTrigger = true;
            collider->halfSize = { 0.1f, 0.1f, 0.1f };
        }

        auto model = itemPtr->AddComponent<GltfModelCommponennt>();
        model->LoadModel(GraphicsDeviceManager::Instance().GetDevice(), ".\\Data\\Model\\Gold\\scene.gltf");
        model->SetLocalScale(0.1f);

        auto itemComp = itemPtr->AddComponent<ItemComponent>();
        itemComp->SetItem(ItemType::Gold);

        // 4. コピーしたリストからのみデータを削除する（メンバのm_ItemInfosは無傷）
        tempItemInfos.erase(tempItemInfos.begin() + randomIndex);
    }
}

void ItemManagerComponent::Update(float et)
{
	Vector4 color = { 0,1,1,1 };
	for (int i = 0;i < m_ItemInfos.size();i++)
	{
		GraphicsDeviceManager::Instance().GetShapeRendererSystem().DrawBox(m_ItemInfos[i].position, m_ItemInfos[i].m_Angle, m_ItemInfos[i].m_Scale, color);
	}
}
