# ANATHEMA タイトル画面改修

## 変更内容・再利用した既存システム

SceneTitleの表示用更新経路を維持し、本編MainSceneの円形床Stage_Floor05と岩壁8個を表示専用データとして配置しました。本編のJSON自体は変更していません。
既存のGltfModel、AnimationControllerComponent、ModelAttachment、CameraComponent、LightComponent、GPU Particle、SpriteRenderer、SpriteRenderSystem、ImGui、ActionRegistry、SceneLoading、AudioManagerを再利用しています。RenderPassやSkinningの経路は変更していません。

背景は17オブジェクトです。専用Camera、床、岩壁8個、主人公、剣、盾、正面寄りの平行光、赤い背面点光源、環境Particle、ロゴSpriteを生成します。新たなRenderTargetは追加していません。
タイトルのデータには、敵AI・操作Component・戦闘Collider・HP・ロックオン・戦闘HUD・戦闘イベントを含めていません。AnimationControllerが内部で必要とするBlackboardとCharacterMovementは既存Startが補いますが、タイトルの更新許可対象には移動処理や物理処理を含めていません。

## ロゴ・画面構成

提供済みのData/Sprite/UI/Title/TitleLogo.pngを加工せず、1枚のSpriteRendererで使用します。1670×942、RGBA、8bit、Alpha値域0〜255、完全透明377,472画素を確認しました。黒色を透過する処理はありません。
ロゴは上部中央から少し左、メニューは左下、主人公は右側に配置しています。ロゴの原寸からアスペクト比を求め、画面幅・高さ・メニュー先頭との余白で表示領域を制限します。実際の大きさや重なりは実機で最終調整してください。
モデル・環境演出・PostEffectの後にロゴSprite、その後にメニューを描画します。

主人公は本編のCombatGirls_Sword_Shield.gltfと材質設定を使用し、同モデル内のSS_Idle（番号23）をループします。Controllerの保存データは待機1状態に限定し、RootMotionの移動・回転、イベント、外部グラフ参照を除いています。
剣はhand_r、盾はlowerarm_lへ、本編と同じモデル・材質・ローカル補正で取り付けます。待機アニメーションの更新後に既存SyncAttachmentを呼びます。

## カメラ・UIの調整

Data/UI/TitlePresentation.jsonで以下を指定できます。

- CameraPosition、LookAtPosition、FieldOfView
- CameraMotionAmplitude、CameraMotionSpeed
- LogoCenterX、LogoTop、LogoWidth、LogoMaxHeight
- MenuLeft、MenuTop、MenuRowHeight

座標・振幅はワールド単位、FieldOfViewは度、速度はsinへ渡す位相の毎秒増分です。UIの位置・ロゴサイズは画面に対する比率、MenuRowHeightは1920×1080基準のピクセルです。
標準CameraPositionは(0, 2.7, -9.5)、注視点は(0, 1.8, 0)、FOV42度、横移動振幅0.16、速度0.12です。16:9より横幅の狭い画面では縦FOVを広げます。
開発版ではF2で調整Windowを開き、SaveでUTF-8のJSONへ保存、Reloadで読み直せます。Window表示中はタイトルメニューの操作を停止します。配布版には調整Windowを出しません。F1から従来のEditorへ進む入口も維持しています。

Data/UI/TitleMenu.jsonのId・Label・Action・Visible・Enabled・OrderとSE設定を維持しています。GAME START、EXIT GAMEを操作できます。TUTORIALは既存Sceneが仮実装かつ必須モデル欠損、SETTINGSはAction未実装のため、従来どおり無効表示を残しています。
文字は淡い金色、選択時は金色・菱形・薄い発光・位置補間で示します。キーボード、マウス、ゲームパッドを共通の選択状態へ接続し、長押しリピート、フォーカス判定、Alt+Enterの除外、二重決定防止を維持しています。

## 環境演出と遷移

既存GPU Particleの煙Textureと赤い粒子Textureを使い、地表付近の霧24個、火の粉64個を上限とします。霧の発生域を低く抑え、火の粉には長めの寿命と弱いNoiseを設定しました。
本編と同じIBL・ToonPBR・影設定を専用Sceneへ複製し、ライトを専用化しました。Bloom強度0.12、閾値1.0、Vignette強度0.65は描画中だけ適用し、直後に共有設定を復元します。

表示開始後に背景、ロゴ、メニューをフェードインします。決定後は入力を止め、メニューを0.2秒で消し、次にロゴを0.3秒で消し、その後0.6秒で全体を暗転します。1.1秒後にActionを一度だけ実行します。
GAME STARTは既存GoSceneGameから、プレイヤーモデルを使うSceneLoadingを経て本編へ進みます。EXIT GAMEは暗転後のPostQuitMessageを使います。
背景のロード失敗時もメニュー演出時間を進め、Start・Exitを操作できる経路を残しています。

