# 弱コンボの速度カーブと攻撃アシスト

## 現在の構造と調査結果

地上弱コンボはWEAK_ATTACKという同じPlayerAttackStateを4回使用し、ComboIndexの0～3でSS_Attack1～4へグラフ遷移する。SS_Attack4は剣と蹴りの2判定を持つ。専用ActionSequenceファイルの再生ではなく、AnimationState内のイベントがInputBufferOn、ComboTransition、キャンセル受付、Trail、AttackTemplate、VFX、ActionEndを呼ぶ構成だった。外部State資産とグラフの定義を確認した。

| 段 | 素材Index | クリップの尺 | 攻撃イベントの区間（アニメーション秒） | コンボ通知 |
| --- | ---: | ---: | --- | --- |
| 1 | 12 | 約1.5000秒 | 剣0.3333～0.5333 | 0.7167開始、1.1667でも終了通知 |
| 2 | 13 | 約2.0667秒 | 剣0.2167～0.4167 | 0.8167開始 |
| 3 | 14 | 約1.5000秒 | 剣0.3333～0.4667 | 0.7333開始、1.2000でも終了通知 |
| 4 | 15 | 約1.8333秒 | 剣0.3000～0.4000、蹴り0.7167～0.9000 | ActionEndで終了 |

表の区間はAttackTemplateイベントの区間であり、武器HitBoxの実有効区間はさらにテンプレートのactiveStartTime/activeEndTimeで切り取られる。両方をコードで確認した。

元の再生速度は1・2段目が2.0倍、3・4段目が1.5倍で、速度カーブは空欄だった。CurveComponentとModelAnimatorには、変更後のアニメーション時間をクリップ尺で割った正規化時間から、絶対再生速度を取得する既存機能がある。今回もこれを使用する。

攻撃のRootMotionScaleは1・2・4段目が約0.02、3段目が約0.025。PlayerModelの描画スケールは約0.02、プレイヤーの物理半径は0.5、Capsuleの中心Offset Yは約1.6。2.5という補正上限は、物理半径や既存の移動速度22と比較して短い接近として扱える値と判断した。

glTF/binと優先ロードされるcerealキャッシュを読み取り、使用クリップのIndex、尺、rootキーを確認した。判定前のroot Z変化に既存RootMotionScaleを掛けた概算は、各段約1.47／1.22／1.26／0.77。実際の予定量はEstimateRootMotionで親階層の行列と既存の倍率を反映し、さらにプレイヤーのYawでワールドへ変換する。

従来のPlayerAttackStateは、向き補正とは別にAddHorizontalVelocityAssistを攻撃中に継続して加える方式だった。また弱攻撃用の従来Warpプリセットは小さい方向補正用だった。今回の地上弱攻撃では従来の速度加算・向き補正を使わず、既存PlayForTargetで両方を管理する。強攻撃、空中攻撃、反撃には既存の処理を維持する。

## 各段の速度カーブ

Curve EditorでPlayer.WeakCombo1.Speed～Player.WeakCombo4.Speedを個別に編集できる。MainScene/HumanBossTestのCurveComponentへ登録し、外部カーブ資産としても保存した。別のSceneでトラックがない場合は既存LoadTrackAssetで補充し、すでに編集済みのトラックは上書きしない。

| 段 | 溜めの最低速度 | 振り抜きの最高速度 | 意図 |
| --- | ---: | ---: | --- |
| 1 | 1.75倍 | 2.85倍 | 初動を保ち、0.16秒付近の構えで少し溜め、判定直前から鋭く振る |
| 2 | 1.75倍 | 2.75倍 | 短い予備動作に合わせて早めに加速し、返しの斬撃をつなぐ |
| 3 | 1.40倍 | 2.60倍 | 元の1.5倍に近い溜めを短く残し、狭い判定区間を素早く振り抜く |
| 4 | 1.30倍 | 剣2.45倍／蹴り2.35倍 | 剣と蹴りの前にそれぞれ溜めを置き、後隙は約1.45～1.50倍へ戻して締めの重さを残す |

