#include "PrecompiledHeader.h"

// 通常 Inspector GUI / Sequencer GUI

#include "AnimationControllerComponent.h"
#include "AnimSequenceAdapter.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "AnimationDataAsset.h"
#include "PoseLibraryResourceManager.h"
#include "IKMapComponent.h"
#include "Editor/AssetSlotGui.h"
#include "Engine/Platform/WindowsDialogHelper.h"
#include "Engine/Debug/TracyProfiler.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <cctype>
#include <unordered_map>
#include <iterator>

namespace ed = ax::NodeEditor;

namespace
{
    bool ContainsCaseInsensitive(const std::string& text_, const std::string& query_)
    {
        if (query_.empty()) return true;

        return std::search(
            text_.begin(), text_.end(),
            query_.begin(), query_.end(),
            [](unsigned char left_, unsigned char right_)
            {
                return std::tolower(left_) == std::tolower(right_);
            }) != text_.end();
    }
}

void AnimationControllerComponent::MarkSequenceAdapterDirty()
{
    // ステートイベントを編集したあとは、ImSequencer用のフレーム配列を次回表示時に組み直す。
    sequenceAdapterDirty = true;
}

void AnimationControllerComponent::RefreshSequenceAdapter()
{
    BS_PROFILE_SCOPE("RefreshSequenceAdapter");

    // SequencerAdapterは編集対象ステートとfpsを持つ表示用キャッシュなので、必要なときだけ同期する。
    if (sequenceAdapterState != editingState)
    {
        sequenceAdapterState = editingState;
        sequenceAdapterDirty = true;
    }

    sequenceAdapter.targetState = editingState;
    sequenceAdapter.duration = 0.0f;
    sequenceAdapter.fps = 60.0f;

    if (editingState && modelComponent)
    {
        sequenceAdapter.duration = modelComponent->GetAnimationDuration(editingState->animationIndex);
    }

    if (sequenceAdapterDirty)
    {
        sequenceAdapter.SyncFrames();
        sequenceAdapterDirty = false;
    }
}

void AnimationControllerComponent::MarkActionCategoryCacheDirty()
{
    actionCategoryCacheDirty = true;
}

void AnimationControllerComponent::RefreshActionCategoryCache()
{
    BS_PROFILE_SCOPE("RefreshActionCategoryCache");

    // アクション候補はローカル、親、子、グローバルをまとめてポップアップに出すため、階層ごとに分類して保持する。
    std::shared_ptr<GameObject> ownerShared_ = GetGameObject();
    GameObject* owner_ = ownerShared_.get();
    if (cachedActionOwner != owner_)
    {
        cachedActionOwner = owner_;
        actionCategoryCacheDirty = true;
    }
    if (!actionCategoryCacheDirty) return;

    cachedActionCategories.clear();

    auto addRegistryCategories_ = [&](ActionRegistry& registry_, const std::string& sourceLabel_)
    {
        std::vector<std::string> categories_ = registry_.GetCategories();
        std::sort(categories_.begin(), categories_.end());
        for (const std::string& category_ : categories_)
        {
            CachedActionCategory entry_;
            entry_.category = category_;
            entry_.menuLabel = sourceLabel_ + " " + category_;
            entry_.actionNames = registry_.GetActionNames(category_);
            std::sort(entry_.actionNames.begin(), entry_.actionNames.end());
            cachedActionCategories.push_back(std::move(entry_));
        }
    };

    if (ownerShared_)
    {
        addRegistryCategories_(ownerShared_->localRegistry, "[Self]");

        auto parentTransform_ = ownerShared_->transform ? ownerShared_->transform->parent.lock() : nullptr;
        while (parentTransform_)
        {
            auto parentObject_ = parentTransform_->gameObject.lock();
            if (!parentObject_) break;

            addRegistryCategories_(parentObject_->localRegistry, "[Parent:" + parentObject_->GetName() + "]");
            parentTransform_ = parentObject_->transform ? parentObject_->transform->parent.lock() : nullptr;
        }

        auto collectChildActions_ = [&](auto&& self_, const std::shared_ptr<GameObject>& object_) -> void
        {
            if (!object_ || !object_->transform) return;

            for (const auto& childTransform_ : object_->transform->children)
            {
                if (!childTransform_) continue;

                auto childObject_ = childTransform_->gameObject.lock();
                if (!childObject_) continue;

                addRegistryCategories_(childObject_->localRegistry, "[Child:" + childObject_->GetName() + "]");
                self_(self_, childObject_);
            }
        };
        collectChildActions_(collectChildActions_, ownerShared_);
    }

    addRegistryCategories_(ActionRegistry::Instance(), "[Global]");
    actionCategoryCacheDirty = false;
}

void AnimationControllerComponent::MarkStateGuiCacheDirty()
{
    stateGuiCacheDirty = true;
}

void AnimationControllerComponent::RefreshStateGuiCache()
{
    // ステート数が増えてもGUIが重くならないよう、名前フィルタ後のindexだけをキャッシュしてリスト表示に使う。
    if (!stateGuiCacheDirty) return;

    cachedStateGuiEntries.clear();
    cachedStateGuiEntries.reserve(states.size());
    for (auto& [name_, statePtr_] : states)
    {
        cachedStateGuiEntries.push_back({ name_, statePtr_.get() });
    }
    std::sort(
        cachedStateGuiEntries.begin(),
        cachedStateGuiEntries.end(),
        [](const StateGuiEntry& left_, const StateGuiEntry& right_)
        {
            return left_.name < right_.name;
        });
    stateGuiCacheDirty = false;
}

void AnimationControllerComponent::MarkBlackboardGuiCacheDirty()
{
    blackboardGuiCacheDirty = true;
}

void AnimationControllerComponent::RefreshBlackboardGuiCache()
{
    // Blackboardのキー一覧は追加削除時だけ更新し、描画中は安定した順序で参照できるようにする。
    if (!blackboardGuiCacheDirty) return;

    const BlackboardCategory& blackboard_ = GetEditableBlackboard();
    cachedBlackboardKeys.clear();
    cachedBlackboardKeys.reserve(blackboard_.size());
    for (const auto& [key_, value_] : blackboard_)
    {
        cachedBlackboardKeys.push_back(key_);
    }
    std::sort(cachedBlackboardKeys.begin(), cachedBlackboardKeys.end());
    blackboardGuiCacheDirty = false;
}

void AnimationControllerComponent::MarkAnimationMaskCacheDirty()
{
    animationMaskCacheDirty = true;
}

