# 戦闘HUD統合報告

## 変更内容

MainSceneの既存HPオブジェクトを再利用し、ボスHP、プレイヤーHP、実入力に従う操作ガイド、スキルパレット、円形クールタイムを接続しました。描画とゲーム動作の実機確認は未実施です。

- 新規 `BattleHudComponent` は表示だけを担当します。Manager、戦闘ロジック、スキルタイマーは新設していません。
- ボスはGameModeの形態から `TestTarget` / `HumanBoss` を選択し、実際の `HealthComponent` から割合と数値を取得します。
- プレイヤーは `PlayerModel` の `HealthComponent` を参照します。未管理のMPやスタミナは表示していません。既存の非表示PlayerStaminaBarも変更していません。
- 現在値と同じ割合でUVを切り詰めます。ダメージ時は現在値を即時反映し、遅延バーは既定0.3秒待って割合0.45/秒で追従します。既存Inspectorから調整できます。
- 形態変更・再生成では `ResetDisplayedRate` で前の対象の遅延表示を初期化します。Objectを毎回解決し、削除予約中・戦闘対象外のボスを参照しません。保持する参照はweak_ptrです。
- 入力設定・装備を変更せず `KeyConfig`、Controllerの入力表と装備定義を読みます。現設定は△強攻撃、□攻撃、○パリィ、×ジャンプ、R2回避、L2ガード、L1スキル、**R3ロックオン切替**です。
- スロット0=□PhantomRush、スロット1=△Breakthrough、スロット2=○RisingJudgmentという実データを優先します。名前・分類・装備変更・残り時間はControllerから取得します。
- Modifier押下に合わせて0.15秒で表示だけをフェードします。表示アニメーションは操作の受付に介入しません。
- READYは外周発光、COOLDOWNは暗転・数値・円形マスク、UNAVAILABLEは暗いアイコンと状態表示、EMPTYは空枠で表します。
- 円形マスクは65段階、128pxセル、9列のPNGアトラスです。`remaining / duration` からUVを選択し、UI側では時間を積算しません。ゼロ秒、負値、非有限値を保護しています。新規ShaderやShaderコンパイルは不要です。
- 通常行動・コンボ受付・スキル可否はControllerの読み取り専用APIと既存Blackboardを参照します。UIからRequestActionやコンボ予約を書き換えません。
- 状態表示は既存のガード、ジャストガード反撃タグ、HurtBoxの無敵だけを参照します。Buff/Debuff素材は準備しましたが、未確認の効果管理へ推測で接続していません。
- 人型AIのBlackboardに公開されたPhase 3で赤い縁を控えめに脈動させます。HPから独自に暴走を判定しません。
- GameModeの演出中と既存UI BoardのShow/Hideに追従し、0.18秒でフェードします。RESULTでは再表示しません。任意の特殊カメラ演出からも `UIRoot_Hud_Hide` / `UIRoot_Hud_Show` を呼べます（GameObject::RegisterGlobalActionのObject名接頭辞形式）。
- Sceneへ配置済みです。実行時にHUD Objectを生成する処理は追加せず、Scene再読込での重複を避けています。

## 調査した既存システム

`SpriteRenderer`、`SpriteRenderSystem`、`CPUResourceLoader`、`UIWidgetComponent`、`WidgetBoardComponent`、`UIProgressBarComponent`、`UITextComponent`、`UISpriteSheetComponent`、`BossNamePresentationComponent`、`GameObjectManager`、`SceneGame`、`PlayerControllerComponent`、PlayerState/StateMachine、PlayerInputResolver/SkillLoadout、`SkillExecution`、`HealthComponent`、DragonBodyPartManager、HumanBossAI、`GameModeManager`、GameModeObjectToggle、Blackboard、ActionRegistry、ActionSequence、ControllerInput/KeyConfigとScene/Skill/Input JSONを確認しました。

## UIの描画順と品質

既存のワールド・VFX・Particle・PostEffectの後、最終UNORM出力へのSprite描画を使用します。HUDをHDR SceneColorへ追加しません。

HPの順番は背景、ダメージ遅延、回復遅延、現在HP、既存Flash/端部、フレーム、装飾、名前・数値です。スキルはアイコン、円形マスク、外周、テキストの順です。

