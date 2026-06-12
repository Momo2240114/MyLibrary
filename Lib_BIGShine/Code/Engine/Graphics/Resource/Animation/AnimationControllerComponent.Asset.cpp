// Serialize / Asset Save Load / JSON 変換
// AnimationControllerComponent の実装を役割ごとに分けたファイルです。
// クラス自体は分離せず、同じクラスのメンバ関数定義だけを別 cpp に置いています。
// そのため既存の参照関係や public/private の扱いはそのままです。

#include "AnimationControllerComponent.h"
#include "AnimSequenceAdapter.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "AnimationDataAsset.h"
#include "PoseLibraryResourceManager.h"
#include "IKMapComponent.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <unordered_map>

namespace ed = ax::NodeEditor;

namespace
{
    // 既存の保存先。既存アセットとの互換性を優先するためパスは据え置き。
    const std::string ASSET_DIR = "Data/Animation/StateData/";
    const std::string GRAPH_ASSET_DIR = "Data/Animation/GraphData/";
    const std::string STATE_ASSET_DIR = "Data/Animation/StateData/";

    constexpr const char* START_NODE_NAME = "__Start__";
    constexpr const char* ANY_NODE_NAME = "__Any__";

    std::string GetFullAssetPath(const std::string& assetName_)
    {
        std::string name_ = assetName_;
        if (name_.find(".json") == std::string::npos)
        {
            name_ += ".json";
        }

        return ASSET_DIR + assetName_ + "/" + name_;
    }

    std::string GetFullStateAssetPath(const std::string& assetName_)
    {
        std::string name_ = assetName_;
        if (name_.find(".json") == std::string::npos)
        {
            name_ += ".json";
        }
        return STATE_ASSET_DIR + name_;
    }

    std::string GetFullGraphAssetPath(const std::string& assetName_)
    {
        std::string name_ = assetName_;
        if (name_.find(".json") == std::string::npos)
        {
            name_ += ".json";
        }
        return GRAPH_ASSET_DIR + name_;
    }
}

void AnimationControllerComponent::Serialize(nlohmann::json& j) const
{
    j["AssetPath"] = m_assetFilePath;
    j["GraphAssetPath"] = m_graphAssetFilePath;
}

void AnimationControllerComponent::Deserialize(const nlohmann::json& j)
{
    m_assetFilePath = j.value("AssetPath", "");
    m_graphAssetFilePath = j.value("GraphAssetPath", "");

    if (!m_assetFilePath.empty())
    {
        LoadAsset(m_assetFilePath);
    }

    graphNodes.clear();
    graphTransitions.clear();

    if (!m_graphAssetFilePath.empty())
    {
        m_pendingGraphAssetPath = m_graphAssetFilePath;
        m_pendingGraphAssetLoad = true;
    }
    else
    {
        EnsureSpecialGraphNodes();
    }

    TryEnterStartState();
}

bool AnimationControllerComponent::SaveStateAsset(const std::string& stateName_)
{
    auto stateIt_ = states.find(stateName_);
    if (stateIt_ == states.end() || !stateIt_->second) return false;

    if (!std::filesystem::exists(STATE_ASSET_DIR))
    {
        std::filesystem::create_directories(STATE_ASSET_DIR);
    }

    nlohmann::json json_;
    json_["AssetType"] = "AnimationState";
    json_["AssetVersion"] = 1;
    json_["State"] = *stateIt_->second;

    std::ofstream file_(GetFullStateAssetPath(stateName_), std::ios::out | std::ios::trunc);
    if (!file_.is_open()) return false;

    file_ << json_.dump(4);
    return true;
}

