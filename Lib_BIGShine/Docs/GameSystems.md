# ANATHEMA のゲームシステム

## 目次

- [戦闘の構成](#戦闘の構成)
- [プレイヤー](#プレイヤー)
- [敵 AI](#敵-ai)
- [当たり判定と演出](#当たり判定と演出)
- [カメラ・時間制御](#カメラ時間制御)
- [見せ方について](#見せ方について)

## 戦闘の構成

ゲーム側の処理は Engine の GameObject／Component、戦闘 System、AI Graph、Action Sequence、EffectManager を組み合わせています。作品概要としては 1 対 1 のボス戦を想定した構成ですが、ここでは実装が確認できるプレイヤー操作・ボス AI・衝突・演出の基盤を紹介します。

## プレイヤー

`PlayerControllerComponent` はプレイヤー制御の中心で、関連する実行処理は `PlayerControllerComponent.Execution.cpp` に分割されています。`PlayerActionState` や AnimationController、CharacterMovement、Combat Component と連携する構成です。操作の状態遷移、コンボ受付、各入力の詳細は Controller と関連 State の実装を確認してください。

## 敵 AI

ドラゴンは `DragonAIComponent`、`DragonAttackControllerComponent` 等、人型ボスは `HumanBossAIComponent` と Action Component 群で構成されています。判断 Graph のノード／Blackboard と、ボス固有の Action 実装が連携します。攻撃パターン名だけで実装条件を推測せず、Graph JSON とコードを照合してください。

## 当たり判定と演出

Physics 側には `HitBox`、`HurtBox`、`JustDodgeCollider` があり、戦闘 System が攻撃判定やジャスト回避等を扱います。`MotionWarpComponent` は移動・アニメーションに関連する距離補正用 Component として存在します。詳細な適用条件は各実装を参照してください。

## カメラ・時間制御

カメラには Lock-On、Director、Follow／Combat composition など複数の Component があり、演出 Action から FOV やカメラ処理を制御できます。TimeScale 系の処理は Manager と演出処理の呼び出し経路を確認した上で利用箇所を追ってください。

## 見せ方について

ソース上では、Just Dodge／Just Guard に関する System と反応 Component、Action Registry の振動 Action、Sequence によるタイミング制御、Particle／MeshVFX の発生口を確認できます。これらを組み合わせることで、成功時の反応や攻撃演出を戦闘ロジックから呼び出せます。スローモーションやカメラ演出の具体的な作品内採用箇所は該当 Action とゲームデータを確認して追記する余地があります。

### 関連コード

- [PlayerControllerComponent.cpp](../Code/Game/Charcter/Player/PlayerControllerComponent.cpp)
- [PlayerControllerComponent.Execution.cpp](../Code/Game/Charcter/Player/PlayerControllerComponent.Execution.cpp)
- [PlayerActionState.cpp](../Code/Game/Charcter/Player/PlayerActionState.cpp)
- [DragonAIComponent.cpp](../Code/Game/AI/Dragon/DragonAIComponent.cpp)
- [DragonAttackControllerComponent.cpp](../Code/Game/AI/Dragon/DragonAttackControllerComponent.cpp)
- [HumanBossAIComponent.cpp](../Code/Game/AI/Human/HumanBossAIComponent.cpp)
- [HitBox.cpp](../Code/Engine/Physics/HitBox.cpp)
- [HurtBox.cpp](../Code/Engine/Physics/HurtBox.cpp)
- [JustDodgeSystem.cpp](../Code/Engine/GamePlay/System/JustDodgeSystem.cpp)
- [JustGuardSystem.cpp](../Code/Engine/GamePlay/System/JustGuardSystem.cpp)
- [MotionWarpComponent.cpp](../Code/Engine/GamePlay/Core/Component/MotionWarpComponent.cpp)
- [CameraDirectorComponent.cpp](../Code/Engine/GamePlay/Camera/CameraDirectorComponent.cpp)

優先して読むコードは [CodeHighlights](CodeHighlights.md) にまとめています。