SceneTitleは表示を開始した時点で既存タイトルBGMを一度だけ再生し、Finalizeでその曲を停止します。本編のBGMと物理用TimeScaleを初期化し、SceneBaseのFinalize/ClearでObject、Particle、入力状態、Action、ロード要求を解放します。復帰時は新しいSceneTitleのInitializeで全表示状態を作り直します。

## チュートリアルの状態

既存SceneTutorialはInitialize・Update・Render・DrawGUIでSceneBase::Finalizeを呼ぶ仮実装です。さらに以下の既存参照モデルが存在しません。

- Data/Model/Stage/Tutorial/SM_Room_OneEntrance.gltf
- Data/Model/Stage/Tutorial/SM_Room_TwoEntrance.gltf
- Data/Model/Stage/Tutorial/SM_Shutter.gltf

読み込み失敗へ誘導しないよう、既存GoSceneTutorialのAction名を残したままTUTORIALは従来どおり無効とします。SceneTutorialのコード・データは元の状態に戻しています。チュートリアルの有効化にはSceneの実装と不足資源の復旧が必要です。

## 今回変更・追加したファイル

- Code/Engine/GamePlay/Scene/SceneTitle.h
- Code/Engine/GamePlay/Scene/SceneTitle.cpp
- Data/UI/TitleMenu.json
- Data/UI/TitlePresentation.json
- Data/Particle/TitleAtmosphere.json
- Data/Scenes/TitleBackground/SceneList.json
- Data/Scenes/TitleBackground/Objects/TitleCamera.json
- Data/Scenes/TitleBackground/Objects/TitleStage.json
- Data/Scenes/TitleBackground/Objects/TitleRockWall0.json
- Data/Scenes/TitleBackground/Objects/TitleRockWall1.json
- Data/Scenes/TitleBackground/Objects/TitleRockWall2.json
- Data/Scenes/TitleBackground/Objects/TitleRockWall3.json
- Data/Scenes/TitleBackground/Objects/TitleRockWall4.json
- Data/Scenes/TitleBackground/Objects/TitleRockWall5.json
- Data/Scenes/TitleBackground/Objects/TitleRockWall6.json
- Data/Scenes/TitleBackground/Objects/TitleRockWall7.json
- Data/Scenes/TitleBackground/Objects/TitlePlayer.json
- Data/Scenes/TitleBackground/Objects/TitleSword.json
- Data/Scenes/TitleBackground/Objects/TitleShield.json
- Data/Scenes/TitleBackground/Objects/TitleKeyLight.json
- Data/Scenes/TitleBackground/Objects/TitleRimLight.json
- Data/Scenes/TitleBackground/Objects/TitleAtmosphere.json
- Data/Scenes/TitleBackground/Objects/TitleLogo.json
- Tool/TitleScene.md

既存のTitleLogo.pngは読み取り確認のみで、画像ファイル自体は変更していません。

## 日本語コメント・Tracy・Visual Studio登録

構図設定の読み込み、ロゴの比率と余白、RootMotionを使わない表示用更新、ボーン装備の同期、入力競合、段階的フェード、BGMとTimeScaleの復帰に日本語コメントを記述しています。
SceneTitleの既存BS_PROFILE_SCOPEを維持し、LoadPresentationSettings、UpdateLogoLayout、DrawPresentationSettingsへ同じマクロを追加しました。既存のGPU計測・Tracy有効化方式は変更していません。

新規C++ソースはありません。SceneTitle.h/.cppは.vcxprojへ登録済みで、.vcxproj.filtersではCode\Engine\GamePlay\Sceneに登録されていることを別々に確認しました。
新規JSONはData/Scenes/TitleBackground/ObjectsとData/UIへ配置しました。実ファイルの移動やFilterの変更はありません。
変更対象のテキストはUTF-8・BOMなし・CRLFです。

## 静的確認と未検証事項

- SceneTitleの宣言・定義、includeと既存APIの呼び出しを確認。
- タイトルのSceneList、Object名、Component種別、モデル・Textureの直接参照先、glTFのbuffer/image依存先を確認。
- SS_Idleの番号とモデル内の名前、剣盾の参照先ボーン名を確認。
- Camera/UI設定の保存・読み込みキー、設定欠損時のデフォルト、非数値・例外時の扱いを確認。
- 新規データに戦闘用Componentを含めていないことと、既存表示用更新・通常描画への接続を確認。
- SceneLoadingを経由するゲーム開始・タイトル復帰、Finalize/Clearの解放経路をコード上で確認。
- プロジェクト登録とFilter登録、厳密なUTF-8、BOM、改行形式を確認。

黒い泥の独立したMeshVFXは追加していません。霧と赤い粒子が今回の環境演出です。
表示の品質、肌・装備・影の描画、解像度別の構図、実入力、音量、フェード、ゲームクリア後の復帰、繰り返し往復時のメモリ使用量は実行確認していません。ビルドの成功は未確認です。
共通開発ルールのビルド・実行禁止に従い、貼り付け依頼中のPhase 8は静的確認までとしています。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
