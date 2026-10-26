BigShine Engine
Editor用 起動・操作ガイド
========================================

【用途】
シーン編集、ゲームの再生確認、エンジン機能の確認に使用します。


【Editorを開く】
Engine_Release / Engine_Debugでは、先読みとロード表示の後にMainSceneの編集画面を直接開きます。
起動時にF1を押す必要はありません。ゲームの再生はPlayボタンで開始してください。
既存のDebug / Release / DevelopmentEditor構成では、タイトル画面からF1でEditorへ進みます。
本編シーン内のF1はEditorの表示・非表示を切り替えます。

【再生と編集】
Play：編集シーンの再生を開始します。
Pause / Resume：再生を一時停止・再開します。
Stop：再生を停止し、編集状態へ戻します。
再生中のF2：ゲーム操作用のマウス捕捉を切り替えます。
再生中のEsc：マウスを解放し、Editorを操作できるようにします。
タイトル画面と本編ではF1・F2の用途が異なるため、画面内の案内も参照してください。

【保存】
SceneメニューのSave Sceneから保存します。
Load Sceneで編集対象を切り替えます。
Windowメニューから各Editorウィンドウを開けます。
編集内容を提出する場合は、必要なシーンや設定を保存してからGame_Releaseをビルドしてください。

【ゲームの提出】
Game_Release / x64をビルドし、Build/Gameの配布内容をZIPにしてください。
ANATHEMA.exe、Data、GamePackage.json、DLL、ゲーム用ReadMe.txtを同梱します。
Build/Game/Debugがある場合は提出物へ含めないでください。
Editor本体やプロジェクト一式は、エンジンの審査などで求められた場合に別途提出してください。
Editorも提出する場合はEngine_Releaseを使用し、ゲーム用フォルダーと分けて案内してください。
現在のBuild/Engine単体にはDataを同梱しないため、EXEだけを送っても動作しません。
Editor用EXEの隣にゲーム用GamePackage.jsonを置くとゲーム専用起動になるため、混在させないでください。

既存のScene > Export Game...は、Play開始時のシーンを配布用に書き出す別の機能です。
この機能では選択したEXEがGame.exeとして保存されます。
