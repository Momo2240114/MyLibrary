# プレイヤー地上移動の改修記録

## 調査した既存処理

- PlayerControllerComponentが入力、Blackboard、PlayerStateMachine、移動予約を管理する。
- 従来のPlayerMoveStateは入力ベクトルにMoveSpeedを掛け、入力がゼロになったフレームでIdleへ戻っていた。MainSceneとHumanBossTestのMoveSpeedは22。
- GetMoveInputAxisはKeyConfigの軸を取得して斜め入力を正規化する。ControllerInputの円形デッドゾーンは0.20で、しきい値外の強度を再マッピングする。KeyConfigにも軸ごとのデッドゾーンがある。これらは変更していない。
- GetCameraRelativeMoveDirectionでカメラの水平Forward/Rightへ変換する。通常移動はロックオン中も進行方向を向き、ガードはApplyGuardFacingToTargetで敵方向を向く。今回もこの分担を維持する。
- Stateが予約した速度はApplyPendingMovementからCharacterMovementComponentへ渡る。物理管理下の位置更新と壁の解決は既存のCollider・物理処理へ任せる。
- MOVEはLOCOMOTIONグループなので、RequestActionは通常攻撃、強攻撃、回避、ガード、ジャストガード、使用可能なスキルへの即時遷移を許可する。StartとStopもMOVEに含めるため、専用のキャンセル受付待ちは増えない。
- 被弾・死亡の既存State変更、入力バッファ、ActionSequenceの通知、攻撃・回避のRootMotion設定は維持する。
- 既存のRunStart/RunEndフラグは時間で通知されていたが、グラフはIsMoveからSS_Sprintへ直接遷移しており、指定素材には接続されていなかった。

## 実データの確認

対象はData/Model/SwordPlayer/CombatGirls_Sword_Shield.gltfと対応するbin。ModelResourceManagerが先に読むcombatgirls_sword_shield.cerealも、GlTFModel.cppのシリアライズ順に沿って読み取り確認した。418本すべてのキャッシュ内アニメーション名・順序がglTFと一致し、以下の尺とroot/pelvisトラックも一致していた。キャッシュの再生成は行っていない。

| クリップ | Index | 元の尺 | 今回のループ設定 |
| --- | ---: | ---: | --- |
| Idle_Combat_Seq | 165 | 約10.00秒 | 有効 |
| Run_Fast_Combat_IP_Start_Seq | 254 | 約0.6667秒 | 無効 |
| Run_Fast_Combat_IP_Loop_Seq | 253 | 約0.4667秒 | 有効 |
| Run_Fast_Combat_IP_Stop_Seq | 255 | 約1.2000秒 | 無効 |
| Run_Turn_Fast_Combat_L_Seq | 305 | 約0.6667秒 | 今回は未使用 |
| Run_Turn_Fast_Combat_R_Seq | 306 | 約0.6667秒 | 今回は未使用 |

glTFそのものには今回のランタイム用ループ指定はなく、AnimationStateのloopで管理する。

Start/Loop/Stopのrootは全キーで平行移動ゼロ、回転一定。pelvisの上下動や体重移動は残っている。Start終端→Loop先頭、Loop終端→Stop先頭の全137ノードのローカル回転差は最大約0.04度だった。

元のSS_Idleの素材ではStart先頭と左前腕で約91度、武器用ノードで最大約118度の差があった。Idle_Combat_Seq先頭とStart先頭は最大約0.04度なので、グラフ上のSS_Idleという名前と接続を維持し、animationIndexを165へ変更した。Stop終端も同じ待機姿勢へ戻る。

左右のTurn素材はrootが約180度回転する。さらにpelvisのローカルZは先頭約4.96から終端約294.62へ移る。rootの平行移動だけを無効化しても、この大きな移動は除去できない。コード旋回と重ねれば回転も重複するため、今回は専用クリップを採用せず、一定角速度の通常旋回で切り返す。未使用の判定角度・最低速度・再発動抑制の設定は追加していない。

## 実装と遷移条件

アニメーションの選択とブレンドはData/Animation/GraphData/Player.jsonのグラフが行う。移動コードはIsMove、IsRunStart、IsRunLoop、IsRunEndとGroundPlaybackRateをBlackboardへ渡し、地上移動クリップを直接ChangeStateしない。

