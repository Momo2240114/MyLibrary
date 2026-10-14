# 第二形態の攻撃範囲・遠距離行動の追加

## 現在の修正：突進予兆・特殊攻撃・近接Warp

### 変更内容と確認できた問題

突進の予兆はOwnerの足元座標を高さとして使っていました。CharacterのTransformは地面の高さと一致しないため、地面の下へ予兆を配置する経路がありました。CharacterとCombat Volumeを除外して実際の地面を取得し、その0.06上へ表示します。DangerAreaComponentの動的更新でもTemplateのGroundOffsetを維持します。

突進の予兆PhaseはACTION_ENDで管理し、予兆ObjectのStart準備完了後に1秒伸長、完成表示を0.2秒維持してから次Phaseへ進みます。予兆の準備に失敗した場合は突進しません。突進中も同じ予兆を保持し、終了・中断で解除します。1Frame最大補正2から0.35、1秒最大補正100から18へ下げ、HumanBoss.ChargeTravelで補正を分散させます。Run_Attack_01_Seq_0の速度は1.65から1.3へ変更しました。移動は引き続きRootMotion/MotionWarpで行い、Transformの直接ワープは使いません。

特殊攻撃は、中央までの直線経路と40以内という条件を開始前に要求していました。現在はHP50%以下・生存・Movement/Collider・中央の地面を開始条件とし、通常のCharacterMovementで中央へ進む準備行動を実行します。実在するRun_Fast_Combat_IP_Loop_Seq（253番）をHumanBossRagnarokAdvanceへ設定し、RootMotionは無効、水平の見た目移動は切断しています。移動速度の初期値は8で、RootMotionとの二重移動はありません。

中央から1.25以内へ到達すると移動を即時停止し、予兆へ進みます。中央から1.5以内で開始する場合は移動を省略します。CCTの壁・Playerとの衝突と足場の処理を維持し、移動時間上限は開始距離÷速度＋3秒です。半円の予兆とHitBoxは同じ地面中心を使用します。初回の予兆前に被弾・時間上限等で中断した場合、再試行までのCooldownを最大2秒へ短縮します。予兆開始前の失敗を使用済みと記録せず、開始直後に止まった行動へ35秒のCooldownを付けません。HP条件・既存Phase・実行中コンボ・強制被弾・AIロックの判定は維持しています。

近接コンボは2段目の実効補正上限が0.45×0.45で約0.2であり、3・4段目はWarp未設定でした。2～4段目へ同じ近接Warpを接続し、最大補正3・PositionStrength1・1Frame上限0.35・1秒上限24へ変更しました。各段の開始時だけ、距離3.25以内かつ前の狙いからの移動3以内の対象位置へ狙いを更新します。距離・向き・補正時間・経路の検証を通った場合だけ更新し、振り始めた後は固定します。大きく横へ回避した相手は追いません。

さらに、Warpの終端をまたぐFrameで最後の補正を適用せず終了する処理と、予測側で終端Frameを除外していた処理を揃えて修正しました。近接攻撃の開始・継続判定と実移動が同じ補正終端を使います。

### Editor・Debug・保存

RagnarokAdvanceSpeedを実行担当のGUI・Serialize・Deserializeへ追加しました。Ranged progressには突進の予兆準備・伸長・完成・走行、特殊攻撃の中央移動・予兆・各回の打撃・終了/停止を表示します。これはRuntime情報として保存しません。実際にロードされる外部BehaviorGraphの参照と3行動の登録を確認し、Prefab・両シーンの保存済みSkill Phase・Animation State・近接Warp設定も同期しました。

### コメント・Tracy・ファイル形式・Visual Studio登録

地面の選別、予兆表示の時計、CCTでの中央移動、段開始時だけの再照準、終端Frameの補正、初回中断時の再試行へ日本語コメントを追加しました。主要処理は既存BS_PROFILE_SCOPEで計測します。新規C++ファイルはなく、既存のvcxproj登録とFilterを維持しています。ChargeTravelは新規Curve JSONです。対象ファイルはUTF-8・BOMなし・CRLFです。

### 静的確認と実機で必要な確認

括弧、宣言・定義、include、呼び出した既存API、JSON、外部Graphの実際の参照先、新行動のSequence/State参照、全4段のWarp設定、実モデルのクリップ番号、予兆表示完了後の遷移、RootMotionとの二重移動がない設定、VS登録を静的に確認しました。

実機では地面上の突進予兆と1秒＋0.2秒の表示、突進の移動量・速度、HP50%以下での中央移動と半円攻撃、2～3の間合いでの近接コンボを確認する必要があります。ヒット成立や特殊攻撃の発動を実機で確認済みとはしていません。シーン・Prefab・Animation Stateの再読込が必要です。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。

