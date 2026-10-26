# ポーズ・死亡画面・タイトルのUI演出

## 変更内容

- ESCのWM_KEYDOWNから終了要求を送る処理を除去。ウィンドウを閉じる操作とタイトルの終了ボタンは維持。
- 本編の通常プレイではESCまたはPadCode::BACK（Select）でポーズを表示。「再開」「タイトルに戻る」を選択可能。
- ポーズ中はSceneBaseの更新を呼ばず、戦闘時計・AI・アニメーション・エフェクトの更新を停止。Managerの専用フラグでフレーム末尾のPhysX更新も停止。
- メニューの時計と入力取得は継続。上下キー／方向キー／左スティックで選択、Enter／A（PlayStationでは×）／マウスで決定。ポーズ中のESC／Selectでも再開。
- 再開前に決定入力の解放を待ち、操作が戦闘へ持ち越されないようにした。カーソルと入力捕捉は開く前の状態へ復元。
- Editor PlayのESCによる入力解放は維持。死亡演出中とボス決着演出中にはポーズを重ねない。
- 死亡後は「コンテニュー」「タイトルに戻る」を表示。既存の死亡アニメーション待機・暗転・回復・起き上がり処理は維持。
- YOU DIEDは1280×720基準で92px相当の赤い見出しへ変更。画面の大きさに合わせて全体を拡縮。
- タイトル／ポーズ／死亡画面に登場時の減速、選択時の拡縮、枠やマーカーの強調、決定時の光の帯と散光を追加。既存のタイトル操作音を共用。
- ゲーム内メニューではMaster音声を停止せず、操作音と死亡時のBGM減衰を維持。Editor Pauseによる音声停止は維持。

## 演出の調整

Curve Asset Editorで次の2ファイルを編集できる。保存後、ポーズを開き直すか、タイトルへ入り直すと読み直す。

- `Data/Curve/UI.MenuReveal.json`：0～1の進行率に対する登場補間。既定はEase Out。
- `Data/Curve/UI.MenuFocus.json`：0～1の進行率に対する選択補間。既定は1.14まで膨らんで1へ収束。

ポーズの登場は0.32秒、選択の反応は0.24秒、決定後の遷移は0.28秒。Curve読込失敗時はコード内の補間へフォールバックする。タイトルの保存済みRectTransformには演出値を書かず、UIWidgetの実行時オフセットと倍率だけを使用する。

MeshVFXの発光が流れて消散する表現を参考に、帯のグラデーションと散光を画面座標で描画している。今回はMeshVFXComponent自体をUIへ配置していない。ゲーム停止中もUIだけの時計で進み、カメラの向きに影響されない。

参考：[ゲームUI演出の基本](https://gameanimation.info/archives/5603)、[イージングについて](https://gameanimation.info/archives/4744)。入力・選択・決定を短い動きで判別できる構成にした。

## 変更ファイル

- `Code/Engine/Core/framework.cpp`
- `Code/Engine/GamePlay/Core/Object/GameObjectManager.h`
- `Code/Engine/GamePlay/Scene/SceneGame.h`
- `Code/Engine/GamePlay/Scene/SceneGame.cpp`
- `Code/Game/GameMode/PlayerDeathDirector.h`
- `Code/Game/GameMode/PlayerDeathDirector.cpp`
- `Code/Engine/UI/MenuNavigationComponent.h`
- `Code/Engine/UI/MenuNavigationComponent.cpp`
- `Code/Engine/UI/UIWidgetComponent.h`
- `Code/Engine/UI/UIWidgetComponent.cpp`
- `Code/Engine/UI/MenuPresentation.h`（新規）
- `Code/Engine/UI/MenuPresentation.cpp`（新規）
- `Data/Curve/UI.MenuReveal.json`（新規）
- `Data/Curve/UI.MenuFocus.json`（新規）
- `BIGShine.vcxproj`
- `BIGShine.vcxproj.filters`
- `Docs/PauseAndMenuPresentation.md`（本書）

## コメント・Tracy・登録

入力持ち越しの防止、通常更新と物理の停止、カーソル復元、保存値を変更しないUI補正、Curve再読込、発光表現に日本語コメントを追加。

MenuPresentationのUpdate／Draw／DrawFeedback／ReloadCurvesに既存のBS_PROFILE_SCOPEを追加。SceneGame、PlayerDeathDirector、MenuNavigationComponent、UIWidgetComponentの既存計測を維持。

MenuPresentationの実ファイルはCode/Engine/UIへ配置。ヘッダーとCPPをBIGShine.vcxprojへ登録し、BIGShine.vcxproj.filtersでは既存メニューと同じCode/Engine/GamePlay/UIへ登録。ComponentではないためComponentRegistryへの登録は不要。Curveアセットは配布機能の既存Dataコピーで収録される。

## 静的確認

宣言と定義、参照する既存API、ESC／Selectの押下経路、メニュー入力とシーン遷移、死亡からの既存回復経路、物理停止、カーソル復元、音声のPause条件、JSON読込形式とプロジェクト登録をコード上で確認。

対象17ファイルはUTF-8・BOMなし・CRLFで保存し、厳密なUTF-8読込、JSONとXMLの構文、Curveの名前と制御点の対応、追加ファイルのプロジェクト・Filter登録を検査。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