グラフにはIdle→Start、Start→Loop/Stop、Loop→Stop、Stop→Start/Loopの接続を追加した。他Actionからの復帰用に、同じPhase条件を持つAny接続も用意した。死亡演出・拘束中に通常移動が割り込まない既存条件を維持している。各PhaseからGuardとIdleへの接続も追加した。

| 場面 | 処理 |
| --- | --- |
| Idleで有効入力 | MOVEへ入りStart。遷移フレームにも加速と移動を予約する。 |
| Start継続 | 短く加速。実再生の終端でLoopへ移る。素材が未登録の場合は設定時間で補完する。 |
| 通常の方向変更 | Startを再開始せず、入力方向へ移動しながら向きだけ旋回する。 |
| 入力解除 | MOVE内のStopへ移る。入力直前の速度から短く減速する。 |
| ごく短い入力 | Start中の入力継続が0.08秒未満、または速度が最大速度の15%未満なら、大きなStopを挟まずIdleへ戻る。 |
| Stop中の再入力 | 終了を待たず復帰。残速が最大速度の35%超かつ停止開始から0.20秒未満ならLoop、それ以外はStart。方向はそのフレームの入力に従う。 |
| Stop終了 | 速度0.01以下かつ実再生が終端に達したらIdleへ戻る。 |
| 戦闘入力 | MoveStateは加減速より先に既存ProcessActionInputを実行し、他Actionへ移ればそのフレームの地上移動更新を中止する。 |
| 被弾・死亡・操作ロック | 既存の強制遷移を維持。MOVEから離れる際や操作ロック時に通常移動の残速を破棄する。 |

## 移動・RootMotion・歩調

水平移動はコードで制御し、既存CharacterMovementComponentを通す。Start/Loop/StopはRootMotion無効で、rootの見た目のXZ移動除去を明示する。骨盤の上下動は残す。攻撃、回避、MotionWarpの設定や抽出経路には変更を加えていない。

加減速は秒単位で計算し、そのフレームの区間平均速度をMovementへ渡す。向きは地面上の符号付きYaw差を毎秒の角度で制限する。ほぼ180度の場合は直前の旋回符号を保持する。専用Turnの左右選択やアニメーション終了待ちは存在しない。

AnimStateへ任意のplaybackSpeedParamNameを追加した。空欄の既存素材は従来どおり再生する。今回の3素材だけGroundPlaybackRateを指定し、ModelAnimatorが現在のBlackboardカテゴリから倍率を取得する。共有State資産のspeedを書き換えないため、実行中の速度が保存値へ混ざらない。

StartとStopは元の尺を設定秒数へ縮める倍率を使用する。Loopは前フレームの実水平移動速度と基準速度の比を0.25～2.0倍へ制限する。壁に押し止められた場合も、要求速度だけで全速の歩調を続けない。ただし、壁際の足の見え方は実機での確認が必要。

## 調整項目と初期値

MainScene/HumanBossTestのPlayerControllerComponentに保存し、Inspectorから編集可能。旧JSONに新しいキーがなければメンバ初期値を使用する。実行時の速度・Phase・位置履歴は保存しない。

| 項目 | 初期値 |
| --- | ---: |
| MoveSpeed | 22（既存値を維持） |
| GroundAcceleration | 110 / 秒 |
| GroundDeceleration | 160 / 秒 |
| GroundTurnSpeed | 1080度 / 秒 |
| GroundInputStartThreshold | 0.18 |
| GroundInputReleaseThreshold | 0.10（開始しきい値以下へ制限） |
| GroundAnimationReferenceSpeed | 22 |
| GroundStartBlendTime | 0.10秒 |
| GroundLoopBlendTime | 0.08秒 |
| GroundStopBlendTime | 0.10秒 |
| RunStartDuration | 0.40秒 |
| RunEndDuration | 0.45秒 |

基準速度22は既存のゲーム移動速度に合わせた初期調整値で、IP素材単独から導いた絶対的な歩幅ではない。実機で歩調を見てGroundAnimationReferenceSpeedを詰める必要がある。

## 確認結果と未確認事項

静的確認として、宣言・定義、include、既存API、nullptr判定、入力の優先順位、JSON保存と復元の9組、グラフのState/ファイル参照、IDの重複、Guard/Idle接続、プロジェクトXMLを確認した。

