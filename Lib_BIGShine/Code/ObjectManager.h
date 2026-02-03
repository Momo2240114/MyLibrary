#pragma once
#include <memory>
#include <vector>
#include <algorithm>
#include "GameObject.h"

class ObjectManager
{
private:
    std::vector<std::shared_ptr<GameObject>> objects;

public:
    GameObject* selectedObject = nullptr; // 選択中のオブジェクト

    // --- 作成 ---
    std::shared_ptr<GameObject> Create(const std::string& name_ = "GameObject")
    {
        std::shared_ptr<GameObject> object_ = std::make_shared<GameObject>();
        {
            static int id_ = 0;
            char name_[256];
            ::sprintf_s(name_, sizeof(name_), "GameObject%d", id_++);
            object_->SetName(name_);
        }
       objects.emplace_back(object_);
        return object_;
    }

    // --- 削除 ---
    void Remove(std::shared_ptr<GameObject> target)
    {
        objects.erase(
            std::remove_if(objects.begin(), objects.end(),
                [&](const std::shared_ptr<GameObject>& obj) {
                    return obj == target;
                }),
            objects.end()
        );
    }

    // --- 更新 ---
    void Update(float deltaTime_)
    {
        for (auto& obj : objects)
        {
            obj->Update(deltaTime_);
        }
    }

    // --- 行列更新 ---
    void UpdateTransform()
    {
        for (auto& obj : objects)
        {
            if (obj->transform)
                obj->transform->UpdateWorldMatrix();
        }
    }

    // --- 描画 ---
    void Submit(ID3D11DeviceContext* context)
    {
        for (auto& obj : objects)
        {
            obj->Submit(context);
        }
    }

    // --- ImGui GUI ---
    // --- GUI 更新 --- 
    void Gui()
    {
        // 選択中のオブジェクトだけ GUI を出す
        if (selectedObject)
        {
            selectedObject->GuiWindow();
        }
    }


    // --- すべてのGameObject取得 ---
    const std::vector<std::shared_ptr<GameObject>>& GetObjects() const { return objects; }

    // --- 名前で検索 ---
    std::shared_ptr<GameObject> FindByName(const std::string& name)
    {
        for (auto& obj : objects)
        {
            if (obj->name == name)
                return obj;
        }
        return nullptr;
    }

    // --- 全削除 ---
    void Clear()
    {
        objects.clear();
    }
};



