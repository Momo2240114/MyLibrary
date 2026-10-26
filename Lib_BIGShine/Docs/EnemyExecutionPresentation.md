# 敵エクスキューション演出

## 変更内容

拘束成功時に、敵の正面寄りのカメラAction、短いスロー、コントローラ振動を開始します。
カウンターと対ガード掴みは共通のBeginExecutionCaptureを通るため、両方へ適用されます。
回避されて拘束が成立しなかった場合は演出を開始しません。

### カメラ

MainSceneとHumanBossTestのMainCameraに`EnemyExecution`プリセットを追加しました。
敵の正面方向をPlayActionAtPositionへ渡し、TARGET_LOCAL_RELATIVEで構図を決定します。
初期値は横1.6、高さ0.8、正面4.5のオフセット、注視点の高さ1.3、FOV48度です。
0.12秒で切り替え、拘束解除時に0.28秒で通常カメラへ戻します。
HOLDの初期値3秒は、解除通知が失われた場合にも通常カメラへ戻る上限として働きます。

MainCameraがPlayerレイヤーのため、CameraActionDirectorの既存useUnscaledTime設定を時間更新へ反映しました。
この設定がtrueのプリセットはRawDeltaTimeで進み、スロー中もカメラの切替時間を維持します。
falseのプリセットは従来どおり受け取ったdeltaTimeを使用します。

StopActionByNameを追加し、拘束中に別の高優先度演出へ切り替わっていた場合は、その演出を止めません。

### 拘束時のスローと振動

Player、Enemy、PlayerBullet、EnemyBulletへ既存TimeManager::PlayTimedScaleを適用します。
初期倍率は0.25、移行0.02秒、保持0.20秒、復帰0.12秒です。
Actorの寿命に依存する遅延Callbackは使わず、既存TimeManagerが実時間で自動復帰します。
振動は既存ControllerInput::PlayRumbleへ渡し、初期強度0.7、時間0.20秒です。
低周波モーターは設定強度の0.75倍、高周波モーターは設定強度で駆動します。
Padが未接続の場合は既存APIの経路で安全に省略されます。

### 叩きつけ

Execution_03_Seqの50フレーム目（60FPS基準で約0.833秒）にAnimation Eventを追加しました。
モデルのExecution_Target_03_Seqのpelvis移動データで、地面付近へ落ちる区間を確認して初期位置を選んでいます。
画面上の接地との一致は実機での確認・調整が必要です。

イベントはHumanBoss_ExecutionSlamFeedbackを呼び、既存CameraShakeComponentへ渡します。
初期値は位置強度0.12、回転強度1.44、時間0.24秒、周波数28、優先度30です。
MainScene/HumanBossTestとも、ShakeのLateUpdatePriorityがCameraActionより後であることを確認しました。
消費済みフラグで同じ拘束中の重複再生を防ぎます。

## Action登録

- HumanBoss_ExecutionCamera：拘束成立後、敵の正面方向を使ってカメラを開始。
- HumanBoss_ExecutionGrabFeedback：拘束時のスローと振動を一度だけ開始。
- HumanBoss_ExecutionSlamFeedback：叩きつけイベントからシェイクを一度だけ開始。

## Editor・保存

HumanBossActionComponentのExecution Presentationで、Camera Action名、Slow倍率・保持時間、振動強度、シェイク強度・時間・周波数を調整できます。
新しい設定はSerialize/Deserializeへ追加し、旧Sceneは初期値で復元します。
構図とカメラの補間時間はMainCameraのEnemyExecutionプリセット、シェイクの発生時刻はAnimation Editorで調整できます。

## 日本語コメント・Tracy

拘束成功後の開始条件、敵の正面方向、時間の同期、終了時の復帰、設定復元に日本語コメントを追加しました。
PlayExecutionCamera、PlayExecutionGrabFeedback、PlayExecutionSlamFeedbackへ既存BS_PROFILE_SCOPEを追加しました。
既存の拘束・カメラ・シェイク・振動APIの計測を維持しています。

## 変更ファイル

- `Code/Game/AI/Human/HumanBossActionComponent.h`
- `Code/Game/AI/Human/HumanBossActionComponent.cpp`
- `Code/Engine/GamePlay/Camera/CameraActionDirectorComponent.h`
- `Code/Engine/GamePlay/Camera/CameraActionDirectorComponent.cpp`
- `Data/Animation/StateData/HumanBoss/States/Execution_03_Seq.json`
- `Data/Scenes/MainScene/Objects/MainCamera.json`
- `Data/Scenes/HumanBossTest/Objects/MainCamera.json`
- `Docs/EnemyExecutionPresentation.md`（本報告）

## 静的確認

- 宣言と定義、include、呼び出しAPI、Action登録、nullptr確認、消費済みフラグの初期化・解除を確認しました。
- 拘束成功後の開始、通常終了・死亡・中断時のRelease経路、カメラ名による停止条件を確認しました。
- 新しいJSON保存/読み込みキー、既存プリセットと既存アニメーションイベントを維持した追加内容を確認しました。
- HumanBossグラフが変更したExecution_03_Seqの外部State Assetを参照することを確認しました。
- 既存ソースのvcxproj・filters登録を別々に確認しました。新規ソースファイルや実ファイルの移動はありません。
- 対象ファイルは厳密なUTF-8、BOMなし、CRLFで確認しました。

## ビルド・実機確認

カメラの画角、叩きつけとの見た目の同期、Padでの手触りは実機確認が必要です。
ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
