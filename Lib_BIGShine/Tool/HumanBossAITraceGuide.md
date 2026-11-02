# 第2形態AIの追跡と行動領域

## 追跡画面

`HumanBossAITraceComponent`を`HumanBossAIComponent`と同じオブジェクトへ追加します。
メインシーン、HumanBossTest、Phase2HumanBoss Prefabには登録済みです。

1. HumanBossのInspectorでHumanBossAITraceComponentを開きます。
2. `Open Trace Window`で独立した追跡画面を開けます。
3. 履歴の行を選ぶと、その時点の判断理由、観測したプレイヤー履歴、行動候補の重み、実際に評価した条件を表示します。
4. `Follow Latest`を外すと選択行の表示を固定します。記録自体を止めるには`Recording`を外します。
5. `Filter`で行動・意図・プレイヤー行動・進行状態・判断理由を検索できます。
6. Inspectorの`Export Trace JSON...`で保持中の履歴を書き出せます。

履歴は初期値256件、設定範囲32～1024件です。上限を超えた最古の行は破棄します。
記録内容は実行時だけ保持し、シーン保存には含めません。再開・再読込・Clear Historyで初期化します。

## 判断理由の読み方

- Decision：既存AIが確定した意図の理由。
- Threat：プレイヤーの攻撃が届くか、反応受付時間に入ったかの観測結果。
- Evaluated Action Candidates：実際のグラフ評価で計算した候補重みと最初の除外理由。
- Player History Used by AI：AIが実際に受け取った直近の行動履歴。観測Systemの時刻は追跡画面の経過秒とは別の基準です。
- Evaluated Conditions and Route：実際に評価したノードの結果と、条件の参照元・比較値。
- Snapshot Details：反応予算、待機時間、行動別設定、暴走の予約・実行状態などの固定値。

正の重みは当選を保証しません。抽選した枝ではEffective draw weightを表示し、個別の抽選重みを取得していない候補はNo individual draw weightと表示します。
通過しなかった枝を追跡のために再評価することはありません。
Action startedは行動開始の途中で記録するため、Graph decisionで評価完了後の経路を確認してください。
状態変更時に新しいグラフ評価がなければ、候補・経路は直前の評価結果です。表示中の評価完了フラグと経過時間で区別できます。

## GameModeManagerとの連携

GameModeの進行状態、戦闘形態、Sequence、AI・ゲームプレイ・演出・ポーズのロックを並べて表示します。
通常AIが実際に参照するGlobalBlackboardのEnemyAILockedも併記し、Managerの値と異なる場合に表示します。
通常AIはGlobalBlackboardのAIロック、暴走開始はManagerの演出・停止状態も条件にしています。
AIDirectorの方針値はSnapshot Detailsに観測値として記録します。値があることと、その値を人型ボスが判断に使用したことは区別してください。
追跡機能はこれらの制御経路を変更しません。

## AIグラフの領域

独立した行動・条件のBoxには横方向の専用列を割り当て、無関係なBoxを下の段へ折り返しません。下方向へ伸ばすのは実際の接続に沿った派生Box、同じ枝の条件、内側の行動Boxです。Rootと直下の判断ノードは、派生を含むBox全体の左右端の中央へ配置します。再整列もこの配置方針を維持します。横方向の移動はホイール押し込みドラッグで行えます。

外部HumanBossグラフの62ノードを整理し、33の行動ノードそれぞれをコメントBoxで囲んでいます。共通条件をまとめる外側の分類も含め、Boxは計44個です。条件・リンク・Priority・重みは維持しています。

- `Organize Action Areas`：現在のPriority順を維持して領域と配置を再作成します。外側の分類は再作成しますが、行動単位で編集したBoxの名称・色・説明は維持します。
- `Create Comment Box from Selection`：選択したノードを囲む領域を作成します。
- InspectorのComment Boxes：領域名を選ぶと該当位置へ移動します。
- Comment Box Settings：名称・説明・色を編集できます。領域のタイトル部分を左ドラッグすると中のノードと移動でき、境界でサイズを変更できます。
- `Fit Area to Members`：作成時に登録したノードへ領域を合わせます。領域だけを削除してもノードは残ります。
- Box上部に行動名、その下に説明を表示します。説明が長い場合はInspectorで全文を編集・確認できます。
- `Save`：コメントBoxと座標を通常のグラフAssetに保存します。
- `Focus selected links`：選択したBox・ノードの接続を強調し、無関係な線を薄く表示します。OFFにすると、選択に関係しない線も通常の明るさで確認できます。
- リンクは上下のピンに沿う曲線で表示します。Box間の線を共通の外周レーンへ集める描画は使用しません。

整列後はOrder by X PositionをOFFにして、配置と評価順を分離します。
既存Assetでこの設定がない場合は従来どおり横位置からPriorityを更新します。
OFF時の評価順変更はノードのPriority欄で行ってください。

## 確認範囲

JSON・XML・登録内容・宣言と定義・既存API・保存形式を静的に確認します。
ビルドおよび実行確認は行っていません。画面の操作と実際の戦闘追跡は実行時の確認が必要です。

## 変更ファイル

新規ソース:
- Code/Game/AI/Human/HumanBossAITraceComponent.h
- Code/Game/AI/Human/HumanBossAITraceComponent.cpp

既存ソース:
- Code/Game/AI/Human/HumanBossAIComponent.h
- Code/Game/AI/Human/HumanBossAIComponent.cpp
- Code/Engine/GamePlay/AI/AIBehaviorGraph.h
- Code/Engine/GamePlay/AI/AIBehaviorGraph.cpp
- Code/Editor/Workspace/AIBehaviorEditorWindow.h
- Code/Editor/Workspace/AIBehaviorEditorWindow.cpp
- Code/Engine/GamePlay/Serialize/ComponentRegistry.cpp

プロジェクト・Asset:
- BIGShine.vcxproj
- BIGShine.vcxproj.filters
- Data/AI/BehaviorGraph/HumanBoss.json
- Data/Scenes/MainScene/Objects/HumanBoss.json
- Data/Scenes/HumanBossTest/Objects/HumanBoss.json
- Data/Prefabs/Phase2HumanBoss.json

操作ガイド:
- Tool/HumanBossAITraceGuide.md

新規ソースは既存のCode/Game/AI/Humanに配置し、実ファイルは移動していません。
.vcxprojへの登録と、.vcxproj.filtersのCode\Game\Logics\AI\Humanへの登録を個別に行っています。
