#include "PrecompiledHeader.h"

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
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Combat/AttackTelegraphRuntime.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace ed = ax::NodeEditor;

namespace
{
    const std::string ASSET_DIR = "Data/Animation/StateData/";
    const std::string GRAPH_ASSET_DIR = "Data/Animation/GraphData/";
    const std::string STATE_ASSET_DIR = "Data/Animation/StateData/";

    constexpr const char* START_NODE_NAME = "__Start__";
    constexpr const char* ANY_NODE_NAME = "__Any__";

    bool IsValidGraphPosition(const ImVec2& position_)
    {
        constexpr float positionLimit_ = 100000.0f;
        return std::isfinite(position_.x) &&
            std::isfinite(position_.y) &&
            std::abs(position_.x) < positionLimit_ &&
            std::abs(position_.y) < positionLimit_;
    }

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
        std::filesystem::path path_(assetName_);
        if (path_.extension() != ".json")
        {
            path_ += ".json";
        }

        if (path_.has_parent_path())
        {
            return path_.lexically_normal().string();
        }
        return (std::filesystem::path(STATE_ASSET_DIR) / path_).lexically_normal().string();
    }

    std::string GetFullGraphAssetPath(const std::string& assetName_)
    {
        std::filesystem::path path_(assetName_);
        if (path_.extension() != ".json")
        {
            path_ += ".json";
        }

        if (path_.has_parent_path())
        {
            return path_.lexically_normal().string();
        }
        return (std::filesystem::path(GRAPH_ASSET_DIR) / path_).lexically_normal().string();
    }

    AnimationDataAsset BuildAnimationDataAsset(
        const std::unordered_map<std::string, std::unique_ptr<AnimState>>& states_,
        const std::string& defaultStateName_)
    {
        AnimationDataAsset asset_;
        for (const auto& [name_, state_] : states_)
        {
            if (state_)
            {
                asset_.states.push_back(*state_);
            }
        }

        if (!defaultStateName_.empty() && states_.count(defaultStateName_))
        {
            asset_.defaultStateName = defaultStateName_;
        }

        return asset_;
    }

    std::string ResolveGraphStartStateName(
        const std::vector<AnimTransition>& graphTransitions_,
        const std::unordered_map<std::string, std::unique_ptr<AnimState>>& states_)
    {
        for (const AnimTransition& transition_ : graphTransitions_)
        {
            if (transition_.fromStateName == START_NODE_NAME &&
                states_.count(transition_.toStateName))
            {
                return transition_.toStateName;
            }
        }

        return "";
    }

    void EnsureUniqueGraphElementIds(
        std::vector<AnimGraphNode>& graphNodes_,
        std::vector<AnimTransition>& graphTransitions_,
        uintptr_t& maxId_)
    {
        BS_PROFILE_SCOPE("AnimationGraphAsset::EnsureUniqueGraphElementIds");

        std::unordered_set<uintptr_t> usedIds_;
        auto getNextUniqueId_ = [&]() -> uintptr_t
            {
                do
                {
                    ++maxId_;
                } while (maxId_ == 0 || usedIds_.count(maxId_) > 0);
                usedIds_.insert(maxId_);
                return maxId_;
            };

        auto fixId_ = [&](uintptr_t id_) -> uintptr_t
            {
                if (id_ == 0)
            {
                return 0;
            }
            if (usedIds_.insert(id_).second)
            {
                return id_;
            }
            return getNextUniqueId_();
            };

        for (AnimGraphNode& node_ : graphNodes_)
        {
            node_.id = ed::NodeId(fixId_(static_cast<uintptr_t>(node_.id.Get())));
            node_.inputPin = ed::PinId(fixId_(static_cast<uintptr_t>(node_.inputPin.Get())));
            node_.outputPin = ed::PinId(fixId_(static_cast<uintptr_t>(node_.outputPin.Get())));
        }

        for (AnimTransition& transition_ : graphTransitions_)
        {
            transition_.id = ed::LinkId(fixId_(static_cast<uintptr_t>(transition_.id.Get())));
        }
    }
}

void AnimationControllerComponent::Serialize(nlohmann::json& json_) const
{
    json_["AssetPath"] = assetFilePath;
    json_["GraphAssetPath"] = graphAssetFilePath;
    json_["UseTransitionGraph"] = useTransitionGraph;
    json_["StateAssets"] = nlohmann::json::object();
    for (const auto& [stateName_, path_] : stateAssetPaths)
    {
        if (!path_.empty())
        {
            json_["StateAssets"][stateName_] = path_;
        }
    }

    std::string defaultStateName_ = ResolveGraphStartStateName(graphTransitions, states);
    if (defaultStateName_.empty() && currentState)
    {
        defaultStateName_ = currentState->name;
    }
    AnimationDataAsset asset_ = BuildAnimationDataAsset(states, defaultStateName_);
    nlohmann::json animationData_;
    asset_.Serialize(animationData_);
    json_["AnimationData"] = animationData_;
}

