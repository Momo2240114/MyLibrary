# 第二形態移行演出 修正報告

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
| Phase2Intro 9.3～11.7秒 | 接近用Animationと等速のMarker移動 |
| Phase2Intro 20.6秒 | Animation制御とCollider停止を解除、戦闘可能に切替、LockOn対象を設定 |
| Phase2Intro 20.8秒 | 一時生成物を削除し、第二形態戦闘へ移行 |

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
- プレイヤーをドラゴンから離し、人型登場地点はプレイヤー前方へ配置しました。約10mの接近を2.4秒の等速移動にしています。吹き飛ばし地点は半径50の壁内に設定しました。

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
