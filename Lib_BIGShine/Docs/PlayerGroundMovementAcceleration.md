# 地上移動の加減速と速度受け渡し

この記録は、PlayerGroundMovement.mdに記載した初版の地上移動を見直した結果を示す。現在の設定値と処理は本書を参照する。

## 原因と調査した経路

以前の実装にもスカラー速度の加減速はあったが、次の経路が不自然な挙動を作っていた。

1. Start中の短い入力解除では、減速処理より先にRequestAction(IDLE)を呼んでいた。Idle.Enterの速度ゼロ設定で残速が消え、短い入力ほど急停止していた。
2. Move.Enterでも速度を更新し、Move.Executeでも更新していた。ガード解除やアニメーションイベントからMOVEへ入ると、同じフレームに加速とPhase時間が二重に進む経路があった。Enterから渡していた時間は直前のController更新時間でもあった。
3. 速度の向きを入力方向へ瞬時に置き換えていた。180度入力でも制動を挟まず、同じ速さで反対へ進んでいた。
4. 衝突後の実移動量は歩調にだけ使用し、現在速度には反映していなかった。壁へ押し続けると内部速度は最高速のまま残った。
5. 最高速に到達する時間は約0.20秒、Start再生は0.40秒、停止する時間は約0.138秒、Stop再生は0.45秒で、足の運びと速度の対応を十分に取れていなかった。
6. Loopの歩調基準が最高速度と同じ22だった。しかし対応する移動付き素材の実データから求めた基準は、Sceneごとのモデル倍率では約16／12だった。

調査した入力・移動経路は以下。

KeyConfig/ControllerInput → GetMoveInputAxis（強度・しきい値）→ GetMoveInput（カメラ相対）→ PlayerStateMachine/PlayerMoveState → UpdateGroundMovement → SetHorizontalVelocity/ApplyPendingMovement → CharacterMovementComponent → ColliderComponent::PreparePhysicsStep → PhysX CharacterController::move → Transformの実位置 → 次回の速度読み戻し。

MovementクラスとMovementComponent::Moveも調査した。これらは方向へ速度と時間を掛けてTransformを動かす旧方式で、カメラやテスト用EnemyのSceneに使用されている。今回のプレイヤーSceneは使用していないため変更していない。敵やスキルも使うCharacterMovementComponentの合成処理も変更していない。

MainScene/HumanBossTestでは、AnimationController、PlayerController、CharacterMovementの順で更新する構成を維持している。物理側のVelocityRotationModeはNONEで、通常移動の向きを別の物理回転処理が上書きする設定ではなかった。

## 入力・速度・実移動の分離

Controllerで次を分けて保持する。実行中の情報はJSONへ保存しない。

- groundTargetVelocity/groundTargetSpeed：入力方向と強度から求める目標。
- groundMoveDirection/groundMoveSpeed/groundCurrentVelocity：旋回・加減速後の現在速度。
- groundRequestedVelocity：そのフレームの区間平均速度。物理へ渡す値。
- groundActualVelocity：前回の物理ステップで実際に動いた水平変位を、前回の時間で割った速度。

Move.Enterは行動開始の通知だけ、Move.Executeは既存の戦闘入力受付だけを担当する。状態更新と行動終了の安全処理が終わってから、Controller.UpdateでMOVEなら速度を一度だけ更新する。Idleから入ったフレームにもこの処理を通り、入力から移動を開始する。EnterとExecuteで時間を二重に消費しない。

速度を目標へ近づける際は加速度×経過時間で上限を設け、目標を通り越さない。加減速中の区間平均を積分して渡すため、最高速へ達したフレームや速度ゼロになるフレームでも移動距離を過剰に積分しない。停止しきい値以下の微小な残速はゼロへ収束させる。

入力解除だけではIdleへ戻らない。現在速度、物理へ渡す平均速度、衝突後の実速度が停止しきい値以下になり、姿勢の条件も満たしてからIdleへ戻す。Idle.Enterのゼロ設定は停止後の終端処理として残す。