void AnimationControllerComponent::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("AnimationControllerComponent::Deserialize");

    assetFilePath = json_.value("AssetPath", "");
    graphAssetFilePath = json_.value("GraphAssetPath", "");
    pendingGraphAssetPath = graphAssetFilePath;
    pendingGraphAssetLoad = !pendingGraphAssetPath.empty();
    useTransitionGraph = true;
    stateAssetPaths.clear();
    pendingStateAssetPaths.clear();
    std::string defaultStateName_;

    if (!assetFilePath.empty())
    {
        BS_PROFILE_SCOPE("Animation Deserialize::LoadAsset");
        LoadAsset(assetFilePath);
    }

    if (json_.contains("AnimationData") && json_["AnimationData"].is_object())
    {
        AnimationDataAsset asset_;
        {
            BS_PROFILE_SCOPE("Animation Deserialize::AnimationDataAsset");
            asset_.Deserialize(json_["AnimationData"]);
        }

        animator = ModelAnimator{};
        currentState = nullptr;
        editingState = nullptr;
        states.clear();
        for (const AnimState& state_ : asset_.states)
        {
            states[state_.name] = std::make_unique<AnimState>(state_);
        }
        MarkStateGuiCacheDirty();
        MarkActionCategoryCacheDirty();
        MarkSequenceAdapterDirty();
        MarkAnimationMaskCacheDirty();

        defaultStateName_ = asset_.defaultStateName;
    }

    if (json_.contains("StateAssets") && json_["StateAssets"].is_object())
    {
        for (const auto& [stateName_, pathJson_] : json_["StateAssets"].items())
        {
            if (pathJson_.is_string())
            {
                pendingStateAssetPaths[stateName_] = pathJson_.get<std::string>();
            }
        }
    }

    if (!defaultStateName_.empty() && states.count(defaultStateName_))
    {
        ChangeState(defaultStateName_, 0.0f);
    }
    graphNodes.clear();
    graphTransitions.clear();

    if (!pendingGraphAssetLoad)
    {
        ApplyPendingStateAssets();
        EnsureSpecialGraphNodes();
    }

    TryEnterStartState();
}

bool AnimationControllerComponent::SaveStateAsset(
    const std::string& stateName_,
    const std::string& assetPath_)
{
    auto stateIt_ = states.find(stateName_);
    if (stateIt_ == states.end() || !stateIt_->second)
    {
        return false;
    }

    if (!std::filesystem::exists(STATE_ASSET_DIR))
    {
        std::filesystem::create_directories(STATE_ASSET_DIR);
    }

    nlohmann::json json_;
    json_["AssetType"] = "AnimationState";
    json_["AssetVersion"] = 2;
    json_["State"] = *stateIt_->second;

    const std::string path_ = assetPath_.empty()
        ? BuildDefaultStateAssetPath(stateName_)
        : GetFullStateAssetPath(assetPath_);
    const std::filesystem::path parentPath_ = std::filesystem::path(path_).parent_path();
    if (!parentPath_.empty())
    {
        std::filesystem::create_directories(parentPath_);
    }

    std::ofstream file_(path_, std::ios::out | std::ios::trunc);
    if (!file_.is_open())
    {
        return false;
    }

    file_ << json_.dump(4);
    stateAssetPaths[stateName_] = path_;
    if (!graphAssetFilePath.empty())
    {
        SaveGraphAsset(graphAssetFilePath);
    }
    return true;
}

std::string AnimationControllerComponent::BuildDefaultStateAssetPath(
    const std::string& stateName_) const
{
    std::filesystem::path ownerPath_(assetFilePath);
    if (ownerPath_.empty())
    {
        ownerPath_ = std::filesystem::path(graphAssetFilePath);
    }

    std::string ownerName_ = ownerPath_.stem().string();
    if (ownerName_.empty())
    {
        ownerName_ = "Shared";
    }

    std::filesystem::path statePath_(stateName_);
    if (statePath_.extension() != ".json")
    {
        statePath_ += ".json";
    }

    return (std::filesystem::path(STATE_ASSET_DIR) /
        ownerName_ /
        "States" /
        statePath_.filename()).lexically_normal().string();
}

bool AnimationControllerComponent::LoadMissingStateAsset(const std::string& stateName_, const std::string& assetName_)
{
    BS_PROFILE_SCOPE("AnimationControllerComponent::LoadMissingStateAsset");
    if (stateName_.empty())
    {
        return false;
    }
    const auto existing_ = states.find(stateName_);
    if (existing_ != states.end() && existing_->second)
    {
        return true;
    }
    // 外部からprivateの読込処理へ直接アクセスさせず、追加時のAction登録も共有する。
    if (!LoadStateAsset(assetName_))
    {
        return false;
    }
    const auto loaded_ = states.find(stateName_);
    return loaded_ != states.end() && loaded_->second != nullptr;
}

