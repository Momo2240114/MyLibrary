#pragma once
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include "Engine/Graphics/Resource/GlTFModel.h" // GltfModelCommponenntの型定義のため

// ポーズを構成するキーフレームの記憶
struct PoseKeyframe 
{
    float time;
    int poseIndex;

    int sourceAnimIdx;
    float sourceTime;

    //cerealやjsonへの書記と解読
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(PoseKeyframe, time, poseIndex, sourceAnimIdx, sourceTime)
};

// ポーズを繋ぎ合わせて創り出す、新たなアニメーション
struct CustomAnimation {
    std::string name;
    float duration;
    std::vector<PoseKeyframe> keyframes;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CustomAnimation, name, duration, keyframes)
};

// 切りとられた一瞬のポーズ（姿勢）の記憶
struct SavedPose {
    std::string name;
    std::vector<GltfModelCommponennt::NodePose> nodePoses;
    int sourceAnimIndex = 0;
    float sourceTime = 0.0f;
};

// JSON交信のための変換（NodePoseはDirectXの型等を含むため、手動展開、あるいはマクロを適用）
inline void to_json(nlohmann::json& j, const SavedPose& p) {
    j = nlohmann::json{
        {"name", p.name},
        {"sourceAnimIndex", p.sourceAnimIndex},
        {"sourceTime", p.sourceTime}
    };
    // nodePoses のシリアライズ（必要に応じて position/rotation/scale を展開して記録）
    for (const auto& np : p.nodePoses) {
        j["nodePoses"].push_back({
            {"pos", {np.position.x, np.position.y, np.position.z}},
            {"rot", {np.rotation.x, np.rotation.y, np.rotation.z, np.rotation.w}},
            {"scl", {np.scale.x, np.scale.y, np.scale.z}}
            });
    }
}

inline void from_json(const nlohmann::json& j, SavedPose& p) {
    p.name = j.value("name", "");
    p.sourceAnimIndex = j.value("sourceAnimIndex", 0);
    p.sourceTime = j.value("sourceTime", 0.0f);
    p.nodePoses.clear();
    if (j.contains("nodePoses")) {
        for (const auto& npj : j["nodePoses"]) {
            GltfModelCommponennt::NodePose np;
            auto pos = npj["pos"].get<std::vector<float>>();
            auto rot = npj["rot"].get<std::vector<float>>();
            auto scl = npj["scl"].get<std::vector<float>>();
            np.position = { pos[0], pos[1], pos[2] };
            np.rotation = { rot[0], rot[1], rot[2], rot[3] };
            np.scale = { scl[0], scl[1], scl[2] };
            p.nodePoses.push_back(np);
        }
    }
}

// モデルごとに「ポーズの秘術」を蓄える神殿のリソース
class PoseLibraryResource
{
public:
    std::string modelName; // 結びつくモデルの真名

    std::vector<SavedPose> poseLibrary;       // 収集されたポーズの書庫
    std::vector<CustomAnimation> customAnims; //　オリジナルアニメーション

    void Clear() {
        poseLibrary.clear();
        customAnims.clear();
        modelName = "";
    }
};