void AnimationControllerComponent::RefreshAnimationMaskCache()
{
    BS_PROFILE_SCOPE("RefreshAnimationMaskCache");

    // ブレンドマスクは階層継承を表示するため、ノード深度と親からの選択状態を先に展開しておく。
    if (animationMaskCacheState != editingState)
    {
        animationMaskCacheState = editingState;
        animationMaskCacheDirty = true;
    }
    if (!animationMaskCacheDirty) return;

    cachedNodeDepths.clear();
    cachedMaskInheritedFlags.clear();
    cachedMaskRootNodeSet.clear();
    if (!editingState || !modelComponent)
    {
        animationMaskCacheDirty = false;
        return;
    }

    const auto& nodes_ = modelComponent->GetNodes();
    cachedNodeDepths.assign(nodes_.size(), 0);
    cachedMaskInheritedFlags.assign(nodes_.size(), false);
    cachedMaskRootNodeSet.insert(
        editingState->blendMaskRootNodes.begin(),
        editingState->blendMaskRootNodes.end());

    for (int nodeIndex_ = 0; nodeIndex_ < static_cast<int>(nodes_.size()); ++nodeIndex_)
    {
        int depth_ = 0;
        int parentIndex_ = nodes_[nodeIndex_].parentIndex;
        bool inherited_ = false;
        while (parentIndex_ >= 0 && parentIndex_ < static_cast<int>(nodes_.size()))
        {
            ++depth_;
            if (cachedMaskRootNodeSet.find(nodes_[parentIndex_].name) != cachedMaskRootNodeSet.end())
            {
                inherited_ = true;
            }
            parentIndex_ = nodes_[parentIndex_].parentIndex;
        }
        cachedNodeDepths[nodeIndex_] = depth_;
        cachedMaskInheritedFlags[nodeIndex_] = inherited_;
    }

    animationMaskCacheDirty = false;
}

void AnimationControllerComponent::MarkEffectAssetCacheDirty()
{
    effectAssetCacheDirty = true;
}

void AnimationControllerComponent::RefreshEffectAssetCache()
{
    BS_PROFILE_SCOPE("RefreshEffectAssetCache");

    // イベントから参照するVFXアセットは管理側のキーをGUI選択用に集め、手入力ミスを減らす。
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (cachedEffectAssetOwner != owner_.get())
    {
        cachedEffectAssetOwner = owner_.get();
        effectAssetCacheDirty = true;
    }
    if (!effectAssetCacheDirty) return;

    cachedParticleAssetKeys.clear();
    cachedMeshEffectAssetKeys.clear();

    if (owner_)
    {
        const auto& effectManager_ = owner_->GetManager().GetEffectManager();
        cachedParticleAssetKeys = effectManager_.GetParticleAssetKeys();
        cachedMeshEffectAssetKeys = effectManager_.GetMeshVFXAssetKeys();

        const std::vector<std::string>& sequenceAssetKeys_ = effectManager_.GetVFXSequenceAssetKeys();
        cachedMeshEffectAssetKeys.insert(
            cachedMeshEffectAssetKeys.end(),
            sequenceAssetKeys_.begin(),
            sequenceAssetKeys_.end());

        std::sort(cachedParticleAssetKeys.begin(), cachedParticleAssetKeys.end());
        std::sort(cachedMeshEffectAssetKeys.begin(), cachedMeshEffectAssetKeys.end());
        cachedMeshEffectAssetKeys.erase(
            std::unique(cachedMeshEffectAssetKeys.begin(), cachedMeshEffectAssetKeys.end()),
            cachedMeshEffectAssetKeys.end());
    }

    effectAssetCacheDirty = false;
}

void AnimationControllerComponent::PreviewStateAtTime(AnimState* state_, float time_)
{
    BS_PROFILE_SCOPE("ScrubAnimationPreview");

    // Sequencer上のフレーム移動は再生状態を変えず、選択ステートの指定時刻だけをモデルへプレビュー反映する。
    if (!state_) return;
    if (animator.GetCurrentState() != state_)
    {
        ChangeState(state_->name, 0.0f);
    }
    SetAnimatorTime(time_);
}