新規HUD素材はストレートアルファです。既存SRC_ALPHA / INV_SRC_ALPHA合成を維持しています。Spriteに選択式の `LinearSampling` と `SdrTexture` を追加しました。SdrTextureは、sRGBデコード後に再エンコードされない既存の最終UNORM出力へ、PNGの表示値を保持して渡すための設定です。旧JSONの既定値は両方falseです。Textureキャッシュは同じ画像のColor/Data解釈を別キーとして保持します。

既存の通常Sprite・ImGuiの描画順、RenderTargetとBlendStateの復元、Tracyの有効化方式は維持しています。

## 座標とスケール

1920×1080を基準に、既存Boardの `EXPAND`（短辺側に合わせた等方スケール）を使用します。ルートWidgetはViewport全体へStretchし、TopCenter / BottomLeft / BottomRightのAnchorを用います。

| 表示 | 基準位置・サイズ |
|---|---|
| ボスHP | TopCenterから(0,55)、850×12 |
| プレイヤー | BottomLeftから(60,-120)、HP塗り440×12 |
| 操作ガイド | BottomRightから(-340,-250)、十字中心はそこから(-80,0) |
| フェード | HUD 0.18秒、パレット0.15秒 |

UIRootのBattleHud Inspectorから位置、倍率、ボスHP幅・高さ、Guide透明度、Icon/CooldownText/Label倍率、フェード時間を調整できます。通常のScene保存でJSONへ保存します。倍率を保存後のRectへ重ねて掛けないよう、基準配置もElementLayoutsへ保存します。

## 素材と保存先

保存先は `Data/Sprite/UI/Hud/` です。元の資料画像は変更していません。

- `AssetManifest.json` に全46点のPNG名、サイズ、Alpha範囲、元画像の矩形座標、抽出または再構成の由来を記録しました。
- `ContactSheet_Checker.png`、`ContactSheet_Black.png`、`ContactSheet_White.png` は全素材をファイル名付きで確認する一覧です。
- `Layout_解像度_Normal.png` / `Layout_解像度_Skill.png` は**静的レイアウト参考**です。実際のゲームのスクリーンショットではありません。
- 文字・HP数値は切り出さず、既存UITextで動的に表示します。
- 資料は背景と装飾が合成済みです。金属装飾の暗部は輪郭マスクと黒マット補正で保持しましたが、元の独立したレイヤーを完全復元したものではありません。
- 元資料のボス・プレイヤーフレームからレールと端部を抽出しました。塗りと重なった内側は除去しています。
- ゲージ塗り、背景、遅延塗り、空枠、パネル、Highlight、赤い発光枠、円形Cooldownは透明面に再構成した素材です。
- スキルには元の四角形と、パレット用の円形派生素材をそれぞれ用意しました。
- フレームの隠れた模様、資料外まで続く棘、独立した炎・黒いエネルギーのアニメーションは復元していません。Particle風の装飾は追加していません。

## 変更ファイル

追加・変更ファイルの完全一覧は末尾の一覧と `HudChangeManifest.json` を参照してください。

| クラス・ファイル | 変更 |
|---|---|
| BattleHudComponent.h/.cpp | 表示と既存戦闘状態の接続を新設 |
| UIProgressBarComponent.h/.cpp | 調整可能な遅延開始、UV切り詰め、対象切替リセット、外部目標値の変化検出 |
| PlayerControllerComponent.h/.cpp | スキル入力表と行動受付表示の読み取り専用API |
| Sprite.h/.cpp | HUD用の選択式Samplerと表示色保持設定 |
| SpriteRenderSystem.h/.cpp | HUDだけ線形Samplerを選択 |
| CPUResourceLoader.cpp | TextureのColor/Data別キャッシュ |
| ComponentRegistry.cpp | BattleHud登録 |
| BIGShine.vcxproj / .filters | 新規ヘッダー・CPPの登録 |
| Data/Scenes/MainScene/ | 既存HPの配置更新、ガイド・状態・数値Widget、SceneList登録 |
| Tool/Hud/ExtractHudAssets.py | 素材とContactSheetの再生成 |
| Tool/Hud/IntegrateHudScene.py | 初期Scene配置の再生成。調整済みSceneへの再実行は配置を初期値へ戻すため注意 |
| Tool/Hud/ReviewHudAssets.py | 資産・JSON・VS登録の静的確認と参考画像の生成 |

## コメント

HPの割合、遅延開始、UV切り詰めと残量0の扱い、Cooldown割合とアトラス選択、表示だけのフェード、Anchor/配置基準の保存、コンボ状態の参照、Scene内Objectの寿命、表示色とSamplerの目的に日本語コメントを追加しました。

