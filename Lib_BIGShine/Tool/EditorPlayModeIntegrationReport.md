# Editor / Play Mode 統合

> 2026-10-19: 一度撤去した後、ユーザー依頼により再導入済み。以下は初回導入時の履歴です。現在の状態は `Tool/PlayModeRestorationReport.md` を参照してください。

## 変更内容

- SceneGameはEditorを初期状態とし、中央のSceneビュー上部にPlay / Pause（Resume）/ Stopを配置。
- Play直前のSceneをメモリ上のJSONへ退避し、実行用Objectを再構築。Sceneファイルへの自動保存は行わない。
- Stopは未保存の配置、Componentのシリアライズ対象設定、階層、選択Object、編集カメラ、Sceneの描画設定を復元する。
- 実行中に別Sceneへ遷移した場合も、開始元のスナップショットへ戻す。Pause中は予約済みの遷移も保留する。
- Editorでは一般ComponentのStart、AI、戦闘、イベント、ゲーム時計を進めず、モデル・UI・光源・カメラ・Attachmentの編集表示を更新する。
- Pauseはゲーム更新、PhysX、FMODのマスター音声を停止する。
- Play中のScene保存・Scene読込・Scene上ObjectのPrefab保存を抑止する。独立したWorkspaceのプレビューManagerは従来の方式を維持する。
- Play中のScene画像クリックでマウスを捕捉し、Escで解放。Editorでは右ドラッグとWASDによる自由カメラ操作を使用する。
- SceneLoadingの二重更新を除去し、ロード完了フラグを初期化済みatomicへ変更。

復元対象はSceneのシリアライズ対象データ。独立したAsset編集画面から保存したマテリアル等の外部ファイルは巻き戻さない。別プロセス化やEditor専用ビルドの分離は行っていない。

## 変更ファイル

すべて既存の実ファイル配置を維持。

- Code/Engine/GamePlay/Core/Object/GameObjectManager.h / .cpp
- Code/Engine/GamePlay/Scene/SceneManager.h / .cpp
- Code/Engine/GamePlay/Scene/IScene.h
- Code/Engine/GamePlay/Scene/SceneBase.h / .cpp
- Code/Engine/GamePlay/Scene/SceneGame.cpp
- Code/Engine/GamePlay/Scene/SceneLoading.cpp
- Code/Engine/GamePlay/Camera/CameraManager.h / .cpp
- Code/Engine/GamePlay/EventSystem/ActionSequence.cpp
- Code/Engine/Audio/AudioManager.h / .cpp
- Code/Engine/Core/framework.cpp
- Code/Editor/EditorCameraData.cpp
- Code/Editor/Window/EditorWindowManager.h / .cpp
- Code/Editor/Window/EditorSceneViewWindow.cpp
- Code/Editor/Window/GameModeWindow.cpp
- Tool/EditorPlayModeIntegrationReport.md（本書）

## コメント / Tracy

メモリ退避・復元、Editor専用の更新、ロードスレッドとの同期、停止中の遷移保留、音声停止、共有編集カメラの同期に日本語コメントを追加。

既存のBS_PROFILE_SCOPEを使用し、CaptureSnapshot / RestoreSnapshot / EnterPlay / ProcessCommand / UpdateEditorScene / BeginSession / EndSession / ReturnToEditor / Toolbar / CameraManager::ResetRuntimeState / LoadSceneDataを計測。既存の描画・物理・音声の計測範囲を維持。

## ファイル形式 / Visual Studio登録

上記の対象ファイルはUTF-8、BOMなし、CRLFに統一し、厳密なUTF-8で読み込み確認。新しいC++ファイルはなく、既存ファイルのvcxproj登録とfilters登録を静的に確認。実フォルダーの移動、Filterの変更はなし。

## 静的確認

宣言と定義、呼出しAPI、include、再生状態による更新経路、保存抑止、同じJSONによる退避・復元、StopとScene遷移の優先順位、既存ファイルのVisual Studio登録を確認。ソースの括弧対応と対象ファイル形式も確認。

実機での起動時Editor表示、Playでのイントロ開始、Pause/Resume、形態変化後のStop、Scene遷移後のStop、未保存編集の復元、マウス解放はユーザー側での実行確認が必要。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
