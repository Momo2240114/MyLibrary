# Play Mode 再導入

## 変更内容

- 通常起動はEditor。Sceneビュー上部中央のPlayからテスト実行し、Pause / Resume / Stopで制御する。
- Play直前のSceneをメモリ上のJSONへ退避し、実行用Objectを再構築する。スナップショット操作は切替要求時のみで、通常フレームでは行わない。
- Stopで、未保存の配置・Componentのシリアライズ対象設定・階層・選択Object・編集カメラ・Scene描画設定を開始前へ復元する。実行中のScene遷移後も開始元へ戻す。
- EditorではゲームComponent全体のStartを呼ばず、モデル・UI・光源・カメラ・Attachmentを編集表示用に更新する。
- Pause中はゲーム更新・PhysX・FMOD音声・予約済みのScene遷移を止める。非同期のGPU転送は完了できる。
- Play中のScene保存・読込・Scene上ObjectのPrefab保存を抑止し、実行状態の上書きを防ぐ。
- Scene画像クリックでマウスを捕捉し、Escで解放する。カーソル捕捉設定は変更が必要なフレームだけ更新し、自由マウスの移動差分を毎フレーム消さない。
- Editor専用カメラは右ドラッグとWASD等で操作し、Playではゲームカメラを使う。

HUD、入力ガイド、文字の太さ、実行時の表示値と保存値の分離、形態変化後の配置Object保存保護、イントロのカメラ優先度修正を維持。独立したWorkspaceと単体プレビューの動作も維持する。SceneのJSON、Asset、画質、VSync、描画解像度は変更していない。

復元対象はSceneのシリアライズ対象データ。別のAsset編集画面から外部ファイルへ保存したマテリアル等は、Stopで巻き戻さない。

## 変更ファイル

既存の17ソース/ヘッダーを変更。

- Code/Engine/GamePlay/Core/Object/GameObjectManager.h
- Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp
- Code/Engine/GamePlay/Scene/SceneManager.h
- Code/Engine/GamePlay/Scene/SceneManager.cpp
- Code/Engine/GamePlay/Scene/SceneGame.cpp
- Code/Engine/GamePlay/Scene/SceneBase.cpp
- Code/Engine/GamePlay/Scene/SceneLoading.cpp
- Code/Engine/GamePlay/Camera/CameraManager.h
- Code/Engine/GamePlay/Camera/CameraManager.cpp
- Code/Engine/Audio/AudioManager.h
- Code/Engine/Audio/AudioManager.cpp
- Code/Engine/Core/framework.cpp
- Code/Editor/Window/EditorWindowManager.h
- Code/Editor/Window/EditorWindowManager.cpp
- Code/Editor/Window/EditorSceneViewWindow.cpp
- Code/Engine/GamePlay/EventSystem/ActionSequence.cpp
- Code/Editor/Window/GameModeWindow.cpp

関連文書を更新・追加。

- Tool/EditorPlayModeIntegrationReport.md
- Tool/PlayModeRemovalReport.md
- Tool/PlayModePerformanceReport.md
- Tool/PlayModeRestorationReport.md（本書）

## コメント

Editor/Playの責務、更新前の要求処理、Scene退避・復元、編集中の表示更新、シーン遷移後の復帰、音声停止、マウス差分保持、ツールバーに日本語コメントを追加。

## Tracy

既存のBS_PROFILE_SCOPEを使用。EditorPlayModeのCaptureSnapshot / RestoreSnapshot / EnterPlay / ProcessCommand / UpdateEditorScene / BeginSession / EndSession / ReturnToEditor / Toolbar、GameObjectManager::LoadSceneData、CameraManager::ResetRuntimeStateを計測。既存の更新・描画・物理・音声の計測と、Tracy有効化方式は維持。

## ファイル形式

変更ファイルはUTF-8、BOMなし、CRLF。保存後に厳密なUTF-8で読み込めることを確認。

## Visual Studio登録

新規C++ファイルなし。実ファイルは既存配置を維持し、17ファイルのvcxproj登録とvcxproj.filters登録を別々に確認。フォルダー移動とFilter変更はなし。

## 確認内容

追加した関数の宣言・定義・呼出し、必要なincludeと既存API、ImGuiのBegin/End対応、保存/復元JSONの対応、非同期読込中のStop、PauseとScene遷移、EditorとPlayの更新分岐、括弧対応を静的に確認。

ユーザー側での実行確認項目は、通常起動でEditor表示、Playでイントロ開始、Pause/Resume、未保存編集を含むStop復元、形態変化・Scene遷移後のStop、Escでのカーソル解放。再導入後のFPSは未測定。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