bool AnimationControllerComponent::LoadStateAsset(const std::string& assetName_)
{
    BS_PROFILE_SCOPE("AnimationControllerComponent::LoadStateAsset");

    if (assetName_.empty())
    {
        return false;
    }

    const std::string path_ = GetFullStateAssetPath(assetName_);
    std::ifstream file_(path_);
    if (!file_.is_open())
    {
        return false;
    }

    AnimState state_;
    try
    {
        nlohmann::json json_;
        {
            BS_PROFILE_SCOPE("AnimationStateAsset::ReadJson");
            file_ >> json_;
        }

        if (json_.contains("AssetType") &&
            json_.value("AssetType", "") != "AnimationState")
        {
            return false;
        }

        if (json_.contains("State"))
        {
            BS_PROFILE_SCOPE("AnimationStateAsset::DeserializeState");
            state_ = json_["State"].get<AnimState>();
        }
        else
        {
            if (!json_.contains("name") || !json_.contains("type"))
            {
                return false;
            }
            state_ = json_.get<AnimState>();
        }
    }
    catch (const nlohmann::json::exception&)
    {
        return false;
    }
    if (state_.name.empty())
    {
        state_.name = std::filesystem::path(assetName_).stem().string();
    }

    auto stateIterator_ = states.find(state_.name);
    if (stateIterator_ != states.end() && stateIterator_->second)
    {
        // Animatorが保持しているStateポインタを無効化しないよう、実体を維持して内容だけ更新する。
        *stateIterator_->second = state_;
    }
    else
    {
        states[state_.name] = std::make_unique<AnimState>(state_);
    }
    stateAssetPaths[state_.name] = path_;
    editingState = states[state_.name].get();

    RegisterStateAction(state_.name);

    return true;
}

void AnimationControllerComponent::ApplyPendingStateAssets()
{
    if (pendingStateAssetPaths.empty())
    {
        return;
    }

    const auto pendingAssets_ = std::move(pendingStateAssetPaths);
    pendingStateAssetPaths.clear();
    for (const auto& [stateName_, path_] : pendingAssets_)
    {
        (void)stateName_;
        if (!path_.empty())
        {
            LoadStateAsset(path_);
        }
    }
}


