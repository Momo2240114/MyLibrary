# ANATHEMA — 忌まわしき影：ローディング基盤

## 1. 調査した既存構造

- SceneManagerはNowSceneとNextSceneをunique_ptrで所有し、フレーム境界でFinalize・破棄・Initializeを行う。
- 既存SceneLoadingは別スレッドから次SceneのInitialize全体を呼び、IsReadyだけで遷移していた。GameObjectManagerのRUNNINGや失敗とは別の完了判定だった。
- GameObjectManagerの通常LoadSceneはSceneList・Object JSONを同期読込し、Componentを一括復元する。その後はLOADING / INITIALIZING / RUNNING / FAILEDでモデル待ちとStartを管理する。
- ModelResourceManagerには既にCPUワーカー、パス正規化、重複要求の統合、Cerealキャッシュ、GPUアップロードキュー、失敗付きModelLoadHandleがある。
- GraphicsDeviceManagerはDeviceとImmediateContext、Scene用RenderTarget、PostEffectを共有する。frameworkはUpdate・Scene Render・ImGui・Presentをメインループで実行する。
- RenderAllはそのManagerのObjectからCamera・Light・Model・MeshVFX・Particleを集め、既存のPostEffect・Sprite経路へ渡す。ライト情報は描画呼出しごとのローカル値。
- AnimationControllerComponentは既存のModelAnimator・Graph・State・CrossFadeでスキニングとRootMotionを更新する。
- ParticleComponentが既存ParticleEmitterのCompute更新を行い、ParticleRenderSystemが既存の描画経路へ投入する。EmitterとGPU資源はComponentが所有する。
- TextureResourceManager、CPUResourceLoader、Shader初期化、ComponentのDeserialize／Startにはメインスレッドやグローバルキャッシュを前提とした処理が残っている。

## 2. 発見した問題と対処

- Initialize全体のワーカー実行は、ImmediateContext・ImGui・ActionRegistry・EventManager・Physicsなどを描画側と並行して操作する危険がある。Scene全体を扱う旧LoadingThreadを置き換えた。
- SceneLoadingはSceneBaseとは別に同名の描画Systemメンバを持ち、二重初期化・二重所有があった。ロード画面では継承元の既存Systemを使う。
- 旧LoadingSceneの配置はStageや岩壁など通常Sceneに近い内容だった。元データは保持し、小さなLoadingPresentationを別途追加した。
- GPUアップロードの予算判定はモデル全体の作成後だった。Material・Meshごとのカーソルを持ち、途中から再開できるようにした。
- 既存の全モデル履歴に基づく進捗は、現在のScene要求と一致しない。今回の段階ロードでは読込Object数・生成Object数・Resource準備完了数を使う。
- ロード失敗を完了判定へ伝える経路が不足していた。状態・エラーの表示、Retry、終了、Play時のEditor復帰を追加した。

## 3. 状態と所有権

新しいLoadingManagerは作成しない。既存SceneLoadingが要求とLoadingStatusを所有する。

PREPARING → LOADING → FINALIZING → COMPLETEDを進め、各段階の例外・Resource失敗はFAILEDへ集約する。ロード中でない時のSceneManager::GetLoadingStatusはIDLEを返す。

SceneLoadingは表示用のGameObjectManagerと次Sceneのunique_ptrを持つ。通常の遷移元Sceneを表示のために残す必要はない。COMPLETEDでも所有権を直ちに渡さず、表示保持時間またはRequestTransitionを待つ。自動遷移を無効にした場合、将来の消滅・暗転演出からRequestTransitionを呼べる。

GameModeManagerにはロード状態を追加しない。次Sceneのゲーム開始・BossIntroなどは従来のGameMode経路を使い、ロード中は通常ゲームUpdateを呼ばない。

## 4. 非同期処理

追加したSceneLoadRequestのjthreadはSceneListとObject JSONのファイル読込・解析だけを行う。ワーカーへSceneやManagerのthis、GameObject、Device、Contextを渡さない。

結果JSONとエラーはmutexで受け渡し、完了と件数はatomicで公開する。ワーカーの標準例外・未知例外とも失敗結果を公開する。破棄時はstop要求後にjoinする。キャンセルの判定はファイル境界であり、処理中の単一ファイル読込・JSON解析を強制中断する方式ではない。

glTFとCerealのCPU構築は既存ModelResourceManagerのワーカーを継続利用する。別のモデル読込システムは作らない。

## 5. メインスレッドに残す処理

