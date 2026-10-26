# シーンUIのComponent移行

タイトルのObject構成とUIを `Data/Scenes/TitleBackground/SceneList.json` および `Objects/*.json` へ集約しました。
`SceneTitle` は表示専用Sceneの指定と `LoadSceneAsync` の呼び出しだけを行います。

## 汎用Component

- `MenuNavigationComponent`: 読み込み済みUIの選択、KeyConfigのAction入力、マウス操作、リピート、効果音と決定Actionを管理します。UIを生成しません。
- `ScreenFadeComponent`: 同じObjectのSpriteの実行時透明度を更新し、暗転後に指定Actionを一度実行します。MainSceneにも配置しています。
- `ScenePresentationComponent`: JSONで指定したCamera、ロゴ、装備の同期、音声、カーソルとPostEffectを管理します。PostEffectは描画後に復元します。

## 編集先

- メニューの項目、参照UI、入力Action、決定Action: `Data/Scenes/TitleBackground/Objects/TitlePresentation.json` の `MenuNavigationComponent.Data`。
- 表示文字、Font、サイズ、配置、装飾色: 同Sceneの `TitleMenu_*` と `TitleGuide*` のObject JSON。
- カメラ、ロゴ、音声と描画設定: `TitlePresentation.json` の `ScenePresentationComponent.Data`。
- タイトルの暗転時間: `TitleScreenFade.json`。本編の暗転時間: `Data/Scenes/MainScene/Objects/GameScreenFade.json`。
- 入力割り当て: `Data/Input/KeyConfig.json` の `UI.MenuPrevious`、`UI.MenuNext`、`UI.MenuConfirm`、`UI.OpenEditor`、`UI.PresentationSettings`。

旧 `Data/UI/TitleMenu.json` と `TitlePresentation.json` は削除していませんが、タイトルからは参照しません。設定はScene Objectから復元します。
KeyConfigは既存の共通起動処理でScene生成前にロードされ、SceneTitleにはロード処理を置いていません。
F2の開発用設定WindowはComponentへ移しました。メニューの配置はUIWidgetのRectTransformで編集します。

## 登録と静的確認

新しいヘッダーとCPPは `Code/Engine/UI/` に配置し、`.vcxproj` と `.vcxproj.filters` の `Code\Engine\GamePlay\UI` にそれぞれ登録しました。
ComponentRegistryへの登録、JSON参照、保存と復元、呼び出し順、include、宣言と定義、UTF-8 BOMなし・CRLFを静的に確認しました。
主要な更新、初期化、描画設定には既存の `BS_PROFILE_SCOPE` を使用しています。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。画面上の見た目と入力の実動作は未確認です。

## 変更ファイル

- `BIGShine.vcxproj`
- `BIGShine.vcxproj.filters`
- `Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp`
- `Code/Engine/GamePlay/Scene/SceneBase.cpp`
- `Code/Engine/GamePlay/Scene/SceneBase.h`
- `Code/Engine/GamePlay/Scene/SceneGame.cpp`
- `Code/Engine/GamePlay/Scene/SceneGame.h`
- `Code/Engine/GamePlay/Scene/SceneTitle.cpp`
- `Code/Engine/GamePlay/Scene/SceneTitle.h`
- `Code/Engine/GamePlay/Serialize/ActionRegistry.cpp`
- `Code/Engine/GamePlay/Serialize/ComponentRegistry.cpp`
- `Code/Engine/UI/MenuNavigationComponent.cpp`
- `Code/Engine/UI/MenuNavigationComponent.h`
- `Code/Engine/UI/ScenePresentationComponent.cpp`
- `Code/Engine/UI/ScenePresentationComponent.h`
- `Code/Engine/UI/ScreenFadeComponent.cpp`
- `Code/Engine/UI/ScreenFadeComponent.h`
- `Data/Input/KeyConfig.json`
- `Data/Scenes/MainScene/Objects/GameScreenFade.json`
- `Data/Scenes/MainScene/SceneList.json`
- `Data/Scenes/TitleBackground/Objects/TitleGuideDeveloper.json`
- `Data/Scenes/TitleBackground/Objects/TitleGuideKeyboard.json`
- `Data/Scenes/TitleBackground/Objects/TitleGuidePlayStation.json`
- `Data/Scenes/TitleBackground/Objects/TitleLogo.json`
- `Data/Scenes/TitleBackground/Objects/TitleMenuPanel.json`
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
- `Data/Scenes/TitleBackground/Objects/TitlePresentation.json`
- `Data/Scenes/TitleBackground/Objects/TitleScreenFade.json`
- `Data/Scenes/TitleBackground/Objects/TitleUI.json`
- `Data/Scenes/TitleBackground/SceneList.json`
