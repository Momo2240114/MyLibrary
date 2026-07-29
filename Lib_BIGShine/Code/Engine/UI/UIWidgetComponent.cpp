#include "PrecompiledHeader.h"

#include "UIWidgetComponent.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/UI/WidgetBoardComponent.h"
#include "imgui.h"

#include <algorithm>
#include <cstring>

namespace
{
    void SerializeFloat2(nlohmann::json& json_, const char* key_, const DirectX::XMFLOAT2& value_)
    {
        json_[key_] = { value_.x, value_.y };
    }

    void DeserializeFloat2(const nlohmann::json& json_, const char* key_, DirectX::XMFLOAT2& value_)
    {
        if (!json_.contains(key_) || !json_[key_].is_array() || json_[key_].size() < 2) return;
        value_.x = json_[key_][0].get<float>();
        value_.y = json_[key_][1].get<float>();
    }
}

void UIWidgetComponent::Start()
{
    BS_PROFILE_SCOPE("UIWidgetComponent::Start");
    if (widgetId.empty())
    {
        if (auto gameObject_ = GetGameObject())
        {
            widgetId = gameObject_->GetName();
        }
    }
    ResolveParentWidget();
    MarkLayoutDirty();
}

void UIWidgetComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("UIWidgetComponent::Update");
    (void)deltaTime_;
    if (parentWidget.expired())
    {
        ResolveParentWidget();
    }
}

void UIWidgetComponent::Gui()
{
    BS_PROFILE_SCOPE("UIWidgetComponent::Gui");

    char widgetIdBuffer_[128]{};
    strncpy_s(widgetIdBuffer_, sizeof(widgetIdBuffer_), widgetId.c_str(), _TRUNCATE);
    if (ImGui::InputText("Widget Id", widgetIdBuffer_, sizeof(widgetIdBuffer_)))
    {
        widgetId = widgetIdBuffer_;
    }

    char parentWidgetIdBuffer_[128]{};
    strncpy_s(parentWidgetIdBuffer_, sizeof(parentWidgetIdBuffer_), parentWidgetId.c_str(), _TRUNCATE);
    if (ImGui::InputText("Parent Widget Id", parentWidgetIdBuffer_, sizeof(parentWidgetIdBuffer_)))
    {
        parentWidgetId = parentWidgetIdBuffer_;
        ResolveParentWidget();
        MarkLayoutDirty();
    }

    if (ImGui::DragFloat2("Anchor Min", &rectTransform.anchorMin.x, 0.01f, 0.0f, 1.0f)) MarkLayoutDirty();
    if (ImGui::DragFloat2("Anchor Max", &rectTransform.anchorMax.x, 0.01f, 0.0f, 1.0f)) MarkLayoutDirty();
    if (ImGui::DragFloat2("Pivot", &rectTransform.pivot.x, 0.01f, 0.0f, 1.0f)) MarkLayoutDirty();
    if (ImGui::DragFloat2("Anchored Position", &rectTransform.anchoredPosition.x, 1.0f)) MarkLayoutDirty();
    if (ImGui::DragFloat2("Size Delta", &rectTransform.sizeDelta.x, 1.0f, 1.0f, 8192.0f)) MarkLayoutDirty();
    if (ImGui::DragFloat2("Local Scale", &rectTransform.localScale.x, 0.01f, 0.01f, 16.0f)) MarkLayoutDirty();
    if (ImGui::DragFloat("Local Rotation", &rectTransform.localRotation, 0.5f)) MarkLayoutDirty();
    if (ImGui::InputInt("Z Order", &zOrder)) MarkLayoutDirty();
    ImGui::Checkbox("Visible", &visible);
    ImGui::Checkbox("Input Enabled", &inputEnabled);
    ImGui::Checkbox("Raycast Target", &raycastTarget);
}

void UIWidgetComponent::Serialize(nlohmann::json& json_) const
{
    BS_PROFILE_SCOPE("UIWidgetComponent::Serialize");

    json_["Version"] = 1;
    json_["WidgetId"] = widgetId;
    json_["ParentWidgetId"] = parentWidgetId;

    nlohmann::json rectJson_;
    SerializeFloat2(rectJson_, "AnchorMin", rectTransform.anchorMin);
    SerializeFloat2(rectJson_, "AnchorMax", rectTransform.anchorMax);
    SerializeFloat2(rectJson_, "Pivot", rectTransform.pivot);
    SerializeFloat2(rectJson_, "AnchoredPosition", rectTransform.anchoredPosition);
    SerializeFloat2(rectJson_, "SizeDelta", rectTransform.sizeDelta);
    SerializeFloat2(rectJson_, "LocalScale", rectTransform.localScale);
    rectJson_["LocalRotation"] = rectTransform.localRotation;
    json_["RectTransform"] = rectJson_;

    json_["ZOrder"] = zOrder;
    json_["Visible"] = visible;
    json_["InputEnabled"] = inputEnabled;
    json_["RaycastTarget"] = raycastTarget;
}

