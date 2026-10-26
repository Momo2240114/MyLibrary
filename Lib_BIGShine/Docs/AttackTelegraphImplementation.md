# 攻撃予兆システム 実装報告

## 実装内容

敵レイヤーのActorに対し、既存Animation Eventが開始する攻撃判定から予兆時刻を導出する処理を追加しました。
攻撃の判定生成・有効化処理は維持しています。独立した攻撃タイマーやParticle描画システムは追加していません。

## 調査した既存構造

- 人型ボスはHumanBossAIComponentからHumanBossActionComponent、SkillExecution、ActionSequenceを通じて各段のAnimation Stateを再生します。
- AnimationControllerComponentはModelAnimatorへ時間を渡し、ModelAnimator::ActiveSlot::Updateが速度カーブと速度イベントを反映したアニメーション時間でAnimEventを処理します。
- ATTACK_TEMPLATEイベントはAttackTemplateRuntime::Start/Update/Endへ接続されます。武器HitBoxの開始はイベント開始時刻＋WeaponHitBoxTemplate::activeStartTimeです。
- ActionSequenceには独立秒数とAnimation State時間に結び付いた予約があり、すべてが同じ時計ではありません。
- DragonAI_PlaySequencerAttackは現在行動に応じて武器判定、弾、範囲攻撃へ分岐します。噛みつきでは呼び出されたその場でBiteHitBoxを有効化します。
- 派生AttackEffectの寿命・HitShapeの有効時間は生成後に別管理されます。Animation Timeからの一律な近似は追加していません。
- 既存VFXManagerとEffectManagerを確認し、現行の攻撃演出と同じEffectManager::SpawnParticle、ParticleComponent、GPU Particleを使用しています。
- ModelAttachmentComponentと同じく、モデルノードのworldTransformを使って発生座標を求めます。

## 同期方法

ANIMATION_TIME方式です。武器判定の予兆時刻は次の式で求めます。

`予兆時刻 = AnimEvent.startTime + weaponHitBox.activeStartTime - leadFrames / 60`

通常は12フレーム、暴走時は9フレームです。描画FPSによってフレーム数を数え直す処理はありません。
再生速度2倍では、12アニメーションフレーム分の実時間は約0.10秒になります。実時間0.20秒を保証する方式ではありません。
速度カーブ・速度イベント・TimeScale反映後の時間を、既存の判定イベントと共用します。
実際の発生は更新フレーム単位です。1回の時間更新で予兆と判定の両方を通過した場合は、遅れた予兆を出しません。

従来のEnable～HitBox Actionイベントにも対応します。動的ActionにはAnimation Editorで`Telegraph Hit Start`を設定し、判定を直接開始するイベントを明示できます。
ドラゴンの噛みつきでは、既存のDragonAI_PlaySequencerAttackイベントをこの開始点に指定しました。

## 頭部位置

- コードに頭部名を固定していません。AttackTemplateまたは従来AnimEventの設定としてノード名を保存します。
- 人型ボスの初期設定は実モデルの`head`、ドラゴンの噛みつきは`PlainsDragon_-Head`です。
- 予約判定とParticle生成を分け、Controllerが最新のブレンド姿勢・RootMotionをModelへ反映した後に生成します。
- ローカルオフセットをノードのワールド行列で座標変換するため、頭部の回転・スケールも反映します。
- ノード未指定・取得不能時は、Actorのワールド行列と`fallbackOffset`を使います。
- 検索結果はAnimator内で保持し、モデル・配列・名前を検証します。State変更や再開始ではキャッシュも破棄します。

## 連続攻撃・キャンセル・暴走

- 各攻撃イベントの開始時刻ごとに予約し、同時刻の複数HitBoxを1件へまとめます。
- 消費済みフラグはAnimatorごとに保持し、別Actorとの共有を避けます。
- State変更、同じStateの再開始、シークで未発生予約を破棄します。ループは次周の予約を再作成します。
- フェードアウト中の旧Stateから予兆を再生成しません。
- 暴走状態はHumanBossAIのBlackboardから読み、Berserk Lead Framesを選びます。
- 所有者削除後に動く遅延Callbackは保持せず、生成直前にも削除予約を確認します。

## Particle

`Data/Particle/EnemyAttackWarning.json`を追加しました。
既存のflare/sparkテクスチャを使った加算合成のBurstで、白い中心光と小さな放射状の光を生成します。
Particle寿命は0.10～0.13秒です。既存Poolを使い、設定倍率はGPUへ渡す一時データへ適用します。Pool再利用時に倍率は初期値へ戻ります。
プリセットを生成できない場合は警告ログを出し、攻撃処理を継続します。

## Editorと保存

