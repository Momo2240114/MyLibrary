# 第二形態移行演出 修正報告

## 泥化場面のカメラを少し引く調整

EventCameraのTransitionDissolveについて、固定カメラ位置を[0, 7.5, -18]から[0, 7.5, -21]へ変更しました。正面側へ3だけ後退し、高さ・注視点・FOV62・切替時間は維持しています。倒れる場面のカメラ設定は変更していません。水平位置はステージ原点から21で、第二形態のバリア半径50より内側です。実際の画面での見え方は未確認です。

変更ファイルはData/Scenes/MainScene/Objects/EventCamera.jsonと本報告です。TransitionDissolveを呼び出すSequence、変更対象が1項目のみであること、JSON読込、UTF-8・BOMなし・CRLFを静的に確認しました。C++変更・コメント追加・Tracy追加・新規ファイル・Visual Studio登録変更はありません。

ビルドおよび実行確認は行っていません。コード上で確認可能な範囲の静的確認のみ実施しました。

## ゲームバランス調整の開始HP

MainSceneとHumanBossTestのPlayerModelについて、HealthComponentの開始HPと最大HPを500へ統一しました。既にロードされた値への反映にはSceneの再読込が必要です。形態移行時の回復処理は追加していません。

通常攻撃を15発受けて死亡する基準は、回復なし・満タンからの実ダメージ34です（14発で残り24、15発目で死亡）。33では16発必要になります。通常34・強攻撃50・大技100は調整候補であり、今回の敵ダメージ設定には適用していません。連撃は各ヒットの合計で評価し、回復量・被弾後の無敵時間・目標戦闘時間を合わせて決める必要があります。

AttackSystem::CalculateDamageは、攻撃者のCombatStatsまたはBlackboardに攻撃力がある場合は攻撃力と技倍率を使用し、それ以外は旧Damageを使用します。固定加算とHurtBoxの被ダメージ倍率も反映されるため、Damageの保存値だけでは実ダメージを判断できません。

変更ファイルはData/Scenes/MainScene/Objects/PlayerModel.json、Data/Scenes/HumanBossTest/Objects/PlayerModel.json、および本報告です。JSON差分をHealthComponentの2項目に限定したこと、JSON読込、両シーンの500/500、厳密なUTF-8・BOMなし・CRLFを静的に確認しました。C++変更・新規ファイル・コメント追加・Tracy追加・Visual Studio登録変更はありません。既存の処理と計測を維持しています。

ビルドおよび実行確認は行っていません。コード上で確認可能な範囲の静的確認のみ実施しました。

## プレイヤーが死亡扱いになる開始データの修正

### 原因・変更内容

MainSceneのPlayerModelにHealthComponent.CurrentHealth=0、MaxHealth=100が保存されていました。HealthComponentは保存値をそのまま有効範囲へ復元し、HumanBossAIはIsDeadを参照するため、プレイヤーを死亡として停止する経路でした。演出の一時停止を死亡へ変換する処理ではありません。

MainSceneの開始時CurrentHealthを既存の最大HP100へ戻しました。最大HPと、実際にHPが0になった場合の死亡判定は維持しています。第二形態移行時の回復や死亡無視の処理は追加しておらず、戦闘で減ったHPはそのまま引き継ぎます。

HumanBossAIの確認表示では、観察開始前もPlayerObjectNameから参照先を確認し、対象名・現在HP・最大HP・Deadを表示します。生成待ち・AIロックと、実際のHP死亡を区別するための表示です。

### 変更ファイル

- [Data/Scenes/MainScene/Objects/PlayerModel.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/PlayerModel.json)
- [Code/Game/AI/Human/HumanBossAIComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Human/HumanBossAIComponent.cpp)
- [Docs/PhaseTransition.md](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Docs/PhaseTransition.md)

### コメント・Tracy・ファイル形式・Visual Studio登録

HPの確認表示へ日本語コメントを追加しました。既存のHumanBossAIComponent::GuiのBS_PROFILE_SCOPEを維持しています。対象はUTF-8・BOMなし・CRLFです。新規C++ファイルはなく、既存の登録を維持しています。

### 確認内容

開始データ、Healthの読込・死亡判定、AIの停止条件、移行SequenceがHPを減らしたり回復したりしないこと、参照先HPの表示を静的に確認しました。既にロードされたプレイヤーのHPへファイル変更は即時反映されないため、Sceneの再読込が必要です。実際のAI動作は未確認です。

## 第二形態ObjectがHierarchyへ表示されない問題の修正

### 調査結果・変更内容

PrefabSpawnActionComponentはHumanBoss、本体のAI、武器、盾をPrefabから生成してManagerのobjectsへ登録する経路を持っています。PrefabのHumanBossAIComponentはEnabled/UpdateEnabledともにtrueです。一方、生成設定はSaveSpawnedObjects=falseであり、HierarchyはsaveFlg=falseのObjectを親・子ともに表示しない実装でした。Objectリストに出ないことだけでは、未生成やAI欠落と判断できない状態でした。

GameObjectへ実行時専用のruntimeHierarchyVisibleを追加し、Scene保存可否と表示可否を分離しました。SpawnerのShowInHierarchyをHumanBossの生成設定でtrueにし、本体・武器・盾すべてへ適用します。saveFlg=falseは維持するため、第二形態をScene開始時から生成するデータへ戻す変更はありません。

HierarchyにはShow Runtime Objectsを追加しました。通常は必要な人型だけを表示し、このCheckboxで一時VFXを含む非保存Objectも調査できます。実行時Objectには[Runtime]の表示を付け、破棄予約済みのObjectは除外します。削除時のPool返却でObject配列が変わっても走査を壊さないよう、表示は配列のコピーを使います。

SpawnerのGUIには最後の生成結果、Object registered、Hierarchy ready、Combat ready、AI名とstarted/enabled状態、Select Spawned Objectを追加しました。キャッシュ未準備、設定不一致、子の名前衝突、生成中例外も状態とログへ出します。Show In Hierarchyは生成後にも変更でき、Prefabキャッシュを無効にせず表示だけを更新します。

### 変更ファイル

- [Code/Engine/GamePlay/Core/Object/GameObject.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Core/Object/GameObject.h)
- [Code/Engine/GamePlay/Core/Object/GameObjectManager.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Core/Object/GameObjectManager.h)
- [Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp)
- [Code/Engine/GamePlay/EventSystem/PrefabSpawnActionComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/EventSystem/PrefabSpawnActionComponent.h)
- [Code/Engine/GamePlay/EventSystem/PrefabSpawnActionComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/EventSystem/PrefabSpawnActionComponent.cpp)
- [Data/Scenes/MainScene/Objects/PhaseTransition.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/PhaseTransition.json)
- [Docs/PhaseTransition.md](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Docs/PhaseTransition.md)

### コメント・Tracy・ファイル形式・Visual Studio登録

保存とHierarchy表示の分離、実行時表示フラグへ日本語コメントを追加しました。Prefab生成の既存計測を維持し、Spawner GUIとHierarchy GUIへBS_PROFILE_SCOPEを追加しました。対象はUTF-8・BOMなし・CRLFです。新規C++ファイルはなく、既存のプロジェクト登録を維持しています。

