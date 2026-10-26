# Play後のFPS低下の調査・負荷削減

> 2026-10-19: ユーザー環境では冷却後にFPSが回復。その後、ユーザー依頼によりPlay Modeを再導入済み。以下は調査時の履歴です。現在の状態は `Tool/PlayModeRestorationReport.md` を参照してください。

## 調査結果

ユーザー環境ではPlay開始から数秒後、イントロ中に144 FPSから100 FPS以下へ低下。F1でEditor表示を消しても改善しない。Pause時のFPSは未確認。

スナップショット生成と復元はPlay/Stop要求時にだけ実行される。Editor用更新はEDITOR分岐でのみ実行され、PLAYと同時には走らない。SceneGameはSceneBase経由でゲーム更新を一度だけ行う。これらの静的確認だけでは、今回のFPS低下の主原因は特定できていない。

## 変更内容・変更ファイル

- `Code/Editor/Window/EditorSceneViewWindow.h` / `.cpp`: 表示中の別視点ビューがRenderAllを再実行する経路について、Play中の追加再描画を既定で停止。CameraメニューのRender During Playで同時表示を再開できる。Editor/Pauseでは既存のプレビュー描画を維持し、中央のゲーム画面は継続して描画する。
- `Code/Editor/Window/HierarchyWindow.cpp`: BeginEditorWindowがfalseの非表示タブでは、全Objectの階層走査を省略。
- `Code/Editor/NodeEditor/NodeGraphMonitorWindow.cpp`: 非表示タブでも実行されていた監視対象検索を、表示判定後へ移動。
- `Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp`: Play切替要求がNONEなら詳細計測前にreturn。既存のObject数Plotに加えて、Start待ちObject数をTracyへ出力。
- `Tool/PlayModePerformanceReport.md`: 本書。

Editor表示を消しても改善しないという結果から、上記のEditor処理削減だけで症状が解消するとは断定しない。Play機能の削除は行っていない。

## コメント

非表示タブの走査省略、別視点の再描画をPlay中に明示的に有効化する理由、要求なしフレームの早期returnに日本語コメントを追加。

## Tracy

既存のBS_PROFILE_SCOPEと有効化方式を維持。`Editor.SceneView.ExtraRender`で表示中の別視点再描画の有無、`GameObject.PendingStartCount`で初期化待ちの増加を確認可能。

継続調査では、低下前後の`SceneRuntime::Running`、`GameObjectManager::ObjectsUpdate`、`GameObjectManager::ObjectsLateUpdate`、`PhysXSystem::Step`、`Render::All`、GPU::Frameと既存のObject数を比較する。PauseでFPSが回復するかも、更新側と描画側を切り分ける補助になる。Pauseで回復した場合も、GPUのアニメーション・描画内容の変化があるためCPU原因と断定しない。

## ファイル形式 / Visual Studio登録

対象ファイルをUTF-8、BOMなし、CRLFへ統一し、厳密なUTF-8として検査。新規C++ファイルはなく、既存5ファイルのvcxprojとvcxproj.filters登録を確認。実ファイルの移動とFilter変更はなし。

## 確認内容

表示判定前後の処理経路、ImGuiのBegin/End対応、メニュー項目と新しいメンバの対応、Pause/Editor時の分岐、既存の保存項目を変更しないこと、括弧対応とVisual Studio登録を静的に確認。FPS改善量は未測定で、主原因の特定と解消は未完了。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
