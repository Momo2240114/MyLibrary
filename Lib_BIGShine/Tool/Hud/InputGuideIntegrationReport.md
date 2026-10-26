# 入力デバイス別操作ガイドの統合

## 変更内容

- 既存の `ControllerInput`（SDL3、接続中の1台）、`KeyInput`、`MouseInput`、`KeyConfig` を利用。別のInputManagerは追加していない。現在の経路にXInput呼び出しはない。
- `framework::update` の既存入力取得後に `KeyConfig::UpdateActiveDevice` を実行する。`GetActiveDevice` はUI以外からも参照できる。保存済み `DeviceType` の数値は変更していない。
- キーボードの新規押下は既存の `KeyPressed` で観測し、ゲーム側の `KeyPressedIn` の状態を消費しない。クリックはMouseInputの取得済み状態、ホイールは消費処理と独立した通知番号を利用する。中央・サイドボタンはMouseInputの既存取得経路へ追加した。
- マウス移動はフレーム内4ピクセル以上、スティックは既存デッドゾーン0.20に0.08を加えた閾値、トリガーは既存閾値0.50を使用する。軸は閾値を越えた瞬間、または0.04を越える変化がある場合だけ新規操作とする。保持や小さなドリフトで入力元を奪わない。ゲームの入力閾値や処理順は変更していない。
- 同一フレームで競合した場合はキーボード押下・クリック・ホイール、Padの新規操作、マウス移動の順に優先する。非フォーカス中の操作は表示元へ反映せず、Pad切断時はK&Mへ戻す。
- SDLの `SDL_GetGamepadType` によるPS3/PS4/PS5識別を利用。それ以外と不明な機種はXbox表示へフォールバックする。仮想Xboxとして見えるPS機器には、HUD Inspectorの `Controller Icon Type`（AUTO / XBOX / PLAYSTATION）で表示だけを指定できる。

## 使用素材・UV

| 入力 | 実際の素材 | 寸法 | セル |
|---|---|---|---|
| K&M | Data/Sprite/Input/KeyMouse/KeyMouseInputSheet.png | 1088×1024 | 64×64、17×16 |
| Xbox | Data/Sprite/Input/GamePad/Xbox/XboxInputSheet.png | 640×640 | 64×64、10×10 |
| PS | Data/Sprite/Input/GamePad/PS/playstation-series_sheet_double.png | 1536×1536 | 128×128、12×12 |

元画像は変更していない。ボタン画像の生成・個別PNGへの分解も行っていない。`InputIconAtlas` が入力コードとセルの対応を管理し、既存 `SpriteRenderer::SetUVRect` にピクセル単位のSourceRectを渡す。セル境界から半テクセル内側を選び、正方形の表示枠で比率を維持する。

A～Z、数字、F1～F12、主要修飾キー・編集キー、マウス5ボタン、Padフェイス・肩・トリガー・スティック押下・方向キーなどを登録。未収録の角括弧キーやPSロゴなどは文字へフォールバックする。キー名はWindowsのキー名取得API、Pad名は入力コードの対応表を使用する。

既存ResourceLoaderのキャッシュから3枚のSRVを保持し、通常更新ではファイルの再読込を行わない。Texture差替えは既存Spriteへ共有参照を渡す。素材パスも差し替え、Sprite保存時のUVと素材が食い違わないようにした。

## KeyConfig・レイアウト・スキル

- Bind/Set/Remove/Rename/Clear/Loadで更新番号を進め、割り当て変更時だけHUDの入力対応を再解決する。表示デバイスだけで成立する最初のOR候補を使用し、その候補のAND入力を表示する。
- 4個までのボタンを画像で並べる。複合入力はAND関係をキー名と `+` でも表示する。画像未収録キーや4個を越える入力も、全キー名を文字で残す。
- Padは現在の割り当てのY/X/B/Aから上下左右を選ぶ。フェイス以外、または同じフェイスへの重複割り当ては独立行へ配置し、操作名の重なりを避ける。
- K&Mは右下の縦型。キーはUI側に固定せず、現在のKeyConfigから取得する。Textには幅上限を持たせる。
- スキルは既存 `PlayerSkillInputBinding` のModifier、Trigger、Slot、優先度を参照。共通スキル素材、残り時間、READY判定は既存Controller/Loadoutの表示APIを使う。
- 現在の設定にはKBMの `Player.SkillModifier` がなく、直接入力 `Player.SkillA/B/C`（現在E/R/T）がある。この場合は通常ガイドに3スロットの直接入力を併記する。KBM修飾キーを設定した場合は、そのModifierの押下中に複合スキル表示へ切り替わる。未設定キーを自動追加していない。
- 入力デバイス切替は操作ガイドだけを0.075秒で消し、素材と配置を交換して0.075秒で表示する。切替要求が続いてもタイマーを初期化しない。HPの再生成、クールタイムのリセット、StateMachineへの変更はない。
- 前回のスキル検証キャッシュ、文字変更時だけのTexture更新、0.1秒単位の残り時間表示、HP更新経路を維持している。