void AnimationControllerComponent::SaveGraphAsset(const std::string& assetName_)
{
    if (assetName_.empty())
    {
        return;
    }

    if (!std::filesystem::exists(GRAPH_ASSET_DIR)) {
        std::filesystem::create_directories(GRAPH_ASSET_DIR);
    }

    nlohmann::json j;
    j["AssetPath"] = assetFilePath;
    j["GraphAssetPath"] = assetName_;
    j["UseTransitionGraph"] = useTransitionGraph;
    j["StateAssets"] = nlohmann::json::object();
    for (const auto& [stateName_, path_] : stateAssetPaths)
    {
        if (!path_.empty())
        {
            j["StateAssets"][stateName_] = path_;
        }
    }

    std::string defaultStateName_ = ResolveGraphStartStateName(graphTransitions, states);
    if (defaultStateName_.empty() && currentState)
    {
        defaultStateName_ = currentState->name;
    }
    AnimationDataAsset asset_ = BuildAnimationDataAsset(states, defaultStateName_);
    nlohmann::json animationData_;
    asset_.Serialize(animationData_);
    j["AnimationData"] = animationData_;

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
        const std::string currentScope = currentGraphScopeId.empty() ? std::string("Root") : currentGraphScopeId;
        if (nodeEditorContext && nodeScope == currentScope)
        {
            ed::SetCurrentEditor(nodeEditorContext);
            const ImVec2 editorPosition_ = ed::GetNodePosition(node.id);
            if (IsValidGraphPosition(editorPosition_))
            {
                pos = editorPosition_;
                node.pos = editorPosition_;
            }
        }
        if (!IsValidGraphPosition(pos))
        {
            pos = ImVec2(0.0f, 0.0f);
            node.pos = pos;
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
        tr["BlackboardRequestRoute"] = t.blackboardRequestRoute;
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

    std::ofstream file(GetFullGraphAssetPath(assetName_));
    if (file.is_open()) {
        file << j.dump(4);
        graphAssetFilePath = assetName_;
    }
}

bool AnimationControllerComponent::LoadGraphAsset(const std::string& assetName_)
{
    BS_PROFILE_SCOPE("AnimationControllerComponent::LoadGraphAsset");

    if (assetName_.empty())
    {
        return false;
    }

    std::ifstream file(GetFullGraphAssetPath(assetName_));
    if (!file.is_open())
    {
        return false;
    }

    nlohmann::json j;
    try
    {
        {
            BS_PROFILE_SCOPE("AnimationGraphAsset::ReadJson");
            file >> j;
        }
    }
    catch (const nlohmann::json::exception&)
    {
        return false;
    }
    if (!j.is_object() ||
        !j.contains("GraphNodes") || !j["GraphNodes"].is_array() ||
        !j.contains("GraphTransitions") || !j["GraphTransitions"].is_array())
    {
        return false;
    }

    graphAssetFilePath = assetName_;
    currentGraphScopeId = "Root";
    graphScopeStack.clear();
    stateAssetPaths.clear();

    std::string stateAssetPath = j.value("AssetPath", "");
    std::string loadedDefaultStateName_;
    if (j.contains("AnimationData") && j["AnimationData"].is_object())
    {
        AnimationDataAsset asset;
        {
            BS_PROFILE_SCOPE("AnimationGraphAsset::AnimationDataAsset");
            asset.Deserialize(j["AnimationData"]);
        }

        animator = ModelAnimator{};
        currentState = nullptr;
        editingState = nullptr;
        states.clear();
        for (auto& s : asset.states) {
            states[s.name] = std::make_unique<AnimState>(s);
        }
        MarkStateGuiCacheDirty();
        MarkActionCategoryCacheDirty();
        MarkSequenceAdapterDirty();
        MarkAnimationMaskCacheDirty();

        assetFilePath = stateAssetPath;
        loadedDefaultStateName_ = asset.defaultStateName;

    }
    else if (!stateAssetPath.empty())
    {
        assetFilePath = stateAssetPath;
        LoadAsset(assetFilePath);
    }

    useTransitionGraph = true;

    graphNodes.clear();
    graphTransitions.clear();

    uintptr_t maxId = 0;

    auto readId = [&](const nlohmann::json& src, const char* key, bool allowZero = false) -> uintptr_t
        {
            uintptr_t id = src.value(key, static_cast<uintptr_t>(0));
            if (id == 0 && !allowZero)
            {
                id = GetNextId();
            }

            if (id > maxId)
            {
                maxId = id;
            }

            return id;
        };

    if (j.contains("GraphNodes") && j["GraphNodes"].is_array())
    {
        std::unordered_map<std::string, int> scopeNodeCounts_;
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
            if (!IsValidGraphPosition(node.pos))
            {
                const std::string scopeName_ = node.scopeId.empty() ? "Root" : node.scopeId;
                const int nodeIndex_ = scopeNodeCounts_[scopeName_]++;
                const int column_ = nodeIndex_ % 4;
                const int row_ = nodeIndex_ / 4;
                node.pos = ImVec2(
                    -480.0f + static_cast<float>(column_) * 260.0f,
                    -180.0f + static_cast<float>(row_) * 180.0f);
            }

            graphNodes.push_back(node);
        }
    }

    if (j.contains("GraphTransitions") && j["GraphTransitions"].is_array())
    {
        for (auto& tr : j["GraphTransitions"])
        {
            AnimTransition transition;
            transition.blackboardRequestRoute = tr.value("BlackboardRequestRoute", false);
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

    EnsureUniqueGraphElementIds(graphNodes, graphTransitions, maxId);
    SetIdCounterFloor(maxId);
    EnsureSpecialGraphNodes();

    if (j.contains("StateAssets") && j["StateAssets"].is_object())
    {
        for (const auto& [stateName_, pathJson_] : j["StateAssets"].items())
        {
            (void)stateName_;
            if (pathJson_.is_string())
            {
                LoadStateAsset(pathJson_.get<std::string>());
            }
        }
    }

    if (nodeEditorContext)
    {
        ed::DestroyEditor(nodeEditorContext);
        nodeEditorContext = nullptr;
    }
    EnsureNodeEditorContext();
    needsPositionRestore = true;
    TryEnterStartState();
    if (!currentState && !loadedDefaultStateName_.empty() && states.count(loadedDefaultStateName_))
    {
        ChangeState(loadedDefaultStateName_, 0.0f);
    }
    return true;
}

void AnimationControllerComponent::LoadAsset(const std::string& assetName_)
{
    BS_PROFILE_SCOPE("AnimationControllerComponent::LoadAsset");

    std::string fullPath = GetFullAssetPath(assetName_);

    std::ifstream file(fullPath);
    if (!file.is_open())
    {
        return;
    }

    nlohmann::json j;
    {
        BS_PROFILE_SCOPE("AnimationAsset::ReadJson");
        file >> j;
    }

    AnimationDataAsset asset;
    {
        BS_PROFILE_SCOPE("AnimationAsset::DeserializeData");
        asset.Deserialize(j);
    }

    animator = ModelAnimator{};
    currentState = nullptr;
    editingState = nullptr;
    states.clear();
    for (auto& s : asset.states) {
        states[s.name] = std::make_unique<AnimState>(s);
    }
    MarkStateGuiCacheDirty();
    MarkActionCategoryCacheDirty();
    MarkSequenceAdapterDirty();
    MarkAnimationMaskCacheDirty();

    assetFilePath = assetName_;

    if (!asset.defaultStateName.empty() && states.count(asset.defaultStateName))
    {
        ChangeState(asset.defaultStateName, 0.0f);
    }
}


void AnimationControllerComponent::SaveAsset(const std::string& assetName_)
{
    if (assetName_.empty())
    {
        return;
    }

    std::string fullPath = GetFullAssetPath(assetName_);
    std::filesystem::create_directories(std::filesystem::path(fullPath).parent_path());

    std::string defaultStateName_ = ResolveGraphStartStateName(graphTransitions, states);
    if (defaultStateName_.empty() && currentState)
    {
        defaultStateName_ = currentState->name;
    }
    AnimationDataAsset asset = BuildAnimationDataAsset(states, defaultStateName_);

    nlohmann::json j;
    asset.Serialize(j);

    std::ofstream file(fullPath);
    if (file.is_open()) {
        file << j.dump(4);
        assetFilePath = assetName_;
    }
}

    void to_json(nlohmann::json& json_, const AnimBlackboardWrite& write_)
    {
        json_ = nlohmann::json{
            {"category", write_.category},
            {"key", write_.key},
            {"type", static_cast<int>(write_.type)},
            {"floatValue", write_.floatValue},
            {"intValue", write_.intValue},
            {"boolValue", write_.boolValue},
            {"stringValue", write_.stringValue}
        };
    }

    void from_json(const nlohmann::json& json_, AnimBlackboardWrite& write_)
    {
        write_.category = json_.value("category", std::string("Attack"));
        write_.key = json_.value("key", std::string("Damage"));
        const int typeIndex_ = (std::clamp)(json_.value("type", 0), 0, 3);
        write_.type = static_cast<ANIM_BLACKBOARD_VALUE_TYPE>(typeIndex_);
        write_.floatValue = json_.value("floatValue", 0.0f);
        write_.intValue = json_.value("intValue", 0);
        write_.boolValue = json_.value("boolValue", false);
        write_.stringValue = json_.value("stringValue", std::string{});
    }

    void to_json(nlohmann::json& json_, const AnimSequenceClip& clip_)
    {
        json_ = nlohmann::json{
            {"stateName", clip_.stateName},
            {"animationIndex", clip_.animationIndex},
            {"startTime", clip_.startTime},
            {"endTime", clip_.endTime},
            {"clipStartTime", clip_.clipStartTime},
            {"playbackSpeed", clip_.playbackSpeed},
            {"fitToRange", clip_.fitToRange}
        };
    }

    void from_json(const nlohmann::json& json_, AnimSequenceClip& clip_)
    {
        clip_.stateName = json_.value("stateName", std::string{});
        clip_.animationIndex = json_.value("animationIndex", -1);
        clip_.startTime = (std::max)(json_.value("startTime", 0.0f), 0.0f);
        clip_.endTime = (std::max)(json_.value("endTime", clip_.startTime + 0.1f), clip_.startTime);
        clip_.clipStartTime = (std::max)(json_.value("clipStartTime", 0.0f), 0.0f);
        clip_.playbackSpeed = (std::max)(json_.value("playbackSpeed", 1.0f), 0.01f);
        clip_.fitToRange = json_.value("fitToRange", true);
    }

    void to_json(nlohmann::json & j, const AnimEvent & e) {
        j = nlohmann::json{
            {"label", e.eventLabel},
            {"startTime", e.startTime},
            {"endTime", e.endTime},
            {"actionStart", e.startAction},
            {"actionUpdate", e.updateAction},
            {"actionEnd", e.endAction},
            {"overridePlaybackSpeed", e.overridePlaybackSpeed},
            {"playbackSpeed", e.playbackSpeed},
            {"callEndOnStateExit", e.callEndOnStateExit},
            {"blackboardWrites", e.blackboardWrites},
            {"eventTemplateType",
                e.eventTemplateType == ANIMATION_EVENT_TEMPLATE_TYPE::ATTACK_TEMPLATE ? "ATTACK_TEMPLATE" :
                e.eventTemplateType == ANIMATION_EVENT_TEMPLATE_TYPE::TRAIL_TEMPLATE ? "TRAIL_TEMPLATE" :
                e.eventTemplateType == ANIMATION_EVENT_TEMPLATE_TYPE::VFX_TEMPLATE ? "VFX_TEMPLATE" :
                e.eventTemplateType == ANIMATION_EVENT_TEMPLATE_TYPE::SOUND_TEMPLATE ? "SOUND_TEMPLATE" : "NONE"},
            {"attackTemplateId", e.attackTemplateId},
            {"telegraph", AttackTelegraphRuntime::SerializeSettings(e.telegraph)},
            {"telegraphHitStart", e.telegraphHitStart},
            {"ownerTarget", e.ownerTarget},
            {"meshVFXAssetPath", e.meshVFXAssetPath},
            {"attachNodeName", e.attachNodeName},
            {"localPosition", { e.localPosition.x, e.localPosition.y, e.localPosition.z }},
            {"localRotation", { e.localRotation.x, e.localRotation.y, e.localRotation.z }},
            {"localScale", { e.localScale.x, e.localScale.y, e.localScale.z }},
            {"meshVFXLifeTime", e.meshVFXLifeTime},
            {"meshVFXPlaybackSpeed", e.meshVFXPlaybackSpeed},
            {"attachToOwner", e.attachToOwner},
            {"mirrorMeshVFX", e.mirrorMeshVFX}
        };
    }

    void from_json(const nlohmann::json & j, AnimEvent & e) {
        e.eventLabel = j.value("label", "");
        e.startTime = j.value("startTime", 0.0f);
        e.endTime = j.value("endTime", 0.0f);
        e.startAction = j.value("actionStart", "");
        e.updateAction = j.value("actionUpdate", "");
        e.endAction = j.value("actionEnd", "");
        e.overridePlaybackSpeed = j.value("overridePlaybackSpeed", false);
        e.playbackSpeed = j.value("playbackSpeed", 1.0f);
        e.callEndOnStateExit = j.value("callEndOnStateExit", false);
        e.blackboardWrites = j.value(
            "blackboardWrites",
            std::vector<AnimBlackboardWrite>{});
        e.eventTemplateType = ANIMATION_EVENT_TEMPLATE_TYPE::NONE;
        if (j.contains("eventTemplateType"))
        {
            if (j["eventTemplateType"].is_string())
            {
                const std::string eventTemplateType_ = j["eventTemplateType"].get<std::string>();
                if (eventTemplateType_ == "ATTACK_TEMPLATE")
            {
                e.eventTemplateType = ANIMATION_EVENT_TEMPLATE_TYPE::ATTACK_TEMPLATE;
            }
            else if (eventTemplateType_ == "TRAIL_TEMPLATE")
            {
                e.eventTemplateType = ANIMATION_EVENT_TEMPLATE_TYPE::TRAIL_TEMPLATE;
            }
            else if (eventTemplateType_ == "VFX_TEMPLATE")
            {
                e.eventTemplateType = ANIMATION_EVENT_TEMPLATE_TYPE::VFX_TEMPLATE;
            }
            else if (eventTemplateType_ == "SOUND_TEMPLATE")
            {
                e.eventTemplateType = ANIMATION_EVENT_TEMPLATE_TYPE::SOUND_TEMPLATE;
            }
        }
            else if (j["eventTemplateType"].is_number_integer())
            {
                const int eventTemplateType_ = (std::clamp)(j["eventTemplateType"].get<int>(), 0, 4);
                e.eventTemplateType = static_cast<ANIMATION_EVENT_TEMPLATE_TYPE>(eventTemplateType_);
            }
        }
        e.attackTemplateId = j.value("attackTemplateId", std::string{});
        e.telegraph = AttackTelegraphRuntime::DeserializeSettings(j.value("telegraph", nlohmann::json::object()));
        e.telegraphHitStart = j.value("telegraphHitStart", false);
        e.ownerTarget = j.value("ownerTarget", std::string("Self"));
        auto readVector3_ = [&j](const char* key_, const DirectX::XMFLOAT3& fallback_)
            {
                const nlohmann::json value_ = j.value(key_, nlohmann::json::array());
                if (!value_.is_array() || value_.size() < 3u)
        {
            return fallback_;
        }
        return DirectX::XMFLOAT3{
                    value_[0].get<float>(),
                    value_[1].get<float>(),
                    value_[2].get<float>() };
            };
        e.meshVFXAssetPath = j.value("meshVFXAssetPath", std::string{});
        e.attachNodeName = j.value("attachNodeName", std::string{});
        e.localPosition = readVector3_("localPosition", { 0.0f, 0.0f, 0.0f });
        e.localRotation = readVector3_("localRotation", { 0.0f, 0.0f, 0.0f });
        e.localScale = readVector3_("localScale", { 1.0f, 1.0f, 1.0f });
        e.meshVFXLifeTime = j.value("meshVFXLifeTime", 1.0f);
        e.meshVFXPlaybackSpeed = j.value("meshVFXPlaybackSpeed", 1.0f);
        e.attachToOwner = j.value("attachToOwner", false);
        e.mirrorMeshVFX = j.value("mirrorMeshVFX", false);
        e.isFired = false;
        e.isEndFired = false;
        e.attackTemplateRuntimeActive = false;
        e.attackTemplateInstanceId = 0u;
        e.spawnedAttackEffects.clear();
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
            {"playbackSpeedParamName", s.playbackSpeedParamName},
            {"sampleWeaponHitBoxes", s.sampleWeaponHitBoxes},
            {"events", s.events},
            // Single用
            {"animationIndex", s.animationIndex},
            // Sequence用
            {"sequenceClips", s.sequenceClips},
            // Blend1D用
            {"animIndexA", s.animIndexA},
            {"animIndexB", s.animIndexB},
            {"blendParamName", s.blendParamName},
            {"blendDefaultWeight", s.blendDefaultWeight},
            {"blendParameterMin", s.blendParameterMin},
            {"blendParameterMax", s.blendParameterMax},
            {"blendPhaseOffsetA", s.blendPhaseOffsetA},
            {"blendPhaseOffsetB", s.blendPhaseOffsetB},
            {"blendSyncNormalizedTime", s.blendSyncNormalizedTime},
            {"blendUseMask", s.blendUseMask},
            {"blendMaskRootNodes", s.blendMaskRootNodes},
            // Blend2D用
            {"blend2DForwardAnimationIndex", s.blend2DForwardAnimationIndex},
            {"blend2DBackwardAnimationIndex", s.blend2DBackwardAnimationIndex},
            {"blend2DLeftAnimationIndex", s.blend2DLeftAnimationIndex},
            {"blend2DRightAnimationIndex", s.blend2DRightAnimationIndex},
            {"blend2DForwardLeftAnimationIndex", s.blend2DForwardLeftAnimationIndex},
            {"blend2DForwardRightAnimationIndex", s.blend2DForwardRightAnimationIndex},
            {"blend2DBackwardLeftAnimationIndex", s.blend2DBackwardLeftAnimationIndex},
            {"blend2DBackwardRightAnimationIndex", s.blend2DBackwardRightAnimationIndex},
            {"blend2DForwardLeft90AnimationIndex", s.blend2DForwardLeft90AnimationIndex},
            {"blend2DForwardRight90AnimationIndex", s.blend2DForwardRight90AnimationIndex},
            {"blend2DBackwardLeft90AnimationIndex", s.blend2DBackwardLeft90AnimationIndex},
            {"blend2DBackwardRight90AnimationIndex", s.blend2DBackwardRight90AnimationIndex},
            {"blend2DIdleAnimationIndex", s.blend2DIdleAnimationIndex},
            {"blend2DParamName", s.blend2DParamName},
            {"blend2DDefaultInput", {s.blend2DDefaultInput.x, s.blend2DDefaultInput.y}},
            {"blend2DUseIdleStop", s.blend2DUseIdleStop},
            {"blend2DUseEightDirections", s.blend2DUseEightDirections},
            {"blend2DUseForwardBackwardLateral", s.blend2DUseForwardBackwardLateral},
            {"blend2DIdleThreshold", s.blend2DIdleThreshold},
            {"blend2DSyncNormalizedTime", s.blend2DSyncNormalizedTime},
            {"blend2DForwardPhaseOffset", s.blend2DForwardPhaseOffset},
            {"blend2DBackwardPhaseOffset", s.blend2DBackwardPhaseOffset},
            {"blend2DLeftPhaseOffset", s.blend2DLeftPhaseOffset},
            {"blend2DRightPhaseOffset", s.blend2DRightPhaseOffset},
            {"blend2DForwardLeftPhaseOffset", s.blend2DForwardLeftPhaseOffset},
            {"blend2DForwardRightPhaseOffset", s.blend2DForwardRightPhaseOffset},
            {"blend2DBackwardLeftPhaseOffset", s.blend2DBackwardLeftPhaseOffset},
            {"blend2DBackwardRightPhaseOffset", s.blend2DBackwardRightPhaseOffset},
            {"blend2DForwardLeft90PhaseOffset", s.blend2DForwardLeft90PhaseOffset},
            {"blend2DForwardRight90PhaseOffset", s.blend2DForwardRight90PhaseOffset},
            {"blend2DBackwardLeft90PhaseOffset", s.blend2DBackwardLeft90PhaseOffset},
            {"blend2DBackwardRight90PhaseOffset", s.blend2DBackwardRight90PhaseOffset},
            {"blend2DIdlePhaseOffset", s.blend2DIdlePhaseOffset},
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
    s.playbackSpeedParamName = j.value("playbackSpeedParamName", "");
    s.sampleWeaponHitBoxes = j.value("sampleWeaponHitBoxes", false);

    // Single用復元
    s.animationIndex = j.value("animationIndex", -1);
    s.sequenceClips = j.value(
        "sequenceClips",
        std::vector<AnimSequenceClip>{});

    // Blend1D用復元
    s.animIndexA = j.value("animIndexA", -1);
    s.animIndexB = j.value("animIndexB", -1);
    s.blendParamName = j.value("blendParamName", "BlendAlpha");
    s.blendDefaultWeight = j.value("blendDefaultWeight", 0.5f);
    s.blendParameterMin = j.value("blendParameterMin", 0.0f);
    s.blendParameterMax = j.value("blendParameterMax", 1.0f);
    s.blendPhaseOffsetA = j.value("blendPhaseOffsetA", 0.0f);
    s.blendPhaseOffsetB = j.value("blendPhaseOffsetB", 0.0f);
    s.blendSyncNormalizedTime = j.value("blendSyncNormalizedTime", true);
    s.blendUseMask = j.value("blendUseMask", false);
    s.blendMaskRootNodes = j.value(
        "blendMaskRootNodes",
        std::vector<std::string>{});

    // Blend2D用復元
    s.blend2DForwardAnimationIndex = j.value("blend2DForwardAnimationIndex", -1);
    s.blend2DBackwardAnimationIndex = j.value("blend2DBackwardAnimationIndex", -1);
    s.blend2DLeftAnimationIndex = j.value("blend2DLeftAnimationIndex", -1);
    s.blend2DRightAnimationIndex = j.value("blend2DRightAnimationIndex", -1);
    s.blend2DForwardLeftAnimationIndex = j.value("blend2DForwardLeftAnimationIndex", -1);
    s.blend2DForwardRightAnimationIndex = j.value("blend2DForwardRightAnimationIndex", -1);
    s.blend2DBackwardLeftAnimationIndex = j.value("blend2DBackwardLeftAnimationIndex", -1);
    s.blend2DBackwardRightAnimationIndex = j.value("blend2DBackwardRightAnimationIndex", -1);
    s.blend2DForwardLeft90AnimationIndex = j.value("blend2DForwardLeft90AnimationIndex", -1);
    s.blend2DForwardRight90AnimationIndex = j.value("blend2DForwardRight90AnimationIndex", -1);
    s.blend2DBackwardLeft90AnimationIndex = j.value("blend2DBackwardLeft90AnimationIndex", -1);
    s.blend2DBackwardRight90AnimationIndex = j.value("blend2DBackwardRight90AnimationIndex", -1);
    s.blend2DIdleAnimationIndex = j.value("blend2DIdleAnimationIndex", -1);
    s.blend2DParamName = j.value("blend2DParamName", "MoveInput");
    if (j.contains("blend2DDefaultInput") &&
        j["blend2DDefaultInput"].is_array() &&
        j["blend2DDefaultInput"].size() >= 2)
    {
        s.blend2DDefaultInput =
        {
            j["blend2DDefaultInput"][0].get<float>(),
            j["blend2DDefaultInput"][1].get<float>()
        };
    }
    s.blend2DUseIdleStop = j.value("blend2DUseIdleStop", false);
    s.blend2DUseEightDirections = j.value("blend2DUseEightDirections", false);
    s.blend2DUseForwardBackwardLateral = j.value("blend2DUseForwardBackwardLateral", false);
    s.blend2DIdleThreshold = (std::max)(j.value("blend2DIdleThreshold", 0.05f), 0.0f);
    s.blend2DSyncNormalizedTime = j.value("blend2DSyncNormalizedTime", true);
    s.blend2DForwardPhaseOffset = j.value("blend2DForwardPhaseOffset", 0.0f);
    s.blend2DBackwardPhaseOffset = j.value("blend2DBackwardPhaseOffset", 0.0f);
    s.blend2DLeftPhaseOffset = j.value("blend2DLeftPhaseOffset", 0.0f);
    s.blend2DRightPhaseOffset = j.value("blend2DRightPhaseOffset", 0.0f);
    s.blend2DForwardLeftPhaseOffset = j.value("blend2DForwardLeftPhaseOffset", 0.0f);
    s.blend2DForwardRightPhaseOffset = j.value("blend2DForwardRightPhaseOffset", 0.0f);
    s.blend2DBackwardLeftPhaseOffset = j.value("blend2DBackwardLeftPhaseOffset", 0.0f);
    s.blend2DBackwardRightPhaseOffset = j.value("blend2DBackwardRightPhaseOffset", 0.0f);
    s.blend2DForwardLeft90PhaseOffset = j.value("blend2DForwardLeft90PhaseOffset", 0.0f);
    s.blend2DForwardRight90PhaseOffset = j.value("blend2DForwardRight90PhaseOffset", 0.0f);
    s.blend2DBackwardLeft90PhaseOffset = j.value("blend2DBackwardLeft90PhaseOffset", 0.0f);
    s.blend2DBackwardRight90PhaseOffset = j.value("blend2DBackwardRight90PhaseOffset", 0.0f);
    s.blend2DIdlePhaseOffset = j.value("blend2DIdlePhaseOffset", 0.0f);

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
