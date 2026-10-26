# 暴走開始Effect・敵HUD・マウスガイドの修正

## 変更内容

暴走開始の約0.6秒に、既存RadialBlurとBloomのColorFilterを使う。25％・10％・5％の段階で強くし、RawDeltaによる同じ時間曲線で出現・復帰させる。

| HP閾値 | ブラー倍率 | 既定の最大追加Radius | 白黒化の強さ |
| --- | --- | --- | --- |
| 25％ | 1.0 | 0.035 | 55％ |
| 10％ | 1.5 | 0.0525 | 80％ |
| 5％ | 2.0 | 0.07 | 100％ |

白黒化では彩度だけでなく色相・RGBバランスも中立へ近づけ、最終段階で色味が残らないようにする。開始前のFilter全体とBlurを保存し、演出終了・リトライ・破棄時に復元する。UIは既存どおりPostEffect後に描画する。

両形態の顔はBossHealthDecorationの独立したSpriteだったため、実行時・MainScene保存値・HUD生成スクリプトで非表示にする。HPバーとフレーム、アセットそのもの、Objectの参照は維持する。

人型の名前は常に「黒き人形」とし、幅325の名前欄で長い状態説明が折り返す原因を除いた。暴走中の「無敵」はHP値欄と紫のフレームで引き続き表示する。

KeyMouseInputSheet.pngの実画像を確認し、VK_LBUTTONをセル(14,2)、VK_RBUTTONを(1,1)へ修正した。赤い押下部のセル内X平均はそれぞれ20.85と42.15で、左右の対応を確認。Data/Input/KeyConfig.jsonの既定割当は弱攻撃=code 1（左）、強攻撃=code 2（右）。実入力の割当は変更せず、表示の参照セルだけ修正する。ラベルも「弱攻撃」「強攻撃」にそろえた。

## 調整先

HumanBossAIComponent Inspector → HP Berserk / Final Stunの各段階にEntrance blur multiplierとEntrance monochromeを追加。全段階共通のBerserk entrance blurが基準Radius。JSONのBerserk.EntranceBlurMultipliers、EntranceMonochromeへ保存し、旧データには既定値を使用する。共通ボス・MainScene・HumanBossTest・Phase2HumanBoss Prefabにも値を設定した。

## 変更ファイル

- Code/Game/AI/Human/HumanBossAIComponent.h
- Code/Game/AI/Human/HumanBossAIComponent.Berserk.cpp
- Code/Engine/UI/BattleHudComponent.cpp
- Code/Engine/UI/InputIconAtlas.cpp
- Data/HumanBoss/HumanBoss.json
- Data/Scenes/MainScene/Objects/HumanBoss.json
- Data/Scenes/HumanBossTest/Objects/HumanBoss.json
- Data/Prefabs/Phase2HumanBoss.json
- Data/Scenes/MainScene/Objects/BossHealthDecoration.json
- Tool/Hud/IntegrateHudScene.py
- Tool/AnathemaHudAndBerserkEffects.md（本書）

## コメント・Tracy・形式・登録

段階ごとの強さ、色バランスの中立化、描画設定の保存復元、顔画像の非表示、名前の折り返し防止、マウスの左右セルについて日本語コメントを追加。既存のUpdateBerserkEntrance、RestoreBerserkEntrance、BattleHudComponent、InputIconAtlasのBS_PROFILE_SCOPEを維持。新しい更新クラスやShaderは追加していない。

全対象ファイルはUTF-8 BOMなし・CRLF。新規C++ファイルはなく、変更した既存ファイルのvcxproj/filters登録を確認した。物理フォルダーやFilterは移動していない。

## 確認内容と手順

静的に、Filter APIの型・項目、保存と復元、GUI/Serialize/Deserializeの対応、4プリセットの値、顔Spriteの非表示経路、固定名と無敵表示の分離、入力割当とAtlas座標、画像内の赤い押下部、JSON・Python構文・C++括弧・VS登録・UTF-8/CRLFを確認。

実機では25％・10％・5％の開始演出を比較し、最後ほどブラーと白黒化が強く、終了・リトライ後に元の色へ戻ることを確認する。両形態で顔画像が出ないこと、暴走中も名前が1行で無敵表示が残ることを確認する。HUDのGuide PreviewをKeyboard & Mouseにするか実入力を切り替え、弱攻撃が左クリック、強攻撃が右クリックとして表示されることを確認する。これらの実画面確認は未実施。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
