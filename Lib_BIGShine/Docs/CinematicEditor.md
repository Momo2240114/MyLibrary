# Cinematic Editor

## ActionSequenceから開く

通常のAction Sequence Editorで対象Sequenceを展開し、`Cinematic editing`を有効にする。そのSequenceを引き継いでCinematic Editorへ移動する。以後は`Open Cinematic Editor`から開ける。Workspaceセレクターから直接開き、ObjectとSequenceを選択することもできる。

シネマティック編集の指定はSequenceごとの`cinematicEditing`に保存する。これは編集方法の指定で、本番の実行方式は従来のActionSequenceのまま。旧JSONは指定なしとして読み込む。ボスイントロ、形態移行、任意の演出に共通の編集経路を使用する。

## 編集とプレビュー

1. `ActionSequence owner`と`Sequence`で対象を選ぶ。対象はSceneへ保存されるObjectの、有効なActionSequenceとする。Editor Playを停止し、Sceneの読み込み完了後に再生する。
2. TimelineのActionを選択し、キーをドラッグして実行時刻を調整する。SnapとFPSでフレーム単位に合わせる。Actionタブでは呼び出す登録済みAction、待機Object、アニメーション基準時刻、Blackboard書込み、完了・中断Actionを編集できる。
3. `Play`で実際のSceneのAnimation、カメラ、Particle、MeshVFXとActionを更新する。Pause、Speed、Loop、コマ送りに対応する。`Preview end`は確認する演出全体の長さで、Sequence自身のdurationとは別に指定する。
4. 進行状態に応じて完了Actionが分岐する場合は、再生前に`Initial GameMode context`を合わせる。例としてBossIntroはBoss intro、Phase1DefeatからPhase2Introへ続く確認はPhase 1 defeatを選び、Preview endを40秒程度へ伸ばす。再生対象の名前に依存した専用分岐は設けていない。
5. `Live actors / VFX`で実際のAnimation Stateとサンプル時刻・尺、選択ObjectのParticle寿命・遅延、MeshVFXの経過時間・duration・loopを確認する。Sequenceの時計とWaitForObjectの待機状態も表示する。
6. Scene内のカメラや演出Componentを変更する場合は停止後に`Scene settings`を開く。既存ComponentのInspectorを使用する。
7. 再生中のAction編集は下書きへ保持する。変更後の結果は`Refresh preview from draft`で再構築して確認する。`Stop + Restore Scene`、Workspace切替、ウィンドウを閉じる操作で試写開始前のSceneへ戻す。

タイムラインに配置するのは選択SequenceのAction。子Sequenceを一本のタイムラインへ展開する機能ではない。アニメーションのStateに依存するActionは絶対秒へ推測変換せず、Animation-boundと表示しInspectorで編集する。

## シークとScene復元

再生開始前のSceneをメモリへ退避し、下書きを反映したSceneで試写する。停止時は既存Editor Playの復元経路を使い、試写中のObject生成・削除やComponent変更を戻す。元Sceneは試写のためにディスクへ保存しない。

目盛りのドラッグを離すとシークを予約する。巻き戻しはSceneを先頭へ戻し、固定1/60秒刻みで指定時刻まで実際の更新を再実行する。1フレーム当たりの再評価回数を制限するため、遠い時刻への移動には時間が掛かる。読み込み・再評価中は画面に状態を表示する。WaitForObjectなど本番の待機条件も維持する。

試写中はプレイヤー入力と敵AI、戦闘の当たり判定・物理シミュレーションを停止する。Scene切替と終了Actionは抑止し、試写の停止・復元操作を維持する。シーク中の音声と振動を抑制する。

## 保存

- `Apply Sequence to Scene`は下書きを元ObjectのActionSequenceへ反映する。対象Componentの他Sequenceも含む同じ既存JSON形式を使用する。
- `Save Scene`は下書きを適用してSceneを保存する。試写中とScene読み込み中は適用・保存を無効にする。
- `Export Sequence data`はActionSequence Component用のJSONを指定先へ出力する。Scene全体や別種のCinematic Assetへ変換する処理ではない。
- Objectの選択変更はそのObjectから下書きを取り直すため、必要な編集は選択変更前に適用する。

## 既存のFinish preset

`Authoring source`を`Finish preset`へ切り替えると、既存BossFinishDirector用の編集を使用できる。Player/Boss、VFX、カメラキー、Easing、Assetsからの追加、Undo/Redo、Save AsとApply to gameを維持する。

このモードの保存形式は従来のBossFinishCinematicプリセット。Apply to gameは`Data/Presentation/AnathemaBossFinish.json`へ反映する。旧プリセットと固定カメラの互換性は維持する。ActionSequence編集とは保存先と時間の意味が異なる。

## 静的確認と登録

新しい補助クラスCinematicSequenceEditorは`Code/Editor/Workspace`へ配置し、`.vcxproj`と`.vcxproj.filters`の`Code\Editor\Workspace`へそれぞれ登録する。Workspace自体は既存のCinematicEditorWindowを使用する。

主要な更新・描画・Scene再構築・Live表示・Timelineに既存の`BS_PROFILE_SCOPE`を使用する。対象ファイルはUTF-8・BOMなし・CRLFとする。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
