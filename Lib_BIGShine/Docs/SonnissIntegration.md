# Sonniss SEのゲーム導入

## 変更内容

取り込んだ10個を、既存の攻撃・演出の発火箇所へ接続しました。すべて既存のカメラListenerを使用した3D OneShot再生です。

| 音源（Data/Audio/SE/Sonniss以下） | 使用箇所 |
|---|---|
| Combat/LightHit.wav | プレイヤーと人型ボスの蹴り、盾突進、着地攻撃、ドラゴンの噛みつき・尻尾・踏みつけの命中 |
| Combat/MetalWeaponHit.wav | 人型ボスのガード崩し・重い単発剣攻撃の命中 |
| Dragon/RoarTrex.wav | BossIntroの既存RoarSound通知 |
| Dragon/FireWhoosh.wav | 単発・直線・扇状ブレスの発射要求 |
| Mud/LiquidHit.wav | 影斬り・影領域・周回弾の命中、影落下攻撃の発動 |
| Mud/SplatHit.wav | 泥の突進・影落下攻撃・ラグナロクの命中 |
| Shadow/TentacleMotion.wav | 影領域の発動、周回弾の生成、泥の突進開始 |
| Boss/MonsterGrowl.wav | 暴走開始 |
| Presentation/BassDropFast.wav | プレイヤーの着地攻撃、ドラゴンの踏みつけ、影落下の強攻撃、ラグナロク発動 |
| Presentation/RattlingDowner.wav | 暴走開始時に唸りへ重ねる低音 |

通常の命中音はヒット成立時だけ再生します。範囲攻撃の複数判定や周回弾の生成ループに発動音を入れず、一回の要求で一度だけ鳴らします。影の残像が継続生成されるSpawnShadowEffectにも音を追加していません。
扇状ブレスは弾ごとではなく発射要求時に一度再生します。暴走の音は開始処理にだけ追加し、毎フレームのオーラ更新では再生しません。
咆哮は、通常攻撃が演出用に一時停止されている間も再生できるよう、Componentの保存上の有効設定を確認します。

MainSceneに配置された人型ボスと、形態移行で生成されるPhase2HumanBossプレハブの両方へ、既存のRangedAttackSoundPathを設定しました。
音量は通常0.45～0.75、暴走の低音は0.3です。攻撃テンプレートの音量・距離は既存Editorから変更できます。
10音源は標準SEの末尾へ追加し、初回使用前に読み込みます。既存のSE番号は変えていません。

音声ファイルの加工・試聴・実機での音量調整は行っていません。

## 変更ファイル

ソース6件：

- Code/Engine/Audio/AudioManager.cpp
- Code/Engine/GamePlay/Serialize/ActionRegistry.cpp
- Code/Game/AI/Dragon/DragonAttackControllerComponent.cpp
- Code/Game/AI/Human/HumanBossAIComponent.Berserk.cpp
- Code/Game/AI/Human/HumanBossActionComponent.cpp
- Code/Game/AI/Human/HumanBossActionComponent.Ranged.cpp

攻撃テンプレート14件（Data/AttackTemplate/以下）：

- WeakAttack04Kick.json
- HumanBossComboKick.json
- SkillPrototypeShieldRush.json
- SkillPrototypeLandingImpact.json
- DragonBiteSpark.json
- HumanBossGuardBreak.json
- HumanBossHeavySingle.json
- HumanBossShadowSlash.json
- HumanBossShadowCharge.json
- HumanBossShadowDive01.json
- HumanBossShadowDive02.json
- HumanBossShadowDomain.json
- HumanBossShadowOrb.json
- HumanBossShadowRagnarok.json

その他：

- Data/Scenes/MainScene/Objects/HumanBoss.json
- Data/Prefabs/Phase2HumanBoss.json
- Data/Audio/SE/Sonniss/README.md
- Docs/SonnissIntegration.md（追加）

## コメント

発動音と命中音の分離、ループ内での多重再生防止、演出中の咆哮再生、先行読み込みの目的へ日本語コメントを追加しました。

## Tracy

咆哮ActionへBS_PROFILE_SCOPEを追加しました。ドラゴンの攻撃生成、暴走開始、影攻撃・周回弾・ラグナロク生成は既存の計測範囲を維持し、音声再生には既存のAudioManager::PlaySpatialAttackSoundの計測を使用します。

## ファイル形式

変更ソース、JSON、資料はUTF-8・BOMなし・CRLF。厳密なUTF-8読み込みを確認しました。WAV本体は変更していません。

## Visual Studio登録

新規C++ソース・ヘッダーはありません。変更6ソースの.vcxproj登録と.vcxproj.filters登録を別々に確認しました。新規資料はDocs以下に配置し、既存ファイルの移動はありません。

## 確認内容

- 音源の存在とWAVヘッダー。
- JSON構文と音声設定以外の値を維持していること。
- 武器・生成HitBoxへのfeedback受け渡し。
- ActorPresentationによる一時停止中も咆哮を再生できること。
- 影の範囲攻撃と周回弾の開始音が生成ループの外にあること。
- 既存RangedAttackSoundPathのGUI・Serialize・Deserialize経路。
- MainScene配置と形態移行用プレハブの設定。
- 既存API、include、Component参照の寿命、Scene切替時の停止経路。

## ビルド

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