### 確認内容

HumanBossの生成ActionとPrefab名・RootName、AI Componentの有無と更新設定、親/子の表示条件、保存条件を変更していないこと、ShowInHierarchyのGUIとSerialize/Deserialize、旧JSONの既定値falseを静的に確認しました。実際の生成結果とAI動作は未確認です。

確認時はHierarchyのHumanBoss [Runtime]を選ぶか、PhaseTransitionのPrefabSpawnActionComponentのSelect Spawned Objectを使います。生成されていない場合は同GUIの生成結果を、存在する場合はAIのstarted/enabledとCombat readyを確認できます。

## プレイヤーへの襲撃を主役にし、泥の振り払いを強める調整

### 変更内容

Phase2Introの開始時に中央のMudPoolとMudVortexのMeshを停止し、VortexのParticleをResetします。発生停止だけでは残る粒子も消します。襲撃・振り払いのプレビューも同じ準備に揃えました。人型形成に入る4.2秒で中央のVortexを再生し直し、形成演出は維持しています。

TransitionMudAttackはPlayerModelを注視し、カメラ[-8, 4, -43]・画角72度の固定構図にしました。原点の大きな泥を画面へ含める自動フレーミングを外しています。

Parry_Counter_Attack_Seqを1.5倍速にしました。RootMotion無効とpelvisの水平移動除去は維持しています。剣Trail、CounterRing、SE、シェイク、泥のBurstと移動開始・停止を同じ比率で詰め、その瞬間のTimeScale演出は外しました。蹴りのTimeScaleは維持しています。反撃終了後はIdleへ戻します。

泥散開は専用のTransitionMudScatter Particleへ分離しました。大きな塊160個と伸びる飛沫100個を外向きへBurstします。速度は8～15と12～22、寿命は0.45～1.0と0.3～0.65です。重力と空気抵抗で減速して落下し、色とAlphaで消します。MaxCountは512と384です。泥本体4つのScatterも0.8秒から0.38秒の等速移動へ短縮しました。Counter用シェイクとリングの再生速度も合わせています。

### 変更ファイル

- [Data/Scenes/MainScene/Objects/PhaseTransition.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/PhaseTransition.json)
- [Data/Scenes/MainScene/Objects/EventCamera.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/EventCamera.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionPlayerCounter.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionPlayerCounter.json)
- [Data/Prefabs/PhaseTransitionEffects.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Prefabs/PhaseTransitionEffects.json)
- [Data/Particle/TransitionMudScatter.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Particle/TransitionMudScatter.json)
- [Data/VFX/TransitionCounterRing.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/VFX/TransitionCounterRing.json)
- [Docs/PhaseTransition.md](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Docs/PhaseTransition.md)

### コメント・Tracy・ファイル形式・Visual Studio登録

設定データと報告の変更のみです。C++のコメント・Tracy・Visual Studio登録は変更していません。新しいParticleデータをPrefabへ接続しました。対象はUTF-8・BOMなし・CRLFです。

### 確認内容

中央の停止/Resetと再開、プレビューとの対応、反撃の速度と同期時刻、RootMotion設定維持、カメラ対象とバリア内の位置、Burst数と容量、素材参照、専用AssetとPrefabの一致を静的に確認しました。実機での勢い、視認性、散開範囲は未確認です。

## ドラゴン攻撃の後片付け・第二形態カメラ・襲撃する泥の増量

### 変更内容

- DragonPresentation_StopはSkillが動作中かどうかに依存せず、DragonAttackControllerのStopAttackも呼びます。独立した危険範囲と攻撃判定を削除し、ブレスの発生予約と踏みつけ岩の予約を破棄します。ControllerのOnDestroyからも同じ終了処理を呼びます。
- Controllerの攻撃・危険範囲の入口16箇所に、有効状態・戦闘準備・HPを確認するガードを追加しました。更新停止後もAnimation Actionから危険範囲を再生成する経路を止めます。AttackEmitterも無効化・破棄・戦闘未引渡し時は新規発生を受け付けません。
- Particle、判定のないMeshVFX、ImpactEffectのParticle/Mesh/Decal、Animationの斬撃、ブレス本体、噛みつき、岩VFXに攻撃者の弱参照を記録します。EffectManagerのStopAttackEffectsはこの記録も使い、判定のない見た目を終了します。元の寿命は変更せず、Poolで別のEffectとして再使用された個体を以前の攻撃者が停止しないよう、現在の管理レコードに所有者を記録します。
- StopAttackはドラゴン本体・子階層・名前指定のブレスEmitterのEffectと遅延予約を整理します。AttackEmitterのOnDestroyも終了処理を持ちます。撃破後のAnimation斬撃イベントは新規VFX・予約を作りません。Sceneの一時泥・人型の常時オーラはこれらの攻撃者所有レコードに属さないため、維持します。
- 第二形態登場とスキップ復旧の開始時に、StageCorruptionのApplyCameraProfile Actionを呼びます。HumanBossTestのMainCameraからLockOn、TPS、CameraCollisionの設定を読み、距離・高さ・Player offset・壁際高さ・衝突除外対象を揃えます。LockOn有効化は従来どおりCommit後のActionが担当します。
- enableCollisionVerticalCorrectionを追加し、第二形態とHumanBossTestではfalseにしました。足元へ下げる注視点候補計算と、その縦見切れに対する距離・位置補正を早期returnで完全に外します。障害物を避けるSweep/LiftとFOV調整は維持します。旧JSONではtrueが既定値です。
- カメラ設定は実行時プロファイルとして保持し、Serializeは切替前の設定を返します。プレビューのRestoreではプロファイルを解除します。対象名変更時は旧対象の弱参照と追従の補間状態を再利用しません。
- プレイヤーへ集まる泥は、既存の泥Particleを84/秒にし、110/秒の小さな塊と36/秒の上昇煙を追加しました。各流れを3層にして、発生範囲の高さを1.2～2.4へ拡大しています。Particleの位置を上下・奥行きに散らし、厚み付きTrailと合わせて立体感を出す設定です。各Emitterには上限を設けています。

### 変更ファイル

