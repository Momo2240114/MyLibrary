# ボス戦イントロの実装と編集方法

MainScene開始時に既存GameModeManagerのBOSS_INTROへ入り、Global登録されたActionSequence「BossIntro」を再生します。演出の時刻はScene JSONに保存し、GameModeManagerには演出内容を追加していません。

## 編集方法

1. MainSceneのBossIntroを選択し、Action Sequence EditorでBossIntroを編集します。各ActionのExecution TimeとTimeline Lengthを変更できます。
2. Completion Actionは `GameMode.SequenceFinished` です。CameraManagerの切り替え時間とAnimation復帰時刻を考慮してTimeline Lengthを設定してください。
3. Camera位置、注視対象、Focus Offset、FOV、Enter／Hold／ReturnはEventCameraのCameraActionDirectorComponentで編集します。門の初期CameraはIntroGateArrival、到着後の近接CameraはIntroPlayerです。Camera位置はワールド座標です。Player／Dragonの配置を変更した場合はCamera位置も調整してください。
4. Animation名、State Asset Path、対象Object、旋回対象、Blend／Turn DurationはBossIntroのActorPresentationActionComponentで変更します。
5. 黒画面と咆哮時の色はBossIntroのColorFilterDirector、BlurはRadialBlurDirectorで編集します。
6. 揺れはEventCameraのCameraShakeComponentのIntroRoarShake、FOVはFovDirectorの追加した最後のWaypointを編集します。
7. 名前と保持・Fade時間はBossIntroNameのBossNamePresentationComponentで編集します。文字のサイズ、色、フォントは同じObjectのUITextComponentで調整します。
8. Sceneの保存でComponent設定とSequenceを保存します。演出用Animation Stateを編集した場合は、既存Animation EditorのState Asset保存も行います。

## 初期Sequence

開幕演出は約16.975秒です。Cameraの移動・カット間の間とRadialBlurの時間を長くしています。歩行・抜刀・咆哮のAnimation再生速度は1.0です。門、Particle、旋回、名前表示、FOV、Shake、色補正の速度・持続時間は従来の設定へ戻しています。Soundは通常速度のAnimationに合わせて開始します。

| 時刻 | 演出 |
| --- | --- |
| 0.000 | EventCameraを優先度8へ変更、入力・AI・LockOnをロック、暗転から復帰、霧の門と霧Particleを再生、Playerを配置位置の3m奥から歩かせる |
| 2.400 | 霧の新規発生を停止し、門を光粒へ変えるBurst Particleを再生 |
| 2.600 | Playerが元の配置位置へ到着し、Idleへ移行 |
| 3.900 | 門のMeshVFXを非表示にする |
| 4.000 | 光粒の新規発生を停止。発生済みParticleは寿命で消える |
| 4.725 | Playerの近接カットへ0.6125秒で補間 |
| 5.425 | IntroDrawWeaponと抜刀Sound Action |
| 7.700 | PlayerをDragonへ0.55秒で旋回 |
| 8.050 | 肩越しCameraへ1.1375秒で補間 |
| 9.450 | Dragon全身Cameraへ0.9625秒で補間 |
| 10.675 | Dragon頭部Cameraへ0.70秒で補間 |
| 11.550 | IntroRoar |
| 11.650 | Camera Shake、RadialBlur、FOV、色補正、咆哮Sound Action |
| 11.650 | 頭ボーンに追従する透明コーンの背景歪みを再生 |
| 12.075 | ボス名FadeIn |
| 14.008 | 透明コーンを非表示にする |
| 15.575 | LockOnを有効にし、EventCameraの優先度を開始時の値へ戻す |
| 16.800 | Player／Dragonの通常AnimationとHUDを復帰 |
| 16.975 | Sequence終了通知で第一形態の戦闘へ移行 |

## 霧の門とPlayer登場演出

