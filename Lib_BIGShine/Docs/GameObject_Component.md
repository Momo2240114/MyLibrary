# GameObject / Component

## 目次

- [役割分担](#役割分担)
- [生成と開始](#生成と開始)
- [更新と削除](#更新と削除)
- [シーンデータ](#シーンデータ)
- [ゲームでの利用](#ゲームでの利用)

## 役割分担

`GameObject` はシーン上の個体を表し、Transform、名前、Component の共有ポインタ配列、ローカル ActionRegistry を持ちます。機能の具体的な処理は Component 側に置かれます。Collider 系は通常 Component と別配列に置かれ、Physics 登録・更新順を区別しています。

`AddComponent<T>` は Component を生成して所有 GameObject を設定し、型に応じて通常配列か Collider 配列へ登録します。`GetComponent<T>` は両方の配列を検索します。`GetOrAddComponent<T>` は既存を検索し、なければ追加します。

## 生成と開始

`GameObjectManager::Create` は GameObject を生成して Manager に登録します。通常のランタイムでは Start 待ちリストに入り、必要なリソースと初期化条件が整った後に `CallAllComponentStart` が呼ばれます。Collider Component は先に Start され、後続の通常 Component が物理情報を参照できる順序です。Component を Object 作成時に追加する場合は `AddComponent` の既定動作でその場で Start しますが、Start を遅らせる引数もあります。

## 更新と削除

`GameObject::Update` は有効かつ開始済みで更新対象の Component を処理します。一般 Component を集めたうえで、CharacterMovement と PhysicsCollision を専用順序で扱う実装が確認できます。具体的な処理順は変更され得るため、正確な順序は関数本体を参照してください。

削除時は Manager が `Destroy` で破棄を予約し、Object は `PrepareForDestroy` により戦闘参加を止め、Component の終了処理を行います。実際のコンテナからの削除は更新中の配列を壊さないよう待機リスト側で反映されます。

## シーンデータ

`GameObject::Save` / `Load` は名前、Layer、Collision Mask、Transform、Component 構成を JSON として扱います。Component の型復元は `ComponentRegistry` と登録処理に接続されています。個々の Component が保存する項目は各実装によります。

## ゲームでの利用

プレイヤーやボスは GameObject に操作・アニメーション・戦闘・描画などの Component を組み合わせて表現されています。例として [PlayerControllerComponent](../Code/Game/Charcter/Player/PlayerControllerComponent.cpp) と [DragonAIComponent](../Code/Game/AI/Dragon/DragonAIComponent.cpp) を参照してください。Object 単位の生成・更新管理を共有しながら、ゲーム固有の処理を別 Component として実装できます。

### 関連コード

- [GameObject.h](../Code/Engine/GamePlay/Core/Object/GameObject.h)
- [GameObject.cpp](../Code/Engine/GamePlay/Core/Object/GameObject.cpp)
- [GameObjectManager.h](../Code/Engine/GamePlay/Core/Object/GameObjectManager.h)
- [GameObjectManager.cpp](../Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp)
- [ComponentBase.h](../Code/Engine/GamePlay/Core/Component/ComponentBase.h)
- [ComponentRegistry.cpp](../Code/Engine/GamePlay/Serialize/ComponentRegistry.cpp)

全体像は [Architecture](Architecture.md)、実装の見どころは [CodeHighlights](CodeHighlights.md) を参照してください。
