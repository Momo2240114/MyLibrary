#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <filesystem>
#include "CurveData.h" // æ‚Ù‚Ç‚ÌCurveData‚ğg—p
#include "CurveSerializer.h"

class CurveAssetComponent : public Component
{
public:
    CurveData curveData;
    std::string filePath;

    const std::string& GetName() const override;
    void SaveCurve();
    void LoadCurve(const std::string& filename);
    void Gui() override;
};