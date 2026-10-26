# ScreenTransition・ロードUI・ゲームメニュー 実装記録

設計参考：[Transition Idea_100](https://gameanimation.info/archives/2615)。演出の具体的な挙動は依頼仕様を基準に実装。

## 構成

- `ScreenTransitionComponent`：GameObjectからのCover / Reveal / Play / Hold / CompleteWork / Cancel、Inspector、JSON、Owner名付きAction。
- `ScreenTransitionManager`：Sceneに依存しない競合・優先順位・Token・弱参照・永続マスク・最終描画・F9プレビュー。
- `TransitionSequence`：Cover / Hold / Revealの状態・時間・完全被覆の描画確認・一度だけの通知。
- `ITransitionEffect` / `TransitionEffectFactory`：種類名と生成関数のRegistry。毎フレームの種類判定を行わない。
- `ShaderTransitionEffect`：FullScreenVS、定数バッファ、生成Noise、Sampler、描画StateとTextureキャッシュの共有。
- `FadeTransition` / `RadialTransition` / `SlashTransition` / `DissolveTransition` / `ShutterTransition` / `WipeTransition` / `TextureMaskTransition`：種類別クラス・専用PS・固有Inspector。
- `LoadingPresentationComponent`：Managerが値で公開する進捗・失敗・再試行要求を扱う汎用UI。見た目は`LoadingPresentation`で共有。
- `GameMenuComponent`：通常ゲームSceneの二択メニュー。文言、Action、ポーズ、開閉キーをJSONから変更可能。

## SceneとGameObjectManager

SceneManagerは次Sceneの予約、完全被覆待ち、所有権移譲、準備完了後のReveal通知だけを担当する。トランジション側はSceneManager、ボス、プレイヤーを呼び出さない。

GameObjectManagerのUpdate入口で`Component::UpdateUnscaled`を呼び、ポーズとTimeScaleに依存しないUI時間を渡す。通常のComponent Update / LateUpdateの順序は維持する。`RenderAll`から`RenderUI`を提出する。ロードSceneも通常の`Update`入口で表示用更新へ振り分ける。

ホストのframeworkはScreenTransitionManagerをフレーム開始時に更新し、Scene・Sprite・Particle・主画面ImGuiの後、Present前に一度描画する。Sceneを使わないホストでも、このUpdate / DrawGUI / Renderを同じ順序で呼べば利用できる。

SceneStartUpとSceneLoadingのロゴ・進捗・ヒント・失敗表示・再試行UIはJSONで配置するComponentへ移した。SceneGameのポーズメニュー本体も削除し、MainSceneに配置したGameMenuComponentが担当する。

## 再生API

- `Cover()`：被覆後は待機。`CompleteWork()`で指定Hold時間後に開く。`Reveal()`なら待機時間を省いて開く。
- `Play()`：被覆通知後、Hold時間を経て開く。`WaitForCompletion`を有効にすると明示的な`CompleteWork()`待ち。
- `Cover(callback)` / `Play(onCovered, onCompleted)`：完全被覆を描画した次のフレーム開始時に通知。通知は各一回。
- `Hold()`：完全被覆を維持する。
- `Cancel(CLEAR)`：通知を破棄し、透明へ戻す。
- `Cancel(KEEP_CURRENT)`：現在の進行度を凍結する。Revealで現在の進行度から開ける。
- `Cancel(KEEP_COVERED)`：完全被覆を保持する。RevealまたはCLEARで解除する。

非同期処理の完了は主スレッドへ戻し、返されたTransitionHandleで`CompleteWork(handle)`を通知する。ワーカースレッドからImmediate ContextやManagerを直接操作しない。

通常Componentの要求はOwnerを弱参照し、破棄・無効化時には取消する。Scene切替で被覆を引き継ぐときは新しいTokenに変更し、旧ComponentのOnDestroyから取消されない。古い通知は引き継がない。コールバック内で他Objectを参照する場合もweak_ptrを使用し、削除は既存のGameObjectManager::Destroy等へ予約する。

競合時は同順位以下の後発要求を拒否する。優先順位が高い要求は既存演出を取消して置換する。Scene切替用は100、通常ComponentのInspector範囲は-50～50、F9試写は-100。Scene遷移予約は先着一件を保持し、LoadingSceneから準備済みSceneへの予約だけは開き中にも一件受け付ける。

## Action / ボス形態移行

Ownerが`PhaseTransition`なら、`PhaseTransition_Cover` / `_Reveal` / `_Play` / `_Hold` / `_CompleteWork` / `_Cancel`を既存ActionRegistryへ登録する。

既存Phase1Defeatは0秒でCoverを要求し、0.7秒のPlacePlayerの手前で`waitForScreenCovered`を待つ。従来のPlacePlayer / PlaceDragon / 向き / カメラ / Idleの順序と時刻は維持し、0.71秒でCompleteWorkを通知する。DragonDeath以降のアニメーション、黒い泥、人型ボス、戦闘再開の既存Actionは維持する。Restore / BattleResumeではCancelする。

ActionSequenceの新しい`waitForScreenCovered`は既定false。旧JSONは従来どおり動作し、待機中は既存の生成待ちと同様にTimelineの時計を戻す。

## 設定とTexture

`Data/UI/SceneTransitions.json`：Profiles、Sceneごとの上書き、任意名のPresetsを保存する。DefaultFade / QuickBlackout / BossPhaseTransition / SlashTransition / MudDissolve / EngineShutter / TitleTransition / TextureMaskを用意した。

共通設定：TransitionType、DurationOut、DurationIn、Delay、HoldDuration、PlaybackSpeed、Color、Easing、BlockInput。固有設定：EdgeColor、EdgeWidth、Direction、NoiseScale、NoiseSpeed、GlowIntensity、CenterPosition、PanelCount、PanelDelay、BoundaryCurve、Softness。

`Effect`内のTexture / NoiseTexture / GlowTextureを既存CPUResourceLoaderのキャッシュで共有する。Texture未指定・欠損時は白Textureまたは生成Noiseを使用する。Shader欠損時はImGui最前面FADEへ退避する。CPUResourceLoaderは破損Textureで失敗したとき、無効SRVをキャッシュせずHRESULTを返すようにした。

TextureMask：Invert（白から覆う）、ScaleX / ScaleY、OffsetX / OffsetY、Rotation（度）、Softness、EdgeColor、GlowIntensity。任意のマスクでも進行度0は完全透明、1は完全不透明を保証する。ShutterのTexture濃度はSurfaceStrength。AssetBrowserのASSET_TEXTUREドロップに対応する。

## 起動・ロード・メニュー

- `Data/UI/BigShineLoading.json`：既存BigShineロゴ、青白いライン、段階表示、進捗・完了発光。
- `Data/UI/AnathemaLoading.json`：既存ANATHEMAロゴ、赤黒い進捗ライン、白緑の光、薄い足元の霧・微粒子・背景の弧、外部ヒント。
- ロード用LoadingRun / LoadingWalk、装備同期とシルエットは既存ScenePresentationComponentの仕組みを維持。
- ヒントは重複連続を避けて選び、HintDuration / HintFadeDuration / ShowHintsで制御。
- 進捗は実進捗を超えず補間し、完了時に100%と発光を表示。新しい固定待ち時間は加えていない。
- ロゴ、バー、文字、装飾はViewportの比率と基準1280x720から配置する。
- StartupPresentationは起動UIのみをJSON配置。LoadingPresentationは既存SceneへUIを追加。MainSceneのGameMenuはJSON配置。
- メニュー中はGameObjectManagerの戦闘更新とPhysicsを止め、生時間UIだけを更新する。再開フレームも戦闘更新を止め、次フレームから再開する。開閉時のカーソルと入力捕捉を復元する。

## 互換性・既存機能

ScreenFadeComponentの任意色、FadeInOnStart、FadeOutDelay、Duration、完了Action、Serialize / Deserialize、Inspectorは維持する。SceneカテゴリのActionは共通トランジションへ引き継ぎ、終了など他のActionは旧Spriteフェードを使う。Scene移譲時に初回フェードが後から再開しないようにする。

既存のColorFilter、ModelDissolve、MeshVFX、BossFinish、PlayerDeath、MenuNavigationは残す。今回は個別の描画仕様を無関係に置換しない。

## Editor

F9でScreen Transitionを開く。Preset選択、Cover / Reveal / Play、停止、全パラメータ、Textureの変更、保存・再読込、BigShine / ANATHEMAのScene Profile編集を行える。試写はSceneを変更しない。InspectorにもComponentの再生操作と保存を用意する。試写中の設定変更はPlayを再開始して反映する。

## 日本語コメント・Tracy・Visual Studio

状態同期、弱参照、Token引継ぎ、再配置待ち、Texture代替、描画順、ポーズ中の生時間UIに日本語コメントを追加した。主要処理は既存BS_PROFILE_SCOPE / BS_PROFILE_GPU_SCOPEを使用し、Tracyの有効化方式は変更していない。

新規C++実ファイルは既存の大分類`Code/Engine/UI`に配置し、FilterはUI/ScreenTransition、同/Effects、UI/Presentation、UI/Menuに分けた。ShaderはShader/Transitionへ追加し、Shaders/PostEffect/Transitionへ登録した。全ファイルを.vcxprojと.vcxproj.filtersにそれぞれ登録した。既存ファイルの移動は行っていない。

## 検証状況

- UTF-8厳密デコード、BOMなし、CRLF：対象76ファイルを確認。
- 新規C++の.vcxproj登録とFilter登録：各一件を確認。
- 設定JSON構文、SceneListの重複と実Object参照：確認。
- 新規7種PS：Debug / Release双方でコンパイル成功の出力を確認。
- 全体Debug / Release：両方ともMSBuild終了コード0で完了。
- メニュー再開処理等を含む最終Debug差分ビルド：終了コード0。
- 最終Release差分ビルド：AGENTS.md更新によるビルド禁止方針を受け、進行中の処理を中断。最後のメニュー修正を含むReleaseの最終再確認は未完了。
- 既存Shaderの一部に`failed writing`の出力があった。新規7種PSの出力は成功。最終Debugでは残っていたToonVSも出力成功。
- 既存のTracy重複登録（MSB8027 / LNK4042）、Transformの未参照ラベル、SpriteShaderのローカル参照返却、GetNodesの戻り値経路に関する警告は依頼範囲外のため変更していない。
- 最終Debugの実行：起動し、ANATHEMAタイトル画面への到達・表示を確認。
- F9の操作を開始する段階で、ユーザーの物理EscによりComputer Useが停止された。以降の画面操作は行っていない。起動したアプリはユーザー操作用に残している。
- F9プレビュー、ゲーム開始からLoadingScene・本編、ポーズメニュー、タイトル復帰、ボス形態移行、短長ロード、ロード失敗・再試行、連続要求、異なる解像度の実画面確認は未実施。コード上の経路確認と区別する。

## 変更ファイル一覧

### 既存ファイル

- `BIGShine.vcxproj`
- `BIGShine.vcxproj.filters`
- `Code/Engine/Core/framework.cpp`
- `Code/Engine/GamePlay/Core/Component/ComponentBase.h`
- `Code/Engine/GamePlay/Core/Object/GameObjectManager.h`
- `Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp`
- `Code/Engine/GamePlay/Input/KeyConfig.cpp`
- `Code/Engine/GamePlay/Serialize/ComponentRegistry.cpp`
- `Code/Engine/GamePlay/EventSystem/ActionSequence.h`
- `Code/Engine/GamePlay/EventSystem/ActionSequence.cpp`
- `Code/Engine/Graphics/Core/CPUResourceLoader.cpp`
- `Code/Engine/UI/ScreenFadeComponent.h`
- `Code/Engine/UI/ScreenFadeComponent.cpp`
- `Code/Engine/UI/MenuNavigationComponent.cpp`
- `Code/Engine/UI/MenuPresentation.h`
- `Code/Engine/UI/MenuPresentation.cpp`
- `Data/Scenes/LoadingPresentation/Objects/LoadingPresentation.json`
- `Data/Scenes/MainScene/SceneList.json`
- `Data/Scenes/MainScene/Objects/PhaseTransition.json`
- `Code/Engine/GamePlay/Scene/SceneManager.h`
- `Code/Engine/GamePlay/Scene/SceneManager.cpp`
- `Code/Engine/GamePlay/Scene/SceneBase.h`
- `Code/Engine/GamePlay/Scene/SceneBase.cpp`
- `Code/Engine/GamePlay/Scene/SceneGame.h`
- `Code/Engine/GamePlay/Scene/SceneGame.cpp`
- `Code/Engine/GamePlay/Scene/SceneLoading.h`
- `Code/Engine/GamePlay/Scene/SceneLoading.cpp`
- `Code/Engine/GamePlay/Scene/SceneStartUp.h`
- `Code/Engine/GamePlay/Scene/SceneStartUp.cpp`

### 新規ファイル

- `Code/Engine/UI/TransitionParameters.h`
- `Code/Engine/UI/TransitionParameters.cpp`
- `Code/Engine/UI/TransitionSequence.h`
- `Code/Engine/UI/TransitionSequence.cpp`
- `Code/Engine/UI/ITransitionEffect.h`
- `Code/Engine/UI/ITransitionEffect.cpp`
- `Code/Engine/UI/ShaderTransitionEffect.h`
- `Code/Engine/UI/ShaderTransitionEffect.cpp`
- `Code/Engine/UI/ScreenTransitionManager.h`
- `Code/Engine/UI/ScreenTransitionManager.cpp`
- `Code/Engine/UI/ScreenTransitionComponent.h`
- `Code/Engine/UI/ScreenTransitionComponent.cpp`
- `Code/Engine/UI/LoadingPresentation.h`
- `Code/Engine/UI/LoadingPresentation.cpp`
- `Code/Engine/UI/LoadingPresentationComponent.h`
- `Code/Engine/UI/LoadingPresentationComponent.cpp`
- `Code/Engine/UI/GameMenuComponent.h`
- `Code/Engine/UI/GameMenuComponent.cpp`
- `Code/Engine/UI/FadeTransition.h`
- `Code/Engine/UI/FadeTransition.cpp`
- `Code/Engine/UI/RadialTransition.h`
- `Code/Engine/UI/RadialTransition.cpp`
- `Code/Engine/UI/SlashTransition.h`
- `Code/Engine/UI/SlashTransition.cpp`
- `Code/Engine/UI/DissolveTransition.h`
- `Code/Engine/UI/DissolveTransition.cpp`
- `Code/Engine/UI/ShutterTransition.h`
- `Code/Engine/UI/ShutterTransition.cpp`
- `Code/Engine/UI/WipeTransition.h`
- `Code/Engine/UI/WipeTransition.cpp`
- `Code/Engine/UI/TextureMaskTransition.h`
- `Code/Engine/UI/TextureMaskTransition.cpp`
- `Shader/Transition/DissolveTransitionPS.hlsl`
- `Shader/Transition/FadeTransitionPS.hlsl`
- `Shader/Transition/RadialTransitionPS.hlsl`
- `Shader/Transition/ShutterTransitionPS.hlsl`
- `Shader/Transition/SlashTransitionPS.hlsl`
- `Shader/Transition/TextureMaskTransitionPS.hlsl`
- `Shader/Transition/TransitionCommon.hlsli`
- `Shader/Transition/WipeTransitionPS.hlsl`
- `Data/UI/SceneTransitions.json`
- `Data/UI/BigShineLoading.json`
- `Data/UI/AnathemaLoading.json`
- `Data/Scenes/MainScene/Objects/GameMenu.json`
- `Data/Scenes/StartupPresentation/SceneList.json`
- `Data/Scenes/StartupPresentation/Objects/StartupUI.json`
- `Data/Scenes/StartupPresentation/Objects/LoadingCamera.json`

## 作業末尾のルール更新

AGENTS.mdが差し替えられたため、以後のビルド・実行確認は行わない。上記の全体Debug / Release成功およびDebug起動確認は、その更新より前の明示的なビルド・実行確認依頼に基づく履歴。更新後は進行中のRelease差分ビルドを停止し、静的な確認のみとした。Computer Useはユーザーの物理Escによる停止後、操作していない。