- [Code/Game/AI/Dragon/DragonAIComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Dragon/DragonAIComponent.cpp)
- [Code/Game/AI/Dragon/DragonAttackControllerComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Dragon/DragonAttackControllerComponent.h)
- [Code/Game/AI/Dragon/DragonAttackControllerComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Dragon/DragonAttackControllerComponent.cpp)
- [Code/Engine/GamePlay/VFX/AttackEmitterComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/VFX/AttackEmitterComponent.h)
- [Code/Engine/GamePlay/VFX/AttackEmitterComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/VFX/AttackEmitterComponent.cpp)
- [Code/Engine/GamePlay/System/EffectManager.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/System/EffectManager.h)
- [Code/Engine/GamePlay/System/EffectManager.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/System/EffectManager.cpp)
- [Code/Engine/GamePlay/Camera/CameraLockOnComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Camera/CameraLockOnComponent.h)
- [Code/Engine/GamePlay/Camera/CameraLockOnComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Camera/CameraLockOnComponent.cpp)
- [Code/Engine/GamePlay/Camera/ThirdPersonCameraComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Camera/ThirdPersonCameraComponent.h)
- [Code/Engine/GamePlay/Camera/ThirdPersonCameraComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Camera/ThirdPersonCameraComponent.cpp)
- [Code/Engine/GamePlay/Camera/CameraCollisionComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Camera/CameraCollisionComponent.h)
- [Code/Engine/GamePlay/Camera/CameraCollisionComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Camera/CameraCollisionComponent.cpp)
- [Code/Game/GameMode/StageCorruptionComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/GameMode/StageCorruptionComponent.h)
- [Code/Game/GameMode/StageCorruptionComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/GameMode/StageCorruptionComponent.cpp)
- [Data/Scenes/HumanBossTest/Objects/MainCamera.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/HumanBossTest/Objects/MainCamera.json)
- [Data/Scenes/MainScene/Objects/PhaseTransition.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/PhaseTransition.json)
- [Data/Prefabs/PhaseTransitionEffects.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Prefabs/PhaseTransitionEffects.json)
- [Data/Particle/TransitionMudCrawl.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Particle/TransitionMudCrawl.json)
- [Docs/PhaseTransition.md](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Docs/PhaseTransition.md)

### コメント・Tracy

独立した予告範囲の停止、無効化中の再生成防止、弱参照によるEffect所有者、Pool返却、カメラプロファイルの保存・復元、縦補正の除外へ日本語コメントを追加しました。既存のStopAttack/StopAttackEffects/Emit計測を維持し、EmitterのOnDestroy、カメラのApplyRuntimeSettings、StageのApplyCameraProfile/RestoreへBS_PROFILE_SCOPEを使用しています。

### ファイル形式・Visual Studio登録

対象はUTF-8・BOMなし・CRLFです。新規C++ファイルはなく、既存のプロジェクト登録とFilterを維持しています。

### 確認内容

生成・停止・破棄と予約の処理経路、攻撃者の所有権とPool返却、追加APIの宣言と定義、include、JSONの保存/読込と旧値、カメラ切替ActionとRestore、Particleの上限・素材参照を静的に確認しました。実機での残留Effect、カメラ挙動、泥の見え方は未確認です。

## 死亡・泥化カットを近づけ、着地シェイクを遅らせる調整

### 変更内容

EventCameraのTransitionDeathWideを[-14, 1.8, -36]、TransitionDissolveを[0, 7.5, -18]の固定構図へ変更しました。両カットのAuto Frameを無効にし、ドラゴンの広いモデル範囲から距離が過大に計算される経路を外しています。死亡カットはプレイヤーとドラゴンの間へ注視点を置き、画角72度を使用します。泥化は胴体付近を正面から少し見下ろします。原点からの水平距離はそれぞれ約38.6と18で、第二形態バリア半径50の内側です。

着地のシェイク、地面Particle、衝突SEは、死亡Animation開始から2.15秒後へ揃えました。Deathクリップの長さは約2.233秒です。本番の発火は2.5秒から3.25秒、死亡プレビューは2.6秒から3.35秒へ変更しています。地面ParticleのStopも後ろへ移しています。

### 変更ファイル

- [Data/Scenes/MainScene/Objects/EventCamera.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/EventCamera.json)
- [Data/Scenes/MainScene/Objects/PhaseTransition.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/PhaseTransition.json)
- [Docs/PhaseTransition.md](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Docs/PhaseTransition.md)

### コメント・Tracy・ファイル形式・Visual Studio登録

JSONと報告の変更のみです。C++コメント・Tracy・Visual Studio登録は変更していません。対象をUTF-8・BOMなし・CRLFで確認しています。

### 確認内容

構図の解釈、固定カメラの水平距離、Sequenceと死亡プレビューの発火時刻、Animation長、Action名とSequence終了時間との対応を静的に確認しました。画面内の見え方と着地の厳密な接触Frameは実機で未確認です。

## 泥の厚み・原点での形成・HP連動オーラ・AI準備の修正

### 変更内容

- 人型の出現Markerを原点付近の[0, -0.122791, 0]へ変更しました。泥のConvergeと剥がれ粒子もこのMarkerを使用します。接近先は既存の剣戟Markerを維持しています。
- 泥の帯は発生量24から72へ増量しました。粒子の発生範囲に高さを持たせ、粒子サイズも拡大しています。TrailMeshへTrailCrossSectionHeightを追加し、泥では0.75を設定しました。左右端と持ち上げた中央をつないだ2面の断面です。GPUの頂点・Index容量もこの断面数へ合わせています。既定値0は従来の平面を維持します。
- 泥溜まりへ3つの球体を潰した塊を追加しました。渦の球体は半径相当6.3から7.4、Orbit粒子は各55から110、ドラゴンからの流出は90から180へ増量しました。各EmitterのMaxCountを指定し、無制限には増やしません。
- HumanBoss本体へHumanBossDarkAuraのParticleを追加しました。CPUスキニング頂点から黒紫の煙、紫の細い流れ、少量の光点を発生させます。各Emitterの頂点候補数は192です。形成後8.1秒に再生し、通常戦闘でも継続します。ボス階層に所属するため、一時VFXのCleanupでは削除しません。
- ParticleComponentへHPによる発生数の補正を追加しました。HP満タンで1倍、50%で2倍、25%で2.5倍、瀕死で約3倍です。計算は1 + 2 * (1 - 現在HP/最大HP)です。実際の発生数は整数へ丸めます。既存の環境遷移倍率と合成し、保存済みのEmitter数値は書き換えません。HP参照がない場合は通常倍率、撃破後は新規発生を停止します。HP連動の既定値はfalseです。
- HP参照元、満タン時倍率、低HP時倍率はEditorとSerialize/Deserializeへ対応しました。HumanBossDarkAuraでは自身のHealthを参照します。MaxCountは黒煙384・紫の流れ192・光点96です。

### AI停止の調査と変更

HumanBossAIのStartではAnimationGraphBridgeを動的に追加します。AcquireがcallStart=falseで追加していたため、元のComponent開始リストの外で追加されたBridgeはIsStarted=falseのままになり得ました。IsHierarchyReadyは全ComponentのStart完了を要求するため、人型の生成待ちとCommitActorの戦闘引渡しを止める経路でした。

このFacadeは独自Start処理を持たないため、Acquire時に既存のStart経路を通し、既に追加済みでも未開始の場合は開始済みにします。HumanBossAIには、未開始・戦闘未引渡し・Executor欠落の停止理由をBlackboardへ公開する処理と、Component suspended / Combat readyのEditor表示を追加しました。

通常の戦闘ロック、HP、Player観察、停止解除、Commit順もコード上で確認しました。実機でAIが動くことは未確認であり、今回の修正はコードで確認できた停止経路への対応です。