## Tracy

新規HUDのStart、LateUpdate、参照解決、HP更新、ガイド更新、Gui、Serialize/Deserialize、HP表示リセットへ既存の `BS_PROFILE_SCOPE` を追加しました。既存ProgressBar・Sprite・描画・ロードの計測を維持しています。小さなGetterには追加していません。

## ファイル形式

変更したソース・スクリプト・設定・報告はUTF-8 BOMなし・CRLFへ統一し、厳密なUTF-8読込と改行を静的確認しました。

## Visual Studio登録

- 実ファイル: `Code/Engine/UI/BattleHudComponent.h` と `.cpp`。既存UI実フォルダーに配置し、既存ファイルの移動はしていません。
- `.vcxproj`: ClIncludeとClCompileへ各1件登録しました。
- `.vcxproj.filters`: 類似UIクラスと同じ `Code\Engine\GamePlay\UI` Filterへ各1件登録しました。

## 確認内容

- RGBA画像46点のサイズ・Alpha、白・黒・市松背景の一覧を確認しました。
- SceneListの重複、親Object/Widget参照、Texture存在、VSの実ファイル登録とFilter登録を静的確認しました。
- 1920×1080、1600×900、1280×720、2560×1080、1024×768で72個の対象Widgetが画面内にあることを保存データ上の計算で確認しました。
- C++の宣言と定義、include、型名、既存API、Serialize/Deserializeの対応、ゼロ除算対策、更新/描画経路、破棄予約中Objectを参照しない経路をコード上で確認しました。
- 参照用PNGは通常/スキル表示の静的合成です。GPU上のBlendやSampler、テキスト実生成、実解像度変更の動作保証には使っていません。

## 未実装・未確認

- 実機でのHP変化、遅延演出、入力切替、装備変更、円形Cooldown、被弾/コンボ/ガード表示、演出後の復帰、形態移行、Scene再読込、Editor保存と再読込は未確認です。
- 実際のゲーム背景での視認性とアルファ境界は未確認です。ゲーム画面のスクリーンショットは保存していません。
- 実行時のUI重複、他のSpriteとImGuiへの影響、ビルドエラーの有無も未確認です。静的確認の結果だけで動作確認済みとはしていません。
- 未管理のMP/スタミナ、Buff/Debuffのゲーム効果、新規入力Action、未登録のスキル、独立したUI Particle演出は追加していません。
- 元資料の実用表示サイズを超える拡大や、失われた装飾の完全復元は保証できません。

## ビルド

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。

## 全素材PNG一覧

