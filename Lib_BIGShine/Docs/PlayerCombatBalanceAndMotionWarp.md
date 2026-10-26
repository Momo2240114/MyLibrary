# プレイヤーの攻撃バランスとMotionWarp比較機能

## 変更内容

通常攻撃を継続的な削り、攻撃スキルを火力とひるみの機会として使い分けるため、AttackTemplateのdamage・damageRate・impactPowerを調整。

| 攻撃 | 変更前 | 変更後 | クールダウン |
|---|---:|---:|---:|
| 地上弱攻撃4段合計（最終段の剣・蹴りを含む） | 700 | 250 | 既存のまま |
| 空中弱攻撃4段合計 | 750 | 270 | 既存のまま |
| 地上強攻撃 | 400 | 140 | 既存のまま |
| 空中強攻撃 | 250 | 120 | 既存のまま |
| PhantomRush | 140 | 530 | 3秒 |
| Breakthrough | 100 | 405 | 5秒 |
| RisingJudgment | 160 | 625 | 8秒 |

上記は各攻撃イベントが1回ずつ命中した場合の設定値の合計。攻撃力100・倍率補正1・固定加算0・被ダメージ倍率1に相当する。実戦では命中数や対象の状態で変わる。回復スキルとジャストガード後の専用反撃は今回の威力調整に含めない。

通常弱攻撃の途中段は小さい衝撃値、最終蹴りと通常強攻撃は中程度、スキルの打ち上げ・締め・着地攻撃は大きい衝撃値へ調整した。

## MotionWarp

- PlayerSkillStateがPhaseのMotionWarpPresetを読み、段ごとに対象と予定RootMotionを既存のPlayForTargetへ渡す。
- 対象は距離6以内・高低差4以内の生存中かつ戦闘可能な敵。ロックオンを優先し、未指定時は前方の近い敵を選ぶ。
- PhantomRushには踏み込み、RisingJudgmentの攻撃と下降には水平補正、Breakthroughには既存の突進速度と重複しない回転補正を設定。
- 初期値は停止距離1.5、段ごとの追加位置補正上限2、補正速度上限6/秒、旋回上限65度。停止距離は双方のCapsule半径も考慮する。
- プリセットが未登録の場合だけ標準値を追加。保存済みプリセットは維持し、MotionWarpComponentのPresetsから調整できる。
- RootMotion無効のモーションも、明示対象へのWarp要求がある間はゼロの元移動に追加補正を適用する。モーション資産のRootMotion設定は変更しない。
- 次段・終了・開始失敗・被弾やキャンセル・破棄では、自分が開始した名前のWarp要求を停止する。

## On/Offの比較方法

1. PlayerModelを選択し、InspectorのPlayerControllerComponentにあるSkill欄を開く。
2. MotionWarp ON (A/B Comparison)を切り替える。同じスイッチはMotionWarpComponentにもある。
3. 同じ位置・同じ対象へ同じスキルを使い、Original movementとApplied movement、Phase correction totalを比較する。

On/OffはMotionWarpの補正だけに適用する。Offでも元のRootMotion、突進速度、重力、スキルの威力とクールダウンは維持される。通常攻撃のPlayerAttackStateが別途実施するAttackAssistはこのスイッチの対象外。

WarpEnabledはComponentの設定として保存・復元し、古いJSONに項目がない場合はOnを初期値にする。

## コメント・Tracy

対象選択、予定RootMotionの座標変換、二重移動の防止、補正の停止と比較処理に日本語コメントを追加。新しい主要処理には既存のBS_PROFILE_SCOPEを使用し、PlayerSkillState::StartPhaseMotionWarpとPlayerControllerComponent::ResolveSkillMotionWarpTargetを計測。既存のAnimation::RootMotionとMotionWarpComponent::ApplyRootMotionWarpなどの計測は維持した。

## 変更ファイル

- `Code/Game/Charcter/Player/PlayerSkillState.h`
- `Code/Game/Charcter/Player/PlayerSkillState.cpp`
- `Code/Game/Charcter/Player/PlayerControllerComponent.h`
- `Code/Game/Charcter/Player/PlayerControllerComponent.cpp`
- `Code/Engine/GamePlay/Core/Component/MotionWarpComponent.h`
- `Code/Engine/GamePlay/Core/Component/MotionWarpComponent.cpp`
- `Code/Engine/Graphics/Resource/Animation/AnimationControllerComponent.cpp`
- `Code/Engine/GamePlay/Combat/SkillDefinition.h`
- `Data/Skills/PlayerSkills.json`
- `Data/AttackTemplate/WeakAttack01.json`
- `Data/AttackTemplate/WeakAttack02.json`
- `Data/AttackTemplate/WeakAttack03.json`
- `Data/AttackTemplate/WeakAttack04Sword.json`
- `Data/AttackTemplate/WeakAttack04Kick.json`
- `Data/AttackTemplate/StrongAttack.json`
- `Data/AttackTemplate/AirCombo_01.json`
- `Data/AttackTemplate/AirCombo_02.json`
- `Data/AttackTemplate/AirCombo_03_01.json`
- `Data/AttackTemplate/AirCombo_03_02.json`
- `Data/AttackTemplate/AirCombo_04.json`
- `Data/AttackTemplate/Attack_Air_Strong.json`
- `Data/AttackTemplate/SkillPrototypeSlash.json`
- `Data/AttackTemplate/SkillPrototypeShieldRush.json`
- `Data/AttackTemplate/SkillPrototypeLauncher.json`
- `Data/AttackTemplate/SkillPrototypeAirSlash.json`
- `Data/AttackTemplate/SkillPrototypeFinisher.json`
- `Data/AttackTemplate/SkillPrototypeLandingImpact.json`
- `Docs/PlayerCombatBalanceAndMotionWarp.md`（本書）

## ファイル形式・Visual Studio登録

対象のソース・JSON・本書はUTF-8、BOMなし、CRLF。厳密なUTF-8読み込みと改行形式を確認。新規ソースはなく、既存8ソースの.vcxprojおよび.vcxproj.filters登録を確認した。本書はDocsに配置した作業記録で、C++ソースとしての登録対象ではない。

## 静的確認

新規APIの宣言と定義、include、既存APIの呼び出し、対象の寿命、Phase開始と中断の処理経路、JSONの保存・復元を確認。攻撃テンプレート18ファイルは、damage・damageRate・impactPower以外に変更がないことを差分で確認。スキルJSONは16個のMotionWarpPreset追加以外に変更がないことを確認。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