### コメント・Tracy

Facadeの開始判定、HP倍率と死亡後の発生停止、泥の断面とバッファ容量へ日本語コメントを追加しました。ParticleComponent::Updateへ既存のBS_PROFILE_SCOPEを追加し、TrailMeshのCreateBuffers/RebuildMesh/Update、AIのUpdateとAcquire呼出し元の計測を維持しています。

### ファイル形式・Visual Studio登録

対象はUTF-8・BOMなし・CRLFです。新規C++ファイルはありません。既存の.vcxproj登録を維持しています。データとしてData/Particle/HumanBossDarkAura.jsonを追加し、人型PrefabとSequenceへ接続しました。

### 確認内容

HP設定の保存/読込と旧JSON既定値、体力別の倍率、Particleの容量と素材参照、Prefabへの所有関係と再生Action、原点のMarker参照、断面別の頂点/Index数とバッファ容量、追加APIの宣言/定義、Bridgeの開始判定とAI診断を静的に確認しました。見た目、負荷、AIの実行動作は未確認です。

### 今回の変更ファイル

- [Code/Engine/Graphics/Resource/Animation/AnimationGraphBridgeComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Graphics/Resource/Animation/AnimationGraphBridgeComponent.cpp)
- [Code/Game/AI/Human/HumanBossAIComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Human/HumanBossAIComponent.cpp)
- [Code/Engine/Graphics/Particle/ParticleComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Graphics/Particle/ParticleComponent.h)
- [Code/Engine/Graphics/Particle/ParticleComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Graphics/Particle/ParticleComponent.cpp)
- [Code/Engine/Graphics/VFX/TrailMeshComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Graphics/VFX/TrailMeshComponent.h)
- [Code/Engine/Graphics/VFX/TrailMeshComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Graphics/VFX/TrailMeshComponent.cpp)
- [Data/Scenes/MainScene/Objects/HumanBossSpawnTransform.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/HumanBossSpawnTransform.json)
- [Data/Scenes/MainScene/Objects/PhaseTransition.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/PhaseTransition.json)
- [Data/Prefabs/Phase2HumanBoss.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Prefabs/Phase2HumanBoss.json)
- [Data/Prefabs/PhaseTransitionEffects.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Prefabs/PhaseTransitionEffects.json)
- [Data/Particle/HumanBossDarkAura.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Particle/HumanBossDarkAura.json)
- [Data/Particle/TransitionDragonMud.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Particle/TransitionDragonMud.json)
- [Data/Particle/TransitionHumanMudSlough.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Particle/TransitionHumanMudSlough.json)
- [Data/Particle/TransitionMudVortex.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Particle/TransitionMudVortex.json)
- [Data/Particle/TransitionMudCrawl.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Particle/TransitionMudCrawl.json)
- [Data/VFX/TransitionMudVortex.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/VFX/TransitionMudVortex.json)
- [Data/VFX/TransitionMudPool.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/VFX/TransitionMudPool.json)
- [Docs/PhaseTransition.md](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Docs/PhaseTransition.md)

## カメラ・RootMotion・剣戟の追加調整

### 変更内容

- プレイヤーをドラゴン正面のMarkerへ配置しました。死亡カットはドラゴンとプレイヤーを対象に、ドラゴン基準の右斜め前から低い視点で撮ります。CameraActionDirectorに主対象の原点からの目線高さを設定する項目を追加し、必要な撮影距離は水平距離で維持します。既定値では目線高さを固定しません。
- 倒れる時刻へ専用のTransitionDragonFallShakeを接続し、0.65秒の減衰する揺れを設定しました。シェイクは構図更新後のLateUpdatePriority=200で適用します。
- 溶解カットは正面から約21度見下ろす方向へ変更しました。泥カットでは引き続き泥とプレイヤーを対象にします。
- 反撃クリップのrootノードは静止していましたが、pelvisには大きな前進が保存されていました。ModelAnimatorはRootMotion無効時に抽出元ノードを解決しないため、見た目の移動が残る経路でした。RootMotionSettingsにcutVisualWhenDisabledを追加し、GameObjectへ移動を適用せずに見た目の移動を除去できるようにしました。旧JSONの既定値はfalseです。
- 演出用のプレイヤー・人型StateはRootMotionを無効化し、pelvisのX/Z移動を除去します。上下の姿勢変化は残します。反撃はParry_Counter_Attack_Seqのままです。
- 人型の接近は254: Run_Fast_Combat_IP_Start_Seq、非ループ、1.25倍速です。Markerへの接近時間を2.4秒から0.55秒へ短縮しました。剣戟3段とプレイヤーのBlock/Parryは1.6倍速にし、接触のSE・火花・シェイク・停止時刻を同じ比率で詰めました。Phase2Introは17.9秒、剣戟プレビューは2.9秒です。
- ActorPresentationへRefreshStateAssetOnPlayを追加し、今回の演出で有効にしました。指定されたState Assetを再生時に反映し、Scene内の古いLoop/RootMotion設定が残ることを避けます。同じ演出の再確認でも再生を先頭から開始します。旧JSONではfalseを既定値にして、従来のScene内State優先を維持します。
- 接触3回にTransitionClashSlashを追加しました。既存のSlashArcRibbonを2枚重ね、約0.14秒の暖色・寒色の閃光を作ります。既存の剣Trailと合わせて使います。火花は1回36個のBurst、最大96個、寿命0.12～0.32秒へ調整しました。移動した配置に火花・蹴り・泥散開の位置も合わせました。

### コメント・Tracy・ファイル形式

移動適用と見た目の移動除去の分離、低い目線の距離計算、State Assetの反映と再開始へ日本語コメントを追加しました。既存のModelAnimator更新、CameraActionDirectorの範囲計算、ActorPresentationの実行計測を維持しています。追加した計算はこれらのBS_PROFILE_SCOPE内で実行します。対象はUTF-8・BOMなし・CRLFです。

### Visual Studio登録

新しいC++ファイルはありません。既存の登録を維持しています。データとしてData/Particle/TransitionClashSparks.jsonとData/VFX/TransitionClashSlash.jsonを追加し、演出PrefabとSequenceに接続しました。

### 確認内容

glTFのAnimation名・長さ・root/pelvisのChannel、RootMotion設定の保存/読込と旧JSON既定値、Camera設定のGUIと保存/読込、非ループ接近の長さ、剣戟の速度とSequence時刻、3回の火花/斬撃の開始停止、Prefabの名前・親子関係・素材参照を静的に確認しました。実画面での構図、接地、剣同士の接触位置、閃光の明るさは未確認です。

### 今回の変更ファイル

