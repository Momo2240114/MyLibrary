#pragma once
#include <DirectXMath.h>
#include <nlohmann/json.hpp>
enum class TransitionType { FADE, RADIAL, SLASH, DISSOLVE, SHUTTER, WIPE, TEXTURE_MASK, DIAMOND, CURTAIN, CHECKER, SPIRAL, GLASS };
enum class TransitionState { IDLE, TRANSITION_OUT, COVERED, WAITING_FOR_SCENE, TRANSITION_IN };
enum class TransitionEasing { LINEAR, EASE_IN_CUBIC, EASE_OUT_CUBIC, EASE_IN_OUT_CUBIC, EASE_OUT_EXPO, EASE_OUT_BACK };

// Sceneデータとは独立して保存し、旧Scene形式との互換性を保つ。
struct TransitionParameters
{
    TransitionType type = TransitionType::FADE;
    TransitionEasing easing = TransitionEasing::EASE_IN_OUT_CUBIC;
    float durationOut = 0.6f;
    float durationIn = 0.65f;
    float delay = 0.0f;
    DirectX::XMFLOAT4 color{0, 0, 0, 1};
    DirectX::XMFLOAT4 edgeColor{0.7f, 1.0f, 0.85f, 1};
    float edgeWidth = 0.012f;
    DirectX::XMFLOAT2 direction{1, 0};
    float noiseScale = 6.0f;
    float noiseSpeed = 0.2f;
    float glowIntensity = 1.3f;
    DirectX::XMFLOAT2 centerPosition{0.5f, 0.5f};
    int panelCount = 8;
    float panelDelay = 0.06f;
    float boundaryCurve = 0.0f;
    float softness = 0.006f;
    float holdDuration = 0.15f;
    float playbackSpeed = 1.0f;
    bool blockInput = true;
    // 固有TextureとUV設定は各Effectが解釈し、共通再生処理には持ち込まない。
    nlohmann::json effectSettings = nlohmann::json::object();
    static TransitionParameters Deserialize(const nlohmann::json& json_);
    static nlohmann::json Serialize(const TransitionParameters& parameters_);
    static const char* GetTypeName(TransitionType type_);
};

