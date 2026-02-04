#include "AnimationController.h"


//#include <nlohmann/json.hpp>
#include <fstream>

#include"RandomHelper.h"

void AnimationController::Update(float deltaTime)
{
    if (!model)
    {
        auto obj = GetGameObject();
        model = obj->GetComponent<GltfModelCommponennt>();
        if (!model) return;
    }

    if (layers.empty()) layers.resize(1);

    size_t nodeCount = model->GetNodes().size();
    if (nodeCount == 0) return;

    // --- 1. 時間の更新 ---
    if (animationPlaying)
    {
        for (auto& layer : layers)
        {
            float animDuration = 1.0f;
            if (layer.isCustomMode) {
                if (layer.customAnimIndex >= 0 && layer.customAnimIndex < (int)customAnims.size())
                    animDuration = customAnims[layer.customAnimIndex].duration;
            }
            else if (layer.animationIndex >= 0) {
                int a = model->GetAnimations().size();
                animDuration = model->GetAnimations()[layer.animationIndex].secondsLength;
            }

            float delta = deltaTime * playbackSpeed * layer.playbackSpeed;
            layer.currentTime += delta;

            if (layer.loop) {
                layer.currentTime = fmodf(layer.currentTime, animDuration);
            }
            else {
                layer.currentTime = (std::min)(layer.currentTime, animDuration);
            }
        }
    }

    // --- 2. アクティブレイヤーのポーズ計算 ---
    if (globalSelectedLayer < 0 || globalSelectedLayer >= (int)layers.size()) return;
    auto& activeLayer = layers[globalSelectedLayer];

    // アニメーションの切り替えを検知して補完を開始
    int currentAnimIdx = activeLayer.isCustomMode ? activeLayer.customAnimIndex : activeLayer.animationIndex;
    if (currentAnimIdx != activeLayer.lastActiveAnimIndex)
    {
        if (activeLayer.lastActiveAnimIndex != -1) 
        {
            activeLayer.isTransitioning = true;
            activeLayer.transitionTimer = 0.0f;
            activeLayer.startPoses.resize(nodeCount);
            model->GetNodePoses(activeLayer.startPoses); // 現在の見た目をキャプチャ
        }
        activeLayer.lastActiveAnimIndex = currentAnimIdx;
    }

    // 計算用バッファ
    std::vector<GltfModelCommponennt::NodePose> targetPoses(nodeCount);
    bool hasAnim = activeLayer.isCustomMode ? (activeLayer.customAnimIndex >= 0) : (activeLayer.animationIndex >= 0);

    
    if (hasAnim)
    {
        //アニメーションの終了時間
        float animDuration = activeLayer.isCustomMode ?
            customAnims[activeLayer.customAnimIndex].duration :
            model->GetAnimations()[activeLayer.animationIndex].secondsLength;

        //アニメーションの現在の時間を取得する
        float evalTime = activeLayer.currentTime;
        if (activeLayer.speedCurve && !activeLayer.speedCurve->keys.empty()) {
            evalTime = activeLayer.speedCurve->Evaluate(activeLayer.currentTime) * animDuration;
        }

        if (activeLayer.isCustomMode) {
            this->ComputeCustomPose(activeLayer.customAnimIndex, evalTime, activeLayer.loop, targetPoses);
        }

        //多分これが通常のアニメーション
        else 
        {
            model->ComputeAnimation(activeLayer.animationIndex, evalTime, targetPoses);
        }

        if (activeLayer.m_IsRootMotion)
        {
            //ルートモーション用の処理
            RootMotion(activeLayer, deltaTime, targetPoses, nodeCount);
        }
        else
        {
            //従来の処理
            Crossfade(activeLayer, deltaTime, targetPoses, nodeCount);
        }
    }

    // 最後にトランスフォームを更新
    model->UpdateTransform();
}

void AnimationController::Crossfade(AnimationLayer& activeLayer,float deltaTime, std::vector<GltfModelCommponennt::NodePose> &targetPoses,int nodeCount)
{
    // --- 3. 補完（クロスフェード）の適用 ---
    if (activeLayer.isTransitioning && !activeLayer.startPoses.empty())
    {
        activeLayer.transitionTimer += deltaTime;
        float t = activeLayer.transitionTimer / activeLayer.transitionDuration;

        if (t >= 1.0f) {
            activeLayer.isTransitioning = false;
            model->SetNodePoses(targetPoses);
        }
        else
        {
            std::vector<GltfModelCommponennt::NodePose> blendedPoses(nodeCount);
            for (size_t i = 0; i < nodeCount; ++i) {
                using namespace DirectX;
                XMVECTOR p0 = XMLoadFloat3(&activeLayer.startPoses[i].position);
                XMVECTOR p1 = XMLoadFloat3(&targetPoses[i].position);
                XMStoreFloat3(&blendedPoses[i].position, XMVectorLerp(p0, p1, t));

                XMVECTOR r0 = XMLoadFloat4(&activeLayer.startPoses[i].rotation);
                XMVECTOR r1 = XMLoadFloat4(&targetPoses[i].rotation);
                XMStoreFloat4(&blendedPoses[i].rotation, XMQuaternionSlerp(r0, r1, t));

                XMVECTOR s0 = XMLoadFloat3(&activeLayer.startPoses[i].scale);
                XMVECTOR s1 = XMLoadFloat3(&targetPoses[i].scale);
                XMStoreFloat3(&blendedPoses[i].scale, XMVectorLerp(s0, s1, t));
            }
            model->SetNodePoses(blendedPoses);
        }
    }
    else {
        model->SetNodePoses(targetPoses);
    }
}

