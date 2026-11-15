# BigShine / ANATHEMA

C++20 と DirectX 11 で開発している自作ゲームエンジン「BigShine」と、その上で制作した 3D ボスバトルアクション「ANATHEMA — 忌まわしき影」のプロジェクトです。

## プロジェクト概要

ANATHEMA は、剣と盾を使うプレイヤーがドラゴン、人型ボスと戦う 1 対 1 のアクションゲームです。攻撃・回避・ガード、アニメーション、AI、カメラ、エフェクトを組み合わせた戦闘を、BigShine のゲームオブジェクト／コンポーネント基盤上に構成しています。

BigShine には、シーン編集、描画、物理、アニメーション制御、Action Sequence、Behavior Graph、Particle／Mesh VFX などのランタイムと ImGui ベースのエディタ機能があります。実装と制約は各技術資料から確認できます。

## 技術

| 分類 | 内容 |
| --- | --- |
| 言語・API | C++20、Direct3D 11、HLSL |
| 開発環境 | Visual Studio 2022 のソリューション／プロジェクト |
| 描画・編集 | ImGui、ImGuizmo、glTF 関連コード |
| ゲーム基盤 | GameObject / Component、PhysX を利用する衝突処理、JSON データ |

## 技術ドキュメント

- [エンジン全体の構成](Docs/Architecture.md)
- [GameObject / Component](Docs/GameObject_Component.md)
- [描画システム](Docs/Rendering.md)
- [Animation Sequence と ActionRegistry](Docs/AnimationSequence.md)
- [AI / Behavior Graph](Docs/AI_BehaviorTree.md)
- [Effect System](Docs/EffectSystem.md)
- [ANATHEMA のゲームシステム](Docs/GameSystems.md)
- [注目してほしいソースコード](Docs/CodeHighlights.md)

## まず見てほしいコード

1. [GameObject](Code/Engine/GamePlay/Core/Object/GameObject.h) と [GameObjectManager](Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp)
2. [ActionSequence](Code/Engine/GamePlay/EventSystem/ActionSequence.cpp) と [ActionRegistry](Code/Engine/GamePlay/Serialize/ActionRegistry.cpp)
3. [AIBehaviorGraph](Code/Engine/GamePlay/AI/AIBehaviorGraph.cpp) と [AIControllerComponent](Code/Engine/GamePlay/AI/AIControllerComponent.cpp)
4. [ModelRenderSystem](Code/Engine/Graphics/RenderSystem/ModelRenderSystem.cpp)
5. [PlayerControllerComponent](Code/Game/Charcter/Player/PlayerControllerComponent.cpp)

## 動画・ポートフォリオ

- ゲームプレイ動画：[URLを追加]
- ポートフォリオ：[URLを追加]

## ビルド・実行

Visual Studio 2022 用の `BIGShine.sln`（C++20）が含まれています。プロジェクト設定は x64 を含む複数構成を持ち、外部依存の一部は `CustomExternalDependencies.props` に定義されています。FBX SDK は Autodesk のローカルインストール先を参照しているため、別環境ではパスや SDK の準備が必要です。実行時データの完全性やクリーン環境での具体的な手順は未確認です。公開前に必要な SDK・ライブラリ・起動手順を追記してください。ルートに実行ファイルやビルド成果物が含まれているため、配布対象は整理が必要です。

## 使用ライブラリ・アセット

`External/` に複数の外部ライブラリが含まれています。再配布条件と著作権表示を確認したうえで、公開時に主要ライブラリとライセンスへのリンクを追記してください。ゲームアセットの権利・公開可否も確認が必要です。現時点では網羅したライセンス一覧を確認できていないため、特定のライセンス条件はここでは断定していません。

---

各システムの説明とコード上の根拠は [CodeHighlights](Docs/CodeHighlights.md) および各技術資料を参照してください。
