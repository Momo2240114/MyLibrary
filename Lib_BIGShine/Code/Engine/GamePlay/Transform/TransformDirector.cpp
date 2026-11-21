#include "PrecompiledHeader.h"

#include "TransformDirector.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/Graphics/PostEffect/ColorFilterDirector.h"


void TransformDirector::Start()
{
    auto go = GetGameObject();
    auto easingComp = go->GetOrAddComponent<EasingComponent>();
    easingIndex = easingComp->AddEasingAndGetIndex();

    // 初期化時にアクションを登録
    RegisterActions();
}

void TransformDirector::Update(float et)
{
    auto go = GetGameObject();
    auto easingComp = go->GetComponent<EasingComponent>();
    if (!isMoving || !easingComp)
    {
        return;
    }

    float easedT = easingComp->GetValue(easingIndex);

    // 補間適用
    if (!m_AddFlag)
    {
        go->transform->localPosition = LerpVector(startPos, targetPos, easedT);
        go->transform->localRotation = LerpVector(startAngle, targetAngle, easedT);
        go->transform->localScale = LerpVector(startScale, targetScale, easedT);
        go->transform->MarkDirty();
        if (easingComp->GetEndFlag(easingIndex))
        {
            isMoving = false;
        }
    }
     else
     {
         Vector3  resultPositon = LerpVector(Vector3::Zero, targetPos, easedT);
         Vector3  resultAngle = LerpVector(Vector3::Zero, targetAngle, easedT);


         //度数をラジアンに変換
         resultAngle.x = DirectX::XMConvertToRadians(resultAngle.x);
         resultAngle.y = DirectX::XMConvertToRadians(resultAngle.y);
         resultAngle.z = DirectX::XMConvertToRadians(resultAngle.z);

         //ローカル空間での座標
         Matrix transition = DirectX::XMMatrixTranslation(resultPositon.x, resultPositon.y, resultPositon.z);

         //現在の角度
         DirectX::XMVECTOR currentPosition, currentQuaternion, currentScale;
         DirectX::XMMatrixDecompose(&currentScale, &currentQuaternion, &currentPosition, m_WorldMatrix);
         Vector3 currentAngle = QuaternionToEuler(currentQuaternion);

         currentAngle += resultAngle;


         Matrix scaleMatrix = DirectX::XMMatrixScalingFromVector(currentScale);
         Matrix rotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(currentAngle.x, currentAngle.y, currentAngle.z);
         Matrix translationMatrix = DirectX::XMMatrixTranslationFromVector(currentPosition);

         Matrix mat = scaleMatrix * rotationMatrix * translationMatrix;

         //新しいワールド行列を求める
         Matrix worldMatrix = mat * transition;

         DirectX::XMVECTOR position, quaternion, scale;
         DirectX::XMMatrixDecompose(&scale, &quaternion, &position, worldMatrix);

         //新しいワールド行列のローカルを求める
         DirectX::XMStoreFloat3(&go->transform->localPosition, position);
         Vector3 angle = QuaternionToEuler(quaternion);

         //ラジアンを度数に変換
         go->transform->localRotation.x = DirectX::XMConvertToDegrees(angle.x);
         go->transform->localRotation.y = DirectX::XMConvertToDegrees(angle.y);
         go->transform->localRotation.z = DirectX::XMConvertToDegrees(angle.z);

         go->transform->MarkDirty();
         if (easingComp->GetEndFlag(easingIndex))
        {
            isMoving = false;
        }
    }
}

void TransformDirector::PlayMove(const TransformWayPoint& wp, EasingType type)
{
    auto go = GetGameObject();
    auto easingComp = go->GetOrAddComponent<EasingComponent>();

    startPos = go->transform->localPosition;
    startAngle = go->transform->localRotation;
    startScale = go->transform->localScale; // 追加

    targetPos = wp.pos;
    targetAngle = wp.angle;
    targetScale = wp.scale; // 追加

    easingComp->SetStartValue(easingIndex, 0.0f);
    // 引数の type ではなく、wp.easing を使用するように変更
    easingComp->PlayEasing(easingIndex, wp.easing, 1.0f / wp.duration, 1.0f);
    isMoving = true;

    //ゲームオブジェクトのワールド行列を保存する
    m_WorldMatrix = go->transform->GetWorldMatrix();
    m_AddFlag = wp.m_AddFlag;
}

