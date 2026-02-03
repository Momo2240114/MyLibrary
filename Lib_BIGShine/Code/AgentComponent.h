#pragma once

#include"ComponentBase.h"

#include <DetourNavMesh.h>
#include <DetourCrowd.h>
#include <Recast.h>
#include "DetourNavMeshBuilder.h"

class MovementComponent;

class AgentComponent :public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name = "Agent";
        return name;
    }
    void Start();
    void Update(float deltaTime);

    //最大速度
    void SetMaxSpeed(float maxSpeed);

    //加速度
    void SetMaxAcceleration(float acceleration);

    const Vector3& GetDirection();

   // void LateUpdate(float et);
    void Gui();

    //bool SetTargetPosition(const Vector3& position,float radius);

    //ランダムな場所を指定する
    bool SetRandomTargetPosition(const Vector3& position, float radius);

    //指定した場所に向かわせる
    void SetTargetPosition(const Vector3& position);

    //スピードの目標の数値を指定する
    void RequestMoveVelocity(float speed);

    //目標地点をリセットする
    void Reset();

    //ターゲットのステートをチェックする
    bool TargetStateCheck(MoveRequestState state);

    const Vector3& GetTargetPosition()
    {
        return m_TargetPosition;
    }


    int GetAgentIndex()
    {
        return m_AgentIndex;
    }

    float GetTargetRadius()
    {
        return m_TargetRadius;
    }

    void SetIsArrive(bool flag)
    {
        m_ArriveFlag = flag;
    }

    bool GetIsArrive()
    {
        return m_ArriveFlag;
    }


    void SetCurrentPositiion(const Vector3& currentPosition)
    {
        m_CurrentPosition = currentPosition;
    }

    const Vector3& GetCurrentPosition()
    {
        return m_CurrentPosition;
    }
private:

    int m_AgentIndex;


    //Crowに必要なデータ
    float m_Radius = 0.5f;
    float m_Height = 1.0f;
    float m_MaxAcceleration = 8.0f;
    float m_MaxSpeed = 3.5f;
    float m_CollisionQueryRange = 6.0f;
    float m_PathOptimizationRange = 15.0f;
    char  m_updateFlags;
    float m_ObstacleAvoidanceType = 3;
    float m_SaparationWeight = 2.0f;
    float m_Position[3] = { 0,2,0 };

    Vector3 m_TargetPosition = { 10,10,10 };

    //1.0fにするとエージェントが目的地を通り越して戻ろうとするベクトルでエージェントがかくついてしまう問題が発生する(多分)
    float m_TargetRadius = 1.5f;

    bool m_ArriveFlag = true;
    Vector3 m_CurrentPosition = { 0,0,0};

    const dtCrowdAgent* m_pAgent;

    std::weak_ptr<MovementComponent> m_pMovementComponent;

    Vector3 m_TestPosition;
};