- [Code/Engine/GamePlay/EventSystem/ActorPresentationActionComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/EventSystem/ActorPresentationActionComponent.h)
- [Code/Engine/Graphics/Resource/Animation/AnimationState.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Graphics/Resource/Animation/AnimationState.h)
- [Code/Engine/Graphics/Resource/Animation/ModelAnimator.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Graphics/Resource/Animation/ModelAnimator.cpp)
- [Code/Engine/Graphics/Resource/Animation/AnimationControllerComponent.Gui.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Graphics/Resource/Animation/AnimationControllerComponent.Gui.cpp)
- [Code/Engine/GamePlay/EventSystem/ActorPresentationActionComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/EventSystem/ActorPresentationActionComponent.cpp)
- [Code/Engine/GamePlay/Camera/CameraActionDirectorComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Camera/CameraActionDirectorComponent.h)
- [Code/Engine/GamePlay/Camera/CameraActionDirectorComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Camera/CameraActionDirectorComponent.cpp)
- [Data/Scenes/MainScene/Objects/EventCamera.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/EventCamera.json)
- [Data/Scenes/MainScene/Objects/PhaseTransition.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/PhaseTransition.json)
- [Data/Prefabs/PhaseTransitionEffects.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Prefabs/PhaseTransitionEffects.json)
- [Data/Particle/TransitionClashSparks.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Particle/TransitionClashSparks.json)
- [Data/VFX/TransitionClashSlash.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/VFX/TransitionClashSlash.json)
- [Docs/PhaseTransition.md](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Docs/PhaseTransition.md)
- [Data/Animation/StateData/PhaseTransition/States/TransitionHumanApproach.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionHumanApproach.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionHumanIdle.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionHumanIdle.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionHumanKick.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionHumanKick.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionHumanSword1.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionHumanSword1.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionHumanSword2.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionHumanSword2.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionHumanSword3.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionHumanSword3.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionKnockdownEnd.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionKnockdownEnd.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionKnockdownLoop.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionKnockdownLoop.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionKnockdownStart.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionKnockdownStart.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionPlayerBlock.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionPlayerBlock.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionPlayerCounter.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionPlayerCounter.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionPlayerIdle.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionPlayerIdle.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionPlayerParry.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionPlayerParry.json)
- [Data/Scenes/MainScene/Objects/PlayerEventTransform.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/PlayerEventTransform.json)
- [Data/Scenes/MainScene/Objects/HumanBossSpawnTransform.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/HumanBossSpawnTransform.json)
- [Data/Scenes/MainScene/Objects/HumanBossClashTransform.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/HumanBossClashTransform.json)
- [Data/Scenes/MainScene/Objects/PlayerKnockdownTransform.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/PlayerKnockdownTransform.json)
- [Data/Scenes/MainScene/Objects/MudSurroundTransform0.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/MudSurroundTransform0.json)
- [Data/Scenes/MainScene/Objects/MudSurroundTransform1.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/MudSurroundTransform1.json)
- [Data/Scenes/MainScene/Objects/MudSurroundTransform2.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/MudSurroundTransform2.json)
- [Data/Scenes/MainScene/Objects/MudSurroundTransform3.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/MudSurroundTransform3.json)
- [Data/Scenes/MainScene/Objects/MudScatterTransform0.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/MudScatterTransform0.json)
- [Data/Scenes/MainScene/Objects/MudScatterTransform1.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/MudScatterTransform1.json)
- [Data/Scenes/MainScene/Objects/MudScatterTransform2.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/MudScatterTransform2.json)
- [Data/Scenes/MainScene/Objects/MudScatterTransform3.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/MudScatterTransform3.json)

## 生成・削除Actionを専用Componentへ分離

### 変更内容

- PrefabSpawnActionComponent: Sceneロード中にPrefab JSONをコピーし、参照モデルを既存ModelResourceManagerで先読みします。この段階ではGameObject、AI、Collider、LockOn対象を生成しません。Action時は保持した定義から実体化し、Prefabファイルを再読込しません。単体と階層の両方のPrefab形式を扱います。
- ObjectDeleteActionComponent: 対象階層の削除、プレビュー中の保護、生成物の整理、LockOn参照の解除をAction化しました。演出に必要な削除方針をこのComponentへ集約しています。
- GameObjectManager: 登録・検索・Start待機・安全な破棄予約とFrame終端のコンテナ更新を担います。LockOn型に依存した演出処理は削除Componentへ移しました。GameModeManagerは既存どおり進行状態とSequenceの選択を担当します。
- ActorPresentationActionComponent: Animation・配置・VFXの制御を維持しています。旧JSONの生成・削除Enum値は残し、専用Componentへの委譲に変更しました。ActionSequenceの既存生成登録APIも同様に委譲します。

ロード時のコピーは設定データであり、生のGameObjectやPhysX Actorを複製する処理ではありません。Action時のComponent初期化やParticle用GPUバッファ作成は残るため、生成後の階層準備を待つ既存のwaitForObjectも維持しています。

| カテゴリ | Action名 | 内容 |
|---|---|---|
| ObjectSpawn | PhaseTransition_SpawnEffects | 演出Prefabを生成 |
| ObjectSpawn | PhaseTransition_SpawnHuman | 人型Prefabを生成 |
| ObjectSpawn | PhaseTransition_CommitHuman | 通常更新と物理を戻し、人型を戦闘へ引渡し |
| ObjectDelete | PhaseTransition_DestroyDragon | ドラゴン階層を削除。プレビューでは保護 |
| ObjectDelete | PhaseTransition_CleanupEffects | この生成Componentが保持する生成物を整理 |

生成ComponentにはSceneへ実体を保存する設定、通常更新の一時停止、モデル先読み、Prefabコピーの再読込、戦闘引渡し先を用意しました。削除Componentには対象名、削除種別、プレビュー中の保護、引渡し済み個体を含める設定を用意しました。設定はEditor GUIとSerialize/Deserializeに対応しています。キャッシュと生成途中の状態は保存しません。

既存のPrefab内名前参照を維持するため、同名の階層を二重に生成しません。子Objectの名前が衝突する場合は生成を止めて警告します。PrefabのRootName、ObjectList、親参照と循環をロード時に検査し、不正データはSceneの既存ロード失敗経路へ報告します。

### 提示されたエラー

GameObjectManager.cppにあったFindComponents<CameraLockOnComponent>は型のincludeを欠いていました。この未定義識別子からテンプレート呼出し、range-forの構文、camera_初期化の診断が連鎖する可能性があります。該当処理をObjectDeleteActionComponentへ移し、CameraLockOnComponent.hとGameObjectManager.hを明示して型とAPIを揃えました。実際にすべての診断が解消したかはビルド未実施のため未確認です。

### コメント・Tracy・ファイル形式

コピーと実体化の分離、親循環の検出、物理の一時停止、引渡し、削除参照の解除へ日本語コメントを追加しました。Prefab準備、生成、引渡し、生成物整理、対象削除に既存のBS_PROFILE_SCOPEを使用しています。今回の対象もUTF-8・BOMなし・CRLFです。

### Visual Studio登録

実配置はCode/Engine/GamePlay/EventSystemです。新規ヘッダー2個とCPP2個を.vcxprojへ登録し、.vcxproj.filtersでは既存のCode\Engine\GamePlay\EventSystem Filterへ登録しました。実ファイルの移動はありません。

### 確認内容

