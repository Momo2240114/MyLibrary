#pragma once
#include <string>

class PlayerControllerComponent;

class PlayerState
{
public:
    virtual ~PlayerState() = default;

    virtual const char* GetName() const = 0;

    virtual void Enter(PlayerControllerComponent& player_) {}
    virtual void Update(PlayerControllerComponent& player_, float dt_) = 0;
    virtual void Exit(PlayerControllerComponent& player_) {}
};

//ステートの定義をインターフェイスといっしょに定義しておく
enum class PLAYER_STATE
{
    IDLE,
    MOVE,
    JUMP,
    FALL,
    ATTACK,

    NONE
};