## 描画・Inspector

- 既存の `LinearSampling` 設定が描画側に未接続だったため、SpriteRenderSystemで設定に応じたSampler選択を追加した。設定のないSpriteは従来のPoint補間を使う。既存のAlphaBlend経路を利用する。
- HUD内で完全に透明な要素は `SetVisible(false)` とし、未使用の複合入力セルがMap/Drawを増やさないようにした。情報停止の診断スイッチは維持している。
- HUD InspectorでActive Device、接続名、表示機種の上書き、Live/K&M/Xbox/PSプレビュー、スキルパレットのプレビュー、元シート、任意入力コードのSourceRectと拡大画像を確認できる。Previewは実際の入力元を書き換えず、Sceneにも保存しない。Controller Icon Typeだけを保存する。

## コメント・Tracy・登録・形式

入力元の観測、ドリフト除外、UVの境界、KeyConfigキャッシュ、実行時配置と保存配置の分離、フェード、非表示セルの描画抑制に日本語コメントを追加した。

`BS_PROFILE_SCOPE` を `KeyConfig::UpdateActiveDevice`、`ControllerInput::HasMeaningfulInput`、`InputIconAtlas::Initialize/Gui` に追加。BattleHudのStart、Resolve、LateUpdate、UpdateGuide、UpdateHealth、Gui、Serialize、Deserialize、SpriteRenderSystemの既存計測を維持した。

新しい実ファイルは `Code/Engine/UI/InputIconAtlas.h/.cpp`。`.vcxproj` にヘッダーとCPPを登録し、`.vcxproj.filters` では既存HUDと同じ `Code\Engine\GamePlay\UI` へ登録した。既存実ファイルの移動はない。

対象のソース、プロジェクト、JSON、確認スクリプトは厳密なUTF-8、BOMなし、CRLFとして確認した。

## 静的確認結果と限界

`ReviewInputGuide.py` はファイル解析と参照画像の作成のみを行う。ゲーム、テストプログラム、コンパイラーは起動していない。

- 3枚の画像寸法・グリッド・Alpha範囲0～255を確認。
- 登録した147領域の範囲内配置と非空Alphaを確認。セル対応はグリッド付き資料で目視した。
- MainSceneの217 Object参照、85 Guide Widgetの名前、複合入力セル、使用Textureの存在を確認。
- 新規H/CPPのプロジェクト登録が各1件、Filter登録が既存HUDと一致することを確認。
- ヘッダーとCPPの宣言・呼び出し、ローカルSDLヘッダーの機種API、null時の短絡条件、変更番号の更新経路、表示設定の保存/復元、Previewの非保存をコード上で確認。
- 1920×1080、1600×900、1280×720について、既存EXPANDの等方Scaleと現在の配置に基づく矩形計算を確認。`InputGuideStaticPreview.png` は素材と主要操作の配置参考図であり、ゲーム画面のキャプチャーではない。計算結果と全UV表は `InputGuideStaticReview.json` に記載。

実機のKeyboard/Mouse/各Controllerの切替、再接続、仮想デバイスの識別、ImGuiからの実画面プレビュー、GPUでの透過・文字の見え方、3解像度の実画面、FPS、Debug/Releaseビルドは未確認。描画性能の改善・維持を測定済みとは扱わない。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。

## 変更ファイル一覧