void AnimationControllerComponent::Gui()
{
    BS_PROFILE_SCOPE("AnimationControllerComponent::Gui");

    if (!modelComponent) return;

    {
        BS_PROFILE_SCOPE("Animation GUI::Runtime Playback");

        // 実行中の再生状態を確認しつつ、エディタから任意ステートへ遷移できるデバッグ領域。
        ImGui::Text("Model: %s", modelComponent->GetModelName().c_str());
        ImGui::TextDisabled("Animation editors are opened from the main Window menu.");

        ImGui::SeparatorText("Runtime Playback");
        const AnimState* runtimeState_ = animator.GetCurrentState();
        if (runtimeState_)
        {
            ImGui::Text("State: %s", runtimeState_->name.c_str());

            const float duration_ = (std::max)(animator.GetCurrentSampleDuration(), 0.001f);
            float playbackTime_ = std::clamp(animator.GetCurrentFadeTime(), 0.0f, duration_);
            if (ImGui::SliderFloat(
                "Playback Time",
                &playbackTime_,
                0.0f,
                duration_,
                "%.3f sec"))
            {
                animator.SetCurrentTime(playbackTime_);
            }

            ImGui::Text("%.3f / %.3f sec", animator.GetCurrentFadeTime(), duration_);
        }
        else
        {
            ImGui::TextDisabled("No animation is currently playing.");
        }


        ImGui::Text(
            "RootMotion %.3f %.3f %.3f",
            rootMotionDelta.x,
            rootMotionDelta.y,
            rootMotionDelta.z);
    }

    {
        BS_PROFILE_SCOPE("Animation GUI::State Assets");

        ImGui::SeparatorText("State Assets");
        if (AssetSlotGui::Draw(
            "Import State",
            AssetSlotType::JSON,
            stateAssetImportPath,
            nullptr,
            "Data/Animation/StateData"))
        {
            if (!stateAssetImportPath.empty())
            {
                const bool loaded_ = LoadStateAsset(stateAssetImportPath);
                stateAssetStatus = loaded_
                    ? "Loaded: " + stateAssetImportPath
                    : "Not an AnimationState asset: " + stateAssetImportPath;
            }
        }
        if (!stateAssetStatus.empty())
        {
            ImGui::TextWrapped("%s", stateAssetStatus.c_str());
        }

        ImGui::Checkbox("useTransitionGraph", &useTransitionGraph);
    }

    ImGui::Separator();


    if (ImGui::CollapsingHeader("Add State From Model Clip", ImGuiTreeNodeFlags_None))
    {
        BS_PROFILE_SCOPE("Animation GUI::Add State From Model Clip");

        // モデル内クリップからステートを作る入口。既存名と衝突する場合は連番を付けて登録する。
        static int selectedAnimIndex_ = -1;
        const auto& animations_ = modelComponent->GetAnimations();

        ImGui::BeginChild("ModelAnimationClips", ImVec2(0.0f, 140.0f), true);
        ImGuiListClipper clipper_;
        clipper_.Begin(static_cast<int>(animations_.size()));
        while (clipper_.Step())
        {
            for (int index_ = clipper_.DisplayStart; index_ < clipper_.DisplayEnd; ++index_)
            {
                if (ImGui::Selectable(
                    animations_[index_].name.c_str(),
                    selectedAnimIndex_ == index_))
                {
                    selectedAnimIndex_ = index_;
                }
            }
        }
        ImGui::EndChild();

        if (selectedAnimIndex_ >= 0 &&
            selectedAnimIndex_ < static_cast<int>(animations_.size()) &&
            ImGui::Button("Create State From Selected Clip", ImVec2(-1.0f, 0.0f)))
        {
            std::string stateName_ = animations_[selectedAnimIndex_].name;
            const std::string baseName_ = stateName_;
            int suffix_ = 2;
            while (states.find(stateName_) != states.end())
            {
                stateName_ = baseName_ + "_" + std::to_string(suffix_++);
            }
            AddState(stateName_, selectedAnimIndex_);
            editingState = states[stateName_].get();
        }
    }

    ImGui::Separator();


    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Registered Animation States");

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));

    if (ImGui::BeginChild("StatesRegion", ImVec2(0, 200), true))
    {
        BS_PROFILE_SCOPE("Animation GUI::State List");

        // ステート一覧は検索とListClipperで軽くし、選択時はSequencerとマスクキャッシュも編集対象へ合わせる。
        std::string targetToDelete = "";

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##StateSearch", "Search states...", &stateSearchText);
        ImGui::Separator();

        RefreshStateGuiCache();
        auto DrawStateRow_ = [&](const StateGuiEntry& entry_)
        {
            const std::string& name_ = entry_.name;
            AnimState* state_ = entry_.state;
            if (!state_) return;

            ImGui::PushID(name_.c_str());

            bool isCurrent_ = (animator.GetCurrentState() == state_);
            bool isEditing_ = (editingState == state_);


            if (isCurrent_) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.4f, 0.2f, 1.0f));
            }



            if (ImGui::Selectable(name_.c_str(), isEditing_, ImGuiSelectableFlags_AllowOverlap)) {
                editingState = state_;
                MarkSequenceAdapterDirty();
                MarkAnimationMaskCacheDirty();
            }

            if (isCurrent_) ImGui::PopStyleColor();


            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 150.0f);
            if (ImGui::SmallButton("Save"))
            {
                const auto pathIt_ = stateAssetPaths.find(name_);
                const std::string path_ = pathIt_ != stateAssetPaths.end()
                    ? pathIt_->second
                    : std::string();
                stateAssetStatus = SaveStateAsset(name_, path_)
                    ? "Saved state: " + name_
                    : "Failed to save state: " + name_;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Play")) {
                ChangeState(name_, 0.2f);

                useTransitionGraph = false;
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.1f, 0.1f, 1.0f));
            if (ImGui::SmallButton("X")) { targetToDelete = name_; }
            ImGui::PopStyleColor();

            ImGui::PopID();
        };

        if (stateSearchText.empty())
        {
            ImGuiListClipper clipper_;
            clipper_.Begin(static_cast<int>(cachedStateGuiEntries.size()));
            while (clipper_.Step())
            {
                for (int index_ = clipper_.DisplayStart;
                    index_ < clipper_.DisplayEnd && index_ < static_cast<int>(cachedStateGuiEntries.size());
                    ++index_)
                {
                    DrawStateRow_(cachedStateGuiEntries[index_]);
                }
            }
        }
        else
        {
            for (const StateGuiEntry& entry_ : cachedStateGuiEntries)
            {
                if (!ContainsCaseInsensitive(entry_.name, stateSearchText)) continue;
                DrawStateRow_(entry_);
            }
        }

        if (!targetToDelete.empty()) {
            if (editingState && editingState->name == targetToDelete) editingState = nullptr;
            RemoveState(targetToDelete);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();


    ImGui::SeparatorText("State Properties");
    if (editingState)
    {
        BS_PROFILE_SCOPE("Animation GUI::State Properties");

        // ここから下は選択中ステートの詳細設定。名前変更、種類、ブレンド、RootMotionまでを直接編集する。
        ImGui::Text("Editing: %s", editingState->name.c_str());

        if (stateRenameSourceName != editingState->name)
        {
            stateRenameSourceName = editingState->name;
            stateRenameText = editingState->name;
        }
        ImGui::SetNextItemWidth(-90.0f);
        const bool renameSubmitted_ = ImGui::InputText(
            "##StateName",
            &stateRenameText,
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        const bool renameClicked_ = ImGui::Button("Rename");
        if (renameSubmitted_ || renameClicked_)
        {
            const std::string oldName_ = editingState->name;
            if (RenameState(oldName_, stateRenameText))
            {
                stateRenameSourceName = stateRenameText;
                stateAssetStatus = "Renamed state: " + oldName_ + " -> " + stateRenameText;
            }
            else
            {
                stateAssetStatus = "Failed to rename state. Name is empty or already in use.";
                stateRenameText = oldName_;
            }
        }

        const auto statePathIt_ = stateAssetPaths.find(editingState->name);
        const std::string currentStatePath_ = statePathIt_ != stateAssetPaths.end()
            ? statePathIt_->second
            : std::string();
        ImGui::TextDisabled(
            "%s",
            currentStatePath_.empty() ? "Unsaved state asset" : currentStatePath_.c_str());

        if (ImGui::Button("Save State"))
        {
            stateAssetStatus = SaveStateAsset(editingState->name, currentStatePath_)
                ? "Saved state: " + editingState->name
                : "Failed to save state: " + editingState->name;
        }
        ImGui::SameLine();
        if (ImGui::Button("Save State As..."))
        {
            const std::string initialPath_ = currentStatePath_.empty()
                ? BuildDefaultStateAssetPath(editingState->name)
                : currentStatePath_;
            const std::string selectedPath_ = WindowsDialogHelper::SaveJsonDialog(
                L"Save Animation State",
                initialPath_);
            if (!selectedPath_.empty())
            {
                stateAssetStatus = SaveStateAsset(editingState->name, selectedPath_)
                    ? "Saved state: " + selectedPath_
                    : "Failed to save state: " + selectedPath_;
            }
        }
        if (!currentStatePath_.empty())
        {
            ImGui::SameLine();
            if (ImGui::Button("Reload State"))
            {
                const std::string stateName_ = editingState->name;
                stateAssetStatus = LoadStateAsset(currentStatePath_)
                    ? "Reloaded state: " + stateName_
                    : "Failed to reload state: " + currentStatePath_;
            }
        }

        ImGui::TextUnformatted("State Type");
        int currentType_ = static_cast<int>(editingState->type);
        if (ImGui::RadioButton("Single", currentType_ == static_cast<int>(AnimStateType::Single)))
        {
            editingState->type = AnimStateType::Single;
            MarkSequenceAdapterDirty();
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Blend 1D", currentType_ == static_cast<int>(AnimStateType::Blend1D)))
        {
            editingState->type = AnimStateType::Blend1D;
            MarkSequenceAdapterDirty();
            MarkAnimationMaskCacheDirty();
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Blend 2D", currentType_ == static_cast<int>(AnimStateType::BLEND_2D)))
        {
            editingState->type = AnimStateType::BLEND_2D;
            MarkSequenceAdapterDirty();
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Custom", currentType_ == static_cast<int>(AnimStateType::Custom)))
        {
            editingState->type = AnimStateType::Custom;
            MarkSequenceAdapterDirty();
        }

        const auto& animations_ = modelComponent->GetAnimations();
        auto DrawAnimationIndex_ = [&](const char* label_, int& targetIndex_)
            {
                // アニメーション参照はインデックスで保存するため、候補名を検索しながら選べる共通UIにしている。
                ImGui::PushID(label_);
                const char* selectedName_ =
                    targetIndex_ >= 0 && targetIndex_ < static_cast<int>(animations_.size())
                    ? animations_[targetIndex_].name.c_str()
                    : "None";
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo(label_, selectedName_))
                {
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputTextWithHint(
                        "##AnimationClipSearch",
                        "Search animation clips...",
                        &animationClipSearchText);
                    ImGui::Separator();

                    if (animationClipSearchText.empty())
                    {
                        ImGuiListClipper clipper_;
                        clipper_.Begin(static_cast<int>(animations_.size()));
                        while (clipper_.Step())
                        {
                            for (int index_ = clipper_.DisplayStart; index_ < clipper_.DisplayEnd; ++index_)
                            {
                                if (ImGui::Selectable(
                                    animations_[index_].name.c_str(),
                                    targetIndex_ == index_))
                                {
                                    targetIndex_ = index_;
                                    MarkSequenceAdapterDirty();
                                    ImGui::CloseCurrentPopup();
                                }
                            }
                        }
                    }
                    else
                    {
                        for (int index_ = 0; index_ < static_cast<int>(animations_.size()); ++index_)
                        {
                            if (!ContainsCaseInsensitive(animations_[index_].name, animationClipSearchText)) continue;

                            if (ImGui::Selectable(
                                animations_[index_].name.c_str(),
                                targetIndex_ == index_))
                            {
                                targetIndex_ = index_;
                                MarkSequenceAdapterDirty();
                                ImGui::CloseCurrentPopup();
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopID();
            };

        if (editingState->type == AnimStateType::Single)
        {
            // Singleは1つのアニメーションクリップをそのまま再生する通常ステート。
            DrawAnimationIndex_("Animation Clip", editingState->animationIndex);
        }
        else if (editingState->type == AnimStateType::Blend1D)
        {
            // Blend1Dはfloatパラメータを入力にして、A/Bクリップの重みと再生位相を補間する。
            DrawAnimationIndex_("Animation A", editingState->animIndexA);
            DrawAnimationIndex_("Animation B", editingState->animIndexB);

            ImGui::TextUnformatted("Blend Parameter");
            if (ImGui::Button(
                editingState->blendParamName.empty()
                ? "Select Blackboard Parameter"
                : editingState->blendParamName.c_str(),
                ImVec2(-1.0f, 0.0f)))
            {
                ImGui::OpenPopup("BlendParameterPopup");
            }
            if (ImGui::BeginPopup("BlendParameterPopup"))
            {
                RefreshBlackboardGuiCache();
                BlackboardCategory& editableBlackboard_ = GetEditableBlackboard();
                for (const std::string& parameterName_ : cachedBlackboardKeys)
                {
                    auto iterator_ = editableBlackboard_.find(parameterName_);
                    if (iterator_ != editableBlackboard_.end() &&
                        std::holds_alternative<float>(iterator_->second) &&
                        ImGui::MenuItem(parameterName_.c_str()))
                    {
                        editingState->blendParamName = parameterName_;
                    }
                }
                ImGui::EndPopup();
            }

            ImGui::SeparatorText("Blend Timing");
            ImGui::SliderFloat(
                "Default Weight",
                &editingState->blendDefaultWeight,
                0.0f,
                1.0f);
            ImGui::DragFloat(
                "Parameter Min",
                &editingState->blendParameterMin,
                0.01f);
            ImGui::DragFloat(
                "Parameter Max",
                &editingState->blendParameterMax,
                0.01f);
            ImGui::Checkbox(
                "Synchronize Normalized Time",
                &editingState->blendSyncNormalizedTime);
            ImGui::SliderFloat(
                "Animation A Phase",
                &editingState->blendPhaseOffsetA,
                -1.0f,
                1.0f);
            ImGui::SliderFloat(
                "Animation B Phase",
                &editingState->blendPhaseOffsetB,
                -1.0f,
                1.0f);
            ImGui::TextDisabled(
                "Phase uses normalized cycles. 0.5 shifts an animation by half a cycle.");

            ImGui::SeparatorText("Animation Mask");
            if (ImGui::Checkbox("Use Animation Mask", &editingState->blendUseMask))
            {
                MarkAnimationMaskCacheDirty();
            }
            if (editingState->blendUseMask)
            {
                // マスクはAnimation Bを適用するボーン範囲を指定するため、選択ノードの子孫もまとめて対象になる。
                ImGui::TextDisabled(
                    "Animation B is blended only on selected nodes and their descendants.");
                if (ImGui::SmallButton("Clear Mask"))
                {
                    editingState->blendMaskRootNodes.clear();
                    MarkAnimationMaskCacheDirty();
                }

                const auto& nodes_ = modelComponent->GetNodes();
                RefreshAnimationMaskCache();
                ImGui::BeginChild("AnimationMaskNodeList", ImVec2(0.0f, 190.0f), true);
                ImGuiListClipper maskClipper_;
                maskClipper_.Begin(static_cast<int>(nodes_.size()));
                while (maskClipper_.Step())
                {
                    for (int nodeIndex_ = maskClipper_.DisplayStart;
                        nodeIndex_ < maskClipper_.DisplayEnd;
                        ++nodeIndex_)
                    {
                        const auto& node_ = nodes_[nodeIndex_];
                        const int depth_ = nodeIndex_ < static_cast<int>(cachedNodeDepths.size())
                            ? cachedNodeDepths[nodeIndex_]
                            : 0;
                        const bool inherited_ = nodeIndex_ < static_cast<int>(cachedMaskInheritedFlags.size())
                            ? cachedMaskInheritedFlags[nodeIndex_]
                            : false;
                        bool selected_ = cachedMaskRootNodeSet.find(node_.name) != cachedMaskRootNodeSet.end();

                        ImGui::PushID(nodeIndex_);
                        ImGui::Indent(static_cast<float>(depth_) * 12.0f);
                        if (ImGui::Checkbox("##MaskNode", &selected_))
                        {
                            if (selected_)
                            {
                                editingState->blendMaskRootNodes.push_back(node_.name);
                            }
                            else
                            {
                                editingState->blendMaskRootNodes.erase(
                                    std::remove(
                                        editingState->blendMaskRootNodes.begin(),
                                        editingState->blendMaskRootNodes.end(),
                                        node_.name),
                                    editingState->blendMaskRootNodes.end());
                            }
                            MarkAnimationMaskCacheDirty();
                        }
                        ImGui::SameLine();
                        if (inherited_ && !selected_)
                        {
                            ImGui::TextDisabled("%s (Inherited)", node_.name.c_str());
                        }
                        else
                        {
                            ImGui::TextUnformatted(node_.name.c_str());
                        }
                        ImGui::Unindent(static_cast<float>(depth_) * 12.0f);
                        ImGui::PopID();
                    }
                }
                ImGui::EndChild();
            }
        }
        else if (editingState->type == AnimStateType::BLEND_2D)
        {
            // Blend2Dは移動入力などのVector2を使い、前後左右や斜め方向のクリップを同期しながら混ぜる。
            ImGui::SeparatorText("Directional Animations");
            if (ImGui::Button("Apply F/B Lateral Template"))
            {
                editingState->blend2DUseEightDirections = false;
                editingState->blend2DUseForwardBackwardLateral = true;
                editingState->blend2DParamName = "MoveInput";
                editingState->blend2DDefaultInput = { 0.0f, 1.0f };
                editingState->blend2DSyncNormalizedTime = true;
                editingState->blend2DIdleThreshold = 0.05f;
            }
            if (ImGui::Checkbox(
                "Use F/B Lateral Clips",
                &editingState->blend2DUseForwardBackwardLateral) &&
                editingState->blend2DUseForwardBackwardLateral)
            {
                editingState->blend2DUseEightDirections = false;
            }
            if (ImGui::Checkbox(
                "Use 8 Direction Clips",
                &editingState->blend2DUseEightDirections) &&
                editingState->blend2DUseEightDirections)
            {
                editingState->blend2DUseForwardBackwardLateral = false;
            }
            ImGui::Checkbox("Use Idle / Stop Clip", &editingState->blend2DUseIdleStop);
            if (editingState->blend2DUseIdleStop &&
                ImGui::TreeNode("Idle / Stop (0, 0)"))
            {
                DrawAnimationIndex_(
                    "Idle Animation", editingState->blend2DIdleAnimationIndex);
                ImGui::SliderFloat(
                    "Idle Phase",
                    &editingState->blend2DIdlePhaseOffset,
                    -1.0f,
                    1.0f);
                ImGui::DragFloat(
                    "Idle Threshold",
                    &editingState->blend2DIdleThreshold,
                    0.01f,
                    0.0f,
                    1.0f);
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Forward (0, +1)"))
            {
                DrawAnimationIndex_(
                    "Forward Animation", editingState->blend2DForwardAnimationIndex);
                ImGui::SliderFloat(
                    "Forward Phase",
                    &editingState->blend2DForwardPhaseOffset,
                    -1.0f,
                    1.0f);
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Backward (0, -1)"))
            {
                DrawAnimationIndex_(
                    "Backward Animation", editingState->blend2DBackwardAnimationIndex);
                ImGui::SliderFloat(
                    "Backward Phase",
                    &editingState->blend2DBackwardPhaseOffset,
                    -1.0f,
                    1.0f);
                ImGui::TreePop();
            }
            if (!editingState->blend2DUseForwardBackwardLateral &&
                ImGui::TreeNode("Left (-1, 0)"))
            {
                DrawAnimationIndex_(
                    "Left Animation", editingState->blend2DLeftAnimationIndex);
                ImGui::SliderFloat(
                    "Left Phase",
                    &editingState->blend2DLeftPhaseOffset,
                    -1.0f,
                    1.0f);
                ImGui::TreePop();
            }
            if (!editingState->blend2DUseForwardBackwardLateral &&
                ImGui::TreeNode("Right (+1, 0)"))
            {
                DrawAnimationIndex_(
                    "Right Animation", editingState->blend2DRightAnimationIndex);
                ImGui::SliderFloat(
                    "Right Phase",
                    &editingState->blend2DRightPhaseOffset,
                    -1.0f,
                    1.0f);
                ImGui::TreePop();
            }
            if (editingState->blend2DUseEightDirections ||
                editingState->blend2DUseForwardBackwardLateral)
            {
                if (ImGui::TreeNode("Forward Left (-1, +1)"))
                {
                    DrawAnimationIndex_(
                        "Forward Left Animation",
                        editingState->blend2DForwardLeftAnimationIndex);
                    ImGui::SliderFloat(
                        "Forward Left Phase",
                        &editingState->blend2DForwardLeftPhaseOffset,
                        -1.0f,
                        1.0f);
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Forward Right (+1, +1)"))
                {
                    DrawAnimationIndex_(
                        "Forward Right Animation",
                        editingState->blend2DForwardRightAnimationIndex);
                    ImGui::SliderFloat(
                        "Forward Right Phase",
                        &editingState->blend2DForwardRightPhaseOffset,
                        -1.0f,
                        1.0f);
                    ImGui::TreePop();
                }
                if (editingState->blend2DUseForwardBackwardLateral &&
                    ImGui::TreeNode("Forward Left 90 (-1, +0)"))
                {
                    DrawAnimationIndex_(
                        "Forward Left 90 Animation",
                        editingState->blend2DForwardLeft90AnimationIndex);
                    ImGui::SliderFloat(
                        "Forward Left 90 Phase",
                        &editingState->blend2DForwardLeft90PhaseOffset,
                        -1.0f,
                        1.0f);
                    ImGui::TreePop();
                }
                if (editingState->blend2DUseForwardBackwardLateral &&
                    ImGui::TreeNode("Forward Right 90 (+1, +0)"))
                {
                    DrawAnimationIndex_(
                        "Forward Right 90 Animation",
                        editingState->blend2DForwardRight90AnimationIndex);
                    ImGui::SliderFloat(
                        "Forward Right 90 Phase",
                        &editingState->blend2DForwardRight90PhaseOffset,
                        -1.0f,
                        1.0f);
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Backward Left (-1, -1)"))
                {
                    DrawAnimationIndex_(
                        "Backward Left Animation",
                        editingState->blend2DBackwardLeftAnimationIndex);
                    ImGui::SliderFloat(
                        "Backward Left Phase",
                        &editingState->blend2DBackwardLeftPhaseOffset,
                        -1.0f,
                        1.0f);
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Backward Right (+1, -1)"))
                {
                    DrawAnimationIndex_(
                        "Backward Right Animation",
                        editingState->blend2DBackwardRightAnimationIndex);
                    ImGui::SliderFloat(
                        "Backward Right Phase",
                        &editingState->blend2DBackwardRightPhaseOffset,
                        -1.0f,
                        1.0f);
                    ImGui::TreePop();
                }
                if (editingState->blend2DUseForwardBackwardLateral &&
                    ImGui::TreeNode("Backward Left 90 (-1, 0)"))
                {
                    DrawAnimationIndex_(
                        "Backward Left 90 Animation",
                        editingState->blend2DBackwardLeft90AnimationIndex);
                    ImGui::SliderFloat(
                        "Backward Left 90 Phase",
                        &editingState->blend2DBackwardLeft90PhaseOffset,
                        -1.0f,
                        1.0f);
                    ImGui::TreePop();
                }
                if (editingState->blend2DUseForwardBackwardLateral &&
                    ImGui::TreeNode("Backward Right 90 (+1, 0)"))
                {
                    DrawAnimationIndex_(
                        "Backward Right 90 Animation",
                        editingState->blend2DBackwardRight90AnimationIndex);
                    ImGui::SliderFloat(
                        "Backward Right 90 Phase",
                        &editingState->blend2DBackwardRight90PhaseOffset,
                        -1.0f,
                        1.0f);
                    ImGui::TreePop();
                }
            }

            ImGui::SeparatorText("Blend Input");
            // 入力パラメータはVector2型のBlackboardだけに限定して、型違いの設定を避ける。
            if (ImGui::Button(
                editingState->blend2DParamName.empty()
                ? "Select Vector2 Parameter"
                : editingState->blend2DParamName.c_str(),
                ImVec2(-1.0f, 0.0f)))
            {
                ImGui::OpenPopup("Blend2DParameterPopup");
            }
            if (ImGui::BeginPopup("Blend2DParameterPopup"))
            {
                RefreshBlackboardGuiCache();
                BlackboardCategory& editableBlackboard_ = GetEditableBlackboard();
                for (const std::string& parameterName_ : cachedBlackboardKeys)
                {
                    auto iterator_ = editableBlackboard_.find(parameterName_);
                    if (iterator_ != editableBlackboard_.end() &&
                        std::holds_alternative<DirectX::SimpleMath::Vector2>(iterator_->second) &&
                        ImGui::MenuItem(parameterName_.c_str()))
                    {
                        editingState->blend2DParamName = parameterName_;
                    }
                }
                ImGui::EndPopup();
            }

            ImGui::DragFloat2(
                "Default Input",
                &editingState->blend2DDefaultInput.x,
                0.01f,
                -1.0f,
                1.0f);
            ImGui::Checkbox(
                "Synchronize Normalized Time##Blend2D",
                &editingState->blend2DSyncNormalizedTime);
            if (editingState->blend2DUseForwardBackwardLateral)
            {
                ImGui::TextDisabled(
                    "Y selects Forward/Backward. X blends 0 -> 45 -> 90 lateral clips.");
            }
            else
            {
                ImGui::TextDisabled(
                    "X blends Left/Right. Y blends Backward/Forward.");
                ImGui::TextDisabled(
                    "Diagonal input automatically blends the two matching directions.");
            }
        }
        else if (editingState->type == AnimStateType::Custom)
        {
            // CustomはPose Animation Creatorで作った独自ポーズ列を参照するため、ライブラリ内の名前から選ぶ。
            if (poseLibrary && !poseLibrary->customAnims.empty())
            {
                ImGui::TextUnformatted("Custom Animation");
                ImGui::BeginChild("CustomAnimationList", ImVec2(0.0f, 110.0f), true);
                ImGuiListClipper customClipper_;
                customClipper_.Begin(static_cast<int>(poseLibrary->customAnims.size()));
                while (customClipper_.Step())
                {
                    for (int index_ = customClipper_.DisplayStart;
                        index_ < customClipper_.DisplayEnd;
                        ++index_)
                    {
                        if (ImGui::Selectable(
                            poseLibrary->customAnims[index_].name.c_str(),
                            editingState->customAnimIndex == index_))
                        {
                            editingState->customAnimIndex = index_;
                        }
                    }
                }
                ImGui::EndChild();
            }
            else
            {
                ImGui::TextDisabled("Create a custom animation in Pose Animation Creator.");
            }
        }
        ImGui::DragFloat("Speed", &editingState->playbackSpeed, 0.05f);
        ImGui::Checkbox("Loop", &editingState->isLoop);


        ImGui::Spacing();
        if (ImGui::TreeNode("Root Motion Settings"))
        {

            // RootMotionは見た目のノード移動を抜き取り、GameObject側へ移動量として適用するための設定。
            ImGui::Checkbox("Enable Root Motion", &editingState->rootMotion.enable);

            if (editingState->rootMotion.enable)
            {
                ImGui::Indent();

                ImGui::DragFloat("Root Motion Scale", &editingState->rootMotionScale, 0.01f);

                ImGui::TextUnformatted("Root Motion Scale Curve");
                if (ImGui::Button(
                    editingState->rootMotionScaleCurveName.empty()
                    ? "No Curve"
                    : editingState->rootMotionScaleCurveName.c_str(),
                    ImVec2(-1.0f, 0.0f)))
                {
                    ImGui::OpenPopup("RootMotionCurvePopup");
                }
                if (ImGui::BeginPopup("RootMotionCurvePopup"))
                {
                    if (ImGui::MenuItem("No Curve"))
                    {
                        editingState->rootMotionScaleCurveName.clear();
                    }
                    const std::vector<std::string> curveNames_ = curveComponent
                        ? curveComponent->GetTrackNames()
                        : std::vector<std::string>();
                    for (const std::string& curveName_ : curveNames_)
                    {
                        if (ImGui::MenuItem(curveName_.c_str()))
                        {
                            editingState->rootMotionScaleCurveName = curveName_;
                        }
                    }
                    ImGui::EndPopup();
                }

                ImGui::TextDisabled("Final RootMotion = Delta * Scale * CurveValue");

                const std::string sourcePreview_ = editingState->rootMotion.sourceNodeName.empty()
                    ? std::string("Model Default (") + modelComponent->GetRootMotionName() + ")"
                    : editingState->rootMotion.sourceNodeName;
                ImGui::TextUnformatted("Motion Source Node");
                if (ImGui::Button(sourcePreview_.c_str(), ImVec2(-1.0f, 0.0f)))
                {
                    ImGui::OpenPopup("RootMotionNodePopup");
                }
                if (ImGui::BeginPopup("RootMotionNodePopup"))
                {
                    if (ImGui::MenuItem("Model Default"))
                    {
                        editingState->rootMotion.sourceNodeName.clear();
                    }

                    for (const auto& node_ : modelComponent->GetNodes())
                    {
                        if (ImGui::MenuItem(node_.name.c_str()))
                        {
                            editingState->rootMotion.sourceNodeName = node_.name;
                        }
                    }
                    ImGui::EndPopup();
                }
                ImGui::TextDisabled("Source-node movement is always removed while Root Motion is enabled.");

                ImGui::Text("Apply Position to GameObject:");
                ImGui::Checkbox("X##Pos", &editingState->rootMotion.applyPositionX); ImGui::SameLine();
                ImGui::Checkbox("Y##Pos", &editingState->rootMotion.applyPositionY); ImGui::SameLine();
                ImGui::Checkbox("Z##Pos", &editingState->rootMotion.applyPositionZ);

                ImGui::Text("Extra Cut Visual Position from Source Node:");
                ImGui::Checkbox("X##CutVisualPos", &editingState->rootMotion.cutVisualPositionX); ImGui::SameLine();
                ImGui::Checkbox("Y##CutVisualPos", &editingState->rootMotion.cutVisualPositionY); ImGui::SameLine();
                ImGui::Checkbox("Z##CutVisualPos", &editingState->rootMotion.cutVisualPositionZ);


                ImGui::Unindent();
            }

            ImGui::TreePop();
        }
        ImGui::TextDisabled("Use Window > Animation Sequence to edit events in the sequencer.");
    }
    else
    {
        ImGui::TextDisabled("Select a state from the list or node editor to edit.");
    }



    if (ImGui::CollapsingHeader("Blackboard (Params)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        BS_PROFILE_SCOPE("Animation GUI::Blackboard Params");

        // ステート遷移やブレンド入力に使うランタイムパラメータを、ここで型付きで編集する。
        static char newVarName[64] = "";
        static int selectedType = 0; // 0:Float, 1:Int, 2:Bool, 3:Vector2
        const char* typeNames[] = { "Float", "Int", "Bool", "Vector2" };


        ImGui::SeparatorText("Add New Parameter");
        ImGui::SetNextItemWidth(150.0f);
        ImGui::InputText("Name##BB", newVarName, IM_ARRAYSIZE(newVarName));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::Combo("Type##BB", &selectedType, typeNames, IM_ARRAYSIZE(typeNames));
        ImGui::SameLine();
        if (ImGui::Button("Add##BB"))
        {
            std::string name(newVarName);
            if (!name.empty() && GetEditableBlackboard().find(name) == GetEditableBlackboard().end())
            {
                switch (selectedType)
                {
                case 0: GetEditableBlackboard()[name] = 0.0f;                                   break;
                case 1: GetEditableBlackboard()[name] = 0;                                      break;
                case 2: GetEditableBlackboard()[name] = false;                                  break;
                case 3: GetEditableBlackboard()[name] = DirectX::SimpleMath::Vector2(0.0f, 0.0f); break;
                }
                MarkBlackboardGuiCacheDirty();
                newVarName[0] = '\0';
            }
        }


        ImGui::SeparatorText("Parameters");

        if (GetEditableBlackboard().empty())
        {
            ImGui::TextDisabled("No parameters defined.");
        }
        else
        {
            std::string toDelete;
            BlackboardCategory& editableBlackboard_ = GetEditableBlackboard();
            RefreshBlackboardGuiCache();
            auto DrawBlackboardRow_ = [&](const std::string& key_, BlackBoardValue& val_)
            {
                ImGui::PushID(key_.c_str());


                ImGui::Text("%-20s", key_.c_str());
                ImGui::SameLine();


                if (auto* f = std::get_if<float>(&val_)) {
                    ImGui::SetNextItemWidth(100.0f);
                    ImGui::DragFloat("##v", f, 0.01f);
                }
                else if (auto* i = std::get_if<int>(&val_)) {
                    ImGui::SetNextItemWidth(100.0f);
                    ImGui::DragInt("##v", i);
                }
                else if (auto* b = std::get_if<bool>(&val_)) {
                    ImGui::Checkbox("##v", b);
                }
                else if (auto* v = std::get_if<DirectX::SimpleMath::Vector2>(&val_)) {
                    ImGui::SetNextItemWidth(180.0f);
                    ImGui::DragFloat2("##v", &v->x, 0.01f);
                }

                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.1f, 0.1f, 1.0f));
                if (ImGui::Button("X##del")) { toDelete = key_; }
                ImGui::PopStyleColor();

                ImGui::PopID();
            };

            ImGuiListClipper clipper_;
            clipper_.Begin(static_cast<int>(cachedBlackboardKeys.size()));
            while (clipper_.Step())
            {
                for (int index_ = clipper_.DisplayStart;
                    index_ < clipper_.DisplayEnd && index_ < static_cast<int>(cachedBlackboardKeys.size());
                    ++index_)
                {
                    const std::string& key_ = cachedBlackboardKeys[index_];
                    auto iterator_ = editableBlackboard_.find(key_);
                    if (iterator_ == editableBlackboard_.end()) continue;
                    DrawBlackboardRow_(iterator_->first, iterator_->second);
                }
            }
            if (!toDelete.empty())
            {
                editableBlackboard_.erase(toDelete);
                MarkBlackboardGuiCacheDirty();
            }
        }
    }

}

void AnimationControllerComponent::ShowSequencerEditor()
{
    BS_PROFILE_SCOPE("AnimationControllerComponent::ShowSequencerEditor");

    // 別ウィンドウでは選択ステートのイベントタイムラインだけを扱い、Inspector側のステート編集と同期する。
    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);


    if (!ImGui::Begin("Animation Sequence Editor", &showSequencerWindow))
    {
        ImGui::End();
        return;
    }

    if (!editingState)
    {
        ImGui::TextDisabled("Select a state to edit events.");
        ImGui::End();
        return;

    }
    else
    {

        RefreshSequenceAdapter();
        if (sequencerSelectedEntry >= sequenceAdapter.GetItemCount())
        {
            sequencerSelectedEntry = -1;
        }


        int currentFrame = (int)(animator.GetCurrentFadeTime() * sequenceAdapter.fps);
        const int previousFrame_ = currentFrame;

        // ImSequencerの編集結果はフレーム値で返るため、対象イベントの秒数へ戻して保存する。


        ImGui::PushItemWidth(-1);



        if (ImGui::BeginChild("SequencerRegion", ImVec2(0, 300), true))
        {
            if (ImSequencer::Sequencer(&sequenceAdapter, &currentFrame, &sequencerExpanded, &sequencerSelectedEntry, &sequencerFirstFrame,
                ImSequencer::SEQUENCER_EDIT_ALL | ImSequencer::SEQUENCER_ADD |
                ImSequencer::SEQUENCER_DEL | ImSequencer::SEQUENCER_CHANGE_FRAME))
            {


                if (sequencerSelectedEntry >= 0 && sequencerSelectedEntry < (int)sequenceAdapter.targetState->events.size())
                {
                    int* pStart, * pEnd;
                    sequenceAdapter.Get(sequencerSelectedEntry, &pStart, &pEnd, nullptr, nullptr);
                    auto& ev = sequenceAdapter.targetState->events[sequencerSelectedEntry];
                    ev.startTime = sequenceAdapter.ToTime(*pStart);
                    ev.endTime = sequenceAdapter.ToTime(*pEnd);
                }
                MarkSequenceAdapterDirty();
            }
            ImGui::EndChild();
        }

        if (currentFrame != previousFrame_)
        {
            const int clampedFrame_ = (std::max)(
                sequenceAdapter.GetFrameMin(),
                (std::min)(sequenceAdapter.GetFrameMax(), currentFrame));
            PreviewStateAtTime(editingState, sequenceAdapter.ToTime(clampedFrame_));
        }


        ImGui::PopItemWidth();


        if (sequencerSelectedEntry >= 0 && sequencerSelectedEntry < (int)sequenceAdapter.targetState->events.size())
        {

            if (ImGui::BeginChild("EventDetails", ImVec2(0, 0), true))
            {
                auto& ev = sequenceAdapter.targetState->events[sequencerSelectedEntry];


                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Editing Event #%d", sequencerSelectedEntry);
                ImGui::SameLine();


                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
                if (ImGui::Button("Delete Event"))
                {

                    sequenceAdapter.Del(sequencerSelectedEntry);
                    MarkSequenceAdapterDirty();
                    sequencerSelectedEntry = -1;
                    ImGui::PopStyleColor(2);
                    ImGui::EndChild();
                    ImGui::End();
                    return;
                }
                ImGui::PopStyleColor(2);

                ImGui::SameLine(ImGui::GetWindowWidth() - 100);
                if (ImGui::Button("Deselect")) { sequencerSelectedEntry = -1; }


                ImGui::SetNextItemWidth(200.0f);
                if (ImGui::InputText("Display Label", &ev.eventLabel)) {

                }
                ImGui::SameLine();
                ImGui::TextDisabled("(Name on timeline)");

                ImGui::Separator();
                RefreshActionCategoryCache();
                if (ImGui::SmallButton("Refresh Actions"))
                {
                    MarkActionCategoryCacheDirty();
                    RefreshActionCategoryCache();
                }

                auto ActionField = [&](const char* label, std::string& targetStr) {
                    // Start/Update/Endの各Actionは直接入力もできるが、登録済みアクションから選ぶのが基本。
                    ImGui::SetNextItemWidth(250.0f);
                    ImGui::InputText(label, &targetStr);

                    ImGui::SameLine();
                    std::string popupName = std::string("ActionSelect##") + label;
                    if (ImGui::Button((std::string("Select##") + label).c_str())) {
                        ImGui::OpenPopup(popupName.c_str());
                    }

                    if (ImGui::BeginPopup(popupName.c_str())) {
                        if (ImGui::MenuItem("Clear")) {
                            targetStr.clear();
                        }
                        ImGui::Separator();

                        for (const CachedActionCategory& entry : cachedActionCategories) {
                            if (ImGui::BeginMenu(entry.menuLabel.c_str())) {
                                for (const std::string& actionName : entry.actionNames) {
                                    if (ImGui::MenuItem(actionName.c_str())) {
                                        targetStr = actionName;
                                    }
                                }
                                ImGui::EndMenu();
                            }
                        }
                        ImGui::EndPopup();
                    }
                    };

                ActionField("Start Action", ev.startAction);
                ActionField("Update Action", ev.updateAction);
                ActionField("End Action", ev.endAction);
                ImGui::SeparatorText("Blackboard Writes");
                // イベント区間に入ったタイミングで書き込む値。攻撃判定やVFX指定をタイムラインから渡す。
                int removeWriteIndex_ = -1;
                for (int writeIndex_ = 0;
                    writeIndex_ < static_cast<int>(ev.blackboardWrites.size());
                    ++writeIndex_)
                {
                    AnimBlackboardWrite& write_ = ev.blackboardWrites[writeIndex_];
                    ImGui::PushID(writeIndex_);
                    ImGui::InputText("Category", &write_.category);
                    ImGui::InputText("Key", &write_.key);

                    int typeIndex_ = static_cast<int>(write_.type);
                    const char* typeNames_[] = { "Float", "Int", "Bool", "String" };
                    if (ImGui::Combo("Type", &typeIndex_, typeNames_, IM_ARRAYSIZE(typeNames_)))
                    {
                        write_.type = static_cast<ANIM_BLACKBOARD_VALUE_TYPE>(typeIndex_);
                    }

                    switch (write_.type)
                    {
                    case ANIM_BLACKBOARD_VALUE_TYPE::FLOAT:
                        ImGui::DragFloat("Value", &write_.floatValue, 0.1f);
                        break;
                    case ANIM_BLACKBOARD_VALUE_TYPE::INT:
                        ImGui::DragInt("Value", &write_.intValue);
                        break;
                    case ANIM_BLACKBOARD_VALUE_TYPE::BOOL:
                        ImGui::Checkbox("Value", &write_.boolValue);
                        break;
                    case ANIM_BLACKBOARD_VALUE_TYPE::STRING:
                    {
                        const bool isParticleAsset_ = write_.key == "HitParticleAssetPath";
                        const bool isMeshVFXAsset_ = write_.key == "HitMeshVFXAssetPath";
                        std::shared_ptr<GameObject> owner_ = GetGameObject();
                        if ((isParticleAsset_ || isMeshVFXAsset_) && owner_)
                        {
                            std::string previewValue_ = write_.stringValue;
                            if (ImGui::BeginCombo("Value", previewValue_.empty() ? "(None)" : previewValue_.c_str()))
                            {
                                RefreshEffectAssetCache();
                                const std::vector<std::string>& assets_ = isParticleAsset_
                                    ? cachedParticleAssetKeys
                                    : cachedMeshEffectAssetKeys;
                                if (ImGui::Selectable("(None)", write_.stringValue.empty())) write_.stringValue.clear();
                                for (const std::string& assetKey_ : assets_)
                                {
                                    const bool selected_ = previewValue_ == assetKey_;
                                    if (ImGui::Selectable(assetKey_.c_str(), selected_)) write_.stringValue = assetKey_;
                                    if (selected_) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                        }
                        else
                        {
                            ImGui::InputText("Value", &write_.stringValue);
                        }
                        break;
                    }
                    }
                    if (ImGui::Button("Remove Write")) removeWriteIndex_ = writeIndex_;
                    ImGui::Separator();
                    ImGui::PopID();
                }
                if (removeWriteIndex_ >= 0)
                {
                    ev.blackboardWrites.erase(ev.blackboardWrites.begin() + removeWriteIndex_);
                }
                auto addAttackWrite_ = [&ev](const char* key_, ANIM_BLACKBOARD_VALUE_TYPE type_)
                {
                    AnimBlackboardWrite write_{};
                    write_.category = "Attack";
                    write_.key = key_;
                    write_.type = type_;
                    ev.blackboardWrites.push_back(write_);
                };
                if (ImGui::Button("+ Damage")) addAttackWrite_("Damage", ANIM_BLACKBOARD_VALUE_TYPE::FLOAT);
                ImGui::SameLine();
                if (ImGui::Button("+ HitStopDuration")) addAttackWrite_("HitStopDuration", ANIM_BLACKBOARD_VALUE_TYPE::FLOAT);
                ImGui::SameLine();
                if (ImGui::Button("+ HitStopScale")) addAttackWrite_("HitStopScale", ANIM_BLACKBOARD_VALUE_TYPE::FLOAT);
                ImGui::SameLine();
                if (ImGui::Button("+ HitStopLayer")) addAttackWrite_("HitStopLayerMask", ANIM_BLACKBOARD_VALUE_TYPE::INT);
                ImGui::SameLine();
                if (ImGui::Button("+ KnockBack")) addAttackWrite_("KnockBackPower", ANIM_BLACKBOARD_VALUE_TYPE::FLOAT);
                ImGui::SameLine();
                if (ImGui::Button("+ HitParticle")) addAttackWrite_("HitParticleAssetPath", ANIM_BLACKBOARD_VALUE_TYPE::STRING);
                ImGui::SameLine();
                if (ImGui::Button("+ HitMeshVFX")) addAttackWrite_("HitMeshVFXAssetPath", ANIM_BLACKBOARD_VALUE_TYPE::STRING);
                if (ImGui::Button("Add Blackboard Write"))
                {
                    ev.blackboardWrites.push_back(AnimBlackboardWrite{});
                }

                ImGui::Checkbox("Call End On State Exit", &ev.callEndOnStateExit);
                ImGui::Checkbox("Override Playback Speed", &ev.overridePlaybackSpeed);
                if (ev.overridePlaybackSpeed)
                {
                    ImGui::SetNextItemWidth(120.0f);
                    ImGui::DragFloat("Playback Speed", &ev.playbackSpeed, 0.05f, 0.01f, 10.0f, "%.2fx");
                }

                ImGui::Separator();


                ImGui::PushItemWidth(120.0f);
                if (ImGui::DragFloat("Start (s)", &ev.startTime, 0.01f, 0.0f, ev.endTime)) {
                    ev.frameStart = sequenceAdapter.ToFrame(ev.startTime);
                    MarkSequenceAdapterDirty();
                }
                ImGui::SameLine();
                if (ImGui::DragFloat("End (s)", &ev.endTime, 0.01f, ev.startTime, sequenceAdapter.duration)) {
                    ev.frameEnd = sequenceAdapter.ToFrame(ev.endTime);
                    MarkSequenceAdapterDirty();
                }
                ImGui::PopItemWidth();

                ImGui::EndChild();
            }
        }
        else
        {
            ImGui::TextDisabled("Click an event bar or name to edit details.");
        }

        ImGui::End();
    }

}

