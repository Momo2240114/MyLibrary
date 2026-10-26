# UI / Screen Transition 演出拡張

## 変更内容

Scene遷移の共通インターフェースを維持したまま、12種類・56プリセットへ拡張しました。
既存のFADE/RADIAL/SLASH/DISSOLVE/SHUTTER/WIPE/TEXTURE_MASKに、DIAMOND/CURTAIN/CHECKER/SPIRAL/GLASSを追加しています。
BigShine / ANATHEMAの既存プロファイルとScene固有設定は維持しています。

画像と文字の演出には、同じGameObjectへ追加できる`UIVisualEffectComponent`と`UIMotionComponent`を用意しました。
SpriteRenderer経由の画像、UITextComponentのFontUI、Spriteを使うUI装飾に対応します。
ImGuiそのものの文字や、子Object全体へ自動的に伝播するGroup Effectは対象に含みません。
UITextは文字列全体を一枚のTextureとして処理するため、文字ごとのアニメーションではありません。

## 試し方

1. EditorのScene選択で`UIEffectsGallery`を開きます。既存タイトル/MainSceneは変更していません。
2. カードのLabel Objectへ付いた二つのComponentをInspectorで開きます。
3. `Previous Preset` / `Next Preset`で切り替え、`Preview / Restart`で編集状態のまま試写できます。
4. `Pause / Resume`、`Stop / Restore`で再生を操作できます。ゲーム実行時は`Play On Start`で再生します。
5. F9でScreen Transitionを開き、同じ前後切替、名前Filter、`Cycle Preview`で比較します。
6. `Cover` / `Reveal` / `Play`はSceneを切り替えずにプレビューします。Windowを閉じると試写を解除します。

設定変更は`Save Name`とSaveボタンで各JSONへ保存します。ComponentのSettingsはScene ObjectにもSerializeされます。
既存のUIAnimationPlayer、メニューのRuntimeColor、RuntimeVisualTransform、保存Transformを直接書き換えずに合成します。
位置・拡縮はWidgetのレイアウト計算に反映し、入力Rectも追従させます。回転後の入力判定は従来と同じ軸平行Rectです。
Widgetを持たないSpriteの移動量は画面ピクセル、Widget付きはBoardの基準座標です。

## MaskTexture Fade

`Transition Type = TEXTURE_MASK`で使用します。選択UIはMeshVFXと同じ`AssetSlotGui::Draw`を再利用しています。
画像プレビュー、画像クリックまたはBrowseによるファイル選択、Clear、Asset BrowserからのDragDropに対応します。
GPU読み込みには既存CPUResourceLoaderのキャッシュを使い、UTF-8の日本語パスはWindowsDialogHelperで変換します。

| Mask Mode | 動作 |
|---|---|
| Threshold | 明度の順に画面を覆う |
| Directional Mask | 方向付きWipeの境界へMaskの凹凸を加える |
| Radial Mask | 中心からの距離とMaskを合成する |
| Flipbook Mask | Columns / Rows / FrameCountで指定した連番画像を進行度に同期する |

Red / Green / Blue / Alpha / Luminance、反転、UV拡縮・移動・回転・スクロールを設定できます。
Flipbookは左上から行方向に並べたグレースケールMaskを使い、白を被覆、黒を表示として扱います。
TransitionInでは同じ進行度が逆向きに進むため、連番も逆再生されます。
非連番のThresholdは暗い部分から覆い、Invertで順序を反転できます。
開始・終了ではTextureの内容に関係なく完全透明・完全被覆を保証します。

`Data/Texture/TransitionMasks/`にLinear/Radial/Diamond/Stripes/Waves/Checker/Spiral/InkとRadialFlipbookを用意しました。
これらは数式から生成したグレースケール素材です。添付動画を素材として切り出したものではありません。
DISSOLVEやSHUTTERなどが持つ既存Textureスロットにも同じ選択・プレビューUIを適用しています。

## ガラス演出

`GLASS`または`Glass_Shatter` / `Glass_LargeShards` / `Glass_FineShards`を選択します。
画面をSRV付きTextureへコピーし、不規則な三角形の破片を拡縮・回転・落下させ、隙間を黒くします。
TransitionOutで旧Scene、TransitionInで新Sceneをそれぞれ一度取り込みます。画面サイズの変更時は再作成します。
Shard Columns、Fall / Spread、Shard Rotation、Edge Color / Width / Glowを編集できます。
背景と最大48枚の三角形はSV_VertexIDで生成し、一回のDrawへまとめています。
完了・キャンセル時に画面コピーを解放します。画面コピーを作成できない構成では通常のFadeへ戻します。
これは2D画面の破片表現であり、物理シミュレーションや3Dメッシュの破壊ではありません。