- `Data/Sprite/UI/Hud/Boss/BossBar_DragonDecoration.png`
- `Data/Sprite/UI/Hud/Boss/BossBar_HumanDecoration.png`
- `Data/Sprite/UI/Hud/Player/PlayerBar_Decoration.png`
- `Data/Sprite/UI/Hud/Status/Status_Buff.png`
- `Data/Sprite/UI/Hud/Status/Status_Debuff.png`
- `Data/Sprite/UI/Hud/Status/Status_Guard.png`
- `Data/Sprite/UI/Hud/Status/Status_Invincible.png`
- `Data/Sprite/UI/Hud/Status/Status_JustGuard.png`
- `Data/Sprite/UI/Hud/Skills/Skill_Combo.png`
- `Data/Sprite/UI/Hud/Skills/Skill_ComboRound.png`
- `Data/Sprite/UI/Hud/Skills/Skill_Charge.png`
- `Data/Sprite/UI/Hud/Skills/Skill_ChargeRound.png`
- `Data/Sprite/UI/Hud/Skills/Skill_Launcher.png`
- `Data/Sprite/UI/Hud/Skills/Skill_LauncherRound.png`
- `Data/Sprite/UI/Hud/Skills/Skill_Heal.png`
- `Data/Sprite/UI/Hud/Skills/Skill_HealRound.png`
- `Data/Sprite/UI/Hud/Skills/Skill_Buff.png`
- `Data/Sprite/UI/Hud/Skills/Skill_BuffRound.png`
- `Data/Sprite/UI/Hud/Skills/Skill_Magic.png`
- `Data/Sprite/UI/Hud/Skills/Skill_MagicRound.png`
- `Data/Sprite/UI/Hud/Buttons/Button_Triangle.png`
- `Data/Sprite/UI/Hud/Buttons/Button_Circle.png`
- `Data/Sprite/UI/Hud/Buttons/Button_Cross.png`
- `Data/Sprite/UI/Hud/Buttons/Button_Square.png`
- `Data/Sprite/UI/Hud/Buttons/Button_L1.png`
- `Data/Sprite/UI/Hud/Buttons/Button_R1.png`
- `Data/Sprite/UI/Hud/Buttons/Button_L2.png`
- `Data/Sprite/UI/Hud/Buttons/Button_R2.png`
- `Data/Sprite/UI/Hud/Boss/BossBar_Frame.png`
- `Data/Sprite/UI/Hud/Boss/BossBar_Background.png`
- `Data/Sprite/UI/Hud/Boss/BossBar_Fill.png`
- `Data/Sprite/UI/Hud/Boss/BossBar_DamageTrail.png`
- `Data/Sprite/UI/Hud/Player/PlayerBar_Frame.png`
- `Data/Sprite/UI/Hud/Player/PlayerBar_Background.png`
- `Data/Sprite/UI/Hud/Player/PlayerBar_HpFill.png`
- `Data/Sprite/UI/Hud/Player/PlayerBar_DamageTrail.png`
- `Data/Sprite/UI/Hud/Player/PlayerBar_ResourceFill.png`
- `Data/Sprite/UI/Hud/Boss/BossBar_HumanFrame.png`
- `Data/Sprite/UI/Hud/Boss/BossBar_AwakenedDecoration.png`
- `Data/Sprite/UI/Hud/ActionGuide/ActionGuide_Background.png`
- `Data/Sprite/UI/Hud/ActionGuide/ActionGuide_LabelBackground.png`
- `Data/Sprite/UI/Hud/ActionGuide/ActionGuide_ButtonFrame.png`
- `Data/Sprite/UI/Hud/ActionGuide/ActionGuide_Highlight.png`
- `Data/Sprite/UI/Hud/Skills/Skill_ReadyFrame.png`
- `Data/Sprite/UI/Hud/Skills/Skill_Empty.png`
- `Data/Sprite/UI/Hud/Skills/Skill_CooldownOverlay.png`

## ソース・設定の全変更一覧

