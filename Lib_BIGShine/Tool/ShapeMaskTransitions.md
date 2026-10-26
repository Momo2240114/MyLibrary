# 任意の輪郭を使う画面Fade

## 操作

ビルド後、EditorでF9を押し、Screen TransitionのReloadを押す。
Shape_Iris_Diamond / Shape_Iris_Circleは中央の開口が縮み、Shape_Tiles_Diamond / Shape_Tiles_Circleはタイルごとの形が拡大するサンプル。
どちらもShape Mask Textureを任意の画像へ差し替えられる。Play、Cover、Revealで既存の試写を利用でき、Save Presetで実行時と同じData/UI/SceneTransitions.jsonへ保存する。

## 画像と形の扱い

- DIAMONDとCHECKERのShape Mask Textureへ画像を指定する。未指定・読込失敗時は既存の数式による形を使う。読込失敗はRenderカテゴリの警告ログへ出す。
- 白が形状、黒が形状外。透明背景の画像ではShape ChannelをAlphaへ変更する。黒い形を使う場合はInvert Shapeを有効にする。
- ShapeThresholdで輪郭を決め、その輪郭は時間で変更せず、画像そのものを相似に拡縮する。星、ハート、ロゴなども同じ仕組みで使用できる。
- 元画像の縦横比を使い、画面比率による引き伸ばしは行わない。CHECKERは画面上で正方形になるセルへ配置し、画面端のセルだけが切れる。
- DIAMONDではCenter PositionとDirectionに加えてRotationを使える。CHECKERのRotationは各セル内の形を回す。
- 画像内に含まれる透明な余白も含めて拡縮する。形の外側に広い余白がある画像は、ShapeEndScaleを大きくして調整する。
- ShapeEndScaleは中央開口の初期サイズ、またはタイルの最終サイズ。中央開口の初期値1、タイルの初期値3、範囲1〜8。
- CompletionFillは終端付近で背景色へ収束する区間。初期値0.08、範囲0.01〜0.25。穴がある形でも新旧Sceneが混ざらないよう最後は完全被覆する。中央開口では開始付近の急な被覆も同じ幅で抑える。
- 元画像の輪郭そのものが変形・歪曲している場合や、形がセルより大きくなって端で切れる場合は、その見た目を保持する。

## 実装と互換性

Effect.Texture / Channel / Invert / Rotation / ShapeThreshold / ShapeEndScale / CompletionFillを既存のEffectオブジェクトへ保存する。旧プリセットとProfilesは変更していない。
既存の176バイトの定数バッファ配置は維持する。DIAMONDとCHECKERだけでmaskAnimation.xを閾値、maskFramesを終端補完幅・予約値・拡大率・画像縦横比として解釈する。
形状用s1はLinearClampを使用し、既存Noise用s0のLinearWrapを変更しない。画像外は反転指定によらず形状外として扱い、別の形が繰り返し現れないようにする。
Tracyは既存のLoadResources / Renderに加え、共通のDrawShapeMaskを計測する。
新規のShapeMaskTransition.hlsliは既存のShader/Transitionへ配置し、vcxprojとShaders\\PostEffect\\Transitionフィルターへ登録した。

## 確認状況

JSON、参照画像の存在、C++の宣言と定義・既存API、CPUとHLSLの定数配置、既存PSとの分岐、Visual Studio登録、UTF-8 BOMなし・CRLFを静的に確認する。
プロジェクトビルド・Shaderコンパイル・実行確認は行っていない。変更反映にはDiamondTransitionPS / CheckerTransitionPSのCSO更新を含むユーザー側のビルドが必要。
実機では星や透明背景画像への差替え、縦長画像、画面比率変更、タイル数・遅延変更、Cover/Reveal、マスクClear・欠損時の代替動作を確認する。