## UI Effect / Motion

見た目は21プリセット、モーションは23プリセットです。
発光・縁取り・影・走査光・明滅・グラデーション・グレースケール・セピア・反転・ピクセル化・ぼかし・Dissolve/Burn・UV移動・残像を組み合わせられます。
UI側のDissolve/BurnもMeshVFX共通のTexture選択UIを使い、Maskの赤チャンネル・反転・UV拡縮/移動を設定できます。
空のMaskは手続きNoiseへ戻ります。GlowやOutlineは字形のAlphaを使うため、FontUIにも適用されます。
効果強度のTween、Loop、PingPong、Delay、Speed、Easing、手動Progressに対応します。

通常の合成効果は一Spriteにつき一回のDrawで処理します。残像は最大6枚の追加Drawです。
残像は頂点・UV・色・SRVだけを保持し、毎回Textureを複製しません。画面サイズの変更時は履歴を破棄します。
UI EffectはDefault SpriteShader向けで、既存の独自ShaderIDへ強制的に差し替えません。
Shader未生成時は通常のSprite描画へ戻り、モーションは継続できます。
実際のGPU所要時間と各解像度での見た目は実行未確認です。

## 更新経路と互換性

GameObjectManagerの既存UpdateUnscaled呼び出しを使い、ロード中・メニューポーズ中もUIの時計を進めます。
Editorでは明示的なPreviewだけ再生します。Editorの再生開始は既存のScene復元経路を使います。
SceneManager、ロード待機状態、被覆完了通知、Scene切替タイミング、TimeManagerの方式は変更していません。
F9の自動試写はScene更新後に最低優先度で要求し、本来のScene遷移が発生したら停止します。

OwnerのActionとして`MotionPlay/Stop/Pause/Resume`と`EffectPlay/Stop/Pause/Resume`を登録しています。
Action名のOwner修飾は既存GameObject APIに従います。OwnerとComponentはweak_ptrで参照します。

## 日本語コメント / Tracy

新規クラスの役割、モーション合成、描画余白、残像の寿命、定数Bufferの対応、画面コピー、破片生成、設定復元へ日本語コメントを付けています。
既存の`BS_PROFILE_SCOPE`を使い、Start、UpdateUnscaled、Gui、プリセット/Texture読み込みを計測します。
描画処理は既存SpriteRenderSystemのZoneとShaderTransitionEffectのCPU/GPU Zoneで追跡します。
Tracyの有効化方式は変更していません。

## Visual Studio登録

実ファイルは既存の大分類`Code/Engine/UI`、`Shader`、`Shader/Transition`へ追加しました。
既存ファイルの移動やincludeの一括変更は行っていません。
`.vcxproj`へ新規8クラスのHeader/CPP、Pixel Shader 6本、Vertex Shader 1本を登録しました。
Filterは汎用UIを`Code\Engine\GamePlay\UI\Effects`、各Transitionを`Code\Engine\GamePlay\UI\ScreenTransition\Effects`に登録しています。
UI Shaderは`Shaders\Sprite`、Transition Shaderは`Shaders\PostEffect\Transition`です。
プリセット、Gallery、Mask素材はNone項目としてData以下の専用Filterへ登録しています。

## 静的確認

- 新規宣言と定義、include参照、既存APIとの呼び出し整合性を確認しました。
- 12種類のEnum、Factory登録、Combo表示と56プリセットの種類を照合しました。
- JSON構文、Effectキー、UI保存・復元、TextureとGallery Objectの参照先を確認しました。
- CPU/HLSLのUI定数Bufferが同じ17個のfloat4（272バイト）であることを照合しました。
- `.vcxproj`と`.filters`のXML、重複項目、新規登録先、Filter参照を確認しました。
- 対象のテキストを厳密なUTF-8として読み込み、BOMなし・CRLFを確認しました。
- リソース所有権、試写の終了、破片の終端被覆、残像枚数と解放経路をコード上で確認しました。

Shaderはソースとプロジェクト登録までの追加です。CSOの生成・コンパイル結果は未確認です。
タイトル開始、ロード時間の長短、ロード失敗、連続要求、Editor切替、解像度別表示の実機確認は実施していません。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。

## 参考