- `Code/Engine/UI/BattleHudComponent.h`
- `Code/Engine/UI/BattleHudComponent.cpp`
- `Code/Engine/UI/UIProgressBarComponent.h`
- `Code/Engine/UI/UIProgressBarComponent.cpp`
- `Code/Game/Charcter/Player/PlayerControllerComponent.h`
- `Code/Game/Charcter/Player/PlayerControllerComponent.cpp`
- `Code/Engine/Graphics/Resource/Sprite.h`
- `Code/Engine/Graphics/Resource/Sprite.cpp`
- `Code/Engine/Graphics/RenderSystem/SpriteRenderSystem.h`
- `Code/Engine/Graphics/RenderSystem/SpriteRenderSystem.cpp`
- `Code/Engine/Graphics/Core/CPUResourceLoader.cpp`
- `Code/Engine/GamePlay/Serialize/ComponentRegistry.cpp`
- `BIGShine.vcxproj`
- `BIGShine.vcxproj.filters`
- `Data/Scenes/MainScene/Objects/BossNamePlate.json`
- `Data/Scenes/MainScene/Objects/BossHealthBar.json`
- `Data/Scenes/MainScene/Objects/BossHealthBackground.json`
- `Data/Scenes/MainScene/Objects/BossHealthDamageDelay.json`
- `Data/Scenes/MainScene/Objects/BossHealthHealDelay.json`
- `Data/Scenes/MainScene/Objects/BossHealthCurrent.json`
- `Data/Scenes/MainScene/Objects/BossHealthFlash.json`
- `Data/Scenes/MainScene/Objects/BossHealthEdgeHighlight.json`
- `Data/Scenes/MainScene/Objects/BossHealthFrame.json`
- `Data/Scenes/MainScene/Objects/PlayerStatusRoot.json`
- `Data/Scenes/MainScene/Objects/PlayerFaceFrame.json`
- `Data/Scenes/MainScene/Objects/PlayerFaceIcon.json`
- `Data/Scenes/MainScene/Objects/PlayerHpFrame.json`
- `Data/Scenes/MainScene/Objects/PlayerHpBar.json`
- `Data/Scenes/MainScene/Objects/PlayerHpBackground.json`
- `Data/Scenes/MainScene/Objects/PlayerHpDamageDelay.json`
- `Data/Scenes/MainScene/Objects/PlayerHpHealDelay.json`
- `Data/Scenes/MainScene/Objects/PlayerHpCurrent.json`
- `Data/Scenes/MainScene/Objects/PlayerHpFlash.json`
- `Data/Scenes/MainScene/Objects/PlayerHpEdgeHighlight.json`
- `Data/Scenes/MainScene/Objects/UIRoot.json`
- `Data/Scenes/MainScene/Objects/BossHealthValue.json`
- `Data/Scenes/MainScene/Objects/BossHealthDecoration.json`
- `Data/Scenes/MainScene/Objects/BossHealthAwakened.json`
- `Data/Scenes/MainScene/Objects/PlayerHpValue.json`
- `Data/Scenes/MainScene/Objects/HudStatusGuard.json`
- `Data/Scenes/MainScene/Objects/HudStatusJustGuard.json`
- `Data/Scenes/MainScene/Objects/HudStatusInvincible.json`
- `Data/Scenes/MainScene/Objects/HudGuideRoot.json`
- `Data/Scenes/MainScene/Objects/HudGuide0Button.json`
- `Data/Scenes/MainScene/Objects/HudGuide0Highlight.json`
- `Data/Scenes/MainScene/Objects/HudGuide0Label.json`
- `Data/Scenes/MainScene/Objects/HudGuide0SkillIcon.json`
- `Data/Scenes/MainScene/Objects/HudGuide0CooldownMask.json`
- `Data/Scenes/MainScene/Objects/HudGuide0ReadyFrame.json`
- `Data/Scenes/MainScene/Objects/HudGuide0SkillLabel.json`
- `Data/Scenes/MainScene/Objects/HudGuide0CooldownText.json`
- `Data/Scenes/MainScene/Objects/HudGuide1Button.json`
- `Data/Scenes/MainScene/Objects/HudGuide1Highlight.json`
- `Data/Scenes/MainScene/Objects/HudGuide1Label.json`
- `Data/Scenes/MainScene/Objects/HudGuide1SkillIcon.json`
- `Data/Scenes/MainScene/Objects/HudGuide1CooldownMask.json`
- `Data/Scenes/MainScene/Objects/HudGuide1ReadyFrame.json`
- `Data/Scenes/MainScene/Objects/HudGuide1SkillLabel.json`
- `Data/Scenes/MainScene/Objects/HudGuide1CooldownText.json`
- `Data/Scenes/MainScene/Objects/HudGuide2Button.json`
- `Data/Scenes/MainScene/Objects/HudGuide2Highlight.json`
- `Data/Scenes/MainScene/Objects/HudGuide2Label.json`
- `Data/Scenes/MainScene/Objects/HudGuide2SkillIcon.json`
- `Data/Scenes/MainScene/Objects/HudGuide2CooldownMask.json`
- `Data/Scenes/MainScene/Objects/HudGuide2ReadyFrame.json`
- `Data/Scenes/MainScene/Objects/HudGuide2SkillLabel.json`
- `Data/Scenes/MainScene/Objects/HudGuide2CooldownText.json`
- `Data/Scenes/MainScene/Objects/HudGuide3Button.json`
- `Data/Scenes/MainScene/Objects/HudGuide3Highlight.json`
- `Data/Scenes/MainScene/Objects/HudGuide3Label.json`
- `Data/Scenes/MainScene/Objects/HudGuide3SkillIcon.json`
- `Data/Scenes/MainScene/Objects/HudGuide3CooldownMask.json`
- `Data/Scenes/MainScene/Objects/HudGuide3ReadyFrame.json`
- `Data/Scenes/MainScene/Objects/HudGuide3SkillLabel.json`
- `Data/Scenes/MainScene/Objects/HudGuide3CooldownText.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint0Background.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint0Text.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint1Background.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint1Text.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint2Background.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint2Text.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint3Background.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint3Text.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint0Button.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint1Button.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint2Button.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint3Button.json`
- `Data/Scenes/MainScene/SceneList.json`
- `Tool/Hud/ExtractHudAssets.py`
- `Tool/Hud/HudIntegrationReport.md`
- `Tool/Hud/IntegrateHudScene.py`
- `Tool/Hud/ReviewHudAssets.py`
- `Tool/Hud/HudChangeManifest.json`