曲線の横軸は「速度変更後の現在アニメーション秒／元のクリップ尺」、縦軸は絶対再生速度。Stateのspeedへさらに掛ける倍率ではない。ゲーム全体のTimeScaleには触れていない。

曲線を時間積分した計算では、最初のAttackTemplateイベントまでの時間は以下のとおり。実際の入力遅延や体感を測定した結果ではない。

| 段 | 旧設定 | 新カーブ |
| --- | ---: | ---: |
| 1 | 約0.1667秒 | 約0.1648秒 |
| 2 | 約0.1083秒 | 約0.1063秒 |
| 3 | 約0.2222秒 | 約0.1987秒 |
| 4 | 約0.2000秒 | 約0.1936秒 |

判定や入力受付を遅らせる待機処理は追加していない。各段の先頭へBeginWeakComboWarpイベントを1個追加し、グラフがその段を再生してから補正を開始する。コンボ遷移直後に旧段の再生時間で次段のWarpを進めないための同期点である。既存のイベント時刻、AttackTemplate、Trail/VFXの参照、音声、ダメージ、ノックバックは変更していない。アニメーション選択は既存グラフのまま。

## MotionWarpとコンボの連続性

MotionWarpComponentのPresetsにWeakAttackWarp1～4を追加した。初段の最大追加位置補正は2.5、後続段は各1.25。毎回その距離を移動する設定ではない。

1. 段の開始時に、判定前の予定RootMotionをEstimateRootMotionで求める。
2. 敵との停止位置までの残距離から、その予定ベクトルを差し引く。
3. 不足分だけを各段の補正上限へ制限し、アニメーション上の補正窓へ分散する。
4. 最終的なRootMotion差分へ停止距離制限を適用し、CharacterMovementと既存の物理処理へ渡す。

停止距離はプリセットのTargetDistanceと、双方のCapsule半径合計＋0.10の大きい方。小さい敵にも大きい敵にも中心まで進ませず、手前の間合いを残す。補正は水平成分だけを追加する。

補正窓は最初のAttackTemplateイベントの0.025アニメーション秒前までに制限する。プリセットを短く設定した場合は短い方を優先する。初期値は以下。

| 段 | 追加位置補正上限 | 追加補正速度上限 | 最大旋回角 | 旋回速度上限 | 補正終了の正規化時間 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 2.50 | 24/秒 | 55度 | 540度/秒 | 約0.2056 |
| 2 | 1.25 | 20/秒 | 45度 | 540度/秒 | 約0.0927 |
| 3 | 1.25 | 20/秒 | 45度 | 540度/秒 | 約0.2056 |
| 4 | 1.25 | 20/秒 | 45度 | 540度/秒 | 約0.1500 |

TargetDistanceは1.2、対象の追従上限は開始位置から0.6かつ15度。補正終了の0.02正規化時間前に対象位置を固定し、旋回もその時点までに終える。判定開始後は新しい目標位置へ追尾しない。プリセットの最大検索距離は12だが、実際には停止距離＋予定RootMotion＋その段の補正上限で届く候補だけを受け付ける。

最大位置補正/秒と旋回速度は既存設定を使用する。最大位置補正/Frameは2.5とし、通常の低FPSで小さな固定フレーム上限が速度上限より先に効かないようにした。総量と秒単位の速度制限は常に適用する。

新しいDiscardUnappliedCorrection設定は、この4プリセットだけで有効にした。速度上限や物理によって進めなかった予定分を、後のフレームへまとめて繰り越さない。他の既存プリセットでは従来の挙動を維持する。補正終了後の停止距離も、この設定が有効な場合は固定済み位置を使用する。

各段に入るたびに予定RootMotionと補正予算を作り直す。前段の残り予算は加算しない。段間に停止用Stateや追加の待機時間は挟んでいない。

## 対象選択と衝突

