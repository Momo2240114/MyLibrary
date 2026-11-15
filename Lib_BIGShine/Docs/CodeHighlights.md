# 注目してほしいソースコード

## 目次

- [GameObject / Component](#gameobject--component)
- [ObjectManager と更新制御](#objectmanager-と更新制御)
- [RenderSystem](#rendersystem)
- [AnimationSequence / ActionRegistry](#animationsequence--actionregistry)
- [Behavior Graph / Blackboard](#behavior-graph--blackboard)
- [EffectManager / GPU Particle](#effectmanager--gpu-particle)
- [PlayerController](#playercontroller)
- [Boss AI](#boss-ai)

## GameObject / Component

**概要**

GameObject が Transform、Component、Collider、Object 固有 Action をまとめ、個別機能を Component へ割り当てます。

**見どころ**

型付き Component 追加／取得と、Collider Component の専用登録・検索の両方が同じ API で扱われます。

**関連コード**

[GameObject.h](../Code/Engine/GamePlay/Core/Object/GameObject.h) · [GameObject.cpp](../Code/Engine/GamePlay/Core/Object/GameObject.cpp)

**注目してほしい処理**

`AddComponent<T>`、`GetComponent<T>`、`CallAllComponentStart`、`Update`、`PrepareForDestroy`、`Save` / `Load`。

**関連ドキュメント**

[GameObject / Component](GameObject_Component.md)

## ObjectManager と更新制御

**概要**

GameObjectManager は Scene の Object 群と複数のゲームプレイ／描画 System を管理します。

**見どころ**

Scene のロード状態と実行状態を分け、開始待ち・削除待ちを使って準備完了とコンテナ変更を扱います。

**関連コード**

[GameObjectManager.h](../Code/Engine/GamePlay/Core/Object/GameObjectManager.h) · [GameObjectManager.cpp](../Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp)

**注目してほしい処理**

`Update`、`UpdateLoadingRuntime`、`UpdateRunningRuntime`、`ProcessPendingStarts`、`Destroy`。

**関連ドキュメント**

[Architecture](Architecture.md)

## RenderSystem

**概要**

モデル描画要求を収集し、描画パスへ発行します。

**見どころ**

描画統計には静的／スキンモデル、描画回数、Shader／Material bind 等の調査値があります。

**関連コード**

[ModelRenderSystem.h](../Code/Engine/Graphics/RenderSystem/ModelRenderSystem.h) · [ModelRenderSystem.cpp](../Code/Engine/Graphics/RenderSystem/ModelRenderSystem.cpp)

**注目してほしい処理**

`PushSubmit`、`RenderAll`、スキンモデル用定数バッファと描画パス。

**関連ドキュメント**

[Rendering](Rendering.md)

## AnimationSequence / ActionRegistry

**概要**

JSON の Sequence データから、時刻指定で Action callback を実行します。

**見どころ**

編集データは関数ポインタではなくカテゴリ／Action 名を保持します。ランタイムは Registry で callback を解決し、実行直前に任意値を Blackboard へ書き込めます。

**関連コード**

[ActionSequence.cpp](../Code/Engine/GamePlay/EventSystem/ActionSequence.cpp) · [ActionRegistry.cpp](../Code/Engine/GamePlay/Serialize/ActionRegistry.cpp)

**注目してほしい処理**

`StartSequence`、`Update`、`RegisterSequence`、`Serialize` / `Deserialize`、`RegisterAllActions`。

**関連ドキュメント**

[AnimationSequence](AnimationSequence.md)

## Behavior Graph / Blackboard

**概要**

ノードと Link で表された AI 判断 Graph を評価します。

**見どころ**

Composite、条件、Action、Wait の実行結果を Success／Failure／Running で返し、Node の状態を保持します。Local／Global Blackboard のスコープ指定があります。

**関連コード**

[AIBehaviorGraph.h](../Code/Engine/GamePlay/AI/AIBehaviorGraph.h) · [AIBehaviorGraph.cpp](../Code/Engine/GamePlay/AI/AIBehaviorGraph.cpp) · [AIControllerComponent.cpp](../Code/Engine/GamePlay/AI/AIControllerComponent.cpp)

**注目してほしい処理**

`Tick`、`TickComposite`、`TickTask`、Blackboard 条件評価。

**関連ドキュメント**

[AI / Behavior Graph](AI_BehaviorTree.md)

## EffectManager / GPU Particle

**概要**

EffectManager が要求に応じた Particle、MeshVFX、Decal 等を生成・追跡し、Pool と寿命を管理します。

**見どころ**

Effect の呼び出しパラメータ、Asset 解決、再利用、描画 System への提出を分けて確認できます。GPU Particle 用 Compute／Graphics Shader も存在します。

**関連コード**

[EffectManager.h](../Code/Engine/GamePlay/System/EffectManager.h) · [EffectManager.cpp](../Code/Engine/GamePlay/System/EffectManager.cpp) · [ParticleEmitter.cpp](../Code/Engine/Graphics/Particle/ParticleEmitter.cpp) · [integrate_particle_cs.hlsl](../Shader/GPUParticle/integrate_particle_cs.hlsl)

**注目してほしい処理**

`SpawnParticle`、`SpawnMeshVFX`、`RegisterManagedEffect`、`AcquireParticleObject`、Emitter 更新と Compute Shader 経路。

**関連ドキュメント**

[EffectSystem](EffectSystem.md)

## PlayerController

**概要**

ANATHEMA のプレイヤー制御 Component と実行処理です。

**見どころ**

入力・状態・移動・攻撃の関係を Controller と実行処理ファイル、PlayerActionState の間で確認できます。

**関連コード**

[PlayerControllerComponent.h](../Code/Game/Charcter/Player/PlayerControllerComponent.h) · [PlayerControllerComponent.cpp](../Code/Game/Charcter/Player/PlayerControllerComponent.cpp) · [PlayerControllerComponent.Execution.cpp](../Code/Game/Charcter/Player/PlayerControllerComponent.Execution.cpp)

**注目してほしい処理**

Controller の Update と状態／Action 実行処理。

**関連ドキュメント**

[GameSystems](GameSystems.md)

## Boss AI

**概要**

Dragon と Human Boss のゲーム固有 AI、および Graph データです。

**見どころ**

Graph のデータだけで完結せず、ボス固有の Action／Component が実際のアニメーションや攻撃処理を担います。

**関連コード**

[DragonAIComponent.cpp](../Code/Game/AI/Dragon/DragonAIComponent.cpp) · [HumanBossAIComponent.cpp](../Code/Game/AI/Human/HumanBossAIComponent.cpp) · [DragonFirstForm.json](../Data/AI/BehaviorGraph/DragonFirstForm.json) · [HumanBoss.json](../Data/AI/BehaviorGraph/HumanBoss.json)

**注目してほしい処理**

Graph の条件／Action ノードと、対応するボス Component の実行関数を行き来して確認してください。

**関連ドキュメント**

[AI / Behavior Graph](AI_BehaviorTree.md) · [GameSystems](GameSystems.md)
