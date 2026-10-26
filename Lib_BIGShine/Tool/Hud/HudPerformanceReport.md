# HUD情報取得・更新の負荷対策

## 変更内容

コード調査では、HUDの情報取得からスキル全Phaseのアニメーション・Action・Collider検証を呼ぶ経路と、文字Textureを更新するたびに日本語フォントをファイルから読み込む経路を確認しました。FPS低下の寄与率と改善後のFPSは未測定です。

- BattleHud: 要素検索を索引化し、非表示スキルの情報取得を省略します。HPのComponentとProgressBarは弱参照で保持し、対象変更時に再解決します。Actorの削除予約・形態変更の確認は維持します。
- HPの数値、操作ガイド、スキル名、ボタン画像パスは表示内容が変わった時だけ生成します。HPゲージの補間・フェード・円形クールタイムは毎フレーム更新します。
- 残り秒数のTextは0.1秒単位の表示値または使用可否が変わった場合にだけ更新します。UI専用のクールタイムは作りません。
- PlayerController: HUD向けの重い素材検証だけをスロット別に0.25秒キャッシュします。装備名変更とスキル定義の再読込では再検証します。アニメーションやActionを別Editorから変更した場合、表示用の素材検証結果は最大0.25秒後に更新されます。
- クールタイム、接地/空中、キャンセル受付、行動状態は従来どおり即時に参照します。実際のスキル発動時のCanExecuteSkillとSkillExecutionの検証は変更していません。
- UIText: 同じフォントのPrivateFontCollectionとFontFamilyを共有し、数値更新での再読込を避けます。フォント所有権はText Componentが持ち、共有索引は弱参照です。Arialへのフォールバック、Editorからの明示的なフォント再読込を維持します。
- 色の更新だけではWidget配置をDirtyにしません。レンダラーの描画方式は変更していません。

## 切り分け方法

実行時の `UIRoot` → `BattleHudComponent` Inspectorで `Freeze HUD Information (keep drawing)` を切り替えます。

チェックすると最後のHUD表示を保持したまま、このComponentのLateUpdateを停止します。ゲーム本体とSprite描画は継続します。チェックを外すと現在の状態へ更新を再開します。この診断設定はSceneへ保存しません。

この切替でFPSが大きく変わる場合、HUDの情報更新経路が負荷に寄与していると判断できます。改善の有無は実際の環境で確認する必要があります。

## 変更ファイル

- `Code/Engine/UI/BattleHudComponent.h`
- `Code/Engine/UI/BattleHudComponent.cpp`
- `Code/Game/Charcter/Player/PlayerControllerComponent.h`
- `Code/Game/Charcter/Player/PlayerControllerComponent.cpp`
- `Code/Engine/UI/UITextComponent.h`
- `Code/Engine/UI/UITextComponent.cpp`

## コメントとTracy

キャッシュの目的、有効期間、再解決条件、情報更新停止、フォント所有権を日本語で説明しました。

既存のHUD・Player・UIText計測を維持し、重い素材検証を `PlayerControllerComponent::ValidateHudSkillAssets`、フォント解析を `UITextComponent::LoadSharedFont` で追跡します。`HUD.InformationUpdatesEnabled` のPlotで診断スイッチの状態を確認できます。

## ファイル形式・Visual Studio登録

対象ファイルはUTF-8 BOMなし・CRLFです。既存ヘッダー・CPP内の変更で、新しいC++ファイルやFilter登録はありません。キャッシュと診断状態は実行時専用で、既存のJSON項目を削除・変更していません。

## 確認内容

宣言と定義、include、既存API、弱参照、フォント破棄順、キャッシュ破棄条件、非表示時の分岐、既存Serialize/DeserializeとVS登録を静的確認しました。

FPSの回復、実際の表示・入力・Editor操作、CPU/GPUの負荷割合は未確認です。

## ビルド

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
