# AI / Behavior Graph

## 目次

- [グラフのデータ](#グラフのデータ)
- [評価方式](#評価方式)
- [Blackboard と Action](#blackboard-と-action)
- [ボス AI](#ボス-ai)
- [関連コード](#関連コード)

## グラフのデータ

実装名は BehaviorTree ではなく `AIBehaviorGraph` です。Root、Selector、Sequence、RandomSelector、BlackboardCondition、SetBlackboard、PlayAction、Wait、WaitBlackboardBool、SubGraph などのノード種別を持ち、Node と Link を JSON 保存できます。ノードの実行順、条件値、Action 名、Blackboard の範囲やキーはノードデータに含まれます。

## 評価方式

`AIControllerComponent` が Graph を保持し、実行時コンテキスト（Owner、Target、Local／Global Blackboard、位置・距離等）を渡します。Graph は Root から子ノードを評価し、`SUCCESS`、`FAILURE`、`RUNNING` を返します。Composite の順序やノードごとの継続状態は `AIBehaviorGraph.cpp` の評価処理で管理されます。

## Blackboard と Action

ノードは Local または Global Blackboard を指定できます。Blackboard Condition は存在・比較条件を評価し、SetBlackboard は値を書き込みます。PlayAction は Owner の Action を開始し、設定に応じて Blackboard の終了フラグを待ちます。これにより、条件・行動の並びを Graph データで表しつつ、具体的な動作は既存の Action 実装へ委譲できます。

## ボス AI

プロジェクトには `Data/AI/BehaviorGraph/DragonFirstForm.json` と `HumanBoss.json` があり、ゲーム側に Dragon／HumanBoss の AI Component 群があります。どの攻撃がどの条件で選ばれるかは各 JSON のノードと対応 Action を合わせて読む必要があります。距離や戦闘フェーズ等の具体的条件は、該当 Graph データと Component の実装に明記されているものに限定して確認してください。

`AIBehaviorEditorWindow` は Graph 編集用の窓です。実行時評価状態を Editor に通知する仕組みも Graph 内にあります。

### 関連コード

- [AIBehaviorGraph.h](../Code/Engine/GamePlay/AI/AIBehaviorGraph.h)
- [AIBehaviorGraph.cpp](../Code/Engine/GamePlay/AI/AIBehaviorGraph.cpp)
- [AIControllerComponent.cpp](../Code/Engine/GamePlay/AI/AIControllerComponent.cpp)
- [AIBehaviorEditorWindow.cpp](../Code/Editor/Workspace/AIBehaviorEditorWindow.cpp)
- [DragonFirstForm.json](../Data/AI/BehaviorGraph/DragonFirstForm.json)
- [HumanBoss.json](../Data/AI/BehaviorGraph/HumanBoss.json)
- [DragonAIComponent.cpp](../Code/Game/AI/Dragon/DragonAIComponent.cpp)
- [HumanBossAIComponent.cpp](../Code/Game/AI/Human/HumanBossAIComponent.cpp)

実際に見てほしい処理は [CodeHighlights](CodeHighlights.md) を参照してください。