void AnimationController::RootMotion(AnimationLayer& activeLayer, float deltaTime, std::vector<GltfModelCommponennt::NodePose>& targetPoses, int nodeCount)
{
    std::vector<GltfModelCommponennt::NodePose> blendedPoses(nodeCount);

    if (activeLayer.isTransitioning && !activeLayer.startPoses.empty())
    {
        activeLayer.transitionTimer += deltaTime;
        float t = activeLayer.transitionTimer / activeLayer.transitionDuration;

        if (t >= 1.0f) {
            activeLayer.isTransitioning = false;
        }
        else
        {
            for (size_t i = 0; i < nodeCount; ++i) {
                using namespace DirectX;
                XMVECTOR p0 = XMLoadFloat3(&activeLayer.startPoses[i].position);
                XMVECTOR p1 = XMLoadFloat3(&targetPoses[i].position);
                XMStoreFloat3(&blendedPoses[i].position, XMVectorLerp(p0, p1, t));

                XMVECTOR r0 = XMLoadFloat4(&activeLayer.startPoses[i].rotation);
                XMVECTOR r1 = XMLoadFloat4(&targetPoses[i].rotation);
                XMStoreFloat4(&blendedPoses[i].rotation, XMQuaternionSlerp(r0, r1, t));

                XMVECTOR s0 = XMLoadFloat3(&activeLayer.startPoses[i].scale);
                XMVECTOR s1 = XMLoadFloat3(&targetPoses[i].scale);
                XMStoreFloat3(&blendedPoses[i].scale, XMVectorLerp(s0, s1, t));
            }
        }
    }

    ////アニメーション同士のブレンドが終了したとき
    //if(activeLayer.isTransitioning == false)
    {
        //ルートモーションノード番号を取得する
        const int rootMotionNodeIndex = model->GetNodeIndex(model->GetRootMotionName().c_str());
        Quaternion currentQuaternion = targetPoses[rootMotionNodeIndex].rotation;

        //現在のルートモーション用のノードの角度を取得する
        GltfModelCommponennt::Node rootNode = model->GetNodes(model->GetRootMotionName());

        GltfModelCommponennt::NodePose baseNodePoses;

        //アニメションの最初のフレームのノードを取得する
        model->ComputeAnimation(activeLayer.animationIndex, rootMotionNodeIndex, 0, baseNodePoses);
        Quaternion baseNodeQuaternion = baseNodePoses.rotation;

        //1フレーム前のノードを保存する
        GltfModelCommponennt::NodePose oldNodePoses;

        //1フレーム前のアニメーションを計算する
        model->ComputeAnimation(activeLayer.animationIndex, rootMotionNodeIndex, activeLayer.m_OldTime, oldNodePoses);

        //1フレーム前のルートモーション用ノードの角度を取得する
        Quaternion oldQuaternion = oldNodePoses.rotation;

        //1フレーム前のクォータニオンを逆行列にする
        Quaternion inverseOldQuaternion;
        oldQuaternion.Inverse(inverseOldQuaternion);

        //1フレーム前と現在のクォータニオンとの差を求める
        Quaternion subQuaternion = inverseOldQuaternion * currentQuaternion;

        //アニメーション同士のブレンドをしているとき
        if (activeLayer.isTransitioning)
        {
            blendedPoses[rootMotionNodeIndex].rotation = baseNodeQuaternion;;
        }
        else
        {
            //ルートモーション用のノードを初期の姿勢に戻す
            targetPoses[rootMotionNodeIndex].rotation = baseNodeQuaternion;
        }

        //現在の回転値を度数で受け取る
        Vector3 angle = GetGameObject()->transform->GetWorldRotation();

        //度数をラジアン角にする
        angle.x = DirectX::XMConvertToRadians(angle.x);
        angle.y = DirectX::XMConvertToRadians(angle.y);
        angle.z = DirectX::XMConvertToRadians(angle.z);

        //angle.y += 0.1f;
        //オイラー角をクォータニオンに変換する
        Quaternion worldQuaternion = DirectX::XMQuaternionRotationRollPitchYaw(angle.x, angle.y, angle.z);

        //Quaternion a =  DirectX::XMQuaternionRotationRollPitchYaw(0,0.1f,0);
        //ルートモーションの回転の差を現在の回転角度に足す
        worldQuaternion = worldQuaternion * subQuaternion;

        ////クォータニオンをオイラー角に変換する
        angle = QuaternionToEuler(worldQuaternion);

        //Y軸以外の回転を削除
        angle.x = angle.z = 0;

        //ラジアン角を度数に変換する
        angle.x = DirectX::XMConvertToDegrees(angle.x);
        angle.y = DirectX::XMConvertToDegrees(angle.y);
        angle.z = DirectX::XMConvertToDegrees(angle.z);

        GetGameObject()->transform->SetRotation(angle);
        //アニメーション同士のブレンドをしているとき
        if (activeLayer.isTransitioning)
        {
            //モデルにノードをセットする
            model->SetNodePoses(blendedPoses);
        }
        else
        {
            //モデルにノードをセットする
            model->SetNodePoses(targetPoses);
        }


        activeLayer.m_OldTime = activeLayer.currentTime;
    }
}



void AnimationController::ComputeCustomPose(int animIndex, float time, bool loop, std::vector<GltfModelCommponennt::NodePose>& outPoses)
{
    // 安全チェック
    if (animIndex < 0 || animIndex >= (int)customAnims.size()) return;
    auto& anim = customAnims[animIndex];
    if (anim.keyframes.empty() || poseLibrary.empty()) return;

    size_t nodeCount = outPoses.size();

    // --- 1. 前後のキーフレームを探す ---
    int idxA = 0;
    int idxB = 0;

    // 現在の時間より前の最後のキーフレームを探す
    for (int i = 0; i < (int)anim.keyframes.size(); ++i) {
        if (anim.keyframes[i].time <= time) {
            idxA = i;
        }
    }

    // 次のキーフレームを決定
    if (idxA + 1 < (int)anim.keyframes.size()) {
        idxB = idxA + 1;
    }
    else {
        // 最後のキーフレームを超えている場合
        idxB = (loop) ? 0 : idxA; // ループなら最初へ、そうでなければ最後のまま
    }

    auto& kfA = anim.keyframes[idxA];
    auto& kfB = anim.keyframes[idxB];

    // --- 2. 補間係数 (t) の計算 ---
    float t = 0.0f;
    float durationBetweenKeys = kfB.time - kfA.time;

    // ループ跨ぎの計算
    if (idxB == 0 && idxA != idxB) {
        durationBetweenKeys = anim.duration - kfA.time;
    }

    if (durationBetweenKeys > 0.0f) {
        t = (time - kfA.time) / durationBetweenKeys;
    }
    t = (std::max)(0.0f, (std::min)(1.0f, t)); // 0.0 ~ 1.0にクランプ

    // --- 3. ポーズデータの取得と補間 ---
    auto& poseA = poseLibrary[kfA.poseIndex].nodePoses;
    auto& poseB = poseLibrary[kfB.poseIndex].nodePoses;

    for (size_t i = 0; i < nodeCount; ++i)
    {
        // 片方のポーズにノードデータがない場合はスキップ（安全策）
        if (i >= poseA.size() || i >= poseB.size()) continue;

        const auto& nA = poseA[i];
        const auto& nB = poseB[i];

        // 位置の補間 (Lerp)
        DirectX::XMVECTOR pA = DirectX::XMLoadFloat3(&nA.position);
        DirectX::XMVECTOR pB = DirectX::XMLoadFloat3(&nB.position);
        DirectX::XMStoreFloat3(&outPoses[i].position, DirectX::XMVectorLerp(pA, pB, t));

        // 回転の補間 (Slerp)
        DirectX::XMVECTOR rA = DirectX::XMLoadFloat4(&nA.rotation);
        DirectX::XMVECTOR rB = DirectX::XMLoadFloat4(&nB.rotation);
        DirectX::XMStoreFloat4(&outPoses[i].rotation, DirectX::XMQuaternionSlerp(rA, rB, t));

        // スケールの補間 (Lerp)
        DirectX::XMVECTOR sA = DirectX::XMLoadFloat3(&nA.scale);
        DirectX::XMVECTOR sB = DirectX::XMLoadFloat3(&nB.scale);
        DirectX::XMStoreFloat3(&outPoses[i].scale, DirectX::XMVectorLerp(sA, sB, t));
    }
}