- 生存、削除予約、IsCombatReady、Enemyレイヤー、有効なHurtBoxを確認する。部位HurtBoxも調べ、受け側のないObjectや明示無敵の対象へ吸着しない。
- ロックオン対象を優先する。明示的に変更された対象は次段の開始時に反映し、同じ段の途中では再選択しない。
- 非ロックオンでは入力方向、入力がなければプレイヤー前方の範囲から選ぶ。背後へ自動的に向き直らない。
- 後続段は前段の有効な対象を優先する。対象を失った場合、明確な移動入力がないまま最近傍の別の敵へ切り替えない。既存の対象引き継ぎ設定を無効にした場合は、その設定に従う。
- プレイヤーの胴体の高さでSphere Sweepし、壁や他の物理的な障害物がある候補を除く。同じCollisionOwnerと対象自身の形状、Combat用Triggerは除外する。
- 補正中の経路にも同じ確認を行い、塞がれた時点で補正を停止する。実移動は既存CharacterControllerのmoveを通すので、対象や壁への物理的な押し戻しも維持する。
- 死亡、消失、明示無敵、補正可能範囲外になった対象は解除する。再生途中に別の敵へ探し直さない。

既存のノックバックは1～3段目が1.2／1.5／2.0、最終段の剣が1.0、蹴りが5.5。位置の瞬間加算ではなくHitReactionからのImpulseであり、後続段の自然なRootMotionと1.25の追加補正で小さなずれを補う構成にした。敵ごとの質量・減衰・移動を含めて後続段が届くかは、実機での確認が必要。

## 高速化とイベント・判定の取りこぼし対策

弱コンボのStateだけsampleWeaponHitBoxesを有効化した。既存Stateで未指定の場合はfalseとなり、既存の時間更新・イベント順を維持する。

- 弱コンボでは最大1/240秒の幅で再生速度を中点積分し、フレーム冒頭のカーブ値だけで大きな時間幅を進めない。
- 開始・更新・終了をアニメーション時刻順に処理する。区間を飛び越えた場合も更新を一度通し、必要な開始・終了通知を取りこぼさない。
- 武器の実有効区間と、そのフレームで通過した時間区間の交差だけを120アニメーションサンプル/秒で採取する。全区間を1フレームで飛び越えた場合も同じ処理を行う。
- 保存した途中姿勢と姿勢間を既存のHitBox/HurtBox、Guard/JustGuard、JustDodge判定へ渡す。姿勢間の隙間には小さな保守的な半径拡張を使用する。
- 一時的に有効化したHitBoxは、全戦闘判定を終えてから閉じる。攻撃IDと既存の重複命中防止を使い、別の弱攻撃用ダメージ処理は追加しない。
- アニメーション更新後にCCTが実際に進めた位置差を、保存した軌道へ反映する。壁で進めなかった予定距離は軌道へ足さない。
- 一時サンプリング後は元の描画姿勢へ戻す。判定前の元RootMotionも先頭サンプルから取得し、予定量との差を抑える。

Trail・VFX・Soundの発火は同じアニメーション時間の既存イベントとAttackTemplateに従う。既存のヒットストップはレイヤーの時間として入力され、曲線処理がTimeScaleを上書きすることはない。VFXの見た目の残光や音声まで実機で同期確認したわけではない。

## 中断と通常移動との接続

回避、ガード、被弾、死亡などで弱攻撃を抜ける際は、Warpと対象情報、旧攻撃の武器判定サンプルを閉じる。別Actionへ移る際は未反映のRootMotionだけを破棄し、被弾由来のImpulseは残す。速度は共有Stateの保存値を書き換えずに評価するため、変更した速度が次の行動へ残らない。

既存のキャンセル受付、先行入力、コンボ通知の時刻と条件は変更していない。ガードなどの遷移が許可される範囲も既存ルールに従う。

前段の地上移動改修では、MOVEを離れる際に地上移動の速度を破棄し、MoveStateは行動入力を先に処理する。今回の弱攻撃では通常の位置アシストを加えず、Warpの向き補正へ統一したため、Start/Loop/Stopの移動予約や旋回を重ねない構成になっている。

## 静的確認と未確認事項

確認済み：4本のカーブが個別で正の速度を持つこと、元の判定・入力イベント・RootMotion設定・基準速度の維持、外部State/グラフのカーブ参照と先頭イベント、MainScene/HumanBossTestの既存設定保持、4段の補正予算と判定前の位置・向き固定、保存復元、宣言定義、include、公開API、括弧対応、Visual Studio登録。

