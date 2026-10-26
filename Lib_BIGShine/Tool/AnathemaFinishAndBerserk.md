# ANATHEMA 人型ボス暴走・決着 実装記録

## 今回の追加修正

暴走中の完全無敵、追従オーラ、格子Particleの視認性を追加修正。詳細はAnathemaBerserkInvulnerability.mdを参照。以下のビルド結果は追加修正前の結果であり、今回の変更はビルド・実行していない。

## 到達状況

実装、プリセット作成、静的確認、Debug x64の実行ファイル生成まで実施。起動確認の途中でユーザー操作により中断されたため、画面・実際の攻防・通しプレイの確認は未実施。完成した演出の見栄えや難易度を確認済みとはしていない。

## 実装した遷移

- 通常戦闘でHP25％・10％・5％への到達を予約。開始済みの段階は同一戦闘で再発動しない。
- 25％は3行動、10％は5行動、5％は12行動が初期値。単位はSkill開始成功1回であり、内部の打撃数ではない。
- 大ダメージで複数閾値を通過しても予約を保持。5％到達時は未実行の25％・10％予約を統合し、最終段階を優先。進行中の連撃・スタンへの割り込みはしない。
- 暴走では既存の影斬撃・突進・空中叩きつけ2種・影の範囲攻撃・通常コンボ2種・単発強打を到達可能性に応じて選ぶ。
- 本体、子の武器、Animation、ActionSequence、移動、独立した攻撃判定/VFXの更新時計を合わせる。速度は1.25/1.5/1.8倍が初期値。
- 暴走連撃中と攻撃間は完全無敵。HP減少、被弾Action、HitStop、押し出しの命中処理を遮断し、スタンで解除する。黒紫の煙と紫電の専用オーラで表示する。
- 暴走中のジャストガードは現在のSkill、予約イベント、所有攻撃判定を終了し、短い受け流し後に次行動へ進む。開始時に回数加算するため重複通知では増えない。最後の行動ならスタンへ進む。
- ジャスト回避Systemの経路は変更していない。暴走中は通常AIのDodge観測による段間延長を行わない。
- 25％・10％後は3秒/4秒スタンして通常戦闘へ戻る。最終スタンは自動復帰せず、Playerの有効打を待つ。
- 最終段階の予約後だけHP下限を設けて通常死亡を保留。有効打は通常のダメージ処理へ渡さず、一度だけ決着へ接続。決め絵撮影時に死亡を確定する。
- 決着は通常Scene更新と物理を停止し、生のフレーム時間で進行。固定配置、泥噴出、実際のカウンターアニメーション、白緑のクロス斬撃、撮影、討伐完了表示、新しい決定入力、暗転、GoSceneTitleの順に進む。
- 本番の固定画像はPostEffect後・Sprite前のRenderTargetを複製。固定SDR画像をSprite経路へ直接表示し、Bloom/ToneMappingを再適用しない。通常UI・LockOn・Gizmo・ImGuiを撮影へ含めない。
- プレビュー中はScene全体の更新を止め、停止時にTransform・モデルポーズ・Collider抑制・操作ロックを復元。本番はScene破棄でVFX/画像資源を解放し、音量とTimeScaleを復旧する。

## 格子

- 初期値は各系統9本、計18本。幅1.4、間隔9.0。偶数設定でも中心線を残す。
- 系統ごとの逆回転は同じRagnarokGridStripを更新し、予兆・打撃の配置・Particle端点で共有する。
- 回転速度85度/秒、回転時間1秒、予兆1.25秒、打撃0.2秒、終了硬直1.6秒。
- 回転は打撃前に停止し、最後に表示した帯の位置を打撃へ引き渡す。
- 打撃時にライン全体へ電気と泥をBurst発生させ、続いて同じ線分を共有するSweepが0.16秒で端から端へ走る。残滓には攻撃判定を持たせない。
- Particleは4Emitter/線、合計最大256粒子/線、最大24線。密度0.55が初期値。既存のGPU ParticleとEffectManager Poolを使用。

## Editorの開き方と保存先

F1でEditorを表示し、Window → Tools → ANATHEMA Boss Finish を開く。

1. MainSceneのモデル読み込み完了後、Play / Restartで専用プレビューを開始。
2. Pause、Before Capture、Step 1/60、Timelineで時刻を調整。
3. Presetツリーで基準点、カメラ位置/注視点/FOV/ロール、各Actor位置/向き/StateAsset/PoseTime、各VFXの開始/停止/PoseTime/位置/回転/スケール/色/発光、文字とズームを編集。
4. Camera from EditorでEditorカメラの構図を基準点からの相対座標へ保存。
5. 変更を反映するにはPlay / RestartまたはSeekを使用。GPU Particleはリセットから固定刻みで再生する。
6. Recaptureで撮り直し、Show fixed 2Dとサムネイルで画像を確認。
7. Save Presetで保存、Load Presetで読込。Stop + RestoreまたはWindowを閉じて復元。

プリセット: Data/Presentation/AnathemaBossFinish.json。実行時も同じファイルを使用する。

## 調整箇所