- 門は `Data/Model/VFX/IntroFogGate.gltf` の楕円の膜と縁の2Meshを使用します。`Data/VFX/IntroFogGate.json` は青白い霧の膜を2層、発光する縁を1層重ねたMeshVFX Assetです。
- MainSceneの `IntroFogGate` に同じMeshVFX設定とCurveComponentを保存しています。GateAlpha、GateDissolve、GateLightをCurve Editorで編集でき、霧の膜がノイズで消える境界へ光を加えます。パーツの再生時間は3.9秒です。
- 霧は `Data/Particle/IntroGateMist.json`、消散の光粒は `Data/Particle/IntroGateLight.json` です。対応するScene ObjectのParticle Editorで色・個数・速度・寿命を編集できます。停止Actionは新規発生を止め、生存中の粒子を残して自然に消します。
- `IntroGateWalk` はモデル内の `SS_Walk`（Index 29）を再利用するLoop Stateです。RootMotionを有効にし、前進成分を持つ `root` を抽出元に指定して見た目のXYZ移動を除去します。GameObjectへのXYZ移動適用は無効にし、移動はActorPresentationのGateEnter Actionへ任せます。歩行ループの巻き戻りと二重移動を防ぎ、攻撃イベントは無効のままです。
- 開幕Actionの後にAnimation Graphが遅延読み込みされる場合も、演出用Stateを再構築後に復元します。通常の開始Stateによって歩行がIdleへ戻ることを防ぎます。
- GateEnterは歩行の一定の歩幅に合わせた等速移動です。消散時のGateLightは最大4.8、Dissolve Edgeの強度は7.0、幅は0.13とし、光粒は800個、最大保持数1000個、サイズ0.045〜0.14、寿命0.75〜1.6秒です。
- GateEnterのEntry Position Offsetは `(0, 0, -3)`、移動時間は2.6秒です。実行時のPlayerのローカル位置を到着地点として保存し、奥からそこへ補間します。Transformの外部変更は既存Colliderの同期経路へ反映します。
- 開幕CameraはEventCameraの `IntroGateArrival`、到着後は `IntroPlayer` です。門は現在のPlayer配置の1.7m奥へ固定配置しています。Playerの初期配置を変える場合は、門・霧・光粒の3ObjectとCameraのワールド位置も合わせて調整してください。
- 追加ActionはBossIntro所有のActorPresentationカテゴリにあります。GateEnter、GateWalk、GateShow／GateHide、GateMistPlay／GateMistStop、GateLightPlay／GateLightStopをSequence Editorへ並べられます。Startでは登録のみを行い、再生はGameModeが予約するBossIntro Sequenceから行います。
- 咆哮のRadialBlurは強度0.065、Samples 24、往路0.1575秒、保持1.0秒、復帰1.2秒です。合計2.3575秒かけ、11.65秒の開始から14.0075秒で消えます。既存の中心・Mask半径は維持しています。
- 咆哮CameraのIntroDragonCloseはワールド位置 `(6, 10, -4)` へ引き、顔の注視オフセット `(0, 9, -12)` を維持します。

今回の変更ファイルは以下です。新規のC++クラスは追加していません。

- Code/Engine/GamePlay/EventSystem/ActorPresentationActionComponent.h
- Code/Engine/GamePlay/EventSystem/ActorPresentationActionComponent.cpp
- Data/Model/VFX/IntroFogGate.gltf
- Data/VFX/IntroFogGate.json
- Data/Particle/IntroGateMist.json
- Data/Particle/IntroGateLight.json
- Data/Scenes/MainScene/Objects/IntroFogGate.json
- Data/Scenes/MainScene/Objects/IntroGateMist.json
- Data/Scenes/MainScene/Objects/IntroGateLight.json
- Data/Animation/StateData/Player/States/IntroGateWalk.json
- Data/Scenes/MainScene/Objects/BossIntro.json
- Data/Scenes/MainScene/Objects/EventCamera.json
- Data/Scenes/MainScene/SceneList.json
- BossIntroImplementation.md

カットのHOLDは次のActionによる置き換え／Returnを待つ初期設定です。カット間隔を大きく延ばす場合はHold Durationも調整してください。復帰時間、名前表示時間、Timeline Lengthの関係も合わせて調整してください。

EventCameraのCameraComponentはUse Event Priorityを有効、Event Priorityを8、通常Priorityを-1に設定しています。位置・揺れ・FOVのActionは優先度8のときだけ再生し、途中で優先度が変わった場合は演出更新を終了します。旧EventCameraのWaypoint位置ActionとBossIntorCamera Sequenceは削除しました。MainCameraの通常戦闘用Actionは維持しています。

## Global Action一覧

| カテゴリ | Action名 |
| --- | --- |
| Sequence | BossIntro |
| GameMode | DisablePlayerControl、EnablePlayerControl、DisableEnemyAI、EnableEnemyAI |
| GameMode | GameMode.SequenceFinished（既存） |
| ActorPresentation | BossIntro_PlayerIdle、BossIntro_DragonIdle |
| ActorPresentation | BossIntro_DrawWeapon、BossIntro_DragonRoar、BossIntro_LookAtEnemy |
| ActorPresentation | BossIntro_ReleasePlayerAnimation、BossIntro_ReleaseDragonAnimation |
| CameraAction | IntroGateArrival、IntroPlayer、IntroShoulder、IntroDragonWide、IntroDragonClose |
| Camera | EventCamera_ActivateEventPriority、EventCamera_RestorePriority |
| CameraShake | IntroRoarShake |
| Camera | LockOn_Disable、LockOn_Enable（既存）、EventCamera_Fov_0（EventCameraの開幕用Waypoint） |
| PostProcess | BossIntro_Color_0、BossIntro_Color_1、BossIntro_RadialBlur_0 |
| UI | BossIntroName_ShowBossName、BossIntroName_HideBossName |
| GameModeObject | UI_GameModeObject_Hide、UI_GameModeObject_Show（既存ComponentのScene設定から登録） |
| Audio | BossIntro.DrawWeaponSound、BossIntro.RoarSound |