数式の検算：5/15/30/60/120fps相当の時間幅で全5判定窓に交差区間が存在し、途中姿勢の採取対象になることを確認した。これはC++コードや実際の敵との衝突を実行した試験ではない。

未確認：実際の溜めと振り抜きの見え方、入力の体感、判定・Trail・VFX・Soundの描画同期、密着/通常/上限/範囲外、小さい敵/大きい敵、移動・ノックバックする敵、ロックオン切替、壁際、キャンセル、被弾・死亡、ヒットストップ中、低FPSでの実命中。特に途中姿勢サンプリングのCPU負荷、保守的な判定拡張の許容範囲、複数の高速移動体同士の接触は実機で確認する。

これらの初期値は、実機調整を完了した値としては扱わない。

## 変更ファイル

- Code/Game/Charcter/Player/PlayerAttackState.h / .cpp：段ごとのWarp準備、対象選択、衝突確認、対象維持、中断。
- Code/Game/Charcter/Player/PlayerControllerComponent.cpp：弱コンボ終了時の対象と未反映RootMotionの破棄。
- Code/Engine/GamePlay/Core/Component/CharacterMovementComponent.h：RootMotionだけを破棄するAPI。
- Code/Engine/GamePlay/Core/Component/MotionWarpComponent.h / .cpp：未消化補正を繰り越さない任意設定と保存復元。
- Code/Engine/Graphics/Resource/Animation/AnimationState.h：途中姿勢サンプリングの任意設定。
- Code/Engine/Graphics/Resource/Animation/ModelAnimator.h / .cpp：弱コンボの曲線積分、イベント順、武器軌道の採取。
- Code/Engine/Graphics/Resource/Animation/AnimationControllerComponent.cpp：描画姿勢更新後の採取呼び出し。
- Code/Engine/Graphics/Resource/Animation/AnimationControllerComponent.Asset.cpp / .Gui.cpp：任意設定の保存復元とEditor表示。
- Code/Engine/GamePlay/Combat/AttackTemplateRuntime.h / .cpp：既存武器判定窓内の姿勢採取。
- Code/Engine/Physics/HitBox.h / .cpp：フレーム内の武器軌道と一時有効化の管理。
- Code/Engine/Physics/CollisionStepSystem.cpp：同じ軌道を回避・ガード・命中へ使用。
- Data/Curve/Player.WeakCombo1.Speed.json～Player.WeakCombo4.Speed.json：新規カーブ4本。
- Data/Animation/StateData/Player/States/SS_Attack1.json～SS_Attack4.json：カーブと任意サンプリングの指定。
- Data/Animation/GraphData/Player.json：グラフ内の同じ4Stateにも指定を反映。他のグラフ設定は維持。
- Data/Scenes/MainScene/Objects/PlayerModel.json / Data/Scenes/HumanBossTest/Objects/PlayerModel.json：カーブ4本とプリセット4個を追記。
- BIGShine.vcxproj / BIGShine.vcxproj.filters：新規カーブ資産をNone項目、Data\Curve Filterへ登録。
- Docs/PlayerWeakComboRhythmAndAssist.md：本記録。

## コメント・Tracy・形式・登録

対象選択、予定RootMotion、判定前の固定、衝突、未消化補正の破棄、途中姿勢の復元、キャンセル時の後片付けへ日本語コメントを追加した。

既存BS_PROFILE_SCOPEを使用し、PlayerAttackState::Execute、FindAssistTarget、PrepareWeakComboMotionWarp、StartWeakComboMotionWarp、ModelAnimator::SampleWeaponHitBoxes、AttackTemplateRuntime::SampleWeaponInterval、HitBox::CheckFrameSweepを計測する。既存のAnimation::RootMotion、ModelAnimator::AdvanceAnimationTime、CollisionStepSystemの計測は維持した。

新しいC++ファイルはない。新規カーブの実配置はData/Curve、.vcxprojではNone、.vcxproj.filtersではData\Curve。既存ファイルの移動やinclude構成の一括変更はしていない。

変更ファイルはUTF-8 BOMなし、CRLF。厳密なUTF-8としての読み取りと単独LF/CRがないことを確認した。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