void TransformDirector::Gui()
{
    auto go = GetGameObject();
    if (ImGui::Button("Capture Transform")) {
        // 初期値として現在の状態とデフォルトイージング（Linear等）を設定
        EditorWaypoints.push_back({ go->transform->localPosition, go->transform->localRotation,go->transform->localScale, 1.0f, EasingType::OutExpo ,false});
    }

    for (int i = 0; i < (int)EditorWaypoints.size(); ++i) {
        ImGui::PushID(i);
        if (ImGui::TreeNode(("WP " + std::to_string(i)).c_str())) {
            auto& wp = EditorWaypoints[i];
            ImGui::DragFloat3("Pos", &wp.pos.x, 0.1f);
            ImGui::DragFloat3("Rot", &wp.angle.x, 0.1f);
            ImGui::DragFloat3("Scale", &wp.scale.x, 0.01f); // 追加
            ImGui::DragFloat("Time", &wp.duration, 0.1f);

            // イージングタイプの選択（intキャストでコンボボックス化）
            int currentEasing = (int)wp.easing;
            if (ImGui::Combo("Easing", &currentEasing, "Linear\0InQuad\0OutQuad\0InOutQuad\0InExpo\0OutExpo\0InOutExpo\0OutBounds\0")) {
                wp.easing = (EasingType)currentEasing;
            }

            if (ImGui::Button("Play"))
            {
                PlayMove(wp, wp.easing);
            }
            if (ImGui::Button("Remove"))
            {
                EditorWaypoints.erase(EditorWaypoints.begin() + i);
                i--;
            }

            //現在の座標から加算するモーションを作る
            ImGui::Checkbox("AddWorldMatrix",&wp.m_AddFlag);

            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

DirectX::XMFLOAT3 TransformDirector::QuaternionToEuler(DirectX::XMVECTOR q)
{
    // 1. 一旦、回転行列に変換する
    DirectX::XMMATRIX m = DirectX::XMMatrixRotationQuaternion(q);

    DirectX::XMFLOAT4X4 mat;
    DirectX::XMStoreFloat4x4(&mat, m);

    float pitch, yaw, roll;

    // 2. 行列の要素から角度を抽出（YXZの順序などの回転順序に依存します）
    // 一般的なピッチ・ヨー・ロールの抽出例
    yaw = asin(-mat._13);

    if (cos(yaw) > 0.0001) {
        pitch = atan2(mat._23, mat._33);
        roll = atan2(mat._12, mat._11);
    }
    else {
        // ジンバルロック時の処理
        pitch = atan2(-mat._32, mat._22);
        roll = 0.0f;
    }

    return   DirectX::XMFLOAT3(pitch, yaw, roll);
}

void TransformDirector::Serialize(nlohmann::json& j) const
{
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& wp : EditorWaypoints) {
        arr.push_back({
            {"px", wp.pos.x}, {"py", wp.pos.y}, {"pz", wp.pos.z},
            {"ax", wp.angle.x}, {"ay", wp.angle.y}, {"az", wp.angle.z},
            {"sx", wp.scale.x}, {"sy", wp.scale.y}, {"sz", wp.scale.z}, // 追加
            {"dur", wp.duration},
            {"ease", (int)wp.easing},
            {"addflag", wp.m_AddFlag}
            });
    }
    j["TransformWaypoints"] = arr;
}

void TransformDirector::Deserialize(const nlohmann::json& j)
{
    if (!j.contains("TransformWaypoints") || !j["TransformWaypoints"].is_array())
    {
        return;
    }

    EditorWaypoints.clear();
    for (const auto& wj : j["TransformWaypoints"]) {
        TransformWayPoint wp;
        wp.pos.x = wj.value("px", 0.0f);
        wp.pos.y = wj.value("py", 0.0f);
        wp.pos.z = wj.value("pz", 0.0f);
        wp.angle.x = wj.value("ax", 0.0f);
        wp.angle.y = wj.value("ay", 0.0f);
        wp.angle.z = wj.value("az", 0.0f);
        wp.scale.x = wj.value("sx", 1.0f);
        wp.scale.y = wj.value("sy", 1.0f);
        wp.scale.z = wj.value("sz", 1.0f);
        wp.duration = wj.value("dur", 1.0f);
        // 保存されたイージングを復元（なければLinear）
        wp.easing = (EasingType)wj.value("ease", (int)EasingType::OutExpo);

        //加算をするかのフラグ
        wp.m_AddFlag = wj.value("addflag",false);
        EditorWaypoints.push_back(wp);
    }
}

void TransformDirector::CaptureCurrentState()
{
    TransformWayPoint wp;
    auto trans = GetGameObject()->transform;
    wp.pos = trans->localPosition;
    wp.angle = trans->localRotation;
    wp.scale = trans->localScale; // 追加
    wp.duration = 1.0f;
    wp.m_AddFlag = false;
    wp.easing = EasingType::OutExpo;
    EditorWaypoints.push_back(wp);
}

void TransformDirector::PlayMoveByIndex(int idx)
{
    if (idx >= 0 && idx < (int)EditorWaypoints.size()) {
        PlayMove(EditorWaypoints[idx], EditorWaypoints[idx].easing);
    }
}

void TransformDirector::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

    for (int i = 0; i < (int)EditorWaypoints.size(); ++i) {
        const std::string actionName_ = "Trans_" + std::to_string(i);

        owner_->RegisterGlobalAction("Transform", actionName_, [this, i]() {
            // 個別設定されたイージングで再生
            this->PlayMoveByIndex(i);
            });
    }
}