## 旋回と切り返し

通常の方向調整は、水平速度の向きを一定の角速度で変更する。速度ベクトルを単純に線形補間せず、速さと向きを分けるため、90度程度の旋回で不必要に減速しない。

120度以上の入力変更では切り返し用の制動を開始する。約0.067秒で最高速からゼロへ落とし、その時点で新方向へ加速する。途中で入力が元方向へ戻った場合は制動を解除する。アニメーションは同じStart/Loopのままで、方向変更のたびにStop→Startを挟まない。

速度の向きとキャラクターの見た目の向きは別に更新する。通常移動はロックオン中も移動入力方向を向き、ガード中は敵方向へ向く、という既存の役割を維持した。

専用Turnアニメーションは使用していない。前回調査した180度のroot回転と大きいpelvis移動を持つ素材を重ねず、コードによる短い制動と旋回で対応している。

## 壁・接地・空中

ColliderComponentへ直前のCCT側面衝突結果を読むWasHorizontalMovementBlockedを追加した。既存のCCT移動・押し戻し・重力・段差・斜面の解決処理は変更していない。

地上で側面衝突があり、要求より実移動が短かった場合だけ現在速度を読み戻す。壁で消えた速度を保持せず、進めた方向と速さを次の初速に反映する。進めなかった変位を後から足す処理はない。壁で完全に止まっている場合は、MOVEで入力を受け続けながらグラフ側でIdleへブレンドする。壁から動き出したときにStartへ戻す。

水平速度の制御でY速度、重力、Impulseを初期化しない。通常のJumpStateは既存の空中操作を使用する。MOVEで段差から離れた場合にもairControlRate、airControlAcceleration、airDecelerationを使用し、地上の減速度160を空中へ適用しない。

接地が一時的に変わる場所、壁沿いの滑走、斜面、段差、動く物体による押し出しは実機での確認が必要。共通の物理解決コードを維持したことと、実際の挙動を実行確認したことは別である。

## グラフとアニメーション

アニメーション選択は引き続きPlayer.jsonのグラフで行う。コードはIsRunStart/IsRunLoop/IsRunEnd、GroundSoftStop、GroundPlaybackRateを渡し、移動クリップを直接ChangeStateしない。

| 遷移 | 条件 |
| --- | --- |
| Idle → Start | 有効入力でMOVEへ入り、最初のフレームから加速する |
| Start → Loop | Startの再生が約98%まで進み、目標速度の85%以上に達する。壁沿いに進めている場合も終端でLoopへ移る |
| Start/Loop → Stop | 入力解除。速度を保持して減速を開始する |
| 低速・短い入力の停止 | Stopの大きい動作を使わず、グラフでIdleへブレンドする。MOVE内の減速は継続する |
| Stop → Idle | 現在・要求・実速度が停止しきい値以下で、低速停止またはStopの約67%以上まで姿勢が進んでいる |
| Stop中の再入力 | 残速を保持して即座に再加速。残速が目標の35%超、停止開始から0.15秒未満、通常StopならLoop。それ以外はStart |
| 壁で静止 | 地上速度を実結果へ戻し、グラフでIdleを選ぶ。入力受付と移動更新は止めない |

GroundSoftStopの接続をグラフへ追加し、既存の死亡・拘束中の抑止条件を維持した。他のRun接続にはGroundSoftStop=falseを追加し、IdleとRunを交互に選ばないようにした。

足先トラックをモデル倍率0.02で計算すると、Startでは元の約0.4秒付近に踏み込み、Stopでは約0.6秒前後に足先が落ち着く区間がある。今回の設定では、最高速到達がStartの元時間約0.40秒、停止がStopの元時間約0.55秒へ対応する。Stop後半の待機姿勢へ戻る区間まで長く滑らせる必要はない。

