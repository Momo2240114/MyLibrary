# 旧SEの置換・BGMとSEのこもり・振動Action

## 変更内容

実行用Code・Data・Scenesに残っていた旧SE指定を、このチャットで追加した音源へ置き換えました。既存のファイルは削除せず、再生時の参照先を変更しています。

| 以前の音源 | 置換先（Data/Audio/SE以下） |
|---|---|
| TitleSelect.wav | Sword-Fight-1.wav |
| TitleMove.wav | Sword-Practice-2.wav |
| heartbeat.wav | Sonniss/Presentation/RattlingDowner.wav |
| Gameover_40.wav | Slow-motion-ends.wav |
| GameClear.wav | Entering-slow-motion.wav |
| discovery.wav | Swordsmanship-3.wav |
| heartbeat2.wav | Sonniss/Presentation/BassDropFast.wav |

対象は標準SEのロード一覧、AudioComponentとAudioWorkspaceの既定値、タイトルの設定とフォールバック、形態移行の斬撃・衝撃音、その生成用Prefabです。旧Sceneや再生スナップショットのパスが残っていても、AudioManagerで同じ新音源へ解決します。停止・音量・Pitch・Pan・個別こもり設定にも同じ解決を適用します。
標準SEの先頭7枠には重複しない音源を割り当て、既存SE_NUMBERと登録順の対応を維持しています。Sword-Practice-2、Swordsmanship-2/3はこのチャットで追加した剣SEなので、その指定は維持しています。

通常ガードとジャストガードにはSword-Fight-1、ガード崩れにはSonnissのMetalWeaponHitを、接触位置から3D再生する処理を追加しました。同じ攻撃の重複登録を除外した後に再生します。

ジャスト回避・ガード成功時は、SEを含むMasterへ軽いこもりを掛け、BGM専用Busでさらに高音を落とします。標準値では全体が4800Hz、BGM専用が1200Hzとなり、BGMを主役にしながら剣戟音や成功SEにもこもりが掛かります。両方のフィルターは同じ時計で立ち上がり・復帰します。死亡時のBGM減衰は別DSPで維持し、スロー終了・コンテニュー・Scene終了・Editor停止で両方を復帰・解放します。

## 振動Action

カテゴリはControllerです。

| Action | 用途 | 優先度 |
|---|---|---:|
| Controller.RumbleAttackStart | 攻撃開始 | 1 |
| Controller.RumbleAttack | 攻撃命中 | 2 |
| Controller.RumbleReceiveHit | 被弾 | 3 |
| Controller.RumbleGuard | 自分の通常ガード成立 | 2 |
| Controller.RumbleGuardBreak | 自分のガードが崩れた時 | 4 |
| Controller.RumbleAttackGuarded | 自分の攻撃を通常ガードされた時 | 2 |
| Controller.RumbleJustGuard | 自分のジャストガード成功 | 4 |
| Controller.RumbleJustGuarded | 自分の攻撃をジャストガードされた時 | 3 |
| Controller.RumbleJustDodge | ジャスト回避成功 | 3 |
| Controller.StopRumble | 即時停止 | — |

ガード・ジャストガードは親階層からPlayerレイヤーを確認して、成立側と受け流された側を判別します。敵同士の接触ではPadへ通知しません。回避は既存の成功確定処理に接続しました。
攻撃開始・命中・被弾の自動振動は、既存AttackTemplateの強さ・時間・有効設定を維持します。これらを別途Actionから呼びたい場合は表の共通Actionを使えます。自動処理へ同じ共通Actionを重ねて呼び出す変更はしていません。

ControllerInputは、振動の残り時間をSDLの実時間で確認し、再生中より低い優先度の通知を抑えます。被弾やジャスト成功の直後に攻撃開始時の弱い振動が届いても、上書きしません。StopRumbleで終了時刻・優先度も破棄し、既存のScene終了・Editor停止・Pad切断の停止処理を維持します。

## 変更ファイル

ソース・ヘッダー：

- Code/Editor/Workspace/AudioWorkspaceWindow.cpp
- Code/Editor/Workspace/AudioWorkspaceWindow.h
- Code/Engine/Audio/AudioComponent.cpp
- Code/Engine/Audio/AudioComponent.h
- Code/Engine/Audio/AudioManager.cpp
- Code/Engine/Audio/AudioManager.h
- Code/Engine/UI/MenuPresentation.cpp
- Code/Engine/GamePlay/System/GuardSystem.cpp
- Code/Engine/GamePlay/System/JustGuardSystem.cpp
- Code/Engine/GamePlay/System/AttackSystem.cpp
- Code/Engine/GamePlay/Input/ControllerInput.cpp
- Code/Engine/GamePlay/Input/ControllerInput.h
- Code/Engine/GamePlay/Serialize/ActionRegistry.cpp
- Code/Game/Charcter/Player/PlayerDodgeState.cpp
- Code/Game/Charcter/Player/PlayerControllerComponent.h
- Code/Game/AI/Human/HumanBossActionComponent.cpp

設定：

- Data/Prefabs/PhaseTransitionEffects.json
- Data/UI/TitleMenu.json
- Data/Scenes/MainScene/Objects/TransitionImpactSound.json
- Data/Scenes/MainScene/Objects/TransitionSlashSound.json
- Data/Scenes/TitleBackground/Objects/TitlePresentation.json

追加資料：Docs/CombatAudioAndRumbleRevision.md。

## コメント

旧パスの互換解決、剣戟SEの成立条件、Player階層の判別、全体とBGM専用のフィルター、振動の優先度と終了時刻へ日本語コメントを追加・更新しました。

## Tracy

既存のGuardSystem、JustGuardSystem、PlayerDodgeState::HandleJustDodgeSuccess、AttackSystem、ControllerInput::PlayRumble、AudioManagerの演出計測を維持しました。小さいパス解決・親階層探索には細かいZoneを増やしていません。

## ファイル形式

変更ソース・JSON・資料はUTF-8・BOMなし・CRLFです。厳密なUTF-8読み込みと改行を確認しています。音源自体は加工していません。

## Visual Studio登録

新規C++ファイルはありません。既存16ソース・ヘッダーの.vcxproj登録と.vcxproj.filters登録を別々に確認しました。資料はDocsに配置し、実ファイルの移動はありません。

## 確認内容

旧7音源の再生パスがCode・Data・Scenesに残っていないこと、採用音源の存在、標準SEの先頭7枠と列挙値の対応、JSONの音源以外の値の維持、互換解決の読み込み・停止・設定変更の整合性を静的に確認しました。
MasterとBGM専用DSPの接続・解除、死亡用DSPとの分離、Action登録と発火条件、ControllerInputの宣言・定義、SDLの同梱API、優先度と停止処理も確認しました。音・振動の実機調整は未実施です。
Build/Exportsやバックアップ内の生成済みデータは編集していません。実行コードの変更はユーザー側でのビルド後に反映されます。

## ビルド

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