- GameObjectとComponentの生成、Deserialize、Start、Hierarchy復元。
- ModelのGPU資源作成、Textureのデコード／GPU作成、既存キャッシュへの登録。
- Animation Graph・Stateの既存復元、スキニング更新。
- Particle Compute、MeshVFX、描画コマンド、PostEffect、Present。
- ActionRegistryの切替、Physicsの初期状態確定、Sceneの受け渡し。

Object生成、Start、GPUアップロードには処理単位間の予算判定を設ける。予算は厳密な最大フレーム時間ではない。単一のComponent復元、Texture作成、Collider初期化など、既存の分割できない処理が予算を超える可能性は残る。重い箇所はTracy実測後、処理固有の追加分割を検討する。

## 6. ロード画面の更新・描画

LoadingPresentationはCamera・LoadingPlayer・LoadingLightだけを持つ。戦闘・AI・入力・当たり判定Componentは配置しない。UpdateLoadingPresentationは表示Componentを更新し、戦闘Systemや通常ゲームの時間管理・Physicsを進めない。

Camera方式を採用した。既存RenderAllがCameraとObject単位のLight収集に対応しており、新しいRenderTarget・Deviceを作らずスキニング・Particle・PostEffect・Sprite・Textを再利用できるため。Cameraとプレイヤー配置は通常ゲームから分離されている。

専用RenderTargetをSpriteとして合成する方式は、今後通常画面との同時表示や厳密な画素単位配置が必要になった時の拡張候補とする。現段階ではScene全体がロード画面であり、追加合成パスは作らない。PostEffect用の基盤・設定は既存GraphicsDeviceManagerを共有する。

## 7. モデル・アニメーション・シルエット・Particle

LoadingPlayerは既存プレイヤーモデルを使用する。実在するクリップSS_SP_Run（5）とSS_SP_Walk（11）をLoadingRun／LoadingWalkへ設定し、ループとCrossFadeを既存Controllerで行う。RootMotionの実移動は無効、見た目の移動除去を有効にしている。Animation Eventは空にし、通常戦闘のActionをロード画面から発火しない。

実行時の白／黒シルエット上書きはGltfModelのインスタンスだけに保持する。既存GBufferのスキニングとUnlit分岐を使い、白テクスチャと指定色を渡す。共有Material、通常ToonPBR設定、保存JSONを変更しない。専用の新規Shaderは追加しない。最終的な輪郭・Dissolve・色調整は未実装。

LoadingLightは既存GPU Particleの小規模な連続発生設定であり、Particleシステムそのものは追加しない。最終的な旋回・螺旋・Run／Walk連動・白赤切替は次段階の調整対象。

## 8. 進捗API

UIは主スレッドからSceneManager::GetLoadingStatus()を値で取得する。

- state：現在状態。
- progress：0～1。
- operation：処理名。
- error：失敗理由。
- IsCompleted()／HasFailed()：結果判定。

次Sceneの段階ロードはJSON読込20%、Object生成40%、Effect先読み10%、Resource／Component準備25%、最終確定5%として扱う。各部分は実際に完了した件数を基準にする。時間経過だけで進捗を増やさない。完了後の表示保持時間は進捗率とは別に管理する。

## 9. 安全性と互換性

- 同一フレームやロード中の重複SceneLoading要求を拒否。
- 元の同期LoadSceneとEditorスナップショット復元は維持し、SceneLoading経由だけLoadSceneAsyncを使用。
- 次SceneのInitializeとObject／GPU操作は主スレッドで実施。
- ロード画面と次SceneのAction登録先はActivateContextで主スレッド上で切り替える。
- 失敗した次SceneはRetry時にFinalizeしてから再準備する。
- モデルCPUワーカーは既存Managerが所有し、Scene破棄後もSceneへの参照を持たない。
- Scene読込ワーカーはCPUの値だけを所有し、Scene解放前に停止する。
- Deviceは追加せず、終了時の既存ResourceManager停止とGraphics破棄順を維持する。

## 10. 今後の演出と未検証事項

完了時のAnimation停止、Particle消滅、Dissolve、暗転、遷移後の暗転解除は未実装。SetAutomaticTransition(false)とRequestTransitionが接続点になる。ActionSequenceの重複システムは作らず、必要になった時だけ既存Sequenceと接続する。

モデルの画面内サイズ・足元位置、RootMotion除去の見え方、白／黒シルエット、Particleの見た目、異なる解像度、実機での連続描画・負荷・Pause／Stop・Retryは画面での確認対象。

## 11. 検証記録