### 今回の変更ファイル

- [Code/Game/AI/Human/HumanBossActionComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Human/HumanBossActionComponent.h)
- [Code/Game/AI/Human/HumanBossActionComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Human/HumanBossActionComponent.cpp)
- [Code/Game/AI/Human/HumanBossActionComponent.Ranged.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Human/HumanBossActionComponent.Ranged.cpp)
- [Code/Game/AI/Human/HumanBossAIComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Human/HumanBossAIComponent.cpp)
- [Code/Engine/GamePlay/Core/Component/MotionWarpComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Core/Component/MotionWarpComponent.cpp)
- [Code/Engine/GamePlay/VFX/DangerAreaComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/VFX/DangerAreaComponent.h)
- [Code/Engine/GamePlay/VFX/DangerAreaComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/VFX/DangerAreaComponent.cpp)
- [Data/Scenes/MainScene/Objects/HumanBoss.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/HumanBoss.json)
- [Data/Scenes/HumanBossTest/Objects/HumanBoss.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/HumanBossTest/Objects/HumanBoss.json)
- [Data/Prefabs/Phase2HumanBoss.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Prefabs/Phase2HumanBoss.json)
- [Data/Animation/StateData/HumanBoss/States/HumanBossRagnarokAdvance.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/HumanBoss/States/HumanBossRagnarokAdvance.json)
- [Data/Animation/StateData/HumanBoss/States/Run_Attack_01_Seq_0.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/HumanBoss/States/Run_Attack_01_Seq_0.json)
- [Data/Curve/HumanBoss.ChargeTravel.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Curve/HumanBoss.ChargeTravel.json)
- [Docs/HumanBossRangedCombat.md](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Docs/HumanBossRangedCombat.md)

---

以下は初期実装と前回修正の記録です。中央移動・突進・近接Warpの設定は上記を最新とします。

## 特殊攻撃が発動しない経路の修正

中央付近でも中央移動用のRun_Attackクリップを必ず再生していたため、固定終点の距離が0または短い場合、自然RootMotionで中央から離れてしまい、続く予兆開始の到達判定で中断する経路がありました。中央から水平距離1.5以内なら移動Phaseを省き、直接予兆へ接続します。移動を行う場合も、停止距離0の明示的な固定終点要求だけはRootMotionを始点から終点への直線と残り距離へ制限し、横方向や終点を通り越す移動を防ぎます。Playerと既存の近接Warp要求は従来の処理を維持します。

初回特殊攻撃の分岐を、通常Guard/Dodgeより前、成立済みカウンターより後へ接続しました。実行中Skill・死亡・強制被弾・AIロックは既存の優先制御を維持します。共通Graphと両シーンおよびPrefabの内包Graphを更新しました。使用履歴は中央移動開始時ではなく、実際に予兆を開始した時点で記録します。

中央まで40を超える移動や、経路・足場の欠落は引き続き拒否します。これらは実機で起きた原因と断定していません。HP超過・無効化・中央の足場・経路遮断・移動距離について個別の開始不可理由を追加しました。AI InspectorではCooldown・Phase Enabled・使用履歴と、実行担当の条件を確認できます。BlackboardではSpecialAttackUnlockedをHP条件、CanUseSpecialAttackを開始可能状態、InitialSpecialReadyを初回優先状態として分けて公開します。

日本語コメントは中央到達済みのPhase省略、固定経路のRootMotion制限、初回優先判断へ追加しました。既存BS_PROFILE_SCOPEを維持しています。宣言と参照、include、括弧、JSON、4つのGraphの接続とPriority、既存Enum番号の維持、使用済み記録の位置、UTF-8・BOMなし・CRLFを静的に確認しました。新規C++ファイルはなく、Visual Studio登録変更はありません。

Scene/Prefabの再読込が必要です。実機での発動は未確認です。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。

今回の変更ファイル：

- [Code/Game/AI/Human/HumanBossActionComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Human/HumanBossActionComponent.h)
- [Code/Game/AI/Human/HumanBossActionComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Human/HumanBossActionComponent.cpp)
- [Code/Game/AI/Human/HumanBossActionComponent.Ranged.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Human/HumanBossActionComponent.Ranged.cpp)
- [Code/Game/AI/Human/HumanBossAIComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Human/HumanBossAIComponent.cpp)
- [Code/Engine/GamePlay/Core/Component/MotionWarpComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Core/Component/MotionWarpComponent.cpp)
- [Data/AI/BehaviorGraph/HumanBoss.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/AI/BehaviorGraph/HumanBoss.json)
- [Data/Scenes/MainScene/Objects/HumanBoss.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/HumanBoss.json)
- [Data/Scenes/HumanBossTest/Objects/HumanBoss.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/HumanBossTest/Objects/HumanBoss.json)
- [Data/Prefabs/Phase2HumanBoss.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Prefabs/Phase2HumanBoss.json)
- [Docs/HumanBossRangedCombat.md](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Docs/HumanBossRangedCombat.md)

