# Play Mode 撤去

> 2026-10-19: この撤去の後、ユーザー依頼によりPlay Modeを再導入済み。以下は撤去時の履歴です。現在の状態は `Tool/PlayModeRestorationReport.md` を参照してください。

## 変更内容

ユーザー依頼により、Scene用のPlay / Pause / Resume / Stopボタン、Editor / Play状態の切り替え、開始前のスナップショット退避と復元、Scene遷移後の編集元への復帰、Play専用の入力捕捉、物理・音声停止、Scene/Prefab保存制限を撤去。

SceneGameはMainSceneを読み込み、GameModeManager::BeginGameSceneFlowで従来どおりイントロを開始する。GameObjectManagerは既存のLOADING / INITIALIZING / RUNNING / FAILEDによる更新へ戻し、シーンはディスクから読み込む。中央SceneのDock表示は導入前の既定値（無効）へ戻した。既存のDock表示切り替えと別視点Editor Scene Viewは残している。

HUD、入力デバイスごとのガイド、文字の太さ、HUDの実行時表示をScene保存値から分離する修正、形態変化で削除された配置Objectの保存保護、イントロのカメラ優先度修正は維持。Workspaceやアニメーション/Sequence単体プレビューの再生機能も維持。

ロード完了フラグのatomic化とSceneLoadingの二重更新防止、非表示タブの走査省略は独立した修正として維持。SceneのJSONとAssetファイルは変更していない。FPSの回復は未測定。

## 変更ファイル

- Code/Engine/GamePlay/Core/Object/GameObjectManager.h
- Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp
- Code/Engine/GamePlay/Scene/SceneManager.h
- Code/Engine/GamePlay/Scene/SceneManager.cpp
- Code/Engine/GamePlay/Scene/SceneGame.cpp
- Code/Engine/GamePlay/Scene/SceneBase.cpp
- Code/Engine/GamePlay/Scene/SceneLoading.cpp
- Code/Engine/GamePlay/Camera/CameraManager.h
- Code/Engine/GamePlay/Camera/CameraManager.cpp
- Code/Engine/GamePlay/EventSystem/ActionSequence.cpp
- Code/Engine/Core/framework.cpp
- Code/Engine/Audio/AudioManager.h
- Code/Engine/Audio/AudioManager.cpp
- Code/Editor/Window/EditorWindowManager.h
- Code/Editor/Window/EditorWindowManager.cpp
- Code/Editor/Window/EditorSceneViewWindow.h
- Code/Editor/Window/EditorSceneViewWindow.cpp
- Code/Editor/Window/GameModeWindow.cpp
- Tool/EditorPlayModeIntegrationReport.md（導入時の履歴である旨を追記）
- Tool/PlayModePerformanceReport.md（調査時の履歴である旨を追記）
- Tool/PlayModeRemovalReport.md（本書）

## コメント

Scene読込からイントロを開始する経路、別視点カメラの初期化と操作開始時の視点同期について、日本語コメントを実装に合わせて記述。撤去した処理専用のコメントも取り除いた。

## Tracy

撤去した機能専用のEditorPlayModeスコープ、別視点のPlay専用Plot、Start待ち数の調査用Plotを削除。既存のScene読込、通常更新、イベント開始、物理、音声、描画の計測と既存マクロの有効化方式は維持。

## ファイル形式

対象ファイルはUTF-8、BOMなし、CRLF。保存後に厳密なUTF-8で検査。

## Visual Studio登録

新規C++ファイルなし。変更した既存18ソース/ヘッダーのvcxprojとvcxproj.filters登録を確認。実ファイルの移動・Filter変更なし。

## 確認内容

Code全体で撤去したPlay専用API・状態・GUIの参照が残っていないこと、宣言と定義、呼出し、起動からMainScene/イントロへの経路、保存制限の解除、音声・物理の従来更新経路、ソースの括弧対応を静的に確認。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