AttackTemplate EditorにAttack Telegraph欄を追加しました。
有効化、Particle名とプリセット一覧、通常/暴走Lead Frames、ノード名と一覧、Local Offset、Fallback Offset、Particle Scale、寿命、Preview Telegraphを編集できます。
従来のAnimation Eventにも、Telegraph Hit Startと同じ設定欄を追加しました。
プレビューは選択Actorのモデルを使います。設定は既存のJSON保存経路へ追加しています。
旧JSONで予兆項目がない場合は既定値を使い、従来イベントの明示開始フラグはfalseになります。
Template設定を再生中に変更した場合、予約は次の再生区間で再構築されます。

## 静的確認結果

- 追加関数の宣言と定義、呼び出しAPI、include、字句上の括弧対応を確認しました。
- 変更JSONの解析、予兆設定の保存/読み込みキー、Particleテクスチャの存在とBurst設定を確認しました。
- 新規ソースのvcxproj登録と、filtersのFilter登録を別々に確認しました。
- 新規実ファイルは`Code/Engine/GamePlay/Combat`に配置しています。Filterは既存AttackTemplateRuntimeと同じ`Code\Engine\GamePlay`です。既存ファイルの移動はありません。
- 主要処理のBuild、Advance、Flush、Spawnに既存のBS_PROFILE_SCOPEを追加し、既存計測を維持しました。
- 処理の目的、同期、重複防止、座標変換、破棄と復元に日本語コメントを追加しました。
- 対象ファイルは厳密なUTF-8、BOMなし、CRLFで確認しました。

## 未対応・実機で確認する項目

- REAL_TIMEによる実時間の反応猶予保証は未実装です。
- ActionSequence単独の攻撃、独立ProjectileやAttackEffectの遅延HitShapeは自動予測しません。判定開始と同期するAnimation Eventを明示できる場合はTelegraph Hit Startを使用できます。
- 初期データを追加したのは人型ボスの武器攻撃7Templateとドラゴンの噛みつきです。他の独立弾・範囲攻撃へ初期設定を一括追加していません。
- MainSceneの人型ボスに保存された武器攻撃イベント16件の静的集計では、通常12フレームで予兆時間を確保できるものが10件、確保できず省略されるものが6件です。実行回数の集計ではありません。
- 例としてCombo_Attack_01の1～3段目は判定開始が約0.11/0.105/0.14秒です。前段へまたがる予測は追加していないため、通常12フレーム・暴走9フレームのどちらでもこれらは省略されます。
- 全段へ9～12フレームの予兆を保証するには、前段からの派生予約や攻撃の予備動作を含む別の調整が必要です。今回、既存の攻撃タイミングは変更していません。
- 通常攻撃、連続攻撃、高速再生、キャンセル、複数HitBox、頭部位置、Editor保存・再読み込みの実機確認は未実施です。Particleの見え方と大きさも実機での確認が必要です。

## 追加・変更ファイル

- `Code/Engine/GamePlay/Combat/AttackTelegraphSettings.h`
- `Code/Engine/GamePlay/Combat/AttackTelegraphRuntime.h`
- `Code/Engine/GamePlay/Combat/AttackTelegraphRuntime.cpp`
- `Code/Engine/GamePlay/Combat/AttackTemplateManager.h`
- `Code/Engine/GamePlay/Combat/AttackTemplateManager.cpp`
- `Code/Engine/Graphics/Resource/Animation/AnimationState.h`
- `Code/Engine/Graphics/Resource/Animation/ModelAnimator.h`
- `Code/Engine/Graphics/Resource/Animation/ModelAnimator.cpp`
- `Code/Engine/Graphics/Resource/Animation/AnimationControllerComponent.cpp`
- `Code/Engine/Graphics/Resource/Animation/AnimationControllerComponent.Asset.cpp`
- `Code/Editor/Window/AnimationEditorWindow.cpp`
- `Code/Engine/Graphics/Particle/ParticleComponent.h`
- `Code/Engine/Graphics/Particle/ParticleComponent.cpp`
- `Code/Game/AI/Human/HumanBossAIComponent.cpp`
- `Data/AttackTemplate/HumanBossSword01.json`
- `Data/AttackTemplate/HumanBossSword02.json`
- `Data/AttackTemplate/HumanBossSword03.json`
- `Data/AttackTemplate/HumanBossComboKick.json`
- `Data/AttackTemplate/HumanBossGuardBreak.json`
- `Data/AttackTemplate/HumanBossHeavySingle.json`
- `Data/AttackTemplate/HumanBossShadowSlash.json`
- `Data/Animation/StateData/DragonFirstForm_Test/States/PlainsDragon_BiteAttack.json`
- `Data/Scenes/MainScene/Objects/TestTarget.json`
- `Data/Particle/EnemyAttackWarning.json`
- `BIGShine.vcxproj`
- `BIGShine.vcxproj.filters`
- `Docs/AttackTelegraphImplementation.md`（本報告）

## ビルド

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
