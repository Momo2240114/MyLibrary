#pragma once
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include "Engine/Graphics/Resource/GlTFModel.h" // GltfModelCommponenntの型定義のため

// ポーズを構成するキーフレームの記憶
struct PoseKeyframe 
{
    // CustomAnimation内の再生時間。
    float time = 0.0f;

    // poseLibrary内のどのSavedPoseを参照するか。
    int poseIndex = -1;

    // 元アニメーションから切り出した時の参照情報。再編集時の手がかりにする。
    int sourceAnimIdx = -1;
    float sourceTime = 0.0f;

    // 0: Linear, 1: SmoothStep, 2: EaseIn, 3: EaseOut, 4: EaseInOut, 5: Hold
    int interpolation = 1;

    //cerealやjsonへの書記と解読
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(PoseKeyframe, time, poseIndex, sourceAnimIdx, sourceTime, interpolation)
};

struct NaturalMotionSettings
{
    // 保存ポーズだけでは固く見える時に、ノードへ小さな揺れを足す設定。
    bool enabled = false;

    // 空なら全ノード、文字列があれば名前に一致するノードだけを揺らす。
    std::string nodeFilter = "";
    float rotationAmplitudeDeg = 2.0f;
    float positionAmplitude = 0.0f;
    float frequency = 1.5f;
    float phase = 0.0f;
    float blend = 0.35f;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(NaturalMotionSettings, enabled, nodeFilter, rotationAmplitudeDeg, positionAmplitude, frequency, phase, blend)
};

// ポーズを繋ぎ合わせて創り出す、新たなアニメーション
struct CustomAnimation {
    // Editor上に表示するCustom Animation名。
    std::string name;

    // ポーズ列全体の再生時間。
    float duration = 1.0f;

    // 下地にする既存Animation。-1ならポーズ補間だけで作る。
    int baseAnimationIndex = -1;
    float baseAnimationStartTime = 0.0f;
    float baseAnimationSpeed = 1.0f;

    // baseAnimationとpose keyframeの混ざり具合。
    float poseBlend = 1.0f;

    // 時間順に並ぶポーズキー。隣同士をinterpolation設定で補間する。
    std::vector<PoseKeyframe> keyframes;
    NaturalMotionSettings naturalMotion;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CustomAnimation, name, duration, baseAnimationIndex, baseAnimationStartTime, baseAnimationSpeed, poseBlend, keyframes, naturalMotion)
};

// 切りとられた一瞬のポーズ（姿勢）の記憶
struct SavedPose {
    // PoseLibrary内で識別する名前。
    std::string name;

    // モデル全ノードぶんの姿勢。
    std::vector<GltfModelCommponennt::NodePose> nodePoses;

    // どの元アニメーションの何秒から採取したかを残す。
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
    // DirectX型はそのままJSON化できないので、配列へ分解して保存する。
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
        // 保存時に分解したposition/rotation/scale配列をNodePoseへ戻す。
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

    // 1フレーム姿勢の保存棚と、それらを組み合わせたCustom Animation。
    std::vector<SavedPose> poseLibrary;       // 収集されたポーズの書庫
    std::vector<CustomAnimation> customAnims; //　オリジナルアニメーション

    void Clear() {
        // モデル切り替え時に、古いポーズやCustom Animationが混ざらないよう空にする。
        poseLibrary.clear();
        customAnims.clear();
        modelName = "";
    }
};