今回の依頼文はビルド実行を明示しているため、従来のビルド禁止ルールの例外としてビルドを実施する。
最初のDevelopmentEditorビルドで、追加コードのC++20 u8path非推奨警告を検出した。警告抑制は行わず、C++20のUTF-8 path生成へ修正した。
最終結果は以下の「最終ビルド・起動確認」に記録した。


## 変更ファイルとVisual Studio登録

追加コード:
- Code/Engine/GamePlay/Scene/LoadingTypes.h
- Code/Engine/GamePlay/Scene/SceneLoadRequest.h
- Code/Engine/GamePlay/Scene/SceneLoadRequest.cpp

上記は既存のScene実フォルダーに配置し、.vcxprojと.vcxproj.filtersのCode/Engine/GamePlay/Sceneへ別々に登録した。

変更コード:
- Code/Engine/GamePlay/Scene/SceneLoading.h / .cpp
- Code/Engine/GamePlay/Scene/SceneBase.h / .cpp
- Code/Engine/GamePlay/Scene/SceneGame.cpp
- Code/Engine/GamePlay/Scene/SceneManager.h / .cpp
- Code/Engine/GamePlay/Core/Object/GameObjectManager.h / .cpp
- Code/Engine/GamePlay/System/EffectManager.h / .cpp
- Code/Engine/Graphics/Resource/ModelLoadTypes.h
- Code/Engine/Graphics/Resource/ModelResourceManager.h / .cpp
- Code/Engine/Graphics/Resource/GlTFModel.h
- Code/Engine/Graphics/RenderSystem/ModelRenderSystem.cpp
- BIGShine.vcxproj
- BIGShine.vcxproj.filters

追加データ:
- Data/Scenes/LoadingPresentation/SceneList.json
- Data/Scenes/LoadingPresentation/Objects/LoadingCamera.json
- Data/Scenes/LoadingPresentation/Objects/LoadingPlayer.json
- Data/Scenes/LoadingPresentation/Objects/LoadingLight.json
- Data/Particle/LoadingLight.json

説明:
- ReadMe.txt：作品名をANATHEMA — 忌まわしき影へ更新。
- Tool/LoadingFoundationReport.md：調査・設計・制約・検証記録。

実ファイルは移動していない。元のLoadingSceneデータ、通常のScene読込・保存、Editor GUI、既存Action／Event登録、ゲームプレイ用Assetは保持した。
日本語コメントはスレッド間の役割、要求の寿命、状態切替、確定処理の順序、シルエット上書きの目的へ追加した。
新規の主要処理には既存のBS_PROFILE_SCOPEを使用し、Tracyの有効化方法は変更していない。
変更対象28ファイルを厳密なUTF-8 BOMなし・CRLFとして確認した。

## 静的確認の結果

- 新規3コードファイルの.vcxproj登録とFilter登録がそれぞれ1件であることを確認。
- LoadingPresentationの全ObjectListに対応するJSONが存在することを確認。
- 専用SceneにPlayerController、AI、ActionSequence、CharacterColliderを含めていないことを確認。
- 実モデルのAnimation 5がSS_SP_Run、11がSS_SP_Walkであることを確認。
- 両Stateのループ、RootMotion実移動の無効化、見た目の移動除去、空のイベント表を確認。
- GPU Particleは既存ParticleComponentを使用し、128個の設定と参照Textureの存在を確認。
- 非同期Scene復元によるResetBattleStateの後、ComponentのStartより前にBeginGameSceneFlowを要求する経路を確認。
- 全体進捗の入力は現在要求の完了件数であり、表示保持時間と分離されていることを確認。


## Effect先読みと破棄処理の補足

EffectManager::Initializeは従来、全Effect AssetのキャッシュとPool生成を同期で実行していた。
SceneLoadingの表示側ではこれを実行せず、次Sceneでは既存のPreloadParticleAsset／PreloadMeshVFXAsset／PreloadVFXSequenceAsset／PreloadImpactEffectAssetをAsset単位で進める。
先読み対象とPoolの内容は既存のものを使い、別のEffectシステムへ置き換えていない。

SceneBase::FinalizeからはGameObjectManager::Clear(false)を呼び、破棄するSceneのPoolを再生成しない。
Sceneの再読込で使うClear()の既定動作は維持する。
RetryでSceneを再初期化するため、EditorWindowは参照先Managerより先に解放する。

## CPU読込の実行確認

tmp/LoadingCpuChecksに、実際のSceneLoadRequest.cppを使う独立した確認用プロジェクトを作成した。
描画PCHとTracyに依存しない構成で、次を実行してすべてPASSした。

