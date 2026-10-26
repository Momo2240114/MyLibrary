#include "PrecompiledHeader.h"
#include "TextureMaskTransition.h"
#include "UIPresentationPresets.h"
#include <imgui.h>
namespace {
const bool REGISTERED = TransitionEffectFactory::Register("TEXTURE_MASK", []() { return std::make_unique<TextureMaskTransition>(); });
}
TextureMaskTransition::TextureMaskTransition() : ShaderTransitionEffect("Data/Shader/TextureMaskTransitionPS.cso") {}
// 固有パラメータだけを編集し、共通の時間と色はComponent側で管理する。
void TextureMaskTransition::Gui(TransitionParameters& parameters_)
{
    DrawEdges(parameters_); DrawDirection(parameters_);
    constexpr const char* MODES[] = {"Threshold","Directional Mask","Radial Mask","Flipbook Mask"};
    constexpr const char* CHANNELS[] = {"Red","Green","Blue","Alpha","Luminance"};
    int mode_ = static_cast<int>(UIPresentationPresets::Number(parameters_.effectSettings,"MaskMode",0,0,3));
    int channel_ = static_cast<int>(UIPresentationPresets::Number(parameters_.effectSettings,"Channel",0,0,4));
    if (ImGui::Combo("Mask Mode",&mode_,MODES,4)) parameters_.effectSettings["MaskMode"] = mode_;
    if (ImGui::Combo("Mask Channel",&channel_,CHANNELS,5)) parameters_.effectSettings["Channel"] = channel_;
    for (const auto key_ : {"ScrollX","ScrollY"})
    {
        float speed_ = UIPresentationPresets::Number(parameters_.effectSettings,key_,0,-5,5);
        if (ImGui::DragFloat(key_,&speed_,.01f,-5,5)) parameters_.effectSettings[key_] = speed_;
    }
    DrawTexture(parameters_,"Texture","Mask Texture"); DrawMaskUV(parameters_);
    for (const auto key_ : {"Columns","Rows","FrameCount"})
    {
        int count_ = static_cast<int>(UIPresentationPresets::Number(parameters_.effectSettings,key_,1,1,256));
        if (ImGui::SliderInt(key_,&count_,1,std::string(key_) == "FrameCount" ? 256 : 16)) parameters_.effectSettings[key_] = count_;
    }
    bool invert_ = UIPresentationPresets::Boolean(parameters_.effectSettings,"Invert",false);
    if (ImGui::Checkbox("White First / Invert", &invert_)) parameters_.effectSettings["Invert"] = invert_;
}
