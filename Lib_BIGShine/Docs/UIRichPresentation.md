# UI演出・表情切り替え・Editorタイトル復帰

## 変更内容

- タイトルのロゴ・ボタン文字・選択装飾へUIVisualEffectComponentを追加。選択中だけ文字の効果量を強める。
- ボス名・HP値・プレイヤーHP・スキル名・クールダウン文字へ控えめな発光と黒い影を追加。文字のぼかし、残像、Dissolve、UV移動は使用しない。
- ポーズ・死亡メニューはUIWidgetComponent、UITextComponent、SpriteRenderer、UIVisualEffectComponentで描画。ImGuiでゲームメニューを描画しない。
- メニューの表示Objectはメニュー自身が所有し、通常Sceneの保存・更新リストへ登録しない。通常HUDの後で既存SpriteRenderSystemへ提出する。
- ポーズ中も既存の非スケール更新で演出時間を進める。決定キー解放待ちと既存Actionによる再開・タイトル遷移、死亡復帰処理は維持。
- EditorのSceneメニューへ「Return to Title...」を追加。Stop後の編集状態で使用できる。確認画面の「Save Scene...」は既存保存画面を開いてEditorへ戻る。保存後に再度「Return to Title...」を選ぶ。「Return Without Saving」は既存Loading Sceneを経由してタイトルへ戻る。

## 表情

既存の横5セル画像を使用。BattleHudComponentのInspectorでセル番号、瀕死HP割合、保持時間を変更でき、JSONへ保存する。既存JSONに設定がなくても既定値で動作する。

| 左から | 状況 |
| --- | --- |
| 1 | 通常 |
| 2 | HP減少時。既定0.8秒保持 |
| 3 | 被ジャストガード、HP25%以下、死亡 |
| 4 | HP回復、回復スキル、ジャストガード成功 |
| 5 | 攻撃スキル、CounterTag所持、カウンター攻撃 |

死亡を最優先とし、一時反応、瀕死、回復スキル、攻撃スキル・CounterTag、通常の順に選ぶ。表情取得ではTriggerやTagを消費しない。UISpriteSheetComponentの実行時セル指定は保存対象のColumnIndexと分離する。

## コメント・Tracy・登録

表情の優先順位、連続ガード判定、表示Objectの所有と保存除外、文字の縦横比保持、Editorからの離脱に日本語コメントを追加。BS_PROFILE_SCOPEをUpdatePortrait、SubmitVisual、DrawBackdrop、RenderMenu、死亡RenderOutput、EditorのDrawMenuBarに追加し、既存のメニュー更新・描画計測を維持。
新規C++ファイルはない。既存の実配置、vcxproj、vcxproj.filters登録を維持し、両登録を静的に照合した。

## 静的確認

宣言と呼び出し、既存API、描画順、ポーズ中の更新経路、JSONの保存と復元、既定値、弱参照の再取得、文字の縦横比保持を確認。変更ソース17ファイルと設定JSON31ファイルについて、厳密UTF-8、BOMなし、CRLF、括弧対応、JSON構文とプリセット参照を確認した。
実画面での可読性・描画位置・操作感・コンパイル結果は未確認。

## 変更ファイル

- `Code/Engine/UI/UISpriteSheetComponent.h`
- `Code/Engine/UI/UISpriteSheetComponent.cpp`
- `Code/Engine/UI/BattleHudComponent.h`
- `Code/Engine/UI/BattleHudComponent.cpp`
- `Code/Engine/UI/UIVisualEffectComponent.h`
- `Code/Engine/UI/UIVisualEffectComponent.cpp`
- `Code/Engine/UI/MenuPresentation.h`
- `Code/Engine/UI/MenuPresentation.cpp`
- `Code/Engine/UI/MenuNavigationComponent.cpp`
- `Code/Engine/UI/GameMenuComponent.h`
- `Code/Engine/UI/GameMenuComponent.cpp`
- `Code/Game/GameMode/PlayerDeathDirector.h`
- `Code/Game/GameMode/PlayerDeathDirector.cpp`
- `Code/Engine/GamePlay/Scene/SceneBase.cpp`
- `Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp`
- `Code/Engine/GamePlay/System/JustGuardSystem.cpp`
- `Code/Editor/Window/EditorWindowManager.cpp`
- `Data/UI/UIVisualEffectPresets.json`
- `Data/Scenes/HumanBossTest/Objects/BossNamePlate.json`
- `Data/Scenes/LoadingScene/Objects/BossNamePlate.json`
- `Data/Scenes/MainScene/Objects/BossHealthValue.json`
- `Data/Scenes/MainScene/Objects/BossNamePlate.json`
- `Data/Scenes/MainScene/Objects/HudGuide0CooldownText.json`
- `Data/Scenes/MainScene/Objects/HudGuide0SkillLabel.json`
- `Data/Scenes/MainScene/Objects/HudGuide1CooldownText.json`
- `Data/Scenes/MainScene/Objects/HudGuide1SkillLabel.json`
- `Data/Scenes/MainScene/Objects/HudGuide2CooldownText.json`
- `Data/Scenes/MainScene/Objects/HudGuide2SkillLabel.json`
- `Data/Scenes/MainScene/Objects/HudGuide3CooldownText.json`
- `Data/Scenes/MainScene/Objects/HudGuide3SkillLabel.json`
- `Data/Scenes/MainScene/Objects/PlayerHpValue.json`
- `Data/Scenes/TitleBackground/Objects/TitleLogo.json`
- `Data/Scenes/TitleBackground/Objects/TitleMenu_exitGlow.json`
- `Data/Scenes/TitleBackground/Objects/TitleMenu_exitHighlight.json`
- `Data/Scenes/TitleBackground/Objects/TitleMenu_exitLabel.json`
- `Data/Scenes/TitleBackground/Objects/TitleMenu_exitMarker.json`
- `Data/Scenes/TitleBackground/Objects/TitleMenu_game_startGlow.json`
- `Data/Scenes/TitleBackground/Objects/TitleMenu_game_startHighlight.json`
- `Data/Scenes/TitleBackground/Objects/TitleMenu_game_startLabel.json`
- `Data/Scenes/TitleBackground/Objects/TitleMenu_game_startMarker.json`
- `Data/Scenes/TitleBackground/Objects/TitleMenu_settingsGlow.json`
- `Data/Scenes/TitleBackground/Objects/TitleMenu_settingsHighlight.json`
- `Data/Scenes/TitleBackground/Objects/TitleMenu_settingsLabel.json`
- `Data/Scenes/TitleBackground/Objects/TitleMenu_settingsMarker.json`
- `Data/Scenes/TitleBackground/Objects/TitleMenu_tutorialGlow.json`
- `Data/Scenes/TitleBackground/Objects/TitleMenu_tutorialHighlight.json`
- `Data/Scenes/TitleBackground/Objects/TitleMenu_tutorialLabel.json`
- `Data/Scenes/TitleBackground/Objects/TitleMenu_tutorialMarker.json`
- `Docs/UIRichPresentation.md`（本報告）

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
