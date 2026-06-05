//#pragma once
//
//#include "Engine/GamePlay/Core/Component/ComponentBase.h"
//
//#include <string>
//#include <vector>
//#include <map>
//#include <memory>
//
//// 遷移条件のデータ構造
//struct StateTransition {
//    std::string triggerAction; // "Attack", "Jump" など
//    int targetStateID;         // 遷移先のノードID
//};
//
//// 状態（ノード）のデータ構造
//struct PlayerState {
//    int id;
//    std::string name;
//    int animationIndex;        // この状態になった時に再生するアニメ
//    std::vector<StateTransition> transitions;
//
//    // オプション：状態ごとの振る舞い制限（移動可能か、など）
//    bool canMove = true;
//};
//
//class PlayerStateMachine : public Component
//{
//private:
//    std::map<int, PlayerState> m_states;
//    int m_currentStateID = 0;
//
//public:
//    void Update(float deltaTime) override;
//
//    void TransitionTo(int nextID);
//
//    // ノードエディタからデータを流し込む用
//    void AddState(const PlayerState& state) { m_states[state.id] = state; }
//};