bool AnimationController::IsAnimationFinished()
{
    const auto& layer = layers[globalSelectedLayer];

    // 2. ループ設定なら「終了」はないので false
    if (layer.loop) return false;

    // 3. アニメーションが未設定なら false
    if (layer.animationIndex == -1 && !layer.isCustomMode) return false;

    float duration = 0.0f;

    if (layer.isCustomMode) {
        // 自作アニメーションの場合
        if (layer.customAnimIndex >= 0 && layer.customAnimIndex < (int)customAnims.size()) {
            duration = customAnims[layer.customAnimIndex].duration;
        }
    }
    else {
        // 通常のglTFアニメーションの場合
        if (model && layer.animationIndex >= 0) {
            // モデルからアニメーションの長さを取得（GetAnimationDurationはモデルクラスに実装されている想定）
            duration = model->GetAnimationDuration(layer.animationIndex);
        }
    }

    // 4. 現在の時間が Duration 以上なら「端まで行っている」と判定
    // 浮動小数点の誤差を考慮して少し余裕を持たせるのが一般的です
    return layer.currentTime >= duration;
}

void ShowNodeTree(GltfModelCommponennt::Node* node, const std::vector<GltfModelCommponennt::Node>& allNodes, int& selectedIdx) {
    if (!node) return;

    // 現在のノードのインデックスを計算
    int nodeIdx = static_cast<int>(node - allNodes.data());

    // --- POPな見た目の設定 ---
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selectedIdx == nodeIdx) flags |= ImGuiTreeNodeFlags_Selected;
    if (node->children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    // アイコンの使い分け (親ならフォルダ、子ならボーン)
    std::string label =  (node->name.empty() ? "Node_" + std::to_string(nodeIdx) : node->name);

    // ツリー項目の作成
    bool isOpen = ImGui::TreeNodeEx((void*)(intptr_t)nodeIdx, flags, "%s", label.c_str());

    // クリックされたら選択中インデックスを更新
    if (ImGui::IsItemClicked()) {
        selectedIdx = nodeIdx;
    }

    // 子ノードがある場合は再帰
    if (isOpen && !node->children.empty()) {
        for (auto* child : node->children) {
            ShowNodeTree(child, allNodes, selectedIdx);
        }
        ImGui::TreePop();
    }
}