音声Actionは指定どおり空の処理です。音声素材をAudio Workspaceへ登録した後、Sequence Editorで上記2つを実際のAudio Actionへ差し替えます。

標準Sequenceの有効化はGameModeの終了通知によってまとめて行います。EnablePlayerControl／EnableEnemyAIは、別Sequenceでも個別に使えるよう登録しています。

## 各システムとの接続

- **CameraDirector**：既存CameraActionDirectorComponentのENTER／HOLD／RETURNを再利用します。イントロの位置ActionはEventCameraへ設定しています。MainCameraの通常TPS／LockOn／Collisionを維持し、手動入力はGameModeのPlayerロックで停止します。終了時はEventCameraの優先度を戻し、CameraManagerでMainCameraへ切り替えます。
- **FOV**：既存FovDirectorの0.10秒往路、0.12秒保持、0.48秒復帰で60→72→60を設定しています。最後のCameraカットはUse External FOV In Holdを有効にし、独立したFOV演出を上書きしません。
- **ColorDirector**：既存ColorFilterDirectorを利用します。Start Waypoint設定により初回描画前から黒を反映し、既存Easingで1.20秒かけて通常色へ戻します。咆哮時の色も別Waypointです。
- **RadialBlur**：既存RadialBlurDirector→GraphicsDeviceManager→PostRenderSystem→RadialBlurSystemを利用します。強度0→0.065へ0.1575秒、1.0秒保持、1.2秒で0へ戻ります。中心、Mask半径、Sample数、往路／保持／復帰時間は編集できます。補間方式は既存の線形補間です。新しいShaderや描画基盤は追加していません。
- **Animation**：PlayerのSS_Take（モデル内Index 28）とDragonのSpreadFire（Index 22）を、IntroDrawWeapon／IntroRoarとして既存State Asset経由で読み込みます。攻撃イベントとRootMotionを無効化しています。演出中はグラフの自動遷移だけを抑制し、Animation・イベント更新を維持します。
- **Player**：GameModeロック中は入力値と移動予約を消し、Player StateMachine更新を停止して演出Animationの上書きを防ぎます。
- **Enemy AI**：既存DragonAI／AIControllerのGameModeロック判定をそのまま利用します。
- **BossName UI**：既存WidgetBoard、UIWidget、UIText、SpriteRendererを組み合わせています。通常HUDとは別Boardを使い、画面下部中央に2行の文字を表示します。SpriteのAlphaをフェードし、Text Texture再生成時にもAlphaを保持します。Dragon固有の型や名前固定処理はありません。
- **Boss HP UI**：既存GameModeObjectToggleComponentを通常HUDのUI Objectへ設定し、Boardの有効状態を復帰します。既存のHP更新、配置、文字設定は維持しています。

LetterBox、SlowMotion、Vignette、Flash、BGM、字幕、VFXなどは既存／追加のGlobal ActionをSequenceへ並べられます。今回、将来用の描画機能は新設していません。

## 新規ファイル

- Code/Engine/GamePlay/EventSystem/ActorPresentationActionComponent.h
- Code/Engine/GamePlay/EventSystem/ActorPresentationActionComponent.cpp
- Code/Engine/UI/BossNamePresentationComponent.h
- Code/Engine/UI/BossNamePresentationComponent.cpp
- Data/Scenes/MainScene/Objects/BossIntro.json
- Data/Scenes/MainScene/Objects/BossIntroName.json
- Data/Animation/StateData/Player/States/IntroDrawWeapon.json
- Data/Animation/StateData/DragonFirstForm_Test/States/IntroRoar.json
- BossIntroImplementation.md（本書）
- BossIntroFileManifest.txt（変更対象一覧）

## 変更ファイル

