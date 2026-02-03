#pragma once

#include"ComponentBase.h"
#include"MovementComponent.h"
#include"AgentComponent.h"
#include"PlayerComponent.h"

enum class EnemyState
{
    Idle,
    Walk,
    Run,
    Attack,
    Turn
};

class BehaviorTree;
class BehaviorData;
class NodeBase;
class AnimationController;
class CapsuleCollider;

class EnemyComponent : public Component
{
public:

    void Start();
    void Update(float et);

    ~EnemyComponent();

    void LateUpdate(float et);

    void Gui();
    const std::string& GetName() const override
    {
        static std::string name = "Enemy";
        return name;
    }

    void Serialize(nlohmann::json& j) const;

    bool Visibility();
    bool AttackRange();

    void PlayerChase();

    bool IsSeePlayer(uint32_t layerMask);

    void SetAttackFlag(bool flag)
    {
        m_AttackFlag = flag;
    }

    void SetTurnFlag(bool flag)
    {
        m_TurnFlag = flag;
    }

    bool GetAttackFlag()
    {
        return  m_AttackFlag;
    }

    Vector3 GetTargetPosition()
    {
        return m_pPlayerGameObject->transform->GetWorldPosition();
    }

    //アタッチメントとゲームオブジェクトが降れいているのかどうか
    bool HitPlayer();

    void SetUseNavMeshFlag(bool flag)
    {
        m_UseNavMeshFlag = flag;
    }

    bool GetUseNavMeshFlag()
    {
        return m_UseNavMeshFlag;
    }

    void Deserialize(const nlohmann::json& j);

    bool StateCheck(PlayerStateType  state);

    bool GetPlayerSeeFlag()
    {
        return m_PlayerSeeFlag;
    }
    void SetPlayerSeeFlag(bool flag)
    {
        m_PlayerSeeFlag = flag;
    }

    //スニークオブジェクトの位置
    const Vector3& GetHidePosition();

    void SetSneakObjectName(std::string name)
    {
        SneakObjectName = name;
    }
    //警戒
    bool Caution();
private:
    std::shared_ptr<MovementComponent> m_pMovementComponent;
    std::shared_ptr<AgentComponent>m_pAgentComponent;
    std::shared_ptr<AnimationController>m_pAnimationComponent;
    std::shared_ptr<CapsuleCollider> m_pCapsuleCollider;


    std::string m_targetObjectName;
    GameObject* m_pPlayerGameObject;
    GameObject* m_AreaGameObject[6];

    bool m_Input = false;

    BehaviorTree* m_pBehaviorTree;
    BehaviorData* m_pBehaviorData;
    NodeBase* m_pActiveNode;

    Vector3 m_OldPlayerPos{0,0,0};
    float m_VisibilityRadius = 15.0f;
    float m_AttackRadius = 2.0f;
    float m_CautionRadius = 7.0f;

    bool m_AttackFlag = false;
    bool m_TurnFlag = false;
    int m_PlayerTrace = -1;

    float m_Fov = 180;

    std::weak_ptr<GameObject> m_pAttachimentGameObject;

    bool m_PlayerSeeFlag = false;

    bool m_UseNavMeshFlag = true;

    std::string SneakObjectName = "";

    bool m_TestFlag = false;
};