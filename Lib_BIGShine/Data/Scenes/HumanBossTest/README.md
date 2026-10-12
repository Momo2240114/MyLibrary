# HumanBossTest：第2形態開発用シーン

Editorの「Load Scene」からHumanBossTestを選択してください。Data/Scenes配下のSceneList.jsonを既存Editorが自動検出するため、シーン登録用コードの変更は不要です。

## 構成

MainSceneの床、壁、照明、GraphicsSettings、Player、通常戦闘HUD、JustDodge/Parry用EventPoolを引き継いだ開発用シーンです。MainSceneの既存ファイルは変更していません。

- ドラゴン本体TestTargetと、その部位・翼・尻尾・付属VFXの子階層を除外。
- HumanBoss、HumanBossWeapon、HumanBossShieldを代わりに配置。
- BossIntro、BossIntroName、EventCamera、IntroFogGate、IntroGateMist、IntroGateLight、IntroRoarAirConeを除外。
- 合計44個のObject。元シーンから66個を除外し、人型ボスの3個を追加。
- HumanBossのXZ位置は元ドラゴンの位置を使用。高さはPlayerと同じ位置に合わせ、Player方向へ向けて配置。
- MainCameraのロックオン、注視先、カメラ衝突の除外対象と、PlayerのTargetNameをHumanBossへ変更。
- HUDはIntroを経由せず表示。ボス名は「HumanBoss（開発用）」に変更。
- PlayerのHealth/HitReactionに保存されていたHP 0を、最大HPへ戻した。

## 戦闘とHUD

HumanBossは既存の専用AI、Animation、CharacterMovement、HurtBox、GuardCollider、LockOnTargetを使用します。ボスHPは1000です。開発用の攻撃力は既存CombatStatsComponentで10に設定しています。通常攻撃の技倍率1.0／1.25から10／12.5ダメージとなり、PlayerのHP100を初回被弾で失わない設定です。

ドラゴンの部位管理Componentを外すとGameModeのボスHP割合を更新する担当がなくなるため、HumanBossAIComponent::UpdateからHealthComponentのHP割合を既存GameModeへ同期する処理を追加しました。既存のBossHealthBarがその値を表示します。

ボスの見た目とHPバーの画像は既存素材を使用しています。黒泥・影の最終マテリアルや人型専用HUD画像は未調整です。

## 読み込みについて

EditorのLoad SceneはGameModeの戦闘状態をリセットするため、HumanBossTestを直接読み込むとIntroによるPlayer/AIロックを経由しません。開発用なので、通常のゲーム開始シーンMainSceneや形態移行Flowは変更していません。

JustGuardとCounterは標準でHP 70%以下のPhase 2から有効です。開始直後から確認したい場合はHumanBossAIComponentのAction DefinitionsでJUST_GUARDとCOUNTER_ATTACKのPHASE_1 Enabledを有効にしてください。

## BehaviorTreeと近距離カメラ

HumanBossの判断は既存BehaviorTree Editorで編集する。Data/AI/BehaviorGraph/HumanBoss.jsonを使用し、Counter、届く攻撃へのReaction、距離調整、接近、観察、攻撃を優先順に評価する。重み付き選択はReactionやOffense内の候補に限定する。HumanBossを選択するとGraph編集、Action選択、実行ノード表示、保存・読込・リセットを利用できる。

接近され続けても観察時間の終了後は、技が届く距離で攻撃判断へ進む。後退時間を毎Frame延長する処理をなくした。

MainCameraは人型戦用に基準距離5、最短4、最大8、距離倍率0.25、基準高さ1.2、肩越しOffset 0.6、Playerの注視点高さ1.5へ設定した。Playerのドラゴン向け近距離補正（距離+12.5、高さ-3）は、このシーンで無効化した。通常の攻撃構図プリセットは維持する。

Close Range Distance Guardをこのシーンで有効にした。近距離の敵横断で周回方向が瞬時に反転しないよう、1.5以内では方向を保持し、それより外では旋回速度を90度/秒へ制限する。構図補正後にも最短距離と高さを確認する。壁・天井の衝突回避は既存CameraCollisionの処理を維持する。

既存のドラゴン用シーンは近距離保護が標準無効で、設定を変更していない。新しいカメラ設定はEditor調整とSerialize/Deserializeに対応する。

## 移動とRootMotion

通常攻撃・Counter・Dodgeは水平RootMotionを使い、地上行動のY移動は無効にした。DodgeにはAIの固定速度を足さず、方向入力だけを渡す。