bool AnimationControllerComponent::LoadStateAsset(const std::string& assetName_)
{
    if (assetName_.empty()) return false;

    std::ifstream file_(GetFullStateAssetPath(assetName_));
    if (!file_.is_open()) return false;

    nlohmann::json json_;
    file_ >> json_;

    AnimState state_;
    if (json_.contains("State"))
    {
        state_ = json_["State"].get<AnimState>();
    }
    else
    {
        state_ = json_.get<AnimState>();
    }

    if (state_.name.empty())
    {
        state_.name = std::filesystem::path(assetName_).stem().string();
    }

    const bool wasCurrent_ = currentState && currentState->name == state_.name;
    states[state_.name] = std::make_unique<AnimState>(state_);
    editingState = states[state_.name].get();

    RegisterStateAction(state_.name);
    EnsureGraphNodeForState(state_.name);

    if (wasCurrent_)
    {
        ChangeState(state_.name, 0.0f);
    }

    return true;
}


void AnimationControllerComponent::SaveGraphAsset(const std::string& assetName)
{
    if (assetName.empty()) return;

    if (!std::filesystem::exists(GRAPH_ASSET_DIR)) {
        std::filesystem::create_directories(GRAPH_ASSET_DIR);
    }

    nlohmann::json j;
    j["AssetPath"] = m_assetFilePath;
    j["GraphAssetPath"] = assetName;

    nlohmann::json nodesArray = nlohmann::json::array();
    for (auto& node : graphNodes)
    {
        nlohmann::json n;
        n["stateName"] = node.stateName;
        n["id"] = static_cast<uintptr_t>(node.id.Get());
        n["inputPin"] = static_cast<uintptr_t>(node.inputPin.Get());
        n["outputPin"] = static_cast<uintptr_t>(node.outputPin.Get());
        n["nodeType"] = static_cast<int>(node.type);
        n["scopeId"] = node.scopeId;
        n["childScopeId"] = node.childScopeId;

        ImVec2 pos = node.pos;
        const std::string nodeScope = node.scopeId.empty() ? std::string("Root") : node.scopeId;
        const std::string currentScope = m_currentGraphScopeId.empty() ? std::string("Root") : m_currentGraphScopeId;
        if (nodeEditorContext && nodeScope == currentScope)
        {
            ed::SetCurrentEditor(nodeEditorContext);
            pos = ed::GetNodePosition(node.id);
        }

        n["posX"] = pos.x;
        n["posY"] = pos.y;

        nodesArray.push_back(n);
    }
    j["GraphNodes"] = nodesArray;

    nlohmann::json transArray = nlohmann::json::array();
    for (auto& t : graphTransitions)
    {
        nlohmann::json tr;
        tr["from"] = t.fromStateName;
        tr["to"] = t.toStateName;
        tr["duration"] = t.duration;
        tr["logic"] = static_cast<int>(t.logic);
        tr["id"] = static_cast<uintptr_t>(t.id.Get());
        tr["scopeId"] = t.scopeId;

        nlohmann::json condsArray = nlohmann::json::array();
        for (auto& c : t.conditions)
        {
            nlohmann::json cj;
            cj["type"] = static_cast<int>(c.type);
            cj["param"] = c.parameterName;
            cj["op"] = static_cast<int>(c.op);
            cj["threshold"] = c.threshold;
            condsArray.push_back(cj);
        }

        tr["conditions"] = condsArray;
        transArray.push_back(tr);
    }
    j["GraphTransitions"] = transArray;

    std::ofstream file(GetFullGraphAssetPath(assetName));
    if (file.is_open()) {
        file << j.dump(4);
        m_graphAssetFilePath = assetName;
    }
}
void AnimationControllerComponent::LoadGraphAsset(const std::string& assetName)
{
    if (assetName.empty()) return;

    std::ifstream file(GetFullGraphAssetPath(assetName));
    if (!file.is_open()) return;

    nlohmann::json j;
    file >> j;

    m_graphAssetFilePath = assetName;
    m_currentGraphScopeId = "Root";
    m_graphScopeStack.clear();

    std::string stateAssetPath = j.value("AssetPath", "");
    if (!stateAssetPath.empty())
    {
        m_assetFilePath = stateAssetPath;
        LoadAsset(m_assetFilePath);
    }

    graphNodes.clear();
    graphTransitions.clear();

    uintptr_t maxId = 0;

    auto readId = [&](const nlohmann::json& src, const char* key, bool allowZero = false) -> uintptr_t
        {
            uintptr_t id = src.value(key, static_cast<uintptr_t>(0));
            if (id == 0 && !allowZero)
            {
                id = GetNextID();
            }

            if (id > maxId)
            {
                maxId = id;
            }

            return id;
        };

    if (j.contains("GraphNodes") && j["GraphNodes"].is_array())
    {
        for (auto& n : j["GraphNodes"])
        {
            AnimGraphNode node;
            node.stateName = n.value("stateName", "");
            node.type = static_cast<AnimGraphNodeType>(n.value("nodeType", static_cast<int>(AnimGraphNodeType::State)));
            node.scopeId = n.value("scopeId", "Root");
            node.childScopeId = n.value("childScopeId", "");

            node.id = ed::NodeId(readId(n, "id"));

            if (node.type == AnimGraphNodeType::Start || node.type == AnimGraphNodeType::Any ||
                node.stateName == START_NODE_NAME || node.stateName == ANY_NODE_NAME)
            {
                node.inputPin = ed::PinId(0);
                node.outputPin = ed::PinId(readId(n, "outputPin"));
            }
            else if (node.type == AnimGraphNodeType::Entry)
            {
                node.inputPin = ed::PinId(0);
                node.outputPin = ed::PinId(readId(n, "outputPin"));
            }
            else if (node.type == AnimGraphNodeType::Exit)
            {
                node.inputPin = ed::PinId(readId(n, "inputPin"));
                node.outputPin = ed::PinId(0);
            }
            else
            {
                node.inputPin = ed::PinId(readId(n, "inputPin"));
                node.outputPin = ed::PinId(readId(n, "outputPin"));
            }

            node.pos = ImVec2(n.value("posX", 0.0f), n.value("posY", 0.0f));

            graphNodes.push_back(node);
        }
    }

    if (j.contains("GraphTransitions") && j["GraphTransitions"].is_array())
    {
        for (auto& tr : j["GraphTransitions"])
        {
            AnimTransition transition;
            transition.fromStateName = tr.value("from", "");
            transition.toStateName = tr.value("to", "");
            transition.duration = tr.value("duration", 0.2f);
            transition.logic = static_cast<TransitionLogic>(tr.value("logic", static_cast<int>(TransitionLogic::And)));
            transition.id = ed::LinkId(readId(tr, "id"));
            transition.scopeId = tr.value("scopeId", "Root");

            if (tr.contains("conditions") && tr["conditions"].is_array())
            {
                for (auto& cj : tr["conditions"])
                {
                    AnimCondition cond;
                    cond.type = static_cast<ConditionType>(cj.value("type", static_cast<int>(ConditionType::Variable)));
                    cond.parameterName = cj.value("param", "");
                    cond.op = static_cast<ConditionOp>(cj.value("op", static_cast<int>(ConditionOp::Equal)));
                    cond.threshold = cj.value("threshold", 0.0f);

                    transition.conditions.push_back(cond);
                }
            }

            if (!transition.fromStateName.empty() && !transition.toStateName.empty())
            {
                graphTransitions.push_back(transition);
            }
        }
    }

    SetIDCounterFloor(maxId);
    EnsureSpecialGraphNodes();

    m_needsPositionRestore = true;
}