- Code/Engine/GamePlay/EventSystem/ActionSequence.h／.cpp
- Code/Engine/GamePlay/Camera/CameraActionDirectorComponent.h／.cpp
- Code/Engine/GamePlay/Camera/CameraLockOnComponent.cpp
- Code/Engine/GamePlay/Camera/ThirdPersonCameraComponent.cpp
- Code/Engine/GamePlay/Camera/FovDirector.cpp
- Code/Engine/GamePlay/Serialize/ActionRegistry.cpp
- Code/Engine/GamePlay/Serialize/ComponentRegistry.cpp
- Code/Engine/Graphics/Resource/Animation/AnimationControllerComponent.h／.cpp
- Code/Engine/Graphics/PostEffect/ColorFilterDirector.h／.cpp
- Code/Engine/Graphics/PostEffect/RadialBlurDirector.cpp
- Code/Engine/UI/UITextComponent.cpp
- Code/Game/Charcter/Player/PlayerControllerComponent.cpp
- Code/Game/GameMode/GameModeManager.cpp
- Data/Scenes/MainScene/Objects/MainCamera.json
- Data/Scenes/MainScene/Objects/PlayerModel.json
- Data/Scenes/MainScene/Objects/TestTarget.json
- Data/Scenes/MainScene/Objects/UI.json
- Data/Scenes/MainScene/SceneList.json
- BIGShine.vcxproj
- BIGShine.vcxproj.filters

変更開始前から存在していた他の編集は保持しています。

## コメント・Tracy・ファイル形式・Visual Studio登録

新規Componentの役割、主要関数、旋回座標の扱い、Animation制御の受け渡し、Camera復帰、初回暗転、透明度保持、Sequence終了通知に日本語コメントを追加しました。

既存BS_PROFILE_SCOPEを使用し、Actor Action実行／旋回更新、BossName表示／更新、ActionSequence開始／更新、FOV／ColorFilter／RadialBlur更新を計測します。既存CameraとAnimationの計測は維持しています。

作業対象の32ファイル（ソース、Scene／State JSON、Visual Studio登録）は厳密なUTF-8として読み込めること、BOMなし、CRLFを確認しました。

Actorの実ファイルは既存EventSystemフォルダー、BossNameの実ファイルは既存UIフォルダーへ配置しています。.vcxprojへCPP／ヘッダーを登録し、.vcxproj.filtersではそれぞれ既存ActionSequence／UITextと同じFilter（Code\\Engine\\GamePlay\\EventSystem、Code\\Engine\\GamePlay\\UI）を使用しています。既存ファイルの移動はありません。

## 静的確認

- Scene JSONとState JSONの読み込み、Visual Studio XMLの構造確認。
- 新規4ソースファイルの実在、.vcxprojとFilterへの重複なし登録、ComponentRegistry登録。
- Sequenceの26個のAction名と登録先、対象Object名、開始時刻とSequence長の照合。
- SS_Take／SpreadFireのIndexとGLTFImporterのAnimation格納順、State名とAsset参照の照合。
- Header／CPPの宣言と定義、include先、呼び出した既存API、nullptrを扱う分岐の確認。
- 保存・読み込み項目の対応と、既存JSONで追加項目がない場合の既定値の確認。
- C++ファイルの字句上の括弧対応確認。コンパイラによる構文保証はしていません。

Cameraの画角、モデルの動き、実際のUI表示については実行確認が必要です。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。

## 咆哮の透明コーン

`Data/VFX/IntroRoarAirCone.json` は既存の内蔵Coneを使い、長さ8m、広がる端の半径2.6mの空気の歪みを作ります。通常色のAlphaは0.018、発光は0、背景歪みは強度14・不透明度0.65です。

MainSceneのIntroRoarAirConeはModelAttachmentでTestTargetのPlainsDragon_-Headへ追従します。Transformは頭のローカル座標で位置(-40, 35, 0)、回転(0, -90, 0)です。SpreadFireでの頭ボーンの前方は-Xなので、内蔵Coneの+Zを-Xへ向けています。頭ボーンの縮尺0.035をTransformの倍率28.5714で補い、上記寸法をワールドのメートルとして扱います。モデルの縮尺を変更する場合は倍率も調整してください。

RoarAirExpandでコーンを広げ、RoarAirAlphaで歪みを出現・消散させます。歪みShaderにも既存Alphaを掛け、色描画と同じCurveでフェードするよう修正しました。通常色の透明度はColor.aで別に設定します。Assetの初期RenderEnabledはfalseです。BossIntroのRoarAirPlayが11.65秒で再生・表示し、RoarAirStopが14.0075秒で非表示にします。攻撃判定はありません。

新規ファイルはVFX AssetとScene Objectの2つです。C++クラスやShaderファイルの新設はなく、Visual Studio登録の追加は不要です。既存のActor Action実行、MeshVFX更新、Render::VFXのTracy計測を使用します。
