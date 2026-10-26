# HUD文字の視認性調整

## 変更内容
操作ガイドの16個のUITextにFontStrokeWidth=0.8を設定。フォントは既存のNotoSansJP-Regularを維持する。
通常操作のラベルを使用不能時に暗くする係数を0.50から0.85へ変更。ボタンの使用可否表示とフェードは維持する。

UITextComponentにText Weight (px)のInspector項目、FontStrokeWidthのSerialize/Deserializeを追加した。古いJSONは0として従来描画を維持する。
GDI+で描画した字形のAlphaを周囲1ピクセルへ重み付きで広げ、既存の日本語フォールバックを保持する。元の字画のAlphaは下げない。
文字Textureの再生成時だけ処理し、フォント再読込や毎フレームのDraw増加はない。Padding=3は最大の補強範囲1ピクセルより大きいことを確認した。

## コメント・Tracy
字画補強の目的と実行タイミングを日本語で説明した。
既存UITextComponent::RebuildTextTextureの計測を維持し、補強ブロックにBS_PROFILE_SCOPE("UITextComponent::ThickenGlyphs")を追加した。

## ファイル形式・登録
変更ソースとJSONは厳密なUTF-8、BOMなし、CRLFを確認。新しいC++ファイルはなく、Visual Studio登録の追加は不要。

## 静的確認
設定値、既定値、保存/復元、GUI変更時のDirty通知、既存フォントキャッシュ、補強ループの画像境界とAlpha範囲、括弧の対応を確認した。
実画面での太さ・文字の見え方、FPSは未確認。フォントファイルの不良とは断定していない。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。

## 変更ファイル
- `Code/Engine/UI/UITextComponent.h`
- `Code/Engine/UI/UITextComponent.cpp`
- `Code/Engine/UI/BattleHudComponent.cpp`
- `Data/Scenes/MainScene/Objects/HudGuide0CooldownText.json`
- `Data/Scenes/MainScene/Objects/HudGuide0Label.json`
- `Data/Scenes/MainScene/Objects/HudGuide0SkillLabel.json`
- `Data/Scenes/MainScene/Objects/HudGuide1CooldownText.json`
- `Data/Scenes/MainScene/Objects/HudGuide1Label.json`
- `Data/Scenes/MainScene/Objects/HudGuide1SkillLabel.json`
- `Data/Scenes/MainScene/Objects/HudGuide2CooldownText.json`
- `Data/Scenes/MainScene/Objects/HudGuide2Label.json`
- `Data/Scenes/MainScene/Objects/HudGuide2SkillLabel.json`
- `Data/Scenes/MainScene/Objects/HudGuide3CooldownText.json`
- `Data/Scenes/MainScene/Objects/HudGuide3Label.json`
- `Data/Scenes/MainScene/Objects/HudGuide3SkillLabel.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint0Text.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint1Text.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint2Text.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint3Text.json`