// label         : 表示するラベル（"On Start" など）
// currentAction : 保存先の std::string（ev.actionStart など）
// owner         : GameObject のポインタ（ローカルアクション取得用）
void DrawActionCombo(const char* label, std::string& currentAction, std::shared_ptr<GameObject> owner)
{
    // IDを分けるための仕組み
    ImGui::PushID(label);

    // 現在選択されているアクションがどれかを表示
    std::string preview = currentAction.empty() ? "(None)" : currentAction;

    if (ImGui::BeginCombo(label, preview.c_str()))
    {
        // 1. 選択解除
        if (ImGui::Selectable("(None)", currentAction.empty())) { currentAction = ""; }
        ImGui::Separator();

        // 2. タブまたは小さなボタンで「Local / Global」を切り替える
        // ここでは簡単に、ツリー形式（Selectableの階層）で分けて選ばせます

        // --- LOCAL 選択セクション ---
        if (owner) {
            if (ImGui::TreeNodeEx("Local Actions", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (auto& cat : owner->localRegistry.GetCategories()) {
                    if (ImGui::BeginMenu(cat.c_str())) {
                        for (auto& name : owner->localRegistry.GetActionNames(cat)) {
                            if (ImGui::Selectable(name.c_str(), currentAction == name)) {
                                currentAction = name;
                                ImGui::CloseCurrentPopup();
                            }
                        }
                        ImGui::EndMenu();
                    }
                }
                ImGui::TreePop();
            }
        }

        // --- GLOBAL 選択セクション ---
        if (ImGui::TreeNodeEx("Global Actions", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& globalReg = ActionRegistry::Instance();
            for (auto& cat : globalReg.GetCategories()) {
                if (ImGui::BeginMenu(cat.c_str())) {
                    for (auto& name : globalReg.GetActionNames(cat)) {
                        if (ImGui::Selectable(name.c_str(), currentAction == name)) {
                            currentAction = name;
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    ImGui::EndMenu();
                }
            }
            ImGui::TreePop();
        }

        ImGui::EndCombo();
    }

    ImGui::PopID();
}

void AnimationController::Gui()
{
    enum class EditorMode {
        PoseCapture,    // 素材（ポーズ）のキャプチャ
        AnimationEdit,  // タイムライン構築（オリジナルアニメ作成）
        EventSetting    // イベント配置
    };
    static EditorMode currentMode = EditorMode::PoseCapture;

    // 共通のパスを生成
    std::string modelName = model->GetModelName();
    std::string folderPath = "Data/AnimeJson/" + modelName + "/";
    std::string filePath = folderPath + modelName + ".json";

    // --- JSON操作ボタン ---
    if (ImGui::Button("Export JSON"))
    {
        // フォルダ作成と保存を実行
        ExportJson(filePath);
    }

    ImGui::SameLine();

    // ファイルが存在するか確認
     bool exists = std::filesystem::exists(filePath);

    // インポートボタン
    if (!exists) {
        // ボタンを半透明にして、クリックしても反応しないようにする（疑似的な無効化）
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }

    if (ImGui::Button("Import JSON") && exists) {
        ImportJson(filePath);
    }

    if (!exists) {
        ImGui::PopStyleVar();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("No save file found for this model.");
        }
    }

    ImGui::Separator();

    // --- A. 全モード共通のグローバル操作 ---
    ImGui::Checkbox("Global Play", &animationPlaying);
    ImGui::SameLine();
    ImGui::DragFloat("Speed", &playbackSpeed, 0.01f, 0.0f, 5.0f);



    // --- 2. 編集対象レイヤーとアニメーションの設定 (全モード共通ヘッダー) ---

    ImGui::Checkbox("loopFlg", &layers[globalSelectedLayer].loop);

    // レイアウトを横に並べて見やすくします
    ImGui::Columns(2, "LayerSettings", false);
    ImGui::SetColumnWidth(0, 150);

    // [レイヤー選択]
    if (ImGui::BeginCombo("##LayerCombo", ("Layer " + std::to_string(globalSelectedLayer)).c_str())) {
        for (int i = 0; i < (int)layers.size(); i++) {
            if (ImGui::Selectable(("Layer " + std::to_string(i)).c_str(), globalSelectedLayer == i))
                globalSelectedLayer = i;
        }

        if (ImGui::Selectable("+ Add New Layer")) {
            AnimationLayer newLayer;
            newLayer.animationIndex = 0;
            newLayer.currentTime = 0.0f;
            newLayer.weight = 1.0f;     // ブレンド用に1.0
            newLayer.isCustomMode = false;
            newLayer.loop = true;       // デフォルトでループON
            newLayer.customAnimIndex = 0;

            layers.push_back(newLayer);
            globalSelectedLayer = (int)layers.size() - 1;
        }
        ImGui::EndCombo();
    }

    ImGui::NextColumn();

    // [アニメーション選択] 選択中のレイヤーに対してアニメを割り当てる
    auto& activeLayer = layers[globalSelectedLayer];
    ImGui::Text("Assigned Animation:");
    if (model) {
        auto& anims = model->GetAnimations();
        std::string currentAnimName = activeLayer.isCustomMode ? "Custom (Timeline)" :
            (activeLayer.animationIndex >= 0 ? anims[activeLayer.animationIndex].name : "None");

        if (ImGui::BeginCombo("##AnimCombo", currentAnimName.c_str())) {
            // glTF内蔵アニメーション
            for (int i = 0; i < (int)anims.size(); i++) {
                if (ImGui::Selectable(anims[i].name.c_str(), (!activeLayer.isCustomMode && activeLayer.animationIndex == i))) {
                    activeLayer.animationIndex = i;
                    activeLayer.isCustomMode = false;
                    activeLayer.currentTime = 0.0f; // ★即座に最初から再生させる
                }
            }
            // 自作カスタムアニメーション（あれば）
            if (ImGui::Selectable("-- Custom Timeline --", activeLayer.isCustomMode)) {
                activeLayer.isCustomMode = true;
                activeLayer.currentTime = 0.0f; // ★リセット
            }
            ImGui::EndCombo();
        }
    }
    ImGui::Columns(1);

    // --- B. 現在の時間（シークバー） ---
    // どのアニメーションの長さに基づいているかを判定
 // --- B. 現在の時間（シークバー） ---
    float maxLen = 1.0f;
    if (!activeLayer.isCustomMode && model && activeLayer.animationIndex >= 0) {
        maxLen = model->GetAnimations()[activeLayer.animationIndex].secondsLength;
    }
    else if (activeLayer.isCustomMode && !customAnims.empty()) {
        // 安全のために範囲をクランプ
        int idx = (std::max)(0, (std::min)(activeLayer.customAnimIndex, (int)customAnims.size() - 1));
        maxLen = customAnims[idx].duration;
    }

    ImGui::Text("Current Time (s):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1); // 右端まで広げる
    if (ImGui::SliderFloat("##GlobalTime", &activeLayer.currentTime, 0.0f, maxLen, "%.3f s")) {
        animationPlaying = false; // シーク中は停止
    }

    ImGui::Separator();

    // --- モード切り替えタブ ---
    ImGui::Text("Editor Mode Selection:");
    if (ImGui::RadioButton("1. Pose Capture", currentMode == EditorMode::PoseCapture)) currentMode = EditorMode::PoseCapture;
    ImGui::SameLine();
    if (ImGui::RadioButton("2. Animation Edit", currentMode == EditorMode::AnimationEdit)) currentMode = EditorMode::AnimationEdit;
    ImGui::SameLine();
    if (ImGui::RadioButton("3. Event Setting", currentMode == EditorMode::EventSetting)) currentMode = EditorMode::EventSetting;

    ImGui::Separator();

    switch (currentMode)
    {
    case EditorMode::PoseCapture:
    {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "[Mode: Pose Capture]");
        auto& activeLayer = layers[globalSelectedLayer];
        auto& allNodes = model->GetNodes(); // モデルのノードリストを取得

        // --- 1. ポーズのキャプチャ ---
        static char poseName[64] = "Action_Pose_01";
        ImGui::InputText("New Pose Name", poseName, sizeof(poseName));

        if (ImGui::Button("Capture Current View", ImVec2(-1, 30))) {
            SavedPose newPose;
            newPose.name = poseName;
            model->GetNodePoses(newPose.nodePoses);
            newPose.sourceAnimIndex = activeLayer.animationIndex;
            newPose.sourceTime = activeLayer.currentTime;
            poseLibrary.push_back(newPose);
            selectedPoseIdx = (int)poseLibrary.size() - 1;

            // 名前を自動インクリメント
            int nextId = (int)poseLibrary.size() + 1;
            sprintf_s(poseName, "Action_Pose_%02d", nextId);
        }

        ImGui::Separator();

        // --- 2. ポーズライブラリ一覧 ---
        if (ImGui::BeginChild("LibraryList", ImVec2(0, 150), true)) {
            for (int i = 0; i < (int)poseLibrary.size(); ++i) {
                ImGui::PushID(i);
                if (ImGui::Selectable(poseLibrary[i].name.c_str(), selectedPoseIdx == i)) {
                    selectedPoseIdx = i;
                    selectedNodeIdx = -1;
                    // プレビュー反映
                    activeLayer.animationIndex = poseLibrary[i].sourceAnimIndex;
                    activeLayer.currentTime = poseLibrary[i].sourceTime;
                    activeLayer.isCustomMode = false;
                    animationPlaying = false;
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();

        // --- 【修正版】選択中のポーズを削除するボタン ---
        if (selectedPoseIdx >= 0 && selectedPoseIdx < (int)poseLibrary.size()) {
            // 選択されている時：赤いボタンで削除実行可能
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));

            if (ImGui::Button("Delete Selected Pose", ImVec2(-1, 25))) {
                poseLibrary.erase(poseLibrary.begin() + selectedPoseIdx);
                selectedPoseIdx = -1;
                selectedNodeIdx = -1;
            }

            ImGui::PopStyleColor(2);
        }
        else {
            // 選択されていない時：グレーアウトした見た目で、押しても何も起きない
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));

            ImGui::Button("Delete Selected Pose (None Selected)", ImVec2(-1, 25));

            ImGui::PopStyleColor(4);
        }

        ImGui::Separator();

        // --- 3. Pose Node Editor (別ウィンドウ) ---
        if (selectedPoseIdx >= 0 && selectedPoseIdx < (int)poseLibrary.size()) {
            auto& targetPose = poseLibrary[selectedPoseIdx];
            auto& allNodes = model->GetNodes(); // モデルの全ノード取得

            ImGui::Begin("Pose Node Editor", nullptr, ImGuiWindowFlags_NoCollapse);
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Editing Pose: %s", targetPose.name.c_str());
            ImGui::Separator();

            if (targetPose.nodePoses.empty()) {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "No Node Data.");
            }
            else {
                // 2カラムレイアウト
                ImGui::Columns(2, "NodeEditorCols", true);

                // --- 左側：階層構造ツリー ---
                ImGui::TextDisabled("Hierarchy");
                if (ImGui::BeginChild("NodeTreeScroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true)) {
                    // ルートノード（parentIndexが-1のもの）から描画を開始
                    for (int i = 0; i < (int)allNodes.size(); ++i) {
                        if (allNodes[i].parentIndex == -1) {
                            ShowNodeTree(&allNodes[i], allNodes, selectedNodeIdx);
                        }
                    }
                }
                ImGui::EndChild();

                ImGui::NextColumn();

                // --- 右側：TRS編集 ---
                if (selectedNodeIdx >= 0 && selectedNodeIdx < (int)targetPose.nodePoses.size()) {
                    auto& nodePose = targetPose.nodePoses[selectedNodeIdx];
                    std::string currentName = allNodes[selectedNodeIdx].name;
                    if (currentName.empty()) currentName = "Node_" + std::to_string(selectedNodeIdx);

                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
                    ImGui::Text("Selected: %s", currentName.c_str());
                    ImGui::PopStyleColor();
                    ImGui::Separator();

                    // POPな背景色付きの編集エリア
                    ImGui::BeginChild("EditProperties", ImVec2(0, 0), false, ImGuiChildFlags_AlwaysUseWindowPadding);

                    model->ShowNodePoseEditor(nodePose); // 既存の関数を利用

                    ImGui::Dummy(ImVec2(0, 10));
                    if (ImGui::Button("Reset to Default", ImVec2(-1, 0))) {
                        nodePose.position = { 0,0,0 };
                        nodePose.rotation = { 0,0,0,1 };
                        nodePose.scale = { 1,1,1 };
                    }
                    ImGui::EndChild();
                }
                else {
                    ImGui::TextDisabled("Select a node from the tree to edit.");
                }
                ImGui::Columns(1);
            }

            ImGui::Separator();
            if (ImGui::Button("Close", ImVec2(120, 0))) selectedPoseIdx = -1;
            ImGui::End();
        }
    }
    break;
    case EditorMode::AnimationEdit:
    {
        ImGui::TextColored(ImVec4(0, 1, 1, 1), "[Mode: Custom Animation Build]");
        auto& activeLayer = layers[globalSelectedLayer];

        // --- 1. カスタムアニメーション全体の管理 ---
        if (ImGui::Button("+ New Custom Anim")) {
            customAnims.push_back({ "New Custom " + std::to_string(customAnims.size()), 2.0f, {} });
            activeLayer.customAnimIndex = (int)customAnims.size() - 1;
            activeLayer.isCustomMode = true;
        }

        if (customAnims.empty()) {
            ImGui::Text("No custom animations. Click the button above.");
            break;
        }

        ImGui::SameLine();
        if (ImGui::Button("Delete This Anim")) {
            customAnims.erase(customAnims.begin() + activeLayer.customAnimIndex);
            activeLayer.customAnimIndex = 0;
            break;
        }

        if (activeLayer.customAnimIndex >= (int)customAnims.size()) activeLayer.customAnimIndex = 0;
        auto& activeAnim = customAnims[activeLayer.customAnimIndex];

        if (ImGui::BeginCombo("Editing/Assigning", activeAnim.name.c_str())) {
            for (int n = 0; n < (int)customAnims.size(); n++) {
                if (ImGui::Selectable(customAnims[n].name.c_str(), activeLayer.customAnimIndex == n)) {
                    activeLayer.customAnimIndex = n;
                    activeLayer.isCustomMode = true;
                }
            }
            ImGui::EndCombo();
        }

        char nameBuf[64];
        strncpy_s(nameBuf, activeAnim.name.c_str(), sizeof(nameBuf));
        if (ImGui::InputText("Edit Name", nameBuf, sizeof(nameBuf))) { activeAnim.name = nameBuf; }

        ImGui::DragFloat("Total Duration", &activeAnim.duration, 0.01f, 0.1f, 60.0f, "%.2f s");

        ImGui::Separator();

        // --- 2. キーフレームの追加 ---
        ImGui::Text("Add Keyframe at %.3f s:", activeLayer.currentTime);

        if (ImGui::Button("Capture Current View", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 30))) {
            SavedPose p;
            p.name = activeAnim.name + "_K" + std::to_string(activeAnim.keyframes.size());
            model->GetNodePoses(p.nodePoses);
            p.sourceAnimIndex = activeLayer.isCustomMode ? -1 : activeLayer.animationIndex;
            p.sourceTime = activeLayer.currentTime;
            poseLibrary.push_back(p);

            activeAnim.keyframes.push_back({ activeLayer.currentTime, (int)poseLibrary.size() - 1, p.sourceAnimIndex, p.sourceTime });

            // 追加時は利便性のため自動ソート
            std::sort(activeAnim.keyframes.begin(), activeAnim.keyframes.end(), [](const PoseKeyframe& a, const PoseKeyframe& b) { return a.time < b.time; });
        }
        ImGui::SameLine();

        if (ImGui::BeginCombo("##AddFromLib", "Add from Library...", ImGuiComboFlags_HeightLarge)) {
            for (int i = 0; i < (int)poseLibrary.size(); i++) {
                if (ImGui::Selectable(poseLibrary[i].name.c_str())) {
                    activeAnim.keyframes.push_back({ activeLayer.currentTime, i, poseLibrary[i].sourceAnimIndex, poseLibrary[i].sourceTime });
                    std::sort(activeAnim.keyframes.begin(), activeAnim.keyframes.end(), [](const PoseKeyframe& a, const PoseKeyframe& b) { return a.time < b.time; });
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();

        // --- 3. タイムラインリストとソートボタン ---
        ImGui::Text("Keyframes List:");
        ImGui::SameLine(ImGui::GetWindowWidth() - 120);
        ImGui::Separator();

        // --- 【追加】タイムスケール変更機能 ---
        static float timeScaleFactor = 1.0f;
        ImGui::SetNextItemWidth(100);
        ImGui::DragFloat("Scale Factor", &timeScaleFactor, 0.01f, 0.01f, 10.0f, "x%.2f");
        ImGui::Separator();

        if (ImGui::Button("Rescale Timeline")) {
            // 全てのキーフレーム時間を倍率で計算
            for (auto& kf : activeAnim.keyframes) {
                kf.time *= timeScaleFactor;
            }

            // 全体時間も変更
            activeAnim.duration *= timeScaleFactor;

            // 時間で再ソート（基本順序は維持されるが安全のため）
            std::sort(activeAnim.keyframes.begin(), activeAnim.keyframes.end(),
                [](const PoseKeyframe& a, const PoseKeyframe& b) { return a.time < b.time; });

            // 現在の再生時間もスケールに合わせて調整（シークバーが飛ばないように）
            activeLayer.currentTime *= timeScaleFactor;
        }
        ImGui::Separator();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Multiply all keyframe times and total duration by the scale factor.");

        ImGui::Separator();

        // 【追加】手動ソートボタン
        if (ImGui::Button("Sort by Time", ImVec2(100, 0))) {
            std::sort(activeAnim.keyframes.begin(), activeAnim.keyframes.end(),
                [](const PoseKeyframe& a, const PoseKeyframe& b) { return a.time < b.time; });
        }

        if (ImGui::BeginChild("TimelineScroll", ImVec2(0, 0), true)) {
            // リスト表示中は勝手に順番が変わらないようにする
            for (int k = 0; k < (int)activeAnim.keyframes.size(); k++) {
                auto& kf = activeAnim.keyframes[k];
                ImGui::PushID(k);

                if (ImGui::Button("Go")) { activeLayer.currentTime = kf.time; animationPlaying = false; }
                ImGui::SameLine();

                ImGui::SetNextItemWidth(80);
                // ここでの DragFloat はフラグを立てるだけで即座にソートしない
                ImGui::DragFloat("s", &kf.time, 0.01f, 0.0f, activeAnim.duration, "%.2f");

                ImGui::SameLine();
                ImGui::Text("%s", poseLibrary[kf.poseIndex].name.c_str());

                ImGui::SameLine(ImGui::GetWindowWidth() - 40);
                if (ImGui::Button("X")) {
                    activeAnim.keyframes.erase(activeAnim.keyframes.begin() + k);
                    ImGui::PopID(); break;
                }

                ImGui::PopID();
            }
        }
        ImGui::EndChild();
    }
    break;
    case EditorMode::EventSetting:
   {
        // --- 1. ズーム倍率と選択レイヤーの取得 ---
        static float timelineZoom = 100.0f;
        auto& layer = layers[globalSelectedLayer];

        // タイムラインの長さを確定
        float animationLength = 10.0f;
        if (!layer.isCustomMode && model && layer.animationIndex >= 0) {
            animationLength = model->GetAnimations()[layer.animationIndex].secondsLength;
        } 
        else if (layer.isCustomMode && !customAnims.empty()) {
            animationLength = customAnims[layer.customAnimIndex].duration;
        }

        // --- 2. 独立したシーケンサーウィンドウ ---
        ImGui::SetNextWindowSize(ImVec2(900, 400), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Timeline Sequencer", nullptr, ImGuiWindowFlags_HorizontalScrollbar))
        {
            // --- A. ウィンドウ上部の操作ツールバー ---
            ImGui::BeginGroup();
            ImGui::SetNextItemWidth(150);
            ImGui::DragFloat("Zoom", &timelineZoom, 1.0f, 10.0f, 1000.0f, "%.0f px/s");
            ImGui::SameLine();

            if (ImGui::Button("+ Add Event", ImVec2(100, 0))) {
                layer.eventTrack.AddEvent(EventType::Generic, "New Event", layer.currentTime, layer.currentTime + 1.0f, 0xFFFF00FF);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("| Layer: %d | Time: %.3fs / %.3fs", globalSelectedLayer, layer.currentTime, animationLength);
            ImGui::EndGroup();

            ImGui::Separator();

            // --- B. シーケンサー本体の描画 ---
            bool expanded = true;
            float firstVisibleTime = 0.0f;
            // pixelsPerSecondにtimelineZoomを適用
            layer.eventTrack.DrawSequencer(&layer.currentTime, &expanded, &layer.selectedEvent, &firstVisibleTime, animationLength,gameObject.lock());

            // --- C. ウィンドウ下部のイベント編集エリア (イベント選択時のみ表示) ---
            if (layer.selectedEvent >= 0)
            {
                ImGui::Separator();
                auto& ev = layer.eventTrack.GetEvent(layer.selectedEvent);

                // 編集エリアを2カラムに分割
                ImGui::Columns(2, "EventEditColumns", false);
                ImGui::SetColumnWidth(0, 400); // アクション選択側を少し広く

                // --- 左カラム: 3つのタイミング別アクション設定 ---
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "Action Triggers:");

                // ヘルパー関数を使って3つ並べる（gameObject.lock() を owner として渡す）
                auto owner = gameObject.lock();
                DrawActionCombo("On Start ", ev.actionStart, owner);
                DrawActionCombo("On Update", ev.actionUpdate, owner);
                DrawActionCombo("On End   ", ev.actionEnd, owner);

                ImGui::NextColumn();

                // --- 右カラム: 時間・色・削除 ---
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "Event Settings:");
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloatRange2("Duration", &ev.startTime, &ev.endTime, 0.01f, 0.0f, animationLength, "Start: %.2fs", "End: %.2fs");

                ImGui::Spacing();
                ImGui::ColorEdit4("Bar Color", (float*)&ev.color);

                ImGui::Spacing();
                if (ImGui::Button("Delete Selected Event", ImVec2(-1, 25))) {
                    layer.eventTrack.DeleteEvent(layer.selectedEvent);
                    layer.selectedEvent = -1;
                }

                ImGui::Columns(1);
            }
            else
            {
                ImGui::TextDisabled("Click an event to edit properties.");
            }

            ImGui::End();
        }

        // メインウィンドウ側には何も表示しないか、簡単な案内のみ
        ImGui::Text("Sequencer is open in a separate window.");
        break;
    }
    }
    if (globalSelectedLayer >= 0 && globalSelectedLayer < (int)layers.size())
    {
        ImGui::Checkbox("RootMotion", &layers[globalSelectedLayer].m_IsRootMotion);
        ImGui::DragFloat("transitionDuration ", &layers[globalSelectedLayer].transitionDuration, 0.1f,0.0f, 1.0f);
    }
}



void AnimationController::ExportJson(const std::string& filename)
{
    // --- ディレクトリの作成 ---
    // 引数 filename ("Data/AnimeJson/ModelName/ModelName.json") からパスを作成
    std::filesystem::path filePath(filename);
    std::filesystem::path dirPath = filePath.parent_path();

    // ディレクトリが存在しない場合は作成 (中間ディレクトリも含めて作成される)
    if (!std::filesystem::exists(dirPath)) {
        std::filesystem::create_directories(dirPath);
    }

    nlohmann::json j;

    // --- A. 自作ポーズライブラリ ---
    j["poseLibrary"] = nlohmann::json::array();
    for (const auto& pose : poseLibrary) {
        nlohmann::json pj;
        pj["name"] = pose.name;
        // 追加：ソース情報の保存
        pj["srcIdx"] = pose.sourceAnimIndex;
        pj["srcTime"] = pose.sourceTime;

        for (const auto& n : pose.nodePoses) {
            pj["poses"].push_back({
                {"p", {n.position.x, n.position.y, n.position.z}},
                {"r", {n.rotation.x, n.rotation.y, n.rotation.z, n.rotation.w}},
                {"s", {n.scale.x, n.scale.y, n.scale.z}}
                });
        }
        j["poseLibrary"].push_back(pj);
    }

    // --- B. 自作アニメーション定義 (複数のアニメに対応) ---
    j["customAnims"] = nlohmann::json::array();
    for (const auto& ca : customAnims) {
        nlohmann::json caj;
        caj["name"] = ca.name;
        caj["duration"] = ca.duration;
        caj["keyframes"] = nlohmann::json::array();
        for (const auto& kf : ca.keyframes) {
            caj["keyframes"].push_back({
                {"time", kf.time},
                {"poseIdx", kf.poseIndex},
                {"srcIdx", kf.sourceAnimIdx}, // 追加：ソース情報
                {"srcTime", kf.sourceTime}    // 追加：ソース情報
                });
        }
        j["customAnims"].push_back(caj);
    }

    // --- C. レイヤーの保存 ---
    j["layers"] = nlohmann::json::array();
    for (const auto& layer : layers) {
        nlohmann::json lj;
        lj["isCustomMode"] = layer.isCustomMode;
        lj["customAnimIndex"] = layer.customAnimIndex;
        lj["animationIndex"] = layer.animationIndex;
        lj["currentTime"] = layer.currentTime;
        lj["weight"] = layer.weight;
        lj["loop"] = layer.loop;
        lj["playbackSpeed"] = layer.playbackSpeed;
        lj["IsRootMotion"] = layer.m_IsRootMotion;
        lj["transitionDuration"] = layer.transitionDuration;

        // イベント保存
        lj["events"] = nlohmann::json::array();
        // eventTrackの有効性チェック（nullptr等の対策）
        for (int i = 0; i < layer.eventTrack.GetItemCount(); ++i) {
            const auto& ev = layer.eventTrack.GetEvent(i);
            lj["events"].push_back({
                {"type", static_cast<int>(ev.type)},
                {"label", ev.label},
                {"start", ev.startTime},
                {"end", ev.endTime},
                {"color", ev.color},
                {"payload", ev.payload}
                });
        }
        j["layers"].push_back(lj);
    }

    //std::ofstream ofs(GetSavePath(filename));
    //if (ofs) {
    //    ofs << j.dump(4);
    //}

    // --- ファイル書き込み ---
    // GetSavePath(filename) が絶対パスを返す仕様ならそれを使用、
    // filename 自体がパスなら filePath を使用してください
    std::ofstream ofs(filename);
    if (ofs.is_open()) {
        ofs << j.dump(4);
        // 成功ログなどがあると安心です
        // printf("Saved: %s\n", filename.c_str());
    }
    else {
        // エラーログなど
        // printf("Failed to open file: %s\n", filename.c_str());
    }

}
void AnimationController::ImportJson(const std::string& filename)
{
    std::ifstream ifs(GetSavePath(filename));
    if (!ifs) return;

    nlohmann::json j;
    try {
        ifs >> j;
    }
    catch (...) { return; }

    // 1. ポーズライブラリ復元
    poseLibrary.clear();
    if (j.contains("poseLibrary")) {
        for (const auto& pj : j["poseLibrary"]) {
            SavedPose p;
            p.name = pj.value("name", "Unknown");
            p.sourceAnimIndex = pj.value("srcIdx", -1);
            p.sourceTime = pj.value("srcTime", 0.0f);

            for (const auto& npj : pj["poses"]) {
                GltfModelCommponennt::NodePose n;
                n.position = { npj["p"][0], npj["p"][1], npj["p"][2] };
                n.rotation = { npj["r"][0], npj["r"][1], npj["r"][2], npj["r"][3] };
                n.scale = { npj["s"][0], npj["s"][1], npj["s"][2] };
                p.nodePoses.push_back(n);
            }
            poseLibrary.push_back(p);
        }
    }

    // 2. 自作アニメーション復元
    customAnims.clear();
    if (j.contains("customAnims")) {
        for (const auto& caj : j["customAnims"]) {
            CustomAnimation ca;
            ca.name = caj.value("name", "Custom");
            ca.duration = caj.value("duration", 1.0f);
            if (caj.contains("keyframes")) {
                for (const auto& kfj : caj["keyframes"]) {
                    PoseKeyframe kf;
                    kf.time = kfj.value("time", 0.0f);
                    kf.poseIndex = kfj.value("poseIdx", 0);
                    kf.sourceAnimIdx = kfj.value("srcIdx", -1);
                    kf.sourceTime = kfj.value("srcTime", 0.0f);
                    ca.keyframes.push_back(kf);
                }
            }
            customAnims.push_back(ca);
        }
    }

    // 3. レイヤー復元
    layers.clear();
    if (j.contains("layers")) {
        for (const auto& lj : j["layers"]) {
            AnimationLayer layer;
            layer.isCustomMode = lj.value("isCustomMode", false);
            layer.customAnimIndex = lj.value("customAnimIndex", 0);
            layer.animationIndex = lj.value("animationIndex", 0);
            layer.currentTime = lj.value("currentTime", 0.0f);
            layer.weight = lj.value("weight", 1.0f);
            layer.loop = lj.value("loop", true);
            layer.playbackSpeed = lj.value("playbackSpeed", 1.0f);
            layer.m_IsRootMotion = lj.value("IsRootMotion", false);
            layer.transitionDuration = lj.value("transitionDuration", 0.75);
            if (lj.contains("events")) {
                for (const auto& ej : lj["events"]) {
                    layer.eventTrack.AddEvent(
                        static_cast<EventType>(ej.value("type", 0)),
                        ej.value("label", ""),
                        ej.value("start", 0.0f),
                        ej.value("end", 0.0f),
                        ej["color"]
                    );
                }
            }
            layers.push_back(layer);
        }
    }
}

void AnimationController::Serialize(nlohmann::json& j) const {
    // 基本パラメータ
    j["playbackSpeed"] = playbackSpeed;
    j["animationBlendSeconds"] = animationBlendSeconds;

    // レイヤーの保存
    j["layers"] = nlohmann::json::array();
    for (const auto& layer : layers) {
        nlohmann::json layerJson;
        LayerToJson(layerJson, layer);
        j["layers"].push_back(layerJson);
    }

    // ポーズライブラリの保存
    j["poseLibrary"] = nlohmann::json::array();
    for (const auto& pose : poseLibrary) {
        nlohmann::json pj;
        pj["name"] = pose.name;
        pj["sourceAnimIndex"] = pose.sourceAnimIndex;
        pj["sourceTime"] = pose.sourceTime;

        // NodePoseの保存（Vector3やQuaternionを配列で保存する想定）
        pj["nodePoses"] = nlohmann::json::array();
        for (const auto& np : pose.nodePoses) {
            pj["nodePoses"].push_back({
                {"t", {np.position.x, np.position.y, np.position.z}},
                {"r", {np.rotation.x, np.rotation.y, np.rotation.z, np.rotation.w}},
                {"s", {np.scale.x, np.scale.y, np.scale.z}}
                });
        }
        j["poseLibrary"].push_back(pj);
    }

    // カスタムアニメーションの保存
    j["customAnims"] = nlohmann::json::array();
    for (const auto& anim : customAnims) {
        nlohmann::json aj;
        aj["name"] = anim.name;
        aj["duration"] = anim.duration;
        aj["keyframes"] = nlohmann::json::array();
        for (const auto& kf : anim.keyframes) {
            aj["keyframes"].push_back({
                {"time", kf.time},
                {"poseIndex", kf.poseIndex},
                {"sourceAnimIdx", kf.sourceAnimIdx},
                {"sourceTime", kf.sourceTime}
                });
        }
        j["customAnims"].push_back(aj);
    }
}

void AnimationController::Deserialize(const nlohmann::json& j)
{
    // 1. レイヤーの復元
    if (j.contains("layers")) {
        layers.clear();
        for (const auto& lj : j["layers"]) {
            AnimationLayer layer;
            LayerFromJson(lj, layer);

            // 【再構築のポイント】
            // ロードしたノードマスクが現在のモデルのノード数と合わない場合、リサイズする
            if (model) {
                size_t nodeCount = model->GetNodes().size();
                if (layer.nodeMask.size() != nodeCount) {
                    layer.nodeMask.resize(nodeCount, true); // デフォルトは全表示
                }
            }
            layers.push_back(layer);
        }
    }

    // 2. データの整合性チェック
    if (layers.empty()) {
        layers.resize(1);
    }

    // 3. UIの選択状態をリセット
    // globalSelectedLayerがメンバ変数の場合、0番目を指すようにする
    globalSelectedLayer = 0;

    // ポーズライブラリの復元
    if (j.contains("poseLibrary")) {
        poseLibrary.clear();
        for (const auto& pj : j["poseLibrary"]) {
            SavedPose pose;
            pose.name = pj.value("name", "unnamed");
            pose.sourceAnimIndex = pj.value("sourceAnimIndex", 0);
            pose.sourceTime = pj.value("sourceTime", 0.0f);

            for (const auto& npj : pj["nodePoses"]) {
                GltfModelCommponennt::NodePose np;
                auto t = npj["t"]; np.position = { t[0], t[1], t[2] };
                auto r = npj["r"]; np.rotation = { r[0], r[1], r[2], r[3] };
                auto s = npj["s"]; np.scale = { s[0], s[1], s[2] };
                pose.nodePoses.push_back(np);
            }
            poseLibrary.push_back(pose);
        }
    }

    // カスタムアニメーションの復元
    if (j.contains("customAnims")) {
        customAnims.clear();
        for (const auto& aj : j["customAnims"]) {
            CustomAnimation anim;
            anim.name = aj.value("name", "unnamed");
            anim.duration = aj.value("duration", 0.0f);
            for (const auto& kj : aj["keyframes"]) {
                PoseKeyframe kf;
                kf.time = kj.value("time", 0.0f);
                kf.poseIndex = kj.value("poseIndex", 0);
                kf.sourceAnimIdx = kj.value("sourceAnimIdx", -1);
                kf.sourceTime = kj.value("sourceTime", 0.0f);
                anim.keyframes.push_back(kf);
            }
            customAnims.push_back(anim);
        }
    }
}

void AnimationController::LayerToJson(nlohmann::json& j, const AnimationLayer& layer)
{
    j = nlohmann::json{
        {"animationIndex", layer.animationIndex},
        {"currentTime", layer.currentTime},
        {"weight", layer.weight},
        {"loop", layer.loop},
        {"playbackSpeed", layer.playbackSpeed},
        {"isCustomMode", layer.isCustomMode},
        {"customAnimIndex", layer.customAnimIndex},
        {"blendMode", (int)layer.blendMode},
        {"nodeMask", layer.nodeMask},
        { "IsRootMotion", layer.m_IsRootMotion }
    };
}


void AnimationController::LayerFromJson(const nlohmann::json& j, AnimationLayer& layer)
{
    layer.animationIndex = j.value("animationIndex", -1);
    layer.currentTime = j.value("currentTime", 0.0f);
    layer.weight = j.value("weight", 1.0f);
    layer.loop = j.value("loop", true);
    layer.playbackSpeed = j.value("playbackSpeed", 1.0f);
    layer.isCustomMode = j.value("isCustomMode", false);
    layer.customAnimIndex = j.value("customAnimIndex", -1);
    layer.blendMode = (AnimationLayer::BlendMode)j.value("blendMode", 0);
    if (j.contains("nodeMask")) {
        layer.nodeMask = j.at("nodeMask").get<std::vector<bool>>();
    }
    if (j.contains("IsRootMotion")) {
        layer.m_IsRootMotion = j.at("IsRootMotion").get<bool>();
    }
}

std::string AnimationController::GetSavePath(const std::string& filename)
{
    std::string modelName = model->GetModelName();
    std::string folderPath = "Data/AnimeJson/" + modelName + "/";
    std::string filePath = folderPath + modelName + ".json";

    return filePath;
}