## 変更内容

第二形態専用の剣・蹴りHitBoxについて、半径とNode始点から終点までの長さを2.2倍にしました。NodeSegmentScaleの既定値は1で、PlayerやDragonのColliderは等倍のままです。HumanBoss専用AttackTemplateの範囲判定の半径・高さ・距離、および斬撃MeshVFXのサイズも2.2倍です。通常技のダメージ、敵本体のモデルサイズ、既存の近接MotionWarp上限は変更していません。

EnemyMudShadowTravelとEnemyShadowTravelは粒サイズ1.8倍・発生範囲1.5倍です。発生数は増やしていません。これらを使う演出にも同じサイズが反映されます。

## AI・追加Action

- HumanBoss_SHADOW_CHARGE：選択距離6～14、最大移動12、予兆幅2、伸長1秒＋追加予兆0.2秒、後隙0.7秒、再使用10秒。Run_Attack_01_Seq_0（モデル内30番・2.0667秒）を使用します。予定経路を固定し、既存RootMotion/MotionWarpだけで移動します。線の伸長はHumanBoss.ChargeGrowthを既存Curve Editorから調整できます。移動中の経路遮断は中断し、補正終了後に自然RootMotionで進み続けないよう対応しました。
- HumanBoss_SHADOW_ORB：選択距離8～100、再使用12秒。Buff_Seq（53番・1.3333秒）の正規化時間0.3で背後上空へ初期3個を生成します。待機1.5秒、順次発射0.4秒間隔、速度3、加速度0.5、上限速度4、旋回上限25度/秒、最大寿命8秒、同時上限6個です。移動と待機は既存ProjectileMotionComponentへ追加した任意の追尾設定で管理します。通常行動へ戻っても弾を停止せず、命中・障害物・寿命・所有者破棄で終了します。死亡・AI強制停止・攻撃中断では既存所有者単位の後処理で解除します。
- HumanBoss_SHADOW_RAGNAROK：HP50%以下、再使用35秒。中央への安全な経路を確認し、既存RootMotion/MotionWarpで中央へ移動します。仮アニメーションは同じ30番クリップを判定なしのHumanBossRagnarokAdvanceとして使用し、以降はBuff_Seqです。半径48（ステージ半径50以内）・180度の予兆を120度/秒で1秒回転し、少なくとも0.5秒停止した後に0.2秒判定を発生させます。初期4回、Editorで1～8回へ変更できます。各回で攻撃IDを分け、回転中にはHitBoxを出しません。既存のHP70%・35% Phaseは維持し、HP条件と初回使用履歴を別に管理します。中央への移動が遮断された場合、または中央へ到達しなかった場合は発動を中断します。

各Cooldownは既存AIと同じく行動開始時から計測します。突進とOrbは既存Intent・退避傾向・遠距離維持時間による重みを使います。特殊攻撃と遠距離攻撃の分岐を、通常Approachより前へ接続しました。実行中コンボ・被弾硬直・AIロック・死亡の優先制御は維持しています。

共通BehaviorGraphだけでなく、MainScene・HumanBossTestおよびPhase2HumanBoss Prefabの内包Graph・Actions・Animation State・ActionSequenceを更新しています。新ActionのEnumは末尾へ追加し、既存保存データのType番号を維持しました。未登録Animation StateとSequenceを補う既存ロード処理にも接続しています。

## 共通システムの対応

DangerAreaComponentを再利用し、生成済み予兆の回転・伸長と所有者破棄時の解除を追加しました。扇形表示を毎フレーム生成し直していません。

AttackEffectHitComponentでは未対応扱いだったFANを受け入れ、広域Sphereで候補を拾った後、HitBoxの角度・半径・高さで絞ります。同じ条件をDamage・Guard・JustDodgeへ適用し、安全側での誤成立を防ぎます。既存形状は従来の処理を維持します。既存のDestroyOnHit設定も、HitBoxの命中履歴を参照して消滅へ接続しました。

ProjectileMotionComponentの追尾は通常弾では無効です。待機中の浮遊、発射時の方向取得、旋回角度上限、加速、静的な壁へのSphere sweepを既存の移動経路へ追加しました。弾の寿命とPlayerへの衝突は既存AttackEffectHitComponentが担当し、移動を二重適用しません。