LoopのIP版と移動付き版は841組の回転キーが一致した。移動付き版は0.466667秒でpelvis Zが約373.624変化する。実ロードで優先するcerealキャッシュでも同じ値を確認した。モデル倍率0.02のMainSceneでは約16.012単位/秒、倍率0.015のHumanBossTestでは約12.009単位/秒に相当する。基準をそれぞれ16.0／12.0へ修正した。

Loopは実速度／基準速度で歩調を調整する。最高速22では約1.375倍／約1.833倍。調整範囲は0.70～2.0倍とし、以前の0.25倍のような極端な低速再生を避ける。低いスティック強度でも実速度は比例して下がるが、同じ走行素材を使用するため低速域の足滑りは実機で確認・調整が必要。

Start/Stopのroot平行移動・回転は一定で、既存設定でもRootMotionは無効。今回もコード移動へ二重加算しない。

## 戦闘とジャンプへの受け渡し

- 攻撃・回避・ガード：MOVEの速度予約を解除し、移動Runtimeを破棄する。次のStateの速度・RootMotion・MotionWarpが移動を所有する。地上減速をその後も実行しない。
- 被弾・死亡：既存の即時停止・移動要求リセットを維持する。新しい減速で反応を遅らせない。
- 戦闘からMOVE：古い地上速度を復活させず、残っている前行動のRootMotion要求を破棄して新しい入力から加速する。
- 地上からジャンプ：現在の水平慣性を渡す。壁で進めなかった直後は実移動を使う。JumpState自身の入力・空中制御は維持する。
- 着地からMOVE：古い走行速度ではなく、その時点の空中水平速度を引き継ぐ。入力がなくても着地の残速があればMOVEで減速してからIdleへ戻す。
- 操作ロック：通常移動のRuntimeを破棄し、ロック解除後に以前の走行速度を持ち越さない。

MoveStateの戦闘入力受付は継続して優先する。停止アニメーションの終了待ちを攻撃や回避の受付条件へ追加していない。弱コンボのカーブ・イベント・MotionWarpプリセットには変更を加えていない。

## 調整項目と採用値

| 項目 | 値・単位 | 役割 |
| --- | --- | --- |
| MoveSpeed | 22単位/秒、既存値維持 | 入力強度1の最高速度 |
| GroundAcceleration | 140単位/秒² | 約0.157秒で最高速へ到達 |
| GroundDeceleration | 160単位/秒²、既存値維持 | 約0.138秒で最高速から停止 |
| GroundReversalDeceleration | 330単位/秒² | 約0.067秒の切り返し制動 |
| GroundDirectionTurnSpeed | 1440度/秒 | 速度の向き。90度を約0.0625秒で変更 |
| GroundTurnSpeed | 1080度/秒、既存値維持 | キャラクターの見た目の向き |
| GroundStopSpeedThreshold | 0.05単位/秒 | 微小な残速の収束と実停止確認 |
| 入力開始/解除 | 0.18／0.10、既存値維持 | 既存デッドゾーン後のヒステリシス |
| GroundAnimationReferenceSpeed | MainScene 16.0／HumanBossTest 12.0 | モデル倍率に応じたLoopの歩調基準 |
| Start/Loop/Stopブレンド | 0.08／0.08／0.06秒 | グラフ遷移の補間時間 |
| RunStartDuration | 0.26秒 | Startアニメーションの再生時間の起点 |
| RunEndDuration | 0.30秒 | Stopアニメーション全体の再生時間の起点 |

速度・加速度・減速度を調整の基準とし、別途「最高速までの時間」は保存していない。到達時間は速度/加速度から求まる。RunStartDuration/RunEndDurationはアニメーション用であり、物理的な加速・停止時間とは意味を分け、Inspectorの表示も変更した。

新規設定は3項目で、Inspector・Serialize・Deserializeへ接続した。旧JSONではメンバ初期値を使う。現在の2つのPlayerControllerを持つSceneへ上記値を反映した。

デバッグ表示には段階名、切り返し中か、壁で静止しているか、目標・現在・前回実速度、目標/要求/現在/実速度のXZを追加した。初回など実測値がないときはno sampleと表示する。

