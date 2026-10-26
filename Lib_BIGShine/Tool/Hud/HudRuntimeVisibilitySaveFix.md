# HUDが表示されなくなる保存状態の修正

## 原因
MainSceneにはHUD Objectの登録が残っていたが、HUDの108 SpriteすべてがColorのAlpha=0、Visible=falseとして保存されていた。
BattleHudComponentが通常のSetColor/SetVisibleで演出中のフェード・描画抑制を適用しており、SpriteRenderer::Serializeがその値を初期設定として保存していた。
再ロード後はその透明色をHUDの基準色として取得するため、フェードインしてもAlpha=0のままで表示されなかった。直前のCamera修正とは別の保存状態の不具合。
文字の明るさも同じ保存経路で累積して暗くなり得るため、保存済みの通常HUD色を白へ復元した。Flash/EdgeHighlightの演出色は維持した。

## 変更内容
SpriteRendererにSetRuntimeColor/SetRuntimeVisibleを追加。既存SetColor/SetVisibleは保存する設定値として維持し、GetColor/IsVisibleはRuntime値があれば描画側へ返す。
Serializeは従来のColor/Visibleのみ保存し、Deserialize時はRuntime上書きを解除する。GetRenderDataとSpriteRenderSystemの描画が同じ実効値を参照することを確認した。
BattleHudは基準色をGetConfiguredColorで読み、フェードと描画停止をRuntime側へ設定する。
UIProgressBarの残量0での非表示、被弾Flash、端部HighlightもRuntime側へ分離した。
UITextのTexture再生成時にはRuntimeのAlphaを保存用色へ書き戻さず、保存設定のAlphaを維持する。
MainSceneの108 HUD Spriteの初期Alphaを1へ復元。複合入力の追加セル40個は初期Visible=falseのままにし、実行中だけ必要に応じて表示する。その他68個は初期表示を復元した。
既存のイントロ中の非表示、終了時のフェードイン、未使用セルの描画抑制、文字の太さ設定は維持している。

## コメント・Tracy・登録・形式
Runtime表示と保存設定を分離する目的に日本語コメントを追加。
BattleHudのLateUpdate/UpdateGuide、UIProgressBarの更新、UITextの再生成、SpriteのDeserializeなど既存のBS_PROFILE_SCOPEを維持した。小さなGetter/Setterへの追加計測はしていない。
新規C++ファイルはなく、Visual Studioへの追加登録は不要。
変更したソースとJSONはUTF-8、BOMなし、CRLFを確認した。

## 静的確認と未確認事項
HUDのScene登録、保存済み透明値、基準色取得経路、Runtime値がSerializeへ入らないこと、描画側Getter、Deserializeでの解除、既存APIとの整合と括弧対応を静的確認した。
実際の描画、イントロ終了後のUI復帰、演出中の保存と再読込、FPS、Debug/Releaseビルドは未確認。
既存の実行ファイルには修正コードが反映されないため、ビルド反映後にMainSceneを再読み込みして確認する。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。