void AnimationControllerComponent::LoadAsset(const std::string& assetName)
{
    std::string fullPath = GetFullAssetPath(assetName);

    std::ifstream file(fullPath);
    if (!file.is_open()) return;

    nlohmann::json j;
    file >> j;

    AnimationDataAsset asset;
    asset.Deserialize(j);

    states.clear();
    for (auto& s : asset.states) {
        states[s.name] = std::make_unique<AnimState>(s);
    }

    m_assetFilePath = assetName;

    if (!asset.defaultStateName.empty() && states.count(asset.defaultStateName)) 
    {
        ChangeState(asset.defaultStateName, 0.0f);
    }
}


void AnimationControllerComponent::SaveAsset(const std::string& assetName)
{
    // ディレクトリが存在しない場合は作成する
    if (!std::filesystem::exists(ASSET_DIR)) {
        std::filesystem::create_directories(ASSET_DIR);
    }

    std::string fullPath = GetFullAssetPath(assetName);

    AnimationDataAsset asset;
    for (auto& [name, ptr] : states) {
        asset.states.push_back(*ptr);
    }
    if (currentState) asset.defaultStateName = currentState->name;

    nlohmann::json j;
    asset.Serialize(j);

    std::ofstream file(fullPath);
    if (file.is_open()) {
        file << j.dump(4);
        m_assetFilePath = assetName; // エディタ上での表示用に名前を保存
    }

}

    void to_json(nlohmann::json & j, const AnimEvent & e) {
        j = nlohmann::json{
            {"label", e.eventLabel},
            {"startTime", e.startTime},
            {"endTime", e.endTime},
            {"actionStart", e.startAction},
            {"actionUpdate", e.updateAction},
            {"actionEnd", e.endAction}
        };
    }

    void from_json(const nlohmann::json & j, AnimEvent & e) {
        e.eventLabel = j.value("label", "");
        e.startTime = j.value("startTime", 0.0f);
        e.endTime = j.value("endTime", 0.0f);
        e.startAction = j.value("actionStart", "");
        e.updateAction = j.value("actionUpdate", "");
        e.endAction = j.value("actionEnd", "");
        e.isFired = false;
    }


    void to_json(nlohmann::json & j, const AnimState & s) {
        j = nlohmann::json
        {
            {"sequencer", s.sequencerDataName},
            {"name", s.name},
            {"type", (int)s.type}, // enumをintとして保存
            {"speed", s.playbackSpeed},
            {"loop", s.isLoop},
            {"rootMotion", s.rootMotion},
            {"rootMotionScale", s.rootMotionScale},
            {"rootMotionScaleCurveName", s.rootMotionScaleCurveName},
            {"speedCurveName", s.speedCurveName},
            {"events", s.events},
            // Single用
            {"animationIndex", s.animationIndex},
            // Blend1D用
            {"animIndexA", s.animIndexA},
            {"animIndexB", s.animIndexB},
            {"blendParamName", s.blendParamName},
            // Custom用 【追加】
            {"customAnimIndex", s.customAnimIndex},
        };
    }