- [Transition Idea_100](https://gameanimation.info/archives/2615)
- [光と残像の演出](https://gameanimation.info/archives/2657)
- [UIモーションのアイデア](https://gameanimation.info/archives/2497)
- [UIEffect紹介](https://anogame.net/unity-oss-uieffect/)
- [UIEffect公式リポジトリ](https://github.com/mob-sakai/UIEffect)

参考にしたのは表現の分類と調整の考え方です。Unityパッケージの導入やソースの移植は行っていません。
添付された二本の動画はコマ送りの静止画で確認しました。

## 変更ファイル一覧

- `Code/Engine/UI/UIPresentationPresets.h`
- `Code/Engine/UI/UIPresentationPresets.cpp`
- `Code/Engine/UI/UIMotionComponent.h`
- `Code/Engine/UI/UIMotionComponent.cpp`
- `Code/Engine/UI/UIVisualEffectComponent.h`
- `Code/Engine/UI/UIVisualEffectComponent.cpp`
- `Code/Engine/UI/DiamondTransition.h`
- `Code/Engine/UI/DiamondTransition.cpp`
- `Code/Engine/UI/CurtainTransition.h`
- `Code/Engine/UI/CurtainTransition.cpp`
- `Code/Engine/UI/CheckerTransition.h`
- `Code/Engine/UI/CheckerTransition.cpp`
- `Code/Engine/UI/SpiralTransition.h`
- `Code/Engine/UI/SpiralTransition.cpp`
- `Code/Engine/UI/GlassTransition.h`
- `Code/Engine/UI/GlassTransition.cpp`
- `Code/Engine/UI/TransitionParameters.h`
- `Code/Engine/UI/TransitionParameters.cpp`
- `Code/Engine/UI/ScreenTransitionManager.h`
- `Code/Engine/UI/ScreenTransitionManager.cpp`
- `Code/Engine/UI/ShaderTransitionEffect.h`
- `Code/Engine/UI/ShaderTransitionEffect.cpp`
- `Code/Engine/UI/TextureMaskTransition.cpp`
- `Code/Engine/UI/UIWidgetComponent.h`
- `Code/Engine/UI/UIWidgetComponent.cpp`
- `Code/Engine/Graphics/RenderSystem/SpriteRenderSystem.h`
- `Code/Engine/Graphics/RenderSystem/SpriteRenderSystem.cpp`
- `Code/Engine/GamePlay/Serialize/ComponentRegistry.cpp`
- `Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp`
- `BIGShine.vcxproj`
- `BIGShine.vcxproj.filters`
- `Shader/UIVisualEffectPS.hlsl`
- `Shader/Transition/TransitionCommon.hlsli`
- `Shader/Transition/TextureMaskTransitionPS.hlsl`
- `Shader/Transition/DiamondTransitionPS.hlsl`
- `Shader/Transition/CurtainTransitionPS.hlsl`
- `Shader/Transition/CheckerTransitionPS.hlsl`
- `Shader/Transition/SpiralTransitionPS.hlsl`
- `Shader/Transition/GlassTransitionPS.hlsl`
- `Data/UI/SceneTransitions.json`
- `Data/UI/UIMotionPresets.json`
- `Data/UI/UIVisualEffectPresets.json`
- `Data/Scenes/UIEffectsGallery/SceneList.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card01.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card01Label.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card02.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card02Label.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card03.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card03Label.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card04.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card04Label.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card05.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card05Label.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card06.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card06Label.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card07.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card07Label.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card08.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card08Label.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card09.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card09Label.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card10.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card10Label.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card11.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card11Label.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card12.json`
- `Data/Scenes/UIEffectsGallery/Objects/Card12Label.json`
- `Data/Scenes/UIEffectsGallery/Objects/GalleryBoard.json`
- `Data/Scenes/UIEffectsGallery/Objects/GalleryCamera.json`
- `Data/Scenes/UIEffectsGallery/Objects/GalleryHeading.json`
- `Data/Scenes/UIEffectsGallery/Objects/GalleryHint.json`
- `Data/Scenes/UIEffectsGallery/Objects/GallerySpriteExample.json`
- `Data/Texture/TransitionMasks/Checker.png`
- `Data/Texture/TransitionMasks/Diamond.png`
- `Data/Texture/TransitionMasks/Ink.png`
- `Data/Texture/TransitionMasks/Linear.png`
- `Data/Texture/TransitionMasks/Radial.png`
- `Data/Texture/TransitionMasks/RadialFlipbook.png`
- `Data/Texture/TransitionMasks/Spiral.png`
- `Data/Texture/TransitionMasks/Stripes.png`
- `Data/Texture/TransitionMasks/Waves.png`
- `Shader/Transition/GlassTransitionVS.hlsl`
- `Tool/UIPresentationEffects.md`
- `Tool/UIPresentationEffects_ChangedFiles.json`