- HumanBossAIComponent Inspector → HP Berserk / Final Stun: ActionCounts、Speeds、StunDurations、ParryDelay、ActionGap、MinimumTell。
- HumanBossActionComponent Inspector → Ranged / Special attacks: RagnarokGridLineCount/Width/Spacing、RotationSpeed/Time、TellTime、HitDuration、RecoveryTime、GridParticleSweepTime/Density/LineLimit/Asset。
- 実戦用MainScene、HumanBossTest、第二形態Prefab、Data/HumanBoss/HumanBoss.jsonの双方へ初期設定を反映。
- カウンターはRun_Attack_01_Seq、通常のCounterAttackが参照するPlayerSwordArc_Strongから決着用クロス素材を作成。主人公の髪や刀身マテリアルを変更していない。
- 全攻撃素材が欠損した場合だけログを出してスタンへ救済。間合い・経路の開始失敗は行動回数へ加算しない。
- 決着のActor/アニメーション/VFX欠損はログを出し、利用可能な表示でタイトル復帰まで進める。

## 確認結果

- 新規・変更C++のコンパイル、宣言/定義/API呼び出しをMSVCで確認。
- Debug x64ビルド成功。出力はx64/Debug/BIGShine.exe。
- 初回全体ビルドはLNK1101（MSPDB140.DLLバージョン不一致）で失敗。同じVisual StudioのHostx64側LinkToolPathをコマンド引数で指定した再ビルドで成功。プロジェクトのビルド設定は変更していない。
- 成功ログ: AnathemaBuildFinal.log。初回失敗ログ: AnathemaBuild.log。ClCompileのみの確認ログ: AnathemaBuildVerify.log（この呼び出しではLinkがスキップされるため全体成功の根拠にはしていない）。
- 既存のTracy重複登録、既存Shaderの警告、一部Shader出力のfailed writingメッセージは残っている。依頼範囲外の整理はしていない。
- JSON構文、プリセットの参照アセット、格子ParticleのTexture、Particle上限を確認。
- 中心線・隙間・帯の四隅が半径48内に収まることを計算で確認。
- 新規ファイルのvcxproj登録とFilter登録を別々に確認。
- 対象ソース25ファイルおよび設定9ファイルの厳密UTF-8・BOMなし・CRLFを検証。

## 実機で未確認の項目

逆回転の視認性、密着時の難易度、予兆と防御の余裕、各閾値の実戦発動、JG後の連撃、規定回数後のスタン、最終スタンからの決着、画面内への収まり、金髪/剣/盾の読みやすさ、泥の覆い方、2D固定表示、BGM/SE、ゲームパッド、画面サイズ変更、TimeScaleゼロ、プレビュー反復/復元、タイトル復帰後の再開始は未確認。

確認手順はMainSceneで第2形態まで進め、HumanBossのHealth InspectorのDamage Customなどで順に25％・10％・5％を通過させる。各段階のBerserkStage/ActionsStarted/queued/usedをInspectorで観察する。5％では12行動後のスタンへ攻撃し、決着、クリア、新しい決定入力、タイトル、新規開始を通して確認する。大ダメージによる同時閾値通過、最後の行動へのJG、多段攻撃、ステージ端、TimeScaleゼロも別途確認する。

## 日本語コメントとTracy

閾値予約・回数加算・JG再入防止・死亡保留、同一帯情報の共有、Particle上限、時計同期、SDR撮影、プレビュー復元の意図を日本語で記載。新規主要処理は既存BS_PROFILE_SCOPEを使用。既存のTracy有効化方式は維持。

## Visual Studio登録

実配置はCode/Game/GameMode、Code/Game/AI/Human、Code/Editor/Window。既存の類似ファイルと同じFilterへ登録。既存ファイルの移動は行っていない。

## 変更ファイル

- Code/Game/GameMode/BossFinishDirector.h
- Code/Game/GameMode/BossFinishDirector.cpp
- Code/Editor/Window/BossFinishWindow.h
- Code/Editor/Window/BossFinishWindow.cpp
- Code/Game/AI/Human/HumanBossAIComponent.Berserk.cpp
- Code/Game/AI/Human/HumanBossAIComponent.h
- Code/Game/AI/Human/HumanBossAIComponent.cpp
- Code/Game/AI/Human/HumanBossActionComponent.h
- Code/Game/AI/Human/HumanBossActionComponent.cpp
- Code/Game/AI/Human/HumanBossActionComponent.Ranged.cpp
- Code/Engine/GamePlay/Core/Object/GameObject.h
- Code/Engine/GamePlay/Core/Object/GameObjectManager.h
- Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp
- Code/Engine/GamePlay/Combat/HealthComponent.h
- Code/Engine/GamePlay/Combat/HealthComponent.cpp
- Code/Engine/GamePlay/System/AttackSystem.cpp
- Code/Engine/GamePlay/System/JustGuardSystem.cpp
- Code/Engine/GamePlay/System/EffectManager.cpp
- Code/Engine/GamePlay/Scene/SceneBase.cpp
- Code/Game/GameMode/GameModeManager.cpp
- Code/Engine/Graphics/Core/RenderContext.h
- Code/Engine/Graphics/Core/GraphicsDeviceManager.cpp
- Code/Engine/Audio/AudioSource.h
- Code/Engine/Audio/AudioManager.h
- Code/Engine/Audio/AudioManager.cpp
- Data/Presentation/AnathemaBossFinish.json
- Data/VFX/AnathemaFinishCross.json
- Data/Particle/HumanBossGridElectricMud.json
- Data/Particle/AnathemaFinishMudBurst.json
- Data/Particle/AnathemaFinishMudSlough.json
- Data/HumanBoss/HumanBoss.json
- Data/Scenes/MainScene/Objects/HumanBoss.json
- Data/Scenes/HumanBossTest/Objects/HumanBoss.json
- Data/Prefabs/Phase2HumanBoss.json
- BIGShine.vcxproj
- BIGShine.vcxproj.filters