## 変更ファイル
- `Code/Engine/Graphics/Resource/Sprite.h`
- `Code/Engine/Graphics/Resource/Sprite.cpp`
- `Code/Engine/UI/BattleHudComponent.cpp`
- `Code/Engine/UI/UIProgressBarComponent.cpp`
- `Code/Engine/UI/UITextComponent.cpp`
- `Data/Scenes/MainScene/Objects/BossHealthAwakened.json`
- `Data/Scenes/MainScene/Objects/BossHealthBackground.json`
- `Data/Scenes/MainScene/Objects/BossHealthCurrent.json`
- `Data/Scenes/MainScene/Objects/BossHealthDamageDelay.json`
- `Data/Scenes/MainScene/Objects/BossHealthDecoration.json`
- `Data/Scenes/MainScene/Objects/BossHealthEdgeHighlight.json`
- `Data/Scenes/MainScene/Objects/BossHealthFlash.json`
- `Data/Scenes/MainScene/Objects/BossHealthFrame.json`
- `Data/Scenes/MainScene/Objects/BossHealthHealDelay.json`
- `Data/Scenes/MainScene/Objects/BossHealthValue.json`
- `Data/Scenes/MainScene/Objects/BossNamePlate.json`
- `Data/Scenes/MainScene/Objects/HudGuide0Button.json`
- `Data/Scenes/MainScene/Objects/HudGuide0Button1.json`
- `Data/Scenes/MainScene/Objects/HudGuide0Button2.json`
- `Data/Scenes/MainScene/Objects/HudGuide0Button3.json`
- `Data/Scenes/MainScene/Objects/HudGuide0CooldownMask.json`
- `Data/Scenes/MainScene/Objects/HudGuide0CooldownText.json`
- `Data/Scenes/MainScene/Objects/HudGuide0Highlight.json`
- `Data/Scenes/MainScene/Objects/HudGuide0Label.json`
- `Data/Scenes/MainScene/Objects/HudGuide0ReadyFrame.json`
- `Data/Scenes/MainScene/Objects/HudGuide0SkillButton.json`
- `Data/Scenes/MainScene/Objects/HudGuide0SkillButton1.json`
- `Data/Scenes/MainScene/Objects/HudGuide0SkillButton2.json`
- `Data/Scenes/MainScene/Objects/HudGuide0SkillButton3.json`
- `Data/Scenes/MainScene/Objects/HudGuide0SkillIcon.json`
- `Data/Scenes/MainScene/Objects/HudGuide0SkillLabel.json`
- `Data/Scenes/MainScene/Objects/HudGuide1Button.json`
- `Data/Scenes/MainScene/Objects/HudGuide1Button1.json`
- `Data/Scenes/MainScene/Objects/HudGuide1Button2.json`
- `Data/Scenes/MainScene/Objects/HudGuide1Button3.json`
- `Data/Scenes/MainScene/Objects/HudGuide1CooldownMask.json`
- `Data/Scenes/MainScene/Objects/HudGuide1CooldownText.json`
- `Data/Scenes/MainScene/Objects/HudGuide1Highlight.json`
- `Data/Scenes/MainScene/Objects/HudGuide1Label.json`
- `Data/Scenes/MainScene/Objects/HudGuide1ReadyFrame.json`
- `Data/Scenes/MainScene/Objects/HudGuide1SkillButton.json`
- `Data/Scenes/MainScene/Objects/HudGuide1SkillButton1.json`
- `Data/Scenes/MainScene/Objects/HudGuide1SkillButton2.json`
- `Data/Scenes/MainScene/Objects/HudGuide1SkillButton3.json`
- `Data/Scenes/MainScene/Objects/HudGuide1SkillIcon.json`
- `Data/Scenes/MainScene/Objects/HudGuide1SkillLabel.json`
- `Data/Scenes/MainScene/Objects/HudGuide2Button.json`
- `Data/Scenes/MainScene/Objects/HudGuide2Button1.json`
- `Data/Scenes/MainScene/Objects/HudGuide2Button2.json`
- `Data/Scenes/MainScene/Objects/HudGuide2Button3.json`
- `Data/Scenes/MainScene/Objects/HudGuide2CooldownMask.json`
- `Data/Scenes/MainScene/Objects/HudGuide2CooldownText.json`
- `Data/Scenes/MainScene/Objects/HudGuide2Highlight.json`
- `Data/Scenes/MainScene/Objects/HudGuide2Label.json`
- `Data/Scenes/MainScene/Objects/HudGuide2ReadyFrame.json`
- `Data/Scenes/MainScene/Objects/HudGuide2SkillButton.json`
- `Data/Scenes/MainScene/Objects/HudGuide2SkillButton1.json`
- `Data/Scenes/MainScene/Objects/HudGuide2SkillButton2.json`
- `Data/Scenes/MainScene/Objects/HudGuide2SkillButton3.json`
- `Data/Scenes/MainScene/Objects/HudGuide2SkillIcon.json`
- `Data/Scenes/MainScene/Objects/HudGuide2SkillLabel.json`
- `Data/Scenes/MainScene/Objects/HudGuide3Button.json`
- `Data/Scenes/MainScene/Objects/HudGuide3Button1.json`
- `Data/Scenes/MainScene/Objects/HudGuide3Button2.json`
- `Data/Scenes/MainScene/Objects/HudGuide3Button3.json`
- `Data/Scenes/MainScene/Objects/HudGuide3CooldownMask.json`
- `Data/Scenes/MainScene/Objects/HudGuide3CooldownText.json`
- `Data/Scenes/MainScene/Objects/HudGuide3Highlight.json`
- `Data/Scenes/MainScene/Objects/HudGuide3Label.json`
- `Data/Scenes/MainScene/Objects/HudGuide3ReadyFrame.json`
- `Data/Scenes/MainScene/Objects/HudGuide3SkillButton.json`
- `Data/Scenes/MainScene/Objects/HudGuide3SkillButton1.json`
- `Data/Scenes/MainScene/Objects/HudGuide3SkillButton2.json`
- `Data/Scenes/MainScene/Objects/HudGuide3SkillButton3.json`
- `Data/Scenes/MainScene/Objects/HudGuide3SkillIcon.json`
- `Data/Scenes/MainScene/Objects/HudGuide3SkillLabel.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint0Background.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint0Button.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint0Button1.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint0Button2.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint0Button3.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint0Text.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint1Background.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint1Button.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint1Button1.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint1Button2.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint1Button3.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint1Text.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint2Background.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint2Button.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint2Button1.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint2Button2.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint2Button3.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint2Text.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint3Background.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint3Button.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint3Button1.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint3Button2.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint3Button3.json`
- `Data/Scenes/MainScene/Objects/HudGuideHint3Text.json`
- `Data/Scenes/MainScene/Objects/HudStatusGuard.json`
- `Data/Scenes/MainScene/Objects/HudStatusInvincible.json`
- `Data/Scenes/MainScene/Objects/HudStatusJustGuard.json`
- `Data/Scenes/MainScene/Objects/PlayerFaceFrame.json`
- `Data/Scenes/MainScene/Objects/PlayerFaceIcon.json`
- `Data/Scenes/MainScene/Objects/PlayerHpBackground.json`
- `Data/Scenes/MainScene/Objects/PlayerHpCurrent.json`
- `Data/Scenes/MainScene/Objects/PlayerHpDamageDelay.json`
- `Data/Scenes/MainScene/Objects/PlayerHpEdgeHighlight.json`
- `Data/Scenes/MainScene/Objects/PlayerHpFlash.json`
- `Data/Scenes/MainScene/Objects/PlayerHpFrame.json`
- `Data/Scenes/MainScene/Objects/PlayerHpHealDelay.json`
- `Data/Scenes/MainScene/Objects/PlayerHpValue.json`