加減速の式を別途計算し、30/60/144fpsと不均一な時間幅で、1秒間の走行距離19.8、最高速からの停止距離1.5125が一致することを確認した。理論上の加速時間は0.20秒、減速時間は0.1375秒、180度旋回は約0.1667秒。これは式の検算であり、C++のコンパイル結果や実機の物理挙動を検証したものではない。

| 要求されたケース | コード上の確認 | 実機確認 |
| --- | --- | --- |
| 静止→走り出し→継続走行 | Enter時の移動予約、Start終端、Loop条件を確認 | 未実施 |
| 短い入力で停止 | 0.08秒/15%条件のIdle復帰を確認 | 未実施 |
| 走行から停止 | 減速とStop終端の両条件を確認 | 未実施 |
| Stop中に同方向へ再入力 | 残速によるStart/Loop復帰を確認 | 未実施 |
| Stop中に逆方向へ再入力 | 当フレームの入力方向を採用し終了待ちしない経路を確認 | 未実施 |
| 90度方向変更 | Phaseを変えず一定角速度で旋回する経路を確認 | 未実施 |
| 約180度の切り返し | 符号安定化と即時の進行方向変更を確認 | 未実施 |
| 細かな交互入力 | 有効入力中はStart/Stopを再開始しないことを確認 | 未実施 |
| Start/Stopから攻撃・回避・ガード | ProcessActionInput優先とMOVE共通の受付条件を確認。専用Turnは未使用 | 未実施 |
| ロックオン有無 | 移動とガードの既存の向き制御を確認 | 未実施 |
| 壁へ移動 | 物理経路の維持と実移動量によるLoop歩調補正を確認 | 未実施 |

実機では、ブレンドの見え方、低速入力の足滑り、逆方向の再入力、ガード解除、被弾・死亡、スロー演出、壁や斜面との接触を優先して確認する。今回の初期値は実機調整済みの値として扱わない。

## 変更ファイル・コメント・Tracy・登録

- Code/Game/Charcter/Player/PlayerControllerComponent.h / .cpp：地上移動、設定、保存復元、Blackboard通知。旧ガード解除時のSS_Run直接再生も通常グラフへ統合。
- Code/Game/Charcter/Player/PlayerMoveState.h / .cpp：MOVE内のStart/Loop/Stop更新と戦闘入力の優先。
- Code/Engine/Graphics/Resource/Animation/AnimationState.h：任意の再生倍率パラメータ。
- Code/Engine/Graphics/Resource/Animation/ModelAnimator.h / .cpp：倍率を再生時間へ適用。
- Code/Engine/Graphics/Resource/Animation/AnimationControllerComponent.Asset.cpp：新しいパラメータの保存復元。
- Code/Engine/Graphics/Resource/Animation/AnimationControllerComponent.Gui.cpp：Stateの倍率パラメータ編集。
- Data/Animation/GraphData/Player.json：3素材と遷移を追加し、待機の素材を適合するCombat姿勢へ変更。
- Data/Animation/StateData/Player/States/Run_Fast_Combat_IP_Start_Seq.json / Run_Fast_Combat_IP_Loop_Seq.json / Run_Fast_Combat_IP_Stop_Seq.json：新規State資産。
- Data/Scenes/MainScene/Objects/PlayerModel.json / Data/Scenes/HumanBossTest/Objects/PlayerModel.json：移動設定の初期値。
- BIGShine.vcxproj / BIGShine.vcxproj.filters：3つのJSON資産をNone項目として登録。Filterは既存のData\Animation\States。実ファイルは既存のStateData/Player/Statesに置き、既存ファイルは移動していない。
- Docs/PlayerGroundMovement.md：本記録。

日本語コメントは、入力ヒステリシス、再入力、短い停止、区間平均速度、旋回、壁での歩調補正、設定とグラフの関係、倍率適用の目的へ追加した。

Tracyは既存BS_PROFILE_SCOPEを使用し、UpdateGroundMovementとUpdateLocomotionAnimationPhaseへ追加。ModelAnimatorの既存AdvanceAnimationTime計測内で倍率処理も追跡できる。既存マクロや有効化方式は変更していない。

変更したソース、JSON、Visual Studio登録ファイルはUTF-8 BOMなし、CRLF。厳密なUTF-8としての読み取りと、単独LF/CRがないことを確認した。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
