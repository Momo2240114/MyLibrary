#pragma once

#include "TrailMeshComponent.h"

class AfterimageTrailComponent : public TrailMeshComponent
{
public:
    AfterimageTrailComponent();

    const std::string& GetName() const override
    {
        static std::string name = "AfterimageTrailComponent";
        return name;
    }

    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

private:
    // ActionSequenceからTrail開始/停止/クリアを呼べるよう登録する。
    void RegisterActions();

    // 所有者Transformから、現在フレームの帯メッシュ左右端点を求める。
    bool GetTrailSegment(
        DirectX::XMFLOAT3& leftPosition_,
            DirectX::XMFLOAT3& rightPosition_) const;

    // 軌跡帯の基本幅。
    float trailWidth = 1.8f;

    // 所有者原点から少し上へずらして帯を出す。
    float verticalOffset = 0.8f;

    // 幅を時間で揺らすための設定。
    float wobbleAmplitude = 0.35f;
    float wobbleSpeed = 18.0f;
};