5つの生成・削除ActionとSequence側の33箇所の呼出しが対応することを確認しました。PrefabのRootName・ObjectList・名前・親参照・非循環性、追加APIの宣言と定義、include、JSON保存と読込の項目、Visual Studio登録を静的に確認しています。ActorPresentationActionComponentとActionSequenceから直接のInstantiate/Destroy呼出しを除き、ManagerのLockOn型依存がなくなったことを確認しています。

### 今回の変更ファイル

- [Code/Engine/GamePlay/EventSystem/PrefabSpawnActionComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/EventSystem/PrefabSpawnActionComponent.h)
- [Code/Engine/GamePlay/EventSystem/PrefabSpawnActionComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/EventSystem/PrefabSpawnActionComponent.cpp)
- [Code/Engine/GamePlay/EventSystem/ObjectDeleteActionComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/EventSystem/ObjectDeleteActionComponent.h)
- [Code/Engine/GamePlay/EventSystem/ObjectDeleteActionComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/EventSystem/ObjectDeleteActionComponent.cpp)
- [Code/Engine/GamePlay/EventSystem/ActorPresentationActionComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/EventSystem/ActorPresentationActionComponent.cpp)
- [Code/Engine/GamePlay/EventSystem/ActionSequence.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/EventSystem/ActionSequence.h)
- [Code/Engine/GamePlay/EventSystem/ActionSequence.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/EventSystem/ActionSequence.cpp)
- [Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp)
- [Data/Scenes/MainScene/Objects/PhaseTransition.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/PhaseTransition.json)
- [BIGShine.vcxproj](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/BIGShine.vcxproj)
- [BIGShine.vcxproj.filters](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/BIGShine.vcxproj.filters)
- [Docs/PhaseTransition.md](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Docs/PhaseTransition.md)

## 変更内容

泥を振り払う反撃は、モデル内の `201: Parry_Counter_Attack_Seq` を専用の `TransitionPlayerCounter` から再生します。RootMotionのenable、移動・回転適用をすべてfalse、Scaleを0にしました。通常戦闘用の元ステートは変更していません。演出用ステートでは通常攻撃イベントを除外し、剣の軌跡と泥の散開はSequenceから制御します。

人型の本体・武器・盾と演出用VFXをMainSceneの開始リストから外し、既存定義を再利用するPrefabへまとめました。元のObject定義ファイルは残しています。実行時の変更はPrefabとPhaseTransitionのSequenceが基準です。

| 時点 | 処理 |
|---|---|
| Phase1Defeat開始 | 演出Prefab生成、階層のLoad・Start・Runtime準備を待機、操作・AI・物理判定停止 |
| Phase1Defeat 8秒 | ドラゴン階層を破棄予約、PhysX ActorとAction登録を終了 |
| Phase1Defeat 11秒 | 第二形態登場Sequenceへ進行 |
| Phase2Intro 4.2秒 | 人型Prefab生成、準備完了を待って配置・非表示・待機姿勢を設定 |
| Phase2Intro 5～8.1秒 | 泥の収束、足元・顔・全身のカット、Dissolveによる形成 |
| Phase2Intro 9.3～9.85秒 | 接近用Animationと等速のMarker移動 |
| Phase2Intro 17.7秒 | Animation制御とCollider停止を解除、戦闘可能に切替、LockOn対象を設定 |
| Phase2Intro 17.9秒 | 一時生成物を削除し、第二形態戦闘へ移行 |

上記はSequence時計での設定値です。読み込み待ち、フレーム間の開始予約、TimeScaleの影響があるため、実時間の所要秒数を保証するものではありません。

### GameObject・Collider・LockOn

- Componentへ終了用OnDestroyを追加しました。削除予約時点で更新を抑制し、PhysX Actor、接触キャッシュ、Object所有のAction登録を終了します。実際の配列削除は従来どおりFrame終端で行います。
- 子階層を参照で破棄し、同名の新規Objectと取り違えないようにしました。Start待ちから破棄済みObjectを除き、Scene終了後は残存参照のManagerを解除します。
- Local/Global Actionは弱参照の所有元を確認して実行します。ActionRegistryは実行中のCallbackを保持し、登録解除による参照無効化を避けます。
- LockOn候補はComponent有効状態、破棄予約、戦闘準備、HP、CharacterColliderの停止状態を確認します。人型のAIも戦闘準備完了まで更新しません。
- PrefabSpawnActionComponentが生成した階層を追跡し、ObjectDeleteActionComponentが削除Actionを担当します。ActionSequenceは完了・中断時の整理を専用Componentへ委譲します。人型だけを戦闘へ引き渡し、VFXは完了・中断・Scene終了で破棄します。プレビュー終了は第一形態へ復元します。
- GameMode WindowへSkip Phase Transitionを追加しました。中断時は復旧Sequenceを通り、生成物、配置、UI、カメラ、色、TimeScaleを整えて戦闘へ戻します。

### カメラ・配置

- CameraActionDirectorに対象範囲へ合わせる設定を追加しました。CPU側の既存スキニング行列から最大1024点をサンプリングし、画角とAspectから必要距離を求めます。同じカット中は最大範囲を保持します。
- ドラゴン全体、泥とプレイヤーの両方、人型の形成、剣戟、吹き飛ばしへカットを分けました。形成の寄りでは指定した範囲と高さを使います。
- プレイヤーをドラゴンから離し、人型登場地点はプレイヤー前方へ配置しました。原点付近から剣戟位置までの接近を0.55秒の素早い等速移動にし、非ループのRun_Fast_Combat_IP_Start_Seqを使用しています。吹き飛ばし地点は半径50の壁内に設定しました。

### 泥・ステージ侵食

- 暗い泥の球体、傾いた複数の帯、脈動する赤い発光、逆方向のOrbit・内向き速度・吸引を持つGPU Particleで渦を構成しました。参考画像の構成を目標にした初期値です。
- 移動経路からTrailMeshの帯を生成し、蛇行・襲撃・散開・収束を表現します。未対応だった内部メッシュ名を既存のSlashArcRibbonへ合わせました。
- ドラゴンのCPUスキニング頂点を発生源とし、Dissolveの方向と進行度から近似した境界帯へ崩壊粒子を絞ります。GPU readbackは追加していません。ShaderのNoise境界との完全一致ではありません。
- ステージ半径は既存HumanBossTestのWall設定から読み込み、70から50へ補間します。PhysX壁は既存MeshのScaleを使い、毎フレームのCookを避けます。見た目、環境Particle、壁際のカメラ制限も合わせます。

## コメント

生成待ち、戦闘への引き渡し、破棄と参照解除、CPU頂点の座標変換、Dissolve境界近似、カメラの距離計算、泥の帯、TimeScale復元へ日本語コメントを追加しました。

## Tracy

既存のBS_PROFILE_SCOPEを維持しています。生成物の整理、GameObjectの破棄準備、カメラ範囲計算、CPU頂点サンプリング、Particle発生候補の更新、TimeScale復元を同じマクロで計測します。

## ファイル形式

