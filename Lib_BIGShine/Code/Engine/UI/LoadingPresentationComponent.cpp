#include "PrecompiledHeader.h"
#include "LoadingPresentationComponent.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include <imgui.h>
const std::string& LoadingPresentationComponent::GetName() const { static const std::string NAME = "LoadingPresentationComponent"; return NAME; }
void LoadingPresentationComponent::Start()
{
    BS_PROFILE_SCOPE("LoadingPresentationComponent::Start");
    presentation.Initialize(brand == "BigShine" || (brand == "AUTO" && GetManager().IsEngineLoadingPresentation()));
}
void LoadingPresentationComponent::OnDestroy() { presentation.Finalize(); }
void LoadingPresentationComponent::UpdateUnscaled(float deltaTime_)
{
    BS_PROFILE_SCOPE("LoadingPresentationComponent::UpdateUnscaled");
    const auto status_ = GetManager().GetPresentationLoadingStatus();
    presentation.Update(deltaTime_,status_.progress,status_.IsCompleted());
    // 旧SceneLoading APIから明示された色だけを、保存済みシルエットへ一時的に重ねる。
    if (GetManager().HasPresentationSilhouetteOverride())
        for (const auto& object_ : GetManager().GetObjects())
        {
            const auto model_ = object_ ? object_->GetComponent<GltfModelCommponennt>() : nullptr;
            if (model_ && model_->HasRuntimeSilhouette()) model_->SetRuntimeSilhouette(true,
                GetManager().IsPresentationSilhouetteWhite() ? DirectX::XMFLOAT4{1,1,1,1} : DirectX::XMFLOAT4{0,0,0,1});
        }
}
void LoadingPresentationComponent::RenderUI()
{
    BS_PROFILE_SCOPE("LoadingPresentationComponent::RenderUI");
    const auto status_ = GetManager().GetPresentationLoadingStatus();
    presentation.Draw(status_.HasFailed(),standalone);
    if (!status_.HasFailed() && !GetManager().SupportsEditorPlayMode()) return;
    ImGui::SetNextWindowPos(ImVec2(30,30),ImGuiCond_Always);
    const auto flags_ = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("Loading status",nullptr,flags_))
    {
        ImGui::TextUnformatted(GetManager().IsEngineLoadingPresentation() ? "BIGSHINE" : "ANATHEMA");
        if (GetManager().SupportsEditorPlayMode())
        {
            const bool paused_ = GetManager().GetEditorPlayMode() == EditorPlayMode::PAUSED;
            if (ImGui::Button(paused_ ? "Resume" : "Pause")) GetManager().RequestEditorPlayCommand(EditorPlayCommand::PAUSE);
            ImGui::SameLine(); if (ImGui::Button("Stop")) GetManager().RequestEditorPlayCommand(EditorPlayCommand::STOP);
        }
        ImGui::TextUnformatted(status_.operation.c_str());
        ImGui::ProgressBar(status_.progress,ImVec2(400,18));
        if (!GetManager().GetPresentationWarning().empty()) ImGui::TextWrapped("Presentation unavailable: %s",GetManager().GetPresentationWarning().c_str());
        if (status_.HasFailed())
        {
            ImGui::TextWrapped("%s",status_.error.c_str());
            if (GetManager().IsPresentationRetryEnabled())
                if (ImGui::Button("Retry") || ImGui::IsKeyPressed(ImGuiKey_R,false)) GetManager().RequestPresentationRetry();
            if (ImGui::Button("Exit")) PostQuitMessage(0);
        }
    }
    ImGui::End();
}
void LoadingPresentationComponent::Gui()
{
    const char* names_[] = {"AUTO","BigShine","ANATHEMA"};
    int selected_ = brand == "BigShine" ? 1 : brand == "ANATHEMA" ? 2 : 0;
    if (ImGui::Combo("Brand",&selected_,names_,3)) brand = names_[selected_];
    ImGui::Checkbox("Standalone Background",&standalone);
    if (ImGui::Button("Reload Presentation JSON")) Start();
}
void LoadingPresentationComponent::Serialize(nlohmann::json& json_) const { json_ = {{"Brand",brand},{"Standalone",standalone}}; }
void LoadingPresentationComponent::Deserialize(const nlohmann::json& json_) { brand = json_.value("Brand",std::string("AUTO")); standalone = json_.value("Standalone",false); }