- 正常なSceneListとObject JSONの読込。
- Objectファイルが存在しない場合の失敗結果。
- JSON構文が不正な場合の失敗結果。
- SceneListが存在しない場合の失敗結果。
- パス区切りを含むScene名の拒否。
- 日本語Scene名・Object名のUTF-8パス。
- 作成直後の要求破棄とキャンセルを32回繰り返す経路。

最終確認の出力はtmp/loading_cpu_checks_result.log、確認用のビルド出力はtmp/loading_cpu_checks_build_final.log。
最初の試行ではPCH付きObjectの単独リンク依存とMSPDB140.DLL不整合を検出したため、実装ソースを直接使うCPU専用構成とMSVC x64ツールの明示指定へ切り替えた。
製品側のPCH・Tracyの有効化方法は変更していない。


参照寿命の追加確認として、RenderAllの開始時に、そのManagerが所有するMotionVelocity描画SystemをPostEffectへ設定する。
次Sceneの再試行・破棄後に、共有PostEffectが古い描画Systemへアクセスしないための処理である。
ロード画面のRenderAllにはLoadingCameraを直接渡し、通常CameraManagerの補間から表示位置を分離する。
Pause中は表示Componentの更新とAnimation切替の両方を止める。


## 最終ビルド・起動確認

- 対象：BIGShine.vcxproj、DevelopmentEditor | x64。
- MSBuildのx64版とMSVC 14.44のHostx64ツールを指定して実施。
- 本体のコンパイル・リンク、および最終調整後の再コンパイル・再リンクは終了コード0で成功。
- 本体全体のログ：tmp/loading_foundation_build_x64.log。
- 最終ソースの再リンクまで確認したログ：tmp/loading_foundation_build_final_verified.log。
- C++のコンパイルエラーとC4828は最終確認ログにない。
- 初期の全体ビルドには既存Shader出力のfailed writingメッセージがある。今回Shaderソースは変更していない。Shader再生成の完全性・最終描画は別途確認が必要。
- CPU読込の確認用プログラムは最新のSceneLoadRequest.cppで再ビルド・再実行し、全項目PASS。

最新EXEをtmpの隔離フォルダーへコピーし、ゲーム専用起動設定で非表示の起動確認を20秒実施した。
Dataは既存資源を参照し、Editorレイアウト・元の配布設定は変更していない。
プロセスの早期終了はなく、標準エラーは空だった。標準出力で遷移先のGameObject／Component復元が進むことを確認した。

確認用プロセスはテスト終了時にそのPIDだけを終了した。非表示プロセスのため通常のWindow終了は使用できず、強制終了している。
したがって、この起動確認は正常なゲーム終了・Scene破棄の実行確認には数えない。
結果：tmp/loading_runtime_smoke_result.json。
実行用の補助スクリプト：tmp/RunLoadingRuntimeSmoke.ps1。

画面の視認によるモデル位置、スキニング、CrossFade、白黒表示、Particle、PostEffect、読み込み完了後の遷移、Retry、Pause／Stopは未検証。
Shipping構成の全体ビルドも今回未実施。
CPU読込の破棄／キャンセルは別途、実装ソースを使った確認プログラムで32回通している。

## 実機での確認手順

1. 通常起動またはSceneLoading経由でSceneGameを開始し、ロード画面の3DモデルとParticleを確認する。
2. LoadingRunとLoadingWalkの切り替え、RootMotionで画面外へ移動しないこと、解像度変更時の位置を確認する。
3. SetAutomaticTransition(false)で完了表示を保持し、GetLoadingStatusの状態・進捗・処理名を確認する。
4. RequestTransitionを呼び、通常のゲーム開始・BossIntro・HUD・入力を確認する。
5. 配布物のコピーでファイル欠落を作り、FAILED表示と復元後のRetryを確認する。開発中の元データは検証用に移動しない。
6. EditorのPlay中にPause／Resume／Stopを操作し、復元後の編集内容と再ロードを確認する。
7. Tracyで単一のTexture・Component・Collider・Effect Asset準備の時間を測り、必要な箇所だけ追加分割する。

SceneGameは準備フラグに応じてLoadSceneAsyncを使うよう接続済み。
今後ほかの独自Scene派生クラスをSceneLoadingへ渡す場合も、そのInitializeで準備フラグに応じたLoadSceneAsync呼び出しを行う。
独自Initialize内の同期処理を無条件に別スレッドへ移す仕組みではない。
