#include "PrecompiledHeader.h"



#include "MovementComponent.h"
#include"Engine/GamePlay/Core/Object/GameObject.h"

#include"Engine/Math/RandomHelper.h"

void MovementComponent::Start()
{
	m_RotationType = RotationType::Turn;
}

void MovementComponent::Update(float et)
{
	//移動処理
	Move(et);

	switch (m_RotationType)
	{
	case RotationType::Turn:
		//旋回処理
		Turn(et);
		break;
	case RotationType::LookAtVelocity:
		LookAtVelocity(et);
		break;
	}

	m_Direction = { 0,0,0 };
}

//移動処理
void MovementComponent::Move(float et)
{
	float speed = m_MoveSpeed * et;
	Vector3 direction = m_Direction * speed;
	GetGameObject()->transform->AddMoveOffset(direction);
}

void MovementComponent::ZeroDirection()
{
	m_Direction = { 0,0,0 };
}

//旋回処理
void MovementComponent::Turn(float et)
{
	// 1. 現在の回転を取得
	Vector3 currentRot = GetGameObject()->transform->localRotation;

	// 2. 加算する量（オフセット）を計算
	Vector3 deltaRot = m_Angle * m_TurnSpeed * et;

	// 3. 合計角度を計算
	Vector3 nextRot = currentRot;
	nextRot.x += deltaRot.x;
	nextRot.y += deltaRot.y;
	nextRot.z += deltaRot.z;

	// 4. X軸（ピッチ）の回転制限
	if (m_IsPitchLimit)
	{
		// 0~360度を -180~180度に変換して計算しやすくする
		float normalizedX = nextRot.x;
		if (normalizedX > 180.0f) normalizedX -= 360.0f;

		// クランプ処理 (m_MinPitch: 上限, m_MaxPitch: 下限)
		// 例: -80度 ～ 80度
		if (normalizedX < m_MinPitch) normalizedX = m_MinPitch;
		if (normalizedX > m_MaxPitch) normalizedX = m_MaxPitch;

		// 0~360度の仕様に戻す
		if (normalizedX < 0.0f) {
			nextRot.x = normalizedX + 360.0f;
		}
		else {
			nextRot.x = normalizedX;
		}
	}

	// 5. 最終的な角度を Transform に反映
	// AddRotateOffset ではなく SetRotation を使うことで、制限後の値を直接上書きする
	GetGameObject()->transform->SetRotation(nextRot);

	// 6. データをリセット
	m_Angle = { 0, 0, 0 };
}

void MovementComponent::Gui()
{
	ImGui::InputFloat3("Direction", &m_Direction.x);
	ImGui::InputFloat("MoveSpeed",&m_MoveSpeed);
	ImGui::InputFloat3("Rotation", &offsetRotation.x);
	ImGui::InputFloat3("Angle", &angle.x);
	ImGui::Separator();
	ImGui::Text("Rotation Limit");
	ImGui::Checkbox("Enable Pitch Limit", &m_IsPitchLimit);
	if (m_IsPitchLimit)
	{
		ImGui::DragFloat("Min Pitch", &m_MinPitch, 1.0f, -90.0f, 0.0f);
		ImGui::DragFloat("Max Pitch", &m_MaxPitch, 1.0f, 0.0f, 90.0f);
	}
}

