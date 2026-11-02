# ポートフォリオ原稿の実装根拠

2026年10月27日時点のファイルを静的に確認。パスはプロジェクトルート基準。行番号は今後の変更で移動するため、関数名と併用する。

## 本人から確認した制作情報

- 正式タイトル：ANATHEMA～忌まわしき影～。最初の回答のANATEMAは本人の訂正によりANATHEMAへ統一。
- 制作期間：2026年5月～。終了時期は未確認。
- 個人制作。基盤コードや素材を含めた個別の担当範囲・出典は未確認。

## 根拠一覧

| 掲載内容 | ファイル・確認箇所 | 確認できたこと |
| --- | --- | --- |
| Direct3D 11 | Code/Engine/Graphics/Core/GraphicsDeviceManager.h：include、GetDevice、GetContext | d3d11.hとID3D11Device、ID3D11DeviceContextを使用 |
| Visual Studio | BIGShine.vcxproj：PlatformToolset | v143を指定。今回ビルドできることを保証する記述ではない |
| RootMotion補正の呼び出し | Code/Engine/Graphics/Resource/Animation/AnimationControllerComponent.cpp：約433～462行 | 正規化時間を計算してApplyRootMotionWarpを呼び、補正移動量をCharacterMovementまたはTransformへ渡す |
| 補正設定 | Code/Engine/GamePlay/Core/Component/MotionWarpComponent.h：MotionWarpPreset | 補正区間、対象の選択、位置・回転のモード、停止距離、各種上限を保持 |
| 補正本体 | Code/Engine/GamePlay/Core/Component/MotionWarpComponent.cpp：ApplyRootMotionWarp、約472行以降 | 有効設定、対象、区間、補正量を評価。明示対象要求には別の経路がある |
| 停止距離 | 同上：LimitRootMotionToStopDistance、約47行以降 | 水平面の移動線分と停止円の最初の交点を使って移動を制限 |
| ON/OFF比較 | 同上：DrawComparisonGui、約964行付近 | warpEnabledの切替と元・補正後差分の表示 |
| 設定保存 | 同上：Serialize、Deserialize、約1033行以降 | WarpEnabled、Presetsおよび補正パラメーターを保存・読込 |
| ボスの設定 | Code/Game/AI/Human/HumanBossMind.h：HumanBossIntent、HumanBossMindSettings | 行動目的、反応時間、予算、各反応コスト、読み違い確率などを定義 |
| 履歴と反応待ち | Code/Game/AI/Human/HumanBossAIComponent.Mind.cpp：UpdateMindMemory、約175行以降 | 反復回数を反応時間へ反映し、予算を条件付きで回復 |
| Intent選択 | 同上：SelectIntent、約292行以降 | 状況別に候補と重みを用意し、読み違い確率と重み付き抽選で目的を選ぶ |
| 具体的な行動への接続 | 同上：GetIntentActionModifier、およびCode/Game/AI/Human/HumanBossAIComponent.cpp：GetGraphActionWeight、TickBehaviorGraph | Intentに応じた補正を行動側へ反映。約1515行でactionWeightModifierを設定 |
| BehaviorGraphの抽選 | Code/Engine/GamePlay/AI/AIBehaviorGraph.cpp：TickComposite、約631行以降 | 選択可能な枝の基本重みへ実行時補正を掛け、非正の候補を除外 |
| グラフのデータ | Code/Engine/GamePlay/AI/AIBehaviorGraph.h：AIBehaviorNode、AIBehaviorContext、AIBehaviorGraph | Blackboard条件、Action、待機、実行状態、JSON入出力の構造 |
| Sequenceの下書き | Code/Editor/Workspace/CinematicSequenceEditor.cpp：Bind、Apply、約69～132行 | Serializeした下書きを編集し、元ComponentへDeserializeして適用 |
| 試写操作 | 同上：Start、Stop、Seek、Update | Managerの試写APIを呼ぶ。シークを予約し、条件を満たした時に実行 |
| Scene退避と復元 | Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp：BeginCinematicSequencePreview、StopCinematicSequencePreview、SeekCinematicSequencePreview、約312行以降 | 元Sceneを保持し、既存Editor Playの復元処理を利用 |
| シークの再評価 | 同上：UpdateCinematicSequencePreview、約369行以降 | FIXED_STEP = 1.0f / 60.0f。先頭からの再構築と固定刻み更新、再評価回数の制限 |
| エディターの時間表示 | Code/Editor/Workspace/CinematicSequenceEditor.cpp：DrawActionInspector、DrawLiveState、DrawTimeline | 登録Action、再生状態、タイムラインを扱う。補助説明はDocs/CinematicEditor.mdにも記載 |

## 確認した既存のTracy計測

新しい計測は追加していない。原稿では計測機能の存在と性能改善の実績を混同しない。

- MotionWarpComponent::ApplyRootMotionWarp：BS_PROFILE_SCOPE。
- HumanBossAIComponent::UpdateMind、UpdateMindMemory、SelectIntent：BS_PROFILE_SCOPE。
- AIBehaviorGraph::TickRandomSelector：BS_PROFILE_SCOPE。
- CinematicSequenceEditor::Update、LiveState、Timeline、Draw：BS_PROFILE_SCOPE。
- CinematicSequence::Begin、Update、Rebuild：BS_PROFILE_SCOPE。

## 保存済み画像

- Docs/K2Presentation/title.jpg：画面内にANATHEMAと忌まわしき影の表示を確認。
- Docs/K2Presentation/arena.jpg：プレイヤー、ドラゴン、体力と操作ガイドを含む戦闘画面を確認。
- Docs/K2Presentation/editor-01.jpg：戦闘画面。ファイル名だけではエディターの機能画像と判断しない。
- 画像は既存の保存データであり、現在のコードの実行結果として検証したものではない。

## 参考URL・本人のページ

以下はユーザーが指定した資料。Web取得は失敗し、ブラウザーもGoogle Sitesへのアクセス設定でブロックされたため、ページ内容やレイアウトは未確認。

- 参考：https://sites.google.com/view/namikoshireiya/就職作品
- 参考：https://sites.google.com/view/tanabeportfolio/就職作品/制作詳細
- 参考：https://sites.google.com/view/seiyawada/個人制作
- 参考：https://sites.google.com/view/ogasone/就職作品
- 本人：https://sites.google.com/view/harukimomota/就職作品/就職作品について
- 編集URLは会話内で共有済み。編集画面にはアクセスしていない。

## 未確認事項と表現の制限

- 実装の存在を本人の担当実績・完成度・動作保証へ自動的に置き換えない。
- フレームレート、処理時間、削減率、命中率、工数の改善値は測定していない。
- 「学習するAI」は掲載しない。今回確認したのは履歴とルールによる選択。
- RootMotion補正を経路探索やすべての障害物回避と同一視しない。
- シークは再評価方式であり、瞬時の状態復元とは説明しない。
- .vcxprojと.filtersは変更していない。追加したMarkdownをコンパイル対象へ登録する必要はない。
- Gitは現在の環境のパス解決で失敗したため、変更状況をGitで比較できていない。既存のソースファイルは編集していない。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