## 確認結果

独立した等加速度運動の式と、時間をフレームへ分割した積分結果を比較した。下表はC++ゲームを実行した結果ではなく、数式の検算である。

| 条件 | 30/60/120fps相当の計算結果 |
| --- | --- |
| 静止から1秒間、強度1 | 最高速度22、距離約20.271429で一致 |
| 最高速から入力解除 | 停止距離1.5125で一致 |
| 50msのタップ後に停止 | 合計約0.328125単位で一致 |
| 強度0.25を継続 | 目標・現在速度5.5。最高速22へは上がらない |
| 停止開始50msで同方向へ再入力 | 残速14から再加速し、ゼロへ初期化しない |
| 停止途中に逆方向へ再入力 | 残速から制動・反転し、新方向へ加速する |
| 90度旋回 | 速さ22を保持し、経路が一致。曲がり角の前方向変位は約0.875単位 |
| 180度切り返し | 元方向への進みは最大約0.733333単位、その後反対へ加速。経路が一致 |
| 細かい交互入力 | 速度上限を超えず、アニメーションの再開始条件は増えない |
| 完全な壁のモデル | 実移動0なら残速を毎回戻す。離れた初回は静止からの加速距離になり、最高速で飛び出さない |

同じ50ms入力を比較した計算であり、実際の入力取得時刻にはフレーム単位の量子化がある。壁の検算も簡略化したモデルで、PhysXの接触解決を実行したものではない。

静的確認：更新呼び出しが1箇所であること、短い入力の早期Idle分岐の撤去、減速後の要求速度・実速度の停止条件、宣言と定義、include、JSON保存復元、グラフの参照とID、既存C++のVisual Studio登録を確認した。Sceneの変更はPlayerControllerの対象項目だけで、他Componentと弱コンボの設定は一致することを比較した。CCTの既存解決コードも結果フラグの記録以外は変更がないことを比較した。

未確認：Start/Stopと足の接地の見え方、低速入力の足滑り、停止途中の見た目、戦闘割り込み、着地、壁沿いの滑走、斜面、段差、接地が切り替わる場所、ロックオン、ヒットストップ中、実際の30/60/120fpsでのCCT結果。初期値は実機調整済みの値として扱わない。

## 変更ファイル・コメント・Tracy・形式

- Code/Game/Charcter/Player/PlayerMoveState.h / .cpp：入力受付と速度更新を分け、二重更新を撤去。
- Code/Game/Charcter/Player/PlayerControllerComponent.h / .cpp：目標・現在・実速度、加減速、切り返し、衝突結果の反映、フェーズ、受け渡し、設定、デバッグ表示。
- Code/Engine/Physics/ColliderComponent.h / .cpp：直前のCCT側面衝突結果を公開。既存の物理解決は維持。
- Data/Animation/GraphData/Player.json：低速停止・壁静止をIdleへブレンドする接続と、Run側の排他条件。
- Data/Scenes/MainScene/Objects/PlayerModel.json：設定値と歩調基準16.0。
- Data/Scenes/HumanBossTest/Objects/PlayerModel.json：設定値と歩調基準12.0。
- Docs/PlayerGroundMovementAcceleration.md：本記録。

日本語コメントは更新の一元化、実移動の読み戻し、加減速の積分、切り返し、空中との分離、戦闘・ジャンプの受け渡し、停止姿勢の条件へ追加した。

Tracyは既存のPlayerControllerComponent::UpdateGroundMovement、UpdateLocomotionAnimationPhase、ColliderComponent::PreparePhysicsStepなどのBS_PROFILE_SCOPE内で追跡できる。共通マクロや有効化方式は変更していない。

新規C++ファイルはない。既存ソースの.vcxproj登録と.vcxproj.filters登録を別々に確認した。実ファイルの移動、Filter変更はない。

対象ファイルはUTF-8 BOMなし、CRLF。厳密なUTF-8読み取り、単独LF/CR、追加行の末尾空白を確認した。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