- `Code/Engine/GamePlay/Input/ControllerInput.h`
- `Code/Engine/GamePlay/Input/ControllerInput.cpp`
- `Code/Engine/GamePlay/Input/KeyConfig.h`
- `Code/Engine/GamePlay/Input/KeyConfig.cpp`
- `Code/Engine/GamePlay/Input/MouseInput.h`
- `Code/Engine/GamePlay/Input/MouseInput.cpp`
- `Code/Engine/Core/framework.cpp`
- `Code/Engine/UI/BattleHudComponent.h`
- `Code/Engine/UI/BattleHudComponent.cpp`
- `Code/Engine/UI/InputIconAtlas.h`
- `Code/Engine/UI/InputIconAtlas.cpp`
- `Code/Engine/Graphics/Resource/Sprite.h`
- `Code/Engine/Graphics/Resource/Sprite.cpp`
- `Code/Engine/Graphics/RenderSystem/SpriteRenderSystem.h`
- `Code/Engine/Graphics/RenderSystem/SpriteRenderSystem.cpp`
- `BIGShine.vcxproj`
- `BIGShine.vcxproj.filters`
- `Data/Scenes/MainScene/SceneList.json`
- `Data/Scenes/MainScene/Objects/HudGuide0Button.json`
- `Data/Scenes/MainScene/Objects/HudGuide0Button1.json`
- `Data/Scenes/MainScene/Objects/HudGuide0Button2.json`
- `Data/Scenes/MainScene/Objects/HudGuide0Button3.json`
- `Data/Scenes/MainScene/Objects/HudGuide0SkillButton.json`
- `Data/Scenes/MainScene/Objects/HudGuide0SkillButton1.json`
- `Data/Scenes/MainScene/Objects/HudGuide0SkillButton2.json`
- `Data/Scenes/MainScene/Objects/HudGuide0SkillButton3.json`
- `Data/Scenes/MainScene/Objects/HudGuide0Label.json`
- `Data/Scenes/MainScene/Objects/HudGuide0SkillLabel.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint0Button.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint0Button1.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint0Button2.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint0Button3.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint0Text.json`
- `Data/Scenes/MainScene/Objects/HudGuide1Button.json`
- `Data/Scenes/MainScene/Objects/HudGuide1Button1.json`
- `Data/Scenes/MainScene/Objects/HudGuide1Button2.json`
- `Data/Scenes/MainScene/Objects/HudGuide1Button3.json`
- `Data/Scenes/MainScene/Objects/HudGuide1SkillButton.json`
- `Data/Scenes/MainScene/Objects/HudGuide1SkillButton1.json`
- `Data/Scenes/MainScene/Objects/HudGuide1SkillButton2.json`
- `Data/Scenes/MainScene/Objects/HudGuide1SkillButton3.json`
- `Data/Scenes/MainScene/Objects/HudGuide1Label.json`
- `Data/Scenes/MainScene/Objects/HudGuide1SkillLabel.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint1Button.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint1Button1.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint1Button2.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint1Button3.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint1Text.json`
- `Data/Scenes/MainScene/Objects/HudGuide2Button.json`
- `Data/Scenes/MainScene/Objects/HudGuide2Button1.json`
- `Data/Scenes/MainScene/Objects/HudGuide2Button2.json`
- `Data/Scenes/MainScene/Objects/HudGuide2Button3.json`
- `Data/Scenes/MainScene/Objects/HudGuide2SkillButton.json`
- `Data/Scenes/MainScene/Objects/HudGuide2SkillButton1.json`
- `Data/Scenes/MainScene/Objects/HudGuide2SkillButton2.json`
- `Data/Scenes/MainScene/Objects/HudGuide2SkillButton3.json`
- `Data/Scenes/MainScene/Objects/HudGuide2Label.json`
- `Data/Scenes/MainScene/Objects/HudGuide2SkillLabel.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint2Button.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint2Button1.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint2Button2.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint2Button3.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint2Text.json`
- `Data/Scenes/MainScene/Objects/HudGuide3Button.json`
- `Data/Scenes/MainScene/Objects/HudGuide3Button1.json`
- `Data/Scenes/MainScene/Objects/HudGuide3Button2.json`
- `Data/Scenes/MainScene/Objects/HudGuide3Button3.json`
- `Data/Scenes/MainScene/Objects/HudGuide3SkillButton.json`
- `Data/Scenes/MainScene/Objects/HudGuide3SkillButton1.json`
- `Data/Scenes/MainScene/Objects/HudGuide3SkillButton2.json`
- `Data/Scenes/MainScene/Objects/HudGuide3SkillButton3.json`
- `Data/Scenes/MainScene/Objects/HudGuide3Label.json`
- `Data/Scenes/MainScene/Objects/HudGuide3SkillLabel.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint3Button.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint3Button1.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint3Button2.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint3Button3.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint3Text.json`
- `Tool/Hud/ReviewInputGuide.py`
- `Tool/Hud/InputGuideStaticReview.json`
- `Tool/Hud/InputGuideStaticPreview.png`
- `Tool/Hud/InputGuideIntegrationReport.md`
