#include "PrecompiledHeader.h"

#include "GameModeObjectToggleComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/Physics/ColliderComponent.h"

#include <imgui.h>

namespace
{
    constexpr const char* ACTION_SHOW = "GameModeObject_Show";
    constexpr const char* ACTION_HIDE = "GameModeObject_Hide";
}

void GameModeObjectToggleComponent::Start()
{
    CaptureEnabledStates();
    RegisterActions();
    SetObjectEnabled(startEnabled);
}

void GameModeObjectToggleComponent::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

    owner_->RegisterAction("GameModeObject", ACTION_SHOW, [this]()
    {
        SetObjectEnabled(true);
    });
    owner_->RegisterAction("GameModeObject", ACTION_HIDE, [this]()
    {
        SetObjectEnabled(false);
    });

    if (registeredGlobalActions)
    {
        owner_->RegisterGlobalAction("GameModeObject", ACTION_SHOW, [this]()
        {
            SetObjectEnabled(true);
        });
        owner_->RegisterGlobalAction("GameModeObject", ACTION_HIDE, [this]()
        {
            SetObjectEnabled(false);
        });
    }
}

void GameModeObjectToggleComponent::SetObjectEnabled(bool enabled_)
{
    if (!capturedStates)
    {
        CaptureEnabledStates();
    }

    ApplyEnabledStates(enabled_);
}

void GameModeObjectToggleComponent::CaptureEnabledStates()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

    normalComponentEnabledStates.clear();
    colliderComponentEnabledStates.clear();

    for (const std::shared_ptr<Component>& component_ : owner_->components)
    {
        normalComponentEnabledStates.push_back(component_ ? component_->IsConfiguredEnabled() : false);
    }
    for (const std::shared_ptr<Component>& component_ : owner_->colliderComponents)
    {
        colliderComponentEnabledStates.push_back(component_ ? component_->IsConfiguredEnabled() : false);
    }

    capturedStates = true;
}

void GameModeObjectToggleComponent::ApplyEnabledStates(bool enabled_)
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

    if (includeNormalComponents)
    {
        for (size_t index_ = 0; index_ < owner_->components.size(); ++index_)
        {
            const std::shared_ptr<Component>& component_ = owner_->components[index_];
            if (!component_ || component_.get() == this)
            {
                continue;
            }

            const bool restoreEnabled_ =
                index_ < normalComponentEnabledStates.size()
                ? normalComponentEnabledStates[index_]
                : true;
            if (enabled_)
            {
                component_->SetEnabled(restoreEnabled_);
            }
            component_->SetRuntimeSuppressed(!enabled_);
        }
    }

    if (includeColliderComponents)
    {
        for (size_t index_ = 0; index_ < owner_->colliderComponents.size(); ++index_)
        {
            const std::shared_ptr<Component>& component_ = owner_->colliderComponents[index_];
            if (!component_)
            {
                continue;
            }

            const bool restoreEnabled_ =
                index_ < colliderComponentEnabledStates.size()
                ? colliderComponentEnabledStates[index_]
                : true;
            if (enabled_)
            {
                component_->SetEnabled(restoreEnabled_);
            }
            // 初期無効のHitBoxもShow時は一時停止を解除し、後続のActivateを受け付ける。
            component_->SetRuntimeSuppressed(!enabled_);
            // 非表示ObjectのColliderがPhysX内へ残り、見えない壁になるのを防ぐ。
            if (!enabled_)
            {
                if (const auto collider_ = std::dynamic_pointer_cast<ColliderComponent>(component_))
                {
                    collider_->currentTriggers.clear();
                    collider_->m_CollisionMainFolds.clear();
                    collider_->ReleasePhysXActor();
                }
            }
        }
    }
}

void GameModeObjectToggleComponent::Gui()
{
    if (ImGui::Checkbox("Start Enabled", &startEnabled))
    {
        SetObjectEnabled(startEnabled);
    }
    ImGui::Checkbox("Normal Components", &includeNormalComponents);
    ImGui::Checkbox("Collider Components", &includeColliderComponents);
    if (ImGui::Checkbox("Register Global Actions", &registeredGlobalActions))
    {
        RegisterActions();
    }
    if (ImGui::Button("Show"))
    {
        SetObjectEnabled(true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Hide"))
    {
        SetObjectEnabled(false);
    }
}

void GameModeObjectToggleComponent::Serialize(nlohmann::json& json_) const
{
    json_["StartEnabled"] = startEnabled;
    json_["IncludeNormalComponents"] = includeNormalComponents;
    json_["IncludeColliderComponents"] = includeColliderComponents;
    json_["RegisteredGlobalActions"] = registeredGlobalActions;
}

void GameModeObjectToggleComponent::Deserialize(const nlohmann::json& json_)
{
    startEnabled = json_.value("StartEnabled", true);
    includeNormalComponents = json_.value("IncludeNormalComponents", true);
    includeColliderComponents = json_.value("IncludeColliderComponents", true);
    registeredGlobalActions = json_.value("RegisteredGlobalActions", true);
}

REGISTER_COMPONENT(GameModeObjectToggleComponent, "GameModeObjectToggleComponent")