void UIWidgetComponent::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("UIWidgetComponent::Deserialize");

    widgetId = json_.value("WidgetId", widgetId);
    parentWidgetId = json_.value("ParentWidgetId", parentWidgetId);

    if (json_.contains("RectTransform") && json_["RectTransform"].is_object())
    {
        const nlohmann::json& rectJson_ = json_["RectTransform"];
        DeserializeFloat2(rectJson_, "AnchorMin", rectTransform.anchorMin);
        DeserializeFloat2(rectJson_, "AnchorMax", rectTransform.anchorMax);
        DeserializeFloat2(rectJson_, "Pivot", rectTransform.pivot);
        DeserializeFloat2(rectJson_, "AnchoredPosition", rectTransform.anchoredPosition);
        DeserializeFloat2(rectJson_, "SizeDelta", rectTransform.sizeDelta);
        DeserializeFloat2(rectJson_, "LocalScale", rectTransform.localScale);
        rectTransform.localRotation = rectJson_.value("LocalRotation", rectTransform.localRotation);
    }

    zOrder = json_.value("ZOrder", zOrder);
    visible = json_.value("Visible", visible);
    inputEnabled = json_.value("InputEnabled", inputEnabled);
    raycastTarget = json_.value("RaycastTarget", raycastTarget);
    ResolveParentWidget();
    MarkLayoutDirty();
}

void UIWidgetComponent::MarkLayoutDirty()
{
    layoutDirty = true;
}

void UIWidgetComponent::ResolveParentWidget()
{
    BS_PROFILE_SCOPE("UIWidgetComponent::ResolveParentWidget");

    parentWidget.reset();

    auto gameObject_ = GetGameObject();
    if (!gameObject_ || !gameObject_->transform) return;

    auto parentTransform_ = gameObject_->transform->parent.lock();
    while (parentTransform_)
    {
        auto parentObject_ = parentTransform_->gameObject.lock();
        if (parentObject_)
        {
            auto widget_ = parentObject_->GetComponent<UIWidgetComponent>();
            if (widget_)
            {
                if (parentWidgetId.empty() || widget_->GetWidgetId() == parentWidgetId)
                {
                    parentWidget = widget_;
                    if (parentWidgetId.empty())
                    {
                        parentWidgetId = widget_->GetWidgetId();
                    }
                    return;
                }
            }
        }

        parentTransform_ = parentTransform_->parent.lock();
    }
}

void UIWidgetComponent::CalculateLayout(
    WidgetBoardComponent* board_,
    UIWidgetComponent* parentWidget_,
    const DirectX::XMFLOAT2& screenSize_)
{
    BS_PROFILE_SCOPE("UIWidgetComponent::CalculateLayout");

    const DirectX::XMFLOAT2 scale_ = board_
        ? board_->CalculateBoardScale(screenSize_)
        : DirectX::XMFLOAT2{ 1.0f, 1.0f };

    const UIScreenRect parentRect_ = parentWidget_
        ? parentWidget_->GetScreenRect()
        : UIScreenRect{ { 0.0f, 0.0f }, screenSize_ };

    const DirectX::XMFLOAT2 parentSize_ = parentRect_.GetSize();
    const DirectX::XMFLOAT2 anchorMinPoint_ =
    {
        parentRect_.min.x + parentSize_.x * rectTransform.anchorMin.x,
        parentRect_.min.y + parentSize_.y * rectTransform.anchorMin.y
    };
    const DirectX::XMFLOAT2 anchorMaxPoint_ =
    {
        parentRect_.min.x + parentSize_.x * rectTransform.anchorMax.x,
        parentRect_.min.y + parentSize_.y * rectTransform.anchorMax.y
    };
    const DirectX::XMFLOAT2 anchorSize_ =
    {
        anchorMaxPoint_.x - anchorMinPoint_.x,
        anchorMaxPoint_.y - anchorMinPoint_.y
    };

    DirectX::XMFLOAT2 size_ =
    {
        (anchorSize_.x + rectTransform.sizeDelta.x * scale_.x) * rectTransform.localScale.x,
        (anchorSize_.y + rectTransform.sizeDelta.y * scale_.y) * rectTransform.localScale.y
    };
    size_.x = (std::max)(1.0f, size_.x);
    size_.y = (std::max)(1.0f, size_.y);

    const DirectX::XMFLOAT2 anchorReference_ =
    {
        anchorMinPoint_.x + anchorSize_.x * rectTransform.pivot.x,
        anchorMinPoint_.y + anchorSize_.y * rectTransform.pivot.y
    };
    const DirectX::XMFLOAT2 pivotPosition_ =
    {
        anchorReference_.x + rectTransform.anchoredPosition.x * scale_.x,
        anchorReference_.y + rectTransform.anchoredPosition.y * scale_.y
    };

    screenRect.min =
    {
        pivotPosition_.x - size_.x * rectTransform.pivot.x,
        pivotPosition_.y - size_.y * rectTransform.pivot.y
    };
    screenRect.max =
    {
        screenRect.min.x + size_.x,
        screenRect.min.y + size_.y
    };

    layoutDirty = false;
}