## Editor・保存・Debug

HumanBossAI Inspectorから距離・Phase別Weight・Cooldown・Skill Phase・後隙を編集できます。HumanBossActionComponentのRanged / Special attacksから突進幅・距離・伸長時間/Curve・Orb数/配置/待機/速度/加速度/旋回/寿命/同時上限・特殊攻撃HP条件/回数/半径/角度/回転速度/時間/判定時間・ステージ中心/半径・任意SEパスを編集しJSONへ保存します。専用WarpはMotionWarp Editor、速度と攻撃イベントはAnimation Editor、危険範囲色はHumanBossShadowDanger.vfxmat、判定はAttackTemplate Editorで編集します。

AI Debugには遠距離維持時間・遠距離攻撃の実効Weightと既存Cooldownを表示します。実行担当には弾数・突進長/角度・特殊攻撃のHP解禁・使用履歴・回数/角度を表示し、各弾のProjectileMotion Inspectorで方向・待機経過を確認できます。実行中の値や初回使用履歴をSceneへ保存しません。

## 日本語コメント・Tracy・Visual Studio登録

固定終点、Node判定の拡張、扇形の絞り込み、追尾制限、命中後の消滅、予兆と攻撃IDの役割へ日本語コメントを追加しました。新しい主要処理には既存BS_PROFILE_SCOPEを使用し、既存Update・GUI・Skill処理の計測を維持しています。

新規C++ファイルは既存クラスの実装分割であるCode/Game/AI/Human/HumanBossActionComponent.Ranged.cppです。実配置は既存AI/Humanフォルダー、BIGShine.vcxprojのClCompileへ登録し、BIGShine.vcxproj.filtersではCode\Game\Logics\AI\Humanへ登録しました。新規クラスやComponent登録の追加はありません。

## 静的確認・実機で必要な確認

JSON読込、Enum番号、Action登録、グラフの接続、SkillのEnd保存キー、Sequence/State参照、実モデルのクリップ名/番号、主要な宣言と定義、include、括弧、VS登録、UTF-8・BOMなし・CRLFを確認しました。通常行動終了と強制停止での独立弾の扱い、所有者破棄時の予兆解除、FANのDamage/Guard/JustDodge条件もコード上で確認しました。

実機では突進の到達距離・RootMotionの見え方・剣イベント時刻、巨大化した攻撃の回避余地、予兆と判定の境界、Orbの回避・壁との衝突、中央移動と4回の範囲攻撃、描画負荷を確認してください。初期ダメージは攻撃力10・被ダメージ倍率1ならCharge50、Orb25、Ragnarok75です。既存通常技全体のゲームバランス調整は今回行っていません。

専用の突進・魔法攻撃SEは既存素材に見つからなかったため、RangedAttackSoundPathは未設定です。存在しない音声ファイルを参照させず、Editorで指定すると突進開始時に再生します。専用Ragnarokアニメーションも仮素材の段階です。素材以外の実機上の問題の有無は未確認です。変更を反映するにはScene/Prefabを再読込してください。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。

## 変更ファイル