下記の作業対象をUTF-8、BOMなし、CRLFで確認しました。厳密なUTF-8読み込みと、不正な改行・置換文字がないことを確認しています。

## Visual Studio登録

新規C++クラスは以下の既存方針に沿った実フォルダーへ配置し、.vcxprojと.vcxproj.filtersを個別に確認しました。既存ファイルの移動はしていません。

- ModelDissolveComponent: Code/Engine/GamePlay/Graphics
- StageCorruptionComponent: Code/Game/GameMode

- Code\Game\GameMode\StageCorruptionComponent.cpp: Filter `Code\Game\Logics\GameMode`
- Code\Engine\GamePlay\Graphics\ModelDissolveComponent.cpp: Filter `Code\Engine\GamePlay\Graphics`
- Code\Game\GameMode\StageCorruptionComponent.h: Filter `Code\Game\Logics\GameMode`
- Code\Engine\GamePlay\Graphics\ModelDissolveComponent.h: Filter `Code\Engine\GamePlay\Graphics`

## 確認内容

JSON構文、Actor/Markerの名前、Prefab階層、State/素材の参照先、カメラ名、Action種別の数値、生成と待機の順序、Collider復帰後のCommit/LockOn順序、保存と読み込みの新規項目、追加APIの宣言・定義・呼び出しを静的に確認しました。プレイヤーモデルのAnimation配列から指定クリップの名前も確認しました。

実機での画面、パフォーマンス、AnimationとVFXの接触タイミングは未確認です。カメラの範囲は頂点の間引きによる近似なので、極端な姿勢で完全な画面内収容を保証しません。部分Dissolve中のShadow/Outlineは抑制しており、影にも同じNoiseで穴を開ける処理は未実装です。

## 変更ファイル