Walk/StrafeはRootMotion Scaleを0にしてコード側の速度で移動する。待機・防御・被弾Stateも移動倍率0。RootMotionのVisual Cutは有効のままなので、モデル内部の移動が重ならない。Shift用Stateを追加する場合も倍率0とVisual Cut有効で、専用Action/Component側の移動へ接続する。

HumanBossの共通ライブラリと保存済みHumanBossCombatWalkの個別Assetを揃えた。Editorで独自Stateを追加している場合は、RootMotionを使用するPhaseへForward Speedも同時設定しない。Dodge距離はSS_DodgeのRootMotion Scaleで調整する。

ビルド後にHumanBossTestを読み直し、攻撃・回避・歩行の切替と同じStateの再開始で急な移動が残らないか確認する。

## 戦闘範囲50とコンボ

このシーンのWallはMeshVFXのX/Z Scaleを70から50、ContainmentVolumeのRadiusを70から50へ変更した。Y Scaleと高さは維持する。壁ParticleのLegacyRadiusは49.5～50へ合わせた。カメラのarenaRadiusは47.5へ変更し、元の設定と同じ2.5の内側余裕を維持する。人型の予備動作を見やすくするため、ドラゴン用の距離8～14から基準5・最短4・最大8へ寄せた。非ロックオンのThirdPerson Cameraも距離6へ設定した。近距離保護と壁衝突回避は維持する。第一形態MainSceneと床の形状は変更していない。

通常攻撃Aは0.14秒（60fpsで約8フレーム）の予備動作から2連撃。Bは0.18秒（約11フレーム）の予備動作から3連撃。段間は0.04秒、3段目の前は0.07秒とする。予備動作は実在するPower_Attack_Start_Seq_0、追撃はSS_Attack3を使う。既存Skill Phase、ActionSequence、攻撃Templateを再利用する。

HumanBoss_Windup / HumanBoss_StrikeSetup Sequenceと5つのHumanBoss Curve Assetを追加した。予備動作中だけ滑らかにPlayerを追い、各打撃の直前に向きを合わせる。攻撃中の方向は固定する。再生速度カーブで溜めと振り抜きを調整し、RootMotionカーブで移動を攻撃区間へ集中させる。

Curve Editorで速度・移動カーブ、HumanBossAIのAction Definitionsで予備動作時間・段間隔・コンボPhaseを編集できる。BehaviorTree Editorは通常攻撃A/Bをコンボ単位で選択し、全段終了まで待つ。中断では共通SkillのCleanupとSequence停止を通して追尾を閉じる。

ビルド後にシーンを読み直し、壁の表示と範囲制限、予備動作の見え方、回避した後の追撃、被弾中断とコンボ終了後の通常AI復帰を確認する。

## 確認項目

1. Load SceneでHumanBossTestが選択でき、床・Player・人型ボス・HUDが表示されるか。
2. ドラゴン、部位、Intro演出用Objectが存在しないか。
3. ロックオン先がHumanBossになり、剣と盾がボス自身へ装着されるか。
4. HumanBossへのダメージでボスHPバーが減り、Playerも生存した状態で開始するか。
5. 回避・Guard・Phase 2以降のJustGuard/Counterを確認する。

ObjectList、JSON構造、親子関係、除外Objectへの参照、ロックオン設定、生存HP、UTF-8 BOMなし/CRLFを静的確認しました。新規C++クラスは追加していません。既存シーンJSONはProject/Filterへ個別登録していないため、今回も実データ配置と既存シーン検出を利用しています。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。

## 駆け引き拡張

Manager経由のPlayer観測、Intent、Reaction Delay/Budget、空振り反撃、Dodge対策、GuardBreak、Bait/Feintを追加した。調整方法と未実装項目はData/HumanBoss/MindImplementation.mdを参照。

## 攻撃テンポの調整

速度カーブはStateのSpeedを上書きするため、振りの区間を2.4～2.6倍速、終盤を1.8倍速へ変更した。短い予備動作は独立Phaseで残す。中間段はSS_Attack1の0.60秒、SS_Attack2の0.48秒（アニメーション内時間）でActionEndを通知し、攻撃判定を閉じた後に次段へ移る。最終段はANIMATION_ENDで終了硬直まで再生する。共有のPlayerアニメーションには変更を加えない。

Curve Editorの速度カーブ、Action DefinitionsのDuration/BlendDuration、Animation EditorのHumanBoss_ComboBranchが調整箇所。各段の予兆、振り、判定終了後のつながり、最終段の反撃可能時間をプレイ時に確認する。