- [Code/Game/AI/Human/HumanBossAIComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Human/HumanBossAIComponent.h)
- [Code/Game/AI/Human/HumanBossAIComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Human/HumanBossAIComponent.cpp)
- [Code/Game/AI/Human/HumanBossAIComponent.Mind.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Human/HumanBossAIComponent.Mind.cpp)
- [Code/Game/AI/Human/HumanBossActionComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Human/HumanBossActionComponent.h)
- [Code/Game/AI/Human/HumanBossActionComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Human/HumanBossActionComponent.cpp)
- [Code/Game/AI/Human/HumanBossActionComponent.Ranged.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Human/HumanBossActionComponent.Ranged.cpp)
- [Code/Engine/GamePlay/Combat/ProjectileMotionComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Combat/ProjectileMotionComponent.h)
- [Code/Engine/GamePlay/Combat/ProjectileMotionComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Combat/ProjectileMotionComponent.cpp)
- [Code/Engine/GamePlay/Combat/AttackEffectHitComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Combat/AttackEffectHitComponent.cpp)
- [Code/Engine/GamePlay/Combat/AttackTemplateManager.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Combat/AttackTemplateManager.cpp)
- [Code/Engine/GamePlay/VFX/DangerAreaComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/VFX/DangerAreaComponent.h)
- [Code/Engine/GamePlay/VFX/DangerAreaComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/VFX/DangerAreaComponent.cpp)
- [Code/Engine/Physics/CapsuleCollider.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Physics/CapsuleCollider.h)
- [Code/Engine/Physics/CapsuleCollider.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Physics/CapsuleCollider.cpp)
- [Code/Engine/Physics/HitBox.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Physics/HitBox.h)
- [Code/Engine/Physics/HitBox.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Physics/HitBox.cpp)
- [Code/Engine/GamePlay/System/GuardSystem.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/System/GuardSystem.cpp)
- [Code/Engine/GamePlay/System/JustDodgeSystem.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/System/JustDodgeSystem.cpp)
- [Code/Engine/GamePlay/Core/Component/MotionWarpComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Core/Component/MotionWarpComponent.h)
- [Code/Engine/GamePlay/Core/Component/MotionWarpComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Core/Component/MotionWarpComponent.cpp)
- [Data/AI/BehaviorGraph/HumanBoss.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/AI/BehaviorGraph/HumanBoss.json)
- [Data/ActionSequence/HumanBossRanged.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/ActionSequence/HumanBossRanged.json)
- [Data/Curve/HumanBoss.ChargeGrowth.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Curve/HumanBoss.ChargeGrowth.json)
- [Data/Prefabs/Phase2HumanBoss.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Prefabs/Phase2HumanBoss.json)
- [Data/Animation/StateData/HumanBoss/States/Run_Attack_01_Seq_0.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/HumanBoss/States/Run_Attack_01_Seq_0.json)
- [Data/Animation/StateData/HumanBoss/States/HumanBossRagnarokAdvance.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/HumanBoss/States/HumanBossRagnarokAdvance.json)
- [Data/VFX/HumanBossShadowOrb.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/VFX/HumanBossShadowOrb.json)
- [Data/VFX/HumanBossShadowRagnarok.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/VFX/HumanBossShadowRagnarok.json)
- [Data/Particle/HumanBossShadowOrb.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Particle/HumanBossShadowOrb.json)
- [Data/Particle/EnemyMudShadowTravel.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Particle/EnemyMudShadowTravel.json)
- [Data/Particle/EnemyShadowTravel.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Particle/EnemyShadowTravel.json)
- [Data/AttackTemplate/HumanBossComboKick.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/AttackTemplate/HumanBossComboKick.json)
- [Data/AttackTemplate/HumanBossGuardBreak.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/AttackTemplate/HumanBossGuardBreak.json)
- [Data/AttackTemplate/HumanBossHeavySingle.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/AttackTemplate/HumanBossHeavySingle.json)
- [Data/AttackTemplate/HumanBossShadowDive01.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/AttackTemplate/HumanBossShadowDive01.json)
- [Data/AttackTemplate/HumanBossShadowDive02.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/AttackTemplate/HumanBossShadowDive02.json)
- [Data/AttackTemplate/HumanBossShadowDomain.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/AttackTemplate/HumanBossShadowDomain.json)
- [Data/AttackTemplate/HumanBossShadowSlash.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/AttackTemplate/HumanBossShadowSlash.json)
- [Data/AttackTemplate/HumanBossSword01.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/AttackTemplate/HumanBossSword01.json)
- [Data/AttackTemplate/HumanBossSword02.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/AttackTemplate/HumanBossSword02.json)
- [Data/AttackTemplate/HumanBossSword03.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/AttackTemplate/HumanBossSword03.json)
- [Data/AttackTemplate/HumanBossShadowCharge.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/AttackTemplate/HumanBossShadowCharge.json)
- [Data/AttackTemplate/HumanBossShadowOrb.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/AttackTemplate/HumanBossShadowOrb.json)
- [Data/AttackTemplate/HumanBossShadowRagnarok.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/AttackTemplate/HumanBossShadowRagnarok.json)
- [Data/Scenes/MainScene/Objects/HumanBoss.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/HumanBoss.json)
- [Data/Scenes/MainScene/Objects/HumanBossWeapon.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/HumanBossWeapon.json)
- [Data/Scenes/MainScene/Objects/HumanBossShield.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/HumanBossShield.json)
- [Data/Scenes/HumanBossTest/Objects/HumanBoss.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/HumanBossTest/Objects/HumanBoss.json)
- [Data/Scenes/HumanBossTest/Objects/HumanBossWeapon.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/HumanBossTest/Objects/HumanBossWeapon.json)
- [Data/Scenes/HumanBossTest/Objects/HumanBossShield.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/HumanBossTest/Objects/HumanBossShield.json)
- [BIGShine.vcxproj](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/BIGShine.vcxproj)
- [BIGShine.vcxproj.filters](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/BIGShine.vcxproj.filters)
- [Docs/HumanBossRangedCombat.md](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Docs/HumanBossRangedCombat.md)