void from_json(const nlohmann::json& j, AnimState& s)
{
    s.sequencerDataName = j.value("sequencer", "");
    s.name = j.value("name", "");
    s.type = (AnimStateType)j.value("type", (int)AnimStateType::Single);
    s.playbackSpeed = j.value("speed", 1.0f);
    s.isLoop = j.value("loop", true);
    s.rootMotionScale = j.value("rootMotionScale", 1.0f);
    s.rootMotionScaleCurveName = j.value("rootMotionScaleCurveName", "");
    s.speedCurveName = j.value("speedCurveName", "");

    // Single用復元
    s.animationIndex = j.value("animationIndex", -1);

    // Blend1D用復元
    s.animIndexA = j.value("animIndexA", -1);
    s.animIndexB = j.value("animIndexB", -1);
    s.blendParamName = j.value("blendParamName", "BlendAlpha");

    // Custom用 【追加】
    s.customAnimIndex = j.value("customAnimIndex", -1);
    if (j.contains("rootMotion") && j["rootMotion"].is_object())
    {
        s.rootMotion = j["rootMotion"].get<RootMotionSettings>();
    }
    else
    {
        s.rootMotion = RootMotionSettings{};
    }
    if (j.contains("events") && j["events"].is_array()) {
        s.events = j.at("events").get<std::vector<AnimEvent>>();
    }
    else
    {
        s.events.clear();
    }
}

