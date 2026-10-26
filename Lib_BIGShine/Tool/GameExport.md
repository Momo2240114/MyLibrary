# 講評会用ゲームの書き出し

配布先へ渡す説明はプロジェクト直下のReadMe.txtです。
この文書は制作者向けの準備・書き出し手順です。

## 手順

1. 今回の変更を含むプロジェクトを、自分でShipping | x64でビルドします。
2. 編集用のDebugまたはDevelopmentEditor版も、今回の変更を含む状態で自分でビルドします。
3. 編集用EXEを起動し、タイトル画面でF1を押して編集画面へ入ります。配布したいシーンを開き、Playして内容を確認します。
4. エディター上部のScene > Export Game...を選択します。
5. Select built EXE and exportで手順1のShipping版EXEを指定します。EXEと同じ場所にランタイムDLLをそろえてください。
6. プロジェクトのExports/ReviewGame_年月日_時分秒_ミリ秒/にゲーム一式が作成されます。
7. フォルダー全体を別の場所へコピーし、自分で起動・操作・音・戦闘進行を確認してから配布します。

書き出し処理自体はビルドもゲーム起動も行いません。
Scene読み込み中は書き出しできません。Data全体をコピーするため時間と空き容量が必要です。
画面にExport completeが出てからコピーしてください。

## 書き出す内容

- 選択したビルド済みEXEをGame.exeとして複製。
- Data全体（モデル、テクスチャ、音、コンパイル済みShader、Scene、Actionなど）。
- 選択したEXEと同じフォルダーのDLL。
- 必須DLLが不足している場合はプロジェクト直下から補完。
- ReadMe.txtと、エディターで現在使っているKeyConfig設定。
- GamePackage.json（ゲーム専用起動と開始Scene名）。

Play中／Pause中は、Play開始直前に退避した編集Sceneを使います。
Play中の移動位置、残HP、撃破状態、再生時刻のセーブ・再開ではありません。
Play前に未保存だった配置・設定もスナップショットから反映します。
Play中に変更した装備やScene設定を反映したい場合は、Stop後に編集してから再度Playしてください。
Playしていない場合は現在の編集Sceneを使います。

スキル・アニメーション・VFXなど別Assetの編集内容は、各エディターで保存してから書き出してください。
Scene内で参照するAssetはData配下へ置いてください。
Data外の独自Assetの依存関係は自動解決しません。シンボリックリンクは書き出しを中止します。
ReadMeの操作表は標準設定です。入力や装備を変えた場合はReadMeも合わせて更新してください。

## ゲーム専用起動

EXEと同じ場所のGamePackage.jsonを読み、エディターWindowを生成せず、
タイトル画面を表示し、GAME STARTのGoSceneGame Actionからロード画面を経由して、
指定Sceneを読み込んで通常のBeginGameSceneFlowを開始します。
HUD・ゲーム内GUI・ロード表示は維持します。
F1でエディターが再表示されることはありません。
Escで終了し、Alt + Enterで全画面を切り替えます。

起動時に作業ディレクトリをEXEの場所へ変更するため、
Visual Studioやプロジェクトの作業ディレクトリに依存しません。
設定のないDebug／Release／DevelopmentEditor起動は従来のエディター動作を維持します。
Shippingは設定がなくてもゲーム専用動作になり、MainSceneから開始します。
設定のないShippingをビルド出力フォルダーから直接起動する場合、EXEの隣にDataがなければ、
作業ディレクトリ、続いてEXEの親階層からBIGShine.vcxprojとDataがあるプロジェクト直下を探します。
GamePackage.jsonがある配布版はEXEの隣のDataだけを使用するため、配布時はフォルダー全体をコピーしてください。
入力設定が見つからない、または読み取れない場合は、エラーに対象ファイルの絶対パスを表示します。

Release版EXEも使用できますが、講評会へ渡すEXEはShipping版を使用してください。
開発用のコードをEXEから完全除去する変更は行っていません。
Shippingの既存マクロと、実行時のWindow生成・表示抑止を組み合わせています。
古いEXEには今回の起動設定の読み取りがないため、必ず変更後のEXEを選択してください。
Debug版EXEはデバッグ用ランタイムに依存するため配布用に指定しないでください。

## DLL・配布確認

必須DLLとしてSDL3、FMOD、PhysX、PhysXCommon、PhysXCooking、PhysXFoundationを検査します。
追加DLLが必要な場合は、選択するEXEと同じフォルダーへ配置してください。
WindowsやMicrosoft Visual C++のランタイムはこの処理ではインストールしません。
配布先PCでの起動確認は別途必要です。DLLの構成・版は使用したビルドと合わせてください。

既存の配布物と元データは上書きしません。
失敗時には未完了フォルダーの場所と理由を表示し、成功用の起動設定は最後に作成します。
未完了フォルダーは配布しないでください。

## 静的確認と未実施事項

宣言・定義、既存ロード／Play開始経路、KeyConfigの保存形式、
Visual Studioへの実ファイル登録とFilter登録、UTF-8 BOMなし・CRLFを確認しました。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。


## 変更ファイル

追加:
- ReadMe.txt
- Tool/GameExport.md
- Code/Engine/Core/GameRuntimeSettings.h
- Code/Engine/Core/GameRuntimeSettings.cpp
- Code/Editor/GamePackageExporter.h
- Code/Editor/GamePackageExporter.cpp

変更:
- Code/Engine/Core/main.cpp
- Code/Engine/Core/framework.cpp
- Code/Engine/GamePlay/Scene/SceneBase.cpp
- Code/Engine/GamePlay/Scene/SceneGame.cpp
- Code/Engine/GamePlay/Core/Object/GameObjectManager.h
- Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp
- Code/Engine/GamePlay/Input/MouseInput.cpp
- Code/Engine/Graphics/RenderSystem/ImGuiRenderer.cpp
- Code/Editor/Window/EditorWindowManager.h
- Code/Editor/Window/EditorWindowManager.cpp
- BIGShine.vcxproj
- BIGShine.vcxproj.filters

新規C++ファイルは上記の大まかな実フォルダーに配置しました。
4ファイルを.vcxprojに登録し、.vcxproj.filtersには既存の
Code/Engine/Core、Code/Editorへそれぞれ登録しました。
実ファイルの移動はありません。

## コメント・計測

ゲーム専用起動、入力の読み込み、Play開始前のScene取得、
配布データの保存、エディター表示抑止に日本語コメントを追加しました。

既存のBS_PROFILE_SCOPEを使用し、起動設定の読み込み、
Sceneスナップショット取得、配布データのコピー、書き出し、
書き出し画面の描画を計測します。Tracyの有効化方式は変更していません。