void MovementComponent::LookAtVelocity(float et)
{
	std::shared_ptr<GameObject> gameObject = GetGameObject();
	float speed = m_TurnSpeed * et;

	//現在の回転行列からクォータニオンを求める
	 angle = gameObject->transform->GetWorldRotation();
	 

	 //ディグリーをラジアンに変換
	 angle.x = DirectX::XMConvertToRadians(angle.x);
	 angle.y = DirectX::XMConvertToRadians(angle.y);
	 angle.z = DirectX::XMConvertToRadians(angle.z);

	 Quaternion quaternion = DirectX::XMQuaternionRotationRollPitchYaw(angle.x,angle.y,angle.z);

	 //前方向ベクトルを求める
	 Matrix  transform = DirectX::XMMatrixRotationQuaternion(quaternion);
	 Vector4 oneZ = { 0,0,1,0 };
	 Vector3 front = DirectX::XMVector3TransformNormal(oneZ,transform);
	 front.Normalize();


	 m_Direction.y = 0;
	 m_Direction.Normalize();

	 //回転軸を求める
	 Vector3 axis = front.Cross(m_Direction);
	 axis.Normalize();


	 if (axis.LengthSquared() < 0.0001f)
	 {
		 return;
	 }

	 float dot = front.Dot(m_Direction);

	 speed = (std::min)(1.0f - dot, speed);

	 axis.Normalize();

	 //Speed分だけ回転するクォータニオンを作成
	 Quaternion turn = DirectX::XMQuaternionRotationAxis(axis,speed);

	 //現在の回転に合成する
	 Quaternion q = quaternion * turn;

	 //// Roll (X軸周り)
	 //float sinRoll = 2.0f * (q.y * q.z + q.w * q.x);
	 //sinRoll = (sinRoll > 1.0f) ? 1.0f : ((sinRoll < -1.0f) ? -1.0f : sinRoll);
	 //float roll = asinf(sinRoll);

	 //// Pitch (Y軸周り)
	 //float t0 = -2.0f * (q.x * q.z - q.w * q.y);
	 //float t1 = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
	 //float pitch = atan2f(t0, t1);

	 //// Yaw (Z軸周り)
	 //float t2 = -2.0f * (q.x * q.y - q.w * q.z);
	 //float t3 = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
	 //float yaw = atan2f(t2, t3);

	 //offsetRotation = { roll, pitch, yaw };


	 Vector3 euler = q.ToEuler();
	 //Vector3 euler = QuaternionToEuler(q);

	 // ディグリーに変換
	 offsetRotation.x = DirectX::XMConvertToDegrees(euler.x);
	 offsetRotation.y = DirectX::XMConvertToDegrees(euler.y);
	 offsetRotation.z = DirectX::XMConvertToDegrees(euler.z);

	 //// ★ X軸（Pitch）の回転制限
	 //if (m_IsPitchLimit)
	 //{
		// // 角度が180度を超えて 270~360度側（マイナス方向）になっている場合の補正
		// if (offsetRotation.x > 180.0f) offsetRotation.x -= 360.0f;

		// // クランプ処理
		// if (offsetRotation.x < m_MinPitch) offsetRotation.x = m_MinPitch;
		// if (offsetRotation.x > m_MaxPitch) offsetRotation.x = m_MaxPitch;
	 //}


	 ////ディグリーに変換
	 //offsetRotation.x = DirectX::XMConvertToDegrees(euler.x);
	 //offsetRotation.y = DirectX::XMConvertToDegrees(euler.y);
	 //offsetRotation.z = DirectX::XMConvertToDegrees(euler.z);


	 GetGameObject()->transform->SetRotation(offsetRotation);
}

void MovementComponent::AddDirection(const Vector3& direction)
{
	//GetGameObject()->transform->MarkDirty();
	m_Direction += direction;
	m_Direction.Normalize();
}

void MovementComponent::SetDirection(const Vector3& direction)
{
	//GetGameObject()->transform->MarkDirty();
	m_Direction = direction;
}

void MovementComponent::AddAngle(const Vector3& angle)
{
	//ディグリーに変換する
	m_Angle.x = DirectX::XMConvertToDegrees(m_Angle.x);
	m_Angle.y = DirectX::XMConvertToDegrees(m_Angle.y);
	m_Angle.z = DirectX::XMConvertToDegrees(m_Angle.z);

	m_Angle += angle ;

	//ラジアン書くに変換する
	m_Angle.x = DirectX::XMConvertToRadians(m_Angle.x);
	m_Angle.y = DirectX::XMConvertToRadians(m_Angle.y);
	m_Angle.z = DirectX::XMConvertToRadians(m_Angle.z);
}


