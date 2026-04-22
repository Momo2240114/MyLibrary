#pragma once

#include <vector>
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "EventTrack.h"
#include "CurveData.h"



struct AnimationLayer
{
    int animationIndex = -1;
    float currentTime = 0.0f;
    float weight = 1.0f;       // レイヤー合成時の重み
    bool loop = true;
    float playbackSpeed = 1.0f;       // 個別再生速度

    bool isCustomMode = false;      // オリジナルアニメを使うか
    int customAnimIndex = -1;       // どの自作アニメか

    enum class BlendMode { Override, Additive };
    BlendMode blendMode = BlendMode::Override;
    // このレイヤーが操作するノードだけ true
    std::vector<bool> nodeMask;
    // このレイヤー専用のイベントトラック
    EventTrack eventTrack;
    // 追加: 選択中のイベントインデックス
    int selectedEvent = -1;
    std::shared_ptr<CurveData> speedCurve = nullptr; // 追加
    float internalTimer = 0.0f; // カーブの横軸用のタイマー

    // 補完（クロスフェード）用に追加
    int lastActiveAnimIndex = -1;  // 前フレームのアニメーションID
    bool isTransitioning = false;  // 遷移中フラグ
    float transitionTimer = 0.0f;  // 遷移経過時間
    float transitionDuration = 0.01f; // 補完にかける時間（秒）

    // 遷移開始時の全ノードのポーズを保存
    std::vector<GltfModelCommponennt::NodePose> startPoses;

    //ルートモーションをするかどうかのフラグ
    bool m_IsRootMotion = false;

    float m_OldTime = 0.0f;
};

class AnimationController : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name = "AnimationController";
        return name;
    }

    void Start() override;


    void Update(float deltaTime) override;
   
    //今までのアニメーション
    void Crossfade(AnimationLayer& activeLayer, float deltaTime, std::vector<GltfModelCommponennt::NodePose>& targetPoses, int nodeCount);

    void RootMotion(AnimationLayer& activeLayer, float deltaTime, std::vector<GltfModelCommponennt::NodePose>& targetPoses, int nodeCount);

    void ComputeCustomPose(int animIndex, float time, bool loop, std::vector<GltfModelCommponennt::NodePose>& outPoses);

    void Play(int layerIndex, int animIndex, bool loop_ = true)
    {
        if (layerIndex < 0 || layerIndex >= (int)layers.size()) return;
        layers[layerIndex].animationIndex = animIndex;
        layers[layerIndex].currentTime = 0.0f;
        layers[layerIndex].loop = loop_;
        globalSelectedLayer = layerIndex;
    }

    void Play(int layerIndex, float blendTime = 0.5f)
    {
        if (layerIndex < 0 || layerIndex >= (int)layers.size()) return;

        auto& layer = layers[layerIndex];

        // 1. 現在のモデルのポーズを「補間開始ポーズ」として保存
        if (model) {
            layer.startPoses.resize(model->GetNodes().size());
            model->GetNodePoses(layer.startPoses);
        }

        // 2. 補間ステートのリセット
        layer.isTransitioning = true;
        layer.transitionTimer = 0.0f;
        layer.transitionDuration = blendTime; // 補間時間を指定

        // 3. 再生状態の設定
        animationPlaying = true;
        layer.currentTime = 0.0f;
        globalSelectedLayer = layerIndex;

        //1フレーム前のアニメーション時間
        layer.m_OldTime = 0.0f;

        if (layerIndex == 4)
        {
            layer.m_IsRootMotion = true;
        }
    }


    bool IsAnimationFinished();
    bool IsActionWindowOpen(const std::string& actionName)const;

    void Stop(int layerIndex)
    {
        if (layerIndex < 0 || layerIndex >= (int)layers.size()) return;
        layers[layerIndex].currentTime = 0.0f;
        layers[layerIndex].animationIndex = -1;
    }

    void SetPlayBackSpeed(float s)
    {
        playbackSpeed = s;
    }

    void Gui() override;

    // -------------------------------
    // JSON書き出し
    void ExportJson(const std::string& filename);

    // -------------------------------
    // JSON読み込み
    void ImportJson(const std::string& filename);

    // セーブ用：引数の json オブジェクトに自分の変数を書き込む
    void Serialize(nlohmann::json& j) const override;
    // ロード用：引数の json オブジェクトから自分の変数を読み出す
    void Deserialize(const nlohmann::json& j)override;
private:
    // staticにすることで、const関数内からでも呼び出せるようになります
    static void LayerToJson(nlohmann::json& j, const AnimationLayer& layer);
    static void LayerFromJson(const nlohmann::json& j, AnimationLayer& layer);
private:
    // AnimationControllerクラスのprivate/publicに追加
    struct PoseKeyframe { 
        float time;
        int poseIndex;

        // 【追加】どのソースから来たかを記録
        int sourceAnimIdx;   
        float sourceTime;
    };
    struct CustomAnimation
    { 
        std::string name; 
        float duration; 
        std::vector<PoseKeyframe> keyframes; 
    };
    struct SavedPose 
    { 
        std::string name;
        std::vector<GltfModelCommponennt::NodePose> nodePoses;
        int sourceAnimIndex = 0;
        float sourceTime = 0.0f;
    };

public:
    std::vector<AnimationLayer> layers;

private:
    std::shared_ptr<GltfModelCommponennt> model;
    std::vector<GltfModelCommponennt::NodePose> nodePoses;
    float playbackSpeed = 1.0f;
    float animationBlendSeconds = 1.0f;
    bool animationPlaying = false;

    float currentTime = 0.0f;   // コントローラ全体の時間

    inline static int selectedPoseIdx = -1;  // 選択中のポーズ
    inline static int selectedNodeIdx = -1;  // 選択中のノード（追加）
    int selectedLayer = -1;               // レイヤー選択
    int globalSelectedLayer = 0;
    std::vector<int> selectedEventPerLayer; // レイヤーごとの選択イベント保持
public:
    std::vector<CustomAnimation> customAnims;
    std::vector<SavedPose> poseLibrary;
private:
    std::string GetSavePath(const std::string& filename); // モデル名に基づいたパス解決用
};