- [BIGShine.vcxproj](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/BIGShine.vcxproj)
- [BIGShine.vcxproj.filters](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/BIGShine.vcxproj.filters)
- [Code/Editor/Window/GameModeWindow.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Editor/Window/GameModeWindow.cpp)
- [Code/Engine/Audio/AudioComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Audio/AudioComponent.h)
- [Code/Engine/GamePlay/Camera/CameraActionDirectorComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Camera/CameraActionDirectorComponent.cpp)
- [Code/Engine/GamePlay/Camera/CameraActionDirectorComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Camera/CameraActionDirectorComponent.h)
- [Code/Engine/GamePlay/Camera/CameraLockOnComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Camera/CameraLockOnComponent.cpp)
- [Code/Engine/GamePlay/Camera/CameraLockOnComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Camera/CameraLockOnComponent.h)
- [Code/Engine/GamePlay/Camera/CameraShakeComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Camera/CameraShakeComponent.cpp)
- [Code/Engine/GamePlay/Camera/LockOnTargetComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Camera/LockOnTargetComponent.cpp)
- [Code/Engine/GamePlay/Camera/LockOnTargetComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Camera/LockOnTargetComponent.h)
- [Code/Engine/GamePlay/Core/Component/CharacterMovementComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Core/Component/CharacterMovementComponent.cpp)
- [Code/Engine/GamePlay/Core/Component/CharacterMovementComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Core/Component/CharacterMovementComponent.h)
- [Code/Engine/GamePlay/Core/Component/ComponentBase.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Core/Component/ComponentBase.h)
- [Code/Engine/GamePlay/Core/Object/GameObject.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Core/Object/GameObject.cpp)
- [Code/Engine/GamePlay/Core/Object/GameObject.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Core/Object/GameObject.h)
- [Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp)
- [Code/Engine/GamePlay/Core/Object/GameObjectManager.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Core/Object/GameObjectManager.h)
- [Code/Engine/GamePlay/EventSystem/ActionSequence.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/EventSystem/ActionSequence.cpp)
- [Code/Engine/GamePlay/EventSystem/ActionSequence.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/EventSystem/ActionSequence.h)
- [Code/Engine/GamePlay/EventSystem/ActorPresentationActionComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/EventSystem/ActorPresentationActionComponent.cpp)
- [Code/Engine/GamePlay/EventSystem/ActorPresentationActionComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/EventSystem/ActorPresentationActionComponent.h)
- [Code/Engine/GamePlay/EventSystem/TimeScaleDirector.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/EventSystem/TimeScaleDirector.cpp)
- [Code/Engine/GamePlay/EventSystem/TimeScaleDirector.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/EventSystem/TimeScaleDirector.h)
- [Code/Engine/GamePlay/Graphics/ModelDissolveComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Graphics/ModelDissolveComponent.cpp)
- [Code/Engine/GamePlay/Graphics/ModelDissolveComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Graphics/ModelDissolveComponent.h)
- [Code/Engine/GamePlay/Serialize/ActionRegistry.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/Serialize/ActionRegistry.cpp)
- [Code/Engine/GamePlay/System/AttackSystem.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/GamePlay/System/AttackSystem.cpp)
- [Code/Engine/Graphics/Particle/ParticleComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Graphics/Particle/ParticleComponent.cpp)
- [Code/Engine/Graphics/Particle/ParticleComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Graphics/Particle/ParticleComponent.h)
- [Code/Engine/Graphics/PostEffect/ColorFilterDirector.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Graphics/PostEffect/ColorFilterDirector.cpp)
- [Code/Engine/Graphics/RenderSystem/ModelRenderSystem.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Graphics/RenderSystem/ModelRenderSystem.cpp)
- [Code/Engine/Graphics/RenderSystem/ModelRenderSystem.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Graphics/RenderSystem/ModelRenderSystem.h)
- [Code/Engine/Graphics/Resource/GlTFModel.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Graphics/Resource/GlTFModel.cpp)
- [Code/Engine/Graphics/Resource/GlTFModel.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Graphics/Resource/GlTFModel.h)
- [Code/Engine/Graphics/VFX/MeshVFXComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Graphics/VFX/MeshVFXComponent.h)
- [Code/Engine/Graphics/VFX/MeshVFXRenderSystem.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Graphics/VFX/MeshVFXRenderSystem.cpp)
- [Code/Engine/Graphics/VFX/TrailMeshComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Graphics/VFX/TrailMeshComponent.cpp)
- [Code/Engine/Physics/ContainmentVolumeComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Physics/ContainmentVolumeComponent.cpp)
- [Code/Engine/Physics/ContainmentVolumeComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Engine/Physics/ContainmentVolumeComponent.h)
- [Code/Game/AI/Dragon/DragonAIComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Dragon/DragonAIComponent.cpp)
- [Code/Game/AI/Human/HumanBossAIComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/AI/Human/HumanBossAIComponent.cpp)
- [Code/Game/Charcter/Player/PlayerControllerComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/Charcter/Player/PlayerControllerComponent.cpp)
- [Code/Game/GameMode/GameModeManager.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/GameMode/GameModeManager.cpp)
- [Code/Game/GameMode/GameModeManager.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/GameMode/GameModeManager.h)
- [Code/Game/GameMode/GameModeObjectToggleComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/GameMode/GameModeObjectToggleComponent.cpp)
- [Code/Game/GameMode/StageCorruptionComponent.cpp](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/GameMode/StageCorruptionComponent.cpp)
- [Code/Game/GameMode/StageCorruptionComponent.h](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Code/Game/GameMode/StageCorruptionComponent.h)
- [Data/Animation/StateData/PhaseTransition/States/TransitionDragonDeath.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionDragonDeath.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionHumanApproach.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionHumanApproach.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionHumanIdle.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionHumanIdle.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionHumanKick.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionHumanKick.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionHumanSword1.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionHumanSword1.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionHumanSword2.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionHumanSword2.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionHumanSword3.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionHumanSword3.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionKnockdownEnd.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionKnockdownEnd.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionKnockdownLoop.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionKnockdownLoop.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionKnockdownStart.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionKnockdownStart.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionPlayerBlock.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionPlayerBlock.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionPlayerCounter.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionPlayerCounter.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionPlayerIdle.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionPlayerIdle.json)
- [Data/Animation/StateData/PhaseTransition/States/TransitionPlayerParry.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Animation/StateData/PhaseTransition/States/TransitionPlayerParry.json)
- [Data/Curve/PhaseTransition.Corruption.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Curve/PhaseTransition.Corruption.json)
- [Data/Particle/Phase2GroundSmoke.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Particle/Phase2GroundSmoke.json)
- [Data/Particle/TransitionDragonMud.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Particle/TransitionDragonMud.json)
- [Data/Particle/TransitionGroundImpact.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Particle/TransitionGroundImpact.json)
- [Data/Particle/TransitionHumanMudSlough.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Particle/TransitionHumanMudSlough.json)
- [Data/Particle/TransitionMudCrawl.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Particle/TransitionMudCrawl.json)
- [Data/Particle/TransitionMudVortex.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Particle/TransitionMudVortex.json)
- [Data/Prefabs/Phase2HumanBoss.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Prefabs/Phase2HumanBoss.json)
- [Data/Prefabs/PhaseTransitionEffects.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Prefabs/PhaseTransitionEffects.json)
- [Data/Scenes/MainScene/Objects/BossIntro.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/BossIntro.json)
- [Data/Scenes/MainScene/Objects/DragonDeathTransform.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/DragonDeathTransform.json)
- [Data/Scenes/MainScene/Objects/EventCamera.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/EventCamera.json)
- [Data/Scenes/MainScene/Objects/HumanBoss.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/HumanBoss.json)
- [Data/Scenes/MainScene/Objects/HumanBossClashTransform.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/HumanBossClashTransform.json)
- [Data/Scenes/MainScene/Objects/HumanBossShield.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/HumanBossShield.json)
- [Data/Scenes/MainScene/Objects/HumanBossSpawnTransform.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/HumanBossSpawnTransform.json)
- [Data/Scenes/MainScene/Objects/HumanBossWeapon.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/HumanBossWeapon.json)
- [Data/Scenes/MainScene/Objects/MudScatterTransform0.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/MudScatterTransform0.json)
- [Data/Scenes/MainScene/Objects/MudScatterTransform1.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/MudScatterTransform1.json)
- [Data/Scenes/MainScene/Objects/MudScatterTransform2.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/MudScatterTransform2.json)
- [Data/Scenes/MainScene/Objects/MudScatterTransform3.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/MudScatterTransform3.json)
- [Data/Scenes/MainScene/Objects/MudSurroundTransform0.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/MudSurroundTransform0.json)
- [Data/Scenes/MainScene/Objects/MudSurroundTransform1.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/MudSurroundTransform1.json)
- [Data/Scenes/MainScene/Objects/MudSurroundTransform2.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/MudSurroundTransform2.json)
- [Data/Scenes/MainScene/Objects/MudSurroundTransform3.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/MudSurroundTransform3.json)
- [Data/Scenes/MainScene/Objects/MudVortexTransform.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/MudVortexTransform.json)
- [Data/Scenes/MainScene/Objects/Phase2Environment.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/Phase2Environment.json)
- [Data/Scenes/MainScene/Objects/Phase2Music.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/Phase2Music.json)
- [Data/Scenes/MainScene/Objects/PhaseTransition.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/PhaseTransition.json)
- [Data/Scenes/MainScene/Objects/PlayerEventTransform.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/PlayerEventTransform.json)
- [Data/Scenes/MainScene/Objects/PlayerKnockdownTransform.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/PlayerKnockdownTransform.json)
- [Data/Scenes/MainScene/Objects/TestTarget.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/TestTarget.json)
- [Data/Scenes/MainScene/Objects/TransitionClashSparks.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/TransitionClashSparks.json)
- [Data/Scenes/MainScene/Objects/TransitionCounterRing.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/TransitionCounterRing.json)
- [Data/Scenes/MainScene/Objects/TransitionDragonMud.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/TransitionDragonMud.json)
- [Data/Scenes/MainScene/Objects/TransitionGroundImpact.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/TransitionGroundImpact.json)
- [Data/Scenes/MainScene/Objects/TransitionHumanMudSlough.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/TransitionHumanMudSlough.json)
- [Data/Scenes/MainScene/Objects/TransitionImpactSound.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/TransitionImpactSound.json)
- [Data/Scenes/MainScene/Objects/TransitionKickImpact.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/TransitionKickImpact.json)
- [Data/Scenes/MainScene/Objects/TransitionMud0.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/TransitionMud0.json)
- [Data/Scenes/MainScene/Objects/TransitionMud1.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/TransitionMud1.json)
- [Data/Scenes/MainScene/Objects/TransitionMud2.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/TransitionMud2.json)
- [Data/Scenes/MainScene/Objects/TransitionMud3.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/TransitionMud3.json)
- [Data/Scenes/MainScene/Objects/TransitionMudPool.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/TransitionMudPool.json)
- [Data/Scenes/MainScene/Objects/TransitionMudScatter.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/TransitionMudScatter.json)
- [Data/Scenes/MainScene/Objects/TransitionSlashSound.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/Objects/TransitionSlashSound.json)
- [Data/Scenes/MainScene/SceneList.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/Scenes/MainScene/SceneList.json)
- [Data/VFX/TransitionCounterRing.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/VFX/TransitionCounterRing.json)
- [Data/VFX/TransitionMudPool.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/VFX/TransitionMudPool.json)
- [Data/VFX/TransitionMudRibbon.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/VFX/TransitionMudRibbon.json)
- [Data/VFX/TransitionMudVortex.json](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Data/VFX/TransitionMudVortex.json)
- [Docs/PhaseTransition.md](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Docs/PhaseTransition.md)
- [Shader/GBufferPS.hlsl](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Shader/GBufferPS.hlsl)
- [Shader/ToonPBRPS.hlsl](C:/Users/2240114/Desktop/BigShineLibrary/MyLib_BS/CareerPortfolioDevelopment/MyLibrary/Lib_BIGShine/Shader/ToonPBRPS.hlsl)

## ビルド

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
