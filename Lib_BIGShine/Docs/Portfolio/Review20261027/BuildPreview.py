"""掲載原稿から、既存画像を使ったローカル確認用HTMLと説明図を作成する。"""
from pathlib import Path
from html import escape
import hashlib
import json
import re
import shutil
from PIL import Image, ImageDraw, ImageFont

OUTPUT = Path(__file__).resolve().parent
ROOT = OUTPUT.parents[2]
ASSETS = OUTPUT / 'assets'
ASSETS.mkdir(exist_ok=True)
UTF8 = 'utf-8'


def SaveText(path_, text_):
    """資料の改行と文字コードをプロジェクトの規則へ合わせて保存する。"""
    path_.write_bytes(text_.replace('\r\n', '\n').replace('\r', '\n').replace('\n', '\r\n').encode(UTF8))


def Inline(text_):
    """原稿に使う最小限のMarkdownをHTMLへ変換する。"""
    text_ = escape(text_)
    text_ = re.sub(r'\[([^\]]+)\]\((https?://[^ )]+)\)', r'<a href="\2">\1</a>', text_)
    text_ = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', text_)
    text_ = re.sub(r'`([^`]+)`', r'<code>\1</code>', text_)
    return text_


def Markdown(text_):
    """原稿の段落・見出し・表を読みやすい確認画面へ整える。"""
    result_ = []
    lines_ = text_.strip().splitlines()
    index_ = 0
    while index_ < len(lines_):
        line_ = lines_[index_].strip()
        if not line_:
            index_ += 1
            continue
        if line_.startswith('|'):
            rows_ = []
            while index_ < len(lines_) and lines_[index_].strip().startswith('|'):
                row_ = [item_.strip() for item_ in lines_[index_].strip().strip('|').split('|')]
                if not all(re.fullmatch(r'[-: ]+', item_) for item_ in row_):
                    rows_.append(row_)
                index_ += 1
            result_.append('<div class="table-wrap"><table>')
            for rowIndex_, row_ in enumerate(rows_):
                tag_ = 'th' if rowIndex_ == 0 else 'td'
                result_.append('<tr>' + ''.join(f'<{tag_}>{Inline(cell_)}</{tag_}>' for cell_ in row_) + '</tr>')
            result_.append('</table></div>')
            continue
        match_ = re.match(r'^(#{1,6})\s+(.+)', line_)
        if match_:
            level_ = min(len(match_.group(1)) + 1, 4)
            result_.append(f'<h{level_}>{Inline(match_.group(2))}</h{level_}>')
        elif line_.startswith('- '):
            result_.append('<p class="list-item">• ' + Inline(line_[2:]) + '</p>')
        else:
            result_.append('<p>' + Inline(line_) + '</p>')
        index_ += 1
    return '\n'.join(result_)


MANUSCRIPT = (OUTPUT / '02_掲載用完成原稿.md').read_text(UTF8)


def Section(key_):
    """掲載本文だけを切り出し、編集指示がプレビュー本文へ入ることを防ぐ。"""
    match_ = re.search(r'^### ' + re.escape(key_) + r' 掲載本文[^\n]*\n(.*?)(?=^### |^## |\Z)', MANUSCRIPT, re.M | re.S)
    if not match_:
        raise ValueError(key_)
    return re.split(r'掲載本文ここまで。|\n編集指定：', match_.group(1))[0].strip()


def Figure(name_, caption_, alt_):
    """実際の保存画像を出典とキャプション付きで配置する。"""
    return f'<figure><img src="assets/{name_}" alt="{escape(alt_)}" loading="lazy"><figcaption>{escape(caption_)}</figcaption></figure>'


def Diagram(name_, alt_):
    """概要図と同内容の代替テキストを併記する。"""
    return f'<figure class="diagram"><img src="assets/{name_}.svg" alt="{escape(alt_)}"><figcaption>{escape(alt_)}</figcaption></figure>'


def Video():
    """既存の紹介映像は収録日付きの外部リンクとして示す。"""
    return '<div class="video"><span class="eyebrow">既存の紹介映像</span><h3>紹介動画｜2026年7月27日時点</h3><p>映像の収録時点と、現在の実装説明は異なります。</p><a class="button" href="https://www.youtube.com/watch?v=OQk6XoauzAg" target="_blank" rel="noopener">YouTubeで紹介動画を見る ↗</a><p class="small">Google Sites反映時は、この位置へ同じ動画を埋め込みます。</p></div>'


def Pending(text_):
    """未収録素材を、完成した画像や映像と取り違えない表示にする。"""
    return f'<aside class="pending"><strong>追加予定・未収録</strong><p>{escape(text_)}</p></aside>'


CSS = '''
:root{--bg:#0e141b;--panel:#18232e;--text:#eef2f5;--muted:#b9c6d0;--accent:#8de2d0;--line:#344553}
*{box-sizing:border-box}html{scroll-behavior:smooth}body{margin:0;background:var(--bg);color:var(--text);font-family:"Yu Gothic","Meiryo",sans-serif;line-height:1.85;font-size:16px}a{color:var(--accent);text-underline-offset:4px}a:hover{color:white}header{border-bottom:1px solid var(--line);background:#101922}nav{max-width:1140px;margin:auto;padding:20px 24px;display:flex;flex-wrap:wrap;gap:12px 28px;align-items:center}nav .brand{margin-right:auto;color:white;font-weight:bold;text-decoration:none}nav a{text-decoration:none}main{max-width:1140px;margin:auto;padding:0 24px 80px}.notice{padding:10px 24px;background:#283323;color:#e2eddc;text-align:center;font-size:13px}.hero{padding:64px 0 26px}.eyebrow{color:var(--accent);font-size:13px;letter-spacing:.12em}.hero h1{font-size:clamp(36px,6vw,66px);line-height:1.15;letter-spacing:.02em;margin:16px 0}.subtitle{font-size:22px;color:var(--muted)}.lead{max-width:780px;font-size:20px}.actions{display:flex;flex-wrap:wrap;gap:12px;margin:24px 0}.button{display:inline-block;border:1px solid var(--accent);background:var(--accent);color:#10221e;padding:10px 22px;border-radius:4px;text-decoration:none;font-weight:bold}.button.secondary{background:transparent;color:var(--accent)}.button:hover{background:white;color:#10221e}section{padding:34px 0;border-top:1px solid var(--line);scroll-margin-top:20px}h2{font-size:clamp(25px,3vw,34px);line-height:1.5;margin:8px 0 24px}h3{font-size:21px;line-height:1.6}h4{color:var(--accent);font-size:18px;margin-bottom:8px}p{max-width:830px;margin:12px 0 18px}figure{margin:20px 0 28px}figure img{display:block;width:100%;height:auto;border-radius:3px}figcaption,.small{font-size:13px;color:var(--muted);line-height:1.7;margin-top:10px}.grid{display:grid;grid-template-columns:1fr 1fr;gap:26px}.cards{display:grid;grid-template-columns:1fr 1fr;gap:18px}.card{padding:24px;background:var(--panel);border:1px solid var(--line);border-radius:5px}.card h3{margin-top:5px}.video{background:#182c32;padding:30px;border-left:4px solid var(--accent);margin:20px 0 30px}.pending{background:#242b30;border:1px dashed #8f9f9c;padding:18px 24px;color:#ccd5d6;margin:24px 0;font-size:14px}.pending p{margin:6px 0}.table-wrap{overflow:auto}table{width:100%;border-collapse:collapse;font-size:14px;margin:20px 0 30px}th,td{padding:12px 14px;text-align:left;vertical-align:top;border:1px solid var(--line);min-width:130px}th{background:var(--panel)}td code{word-break:break-all}.toc{display:flex;flex-wrap:wrap;gap:12px 24px;padding:18px 0}.diagram{background:#14232d;padding:16px}.diagram img{min-height:100px}.diagram figcaption{max-width:850px}details{margin-top:20px;border:1px solid var(--line);padding:18px 24px;background:var(--panel)}summary{cursor:pointer;font-weight:bold;color:var(--accent)}.details-body{padding-top:12px}.details-body p:nth-child(2n+1){max-width:850px}.tech-label{color:var(--accent);font-size:14px;font-weight:bold}footer{border-top:1px solid var(--line);padding:30px 24px;color:var(--muted);text-align:center;font-size:13px}.list-item{margin:8px 0}code{font-family:monospace;color:#c6e6de}.doc h2{margin-top:30px}.doc p{max-width:100%}.doc td{font-size:13px}.doc h1{font-size:32px}@media(max-width:700px){main{padding:0 18px 50px}nav{padding:15px 18px;gap:8px 18px;font-size:14px}.hero{padding-top:36px}.lead{font-size:18px}.grid,.cards{grid-template-columns:1fr}.card{padding:20px}.diagram{overflow:auto}.diagram img{min-width:850px}table{font-size:13px}.video{padding:20px}.hero h1{font-size:40px}section{padding:28px 0}}@media print{header,.notice,.pending{display:none}body{background:white;color:black}a{color:#164e47}.card,details,.video{background:#eee;color:black}main{max-width:none}section{break-inside:avoid}figcaption,.small{color:#555}}
'''


def Page(title_, body_, filename_, document_=False):
    """ローカルのみで閲覧する確認ページを保存する。"""
    class_ = 'doc' if document_ else ''
    html_ = f'''<!doctype html><html lang="ja"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>{escape(title_)}｜ANATHEMA 掲載案</title><style>{CSS}</style></head><body>
<div class="notice">未公開の構成プレビュー｜実画面は既存の保存画像｜［要入力］・未収録素材は公開前に確認</div>
<header><nav aria-label="ページ移動"><a class="brand" href="Preview.html">桃田 大輝</a><a href="Preview.html">ホーム案</a><a href="Work.html">作品紹介案</a><a href="Techniques.html">技術詳細案</a><a href="EditingGuide.html">編集手順</a></nav></header><main class="{class_}">{body_}</main>
<footer>ポートフォリオ編集用プレビュー／2026年10月27日作成。Google Sitesへの反映・公開は行っていません。</footer></body></html>'''
    SaveText(OUTPUT / filename_, html_)


# 画面の内容を変えず、実際の保存画像をローカルプレビューへコピーする。
for original_, destination_ in [('editor-11.jpg','hero.jpg'),('editor-03.jpg','dragon.jpg'),('editor-47.jpg','human.jpg'),('title.jpg','title.jpg')]:
    shutil.copy2(ROOT / 'Docs/K2Presentation' / original_, ASSETS / destination_)


# 説明図は単方向の概要に限定し、ゲーム画面の代用にしない。
DIAGRAMS = [
    ('input-flow', ['入力を解釈','行動を要求','受付と遷移を検証','即時遷移／1件予約','通知時に確定']),
    ('motion-warp-flow', ['対象と段の設定','予定RootMotion','再生中の差分','区間と上限で補正','移動制御へ']),
    ('ai-flow', ['プレイヤーを観測','履歴と待ち時間','行動目的を選択','Graphの条件と重み','具体的な行動']),
    ('editor-flow', ['下書きを編集','元Sceneを退避','本番経路で試写','必要時は再評価','停止して復元']),
]
FONT = ImageFont.truetype('C:/Windows/Fonts/YuGothM.ttc', 25)
for name_, labels_ in DIAGRAMS:
    parts_ = ['<svg xmlns="http://www.w3.org/2000/svg" width="1400" height="160" viewBox="0 0 1400 160" role="img">',f'<title>{escape(" → ".join(labels_))}</title>','<rect width="1400" height="160" fill="#14232d"/>']
    canvas_ = Image.new('RGB',(1400,160),'#14232d')
    draw_ = ImageDraw.Draw(canvas_)
    for index_, label_ in enumerate(labels_):
        x_ = 15 + index_ * 278
        parts_.append(f'<rect x="{x_}" y="36" width="246" height="82" rx="6" fill="#203a48" stroke="#8de2d0"/><text x="{x_+123}" y="85" text-anchor="middle" fill="#eef2f5" font-family="Yu Gothic,Meiryo,sans-serif" font-size="22">{escape(label_)}</text>')
        draw_.rounded_rectangle((x_,36,x_+246,118),radius=6,fill='#203a48',outline='#8de2d0',width=2)
        draw_.text((x_+123,77),label_,font=FONT,fill='#eef2f5',anchor='mm')
        if index_ < len(labels_)-1:
            parts_.append(f'<path d="M {x_+251} 77 h 19 m -7 -6 l 7 6 l -7 6" fill="none" stroke="#8de2d0" stroke-width="3"/>')
            draw_.line((x_+251,77,x_+270,77),fill='#8de2d0',width=3)
            draw_.polygon([(x_+270,77),(x_+263,71),(x_+263,83)],fill='#8de2d0')
    parts_.append('</svg>')
    SaveText(ASSETS / f'{name_}.svg','\n'.join(parts_))
    canvas_.save(ASSETS / f'{name_}.png')


HOME = '''<div class="hero"><span class="eyebrow">APPLICATION / IN-GAME PROGRAMMER</span><h1>ゲームの動作と、<br>調整する仕組みを作る。</h1><p class="subtitle">桃田 大輝｜アプリケーション・インゲームプログラマ志望</p><p class="lead">ゲームの操作と戦闘の仕組みを考え、調整に使うEditorまで制作しています。</p></div>
<section><span class="eyebrow">開発中の就職作品</span><h2>ANATHEMA — 忌まわしき影 —</h2><p class="lead">敵の攻撃を見極め、防御・回避の成功から反撃へつなぐ、1対1のボスバトルアクション。</p>'''
HOME += Figure('hero.jpg','ANATHEMAのドラゴン戦。保存済みの実ゲーム画面。','プレイヤーと大型ドラゴンが闘技場で対峙するゲーム画面')
HOME += '<div class="actions"><a class="button" href="Work.html">ゲームと実装の工夫を見る</a><a class="button secondary" href="#movie">紹介映像へ</a></div></section><section id="movie"><h2>紹介映像</h2>' + Video() + '</section>'
HOME += '<section><h2>プロフィール</h2>' + Markdown(Section('A3')) + '<h3>自己PR</h3>' + Markdown(Section('A4')) + '</section>'
HOME += '<section><h2>スキル</h2>' + Markdown(Section('A5')) + '</section><section><h2>過去制作・連絡先</h2>' + Markdown(Section('A6')) + '<a href="https://sites.google.com/view/harukimomota/過去制作">現在の過去制作ページを見る ↗</a></section>'
Page('ホーム', HOME, 'Preview.html')

WORK = '<div class="hero"><span class="eyebrow">1対1 ボスバトルアクション／開発中</span><h1>ANATHEMA</h1><p class="subtitle">— 忌まわしき影 —</p><h2>見極めた一瞬を、反撃のきっかけに。</h2><p class="lead">敵の攻撃を見極め、ジャストガード・ジャスト回避から反撃へつなぎます。C++とDirectX 11を使った自作フレームワークで開発しています。</p></div>'
WORK += Figure('hero.jpg','実際のゲーム画面。ゲームの仕組みと、調整を支えるEditorを制作しています。','プレイヤーとドラゴンの戦闘画面') + Video()
WORK += '<div class="toc"><a href="#overview">制作情報</a><a href="#concept">コンセプト</a><a href="#bosses">2つの形態</a><a href="#strengths">技術紹介</a><a href="#editor">制作支援</a><a href="#status">開発状況</a></div>'
WORK += '<section id="overview"><h2>制作情報と担当</h2>' + Markdown(Section('B2').split('| 項目')[1].join(['| 項目','']) if False else '| 項目' + Section('B2').split('| 項目',1)[1]) + '</section>'
WORK += '<section id="concept"><h2>守りの成功を、次の攻撃につなげる</h2>' + Markdown(Section('B3').split('\n\n',1)[1]) + '</section>'
WORK += '<section id="bosses"><h2>ドラゴンと人型、2つの形態</h2><div class="grid"><article><h3>第1形態｜ドラゴン</h3>' + Figure('dragon.jpg','大型の体と、床の攻撃範囲表示が見える戦闘場面。','ドラゴンが翼を広げ、プレイヤーの足元に赤い範囲表示が出ている') + '<p>噛みつき、尻尾の薙ぎ払い、ブレスなどを使う大型のボスです。距離と正面・側面・背後・胴体の下といった位置関係を判断に使います。</p></article><article><h3>第2形態｜人型ボス</h3>' + Figure('human.jpg','剣と盾を持つ相手との対峙を、プレイヤーと一緒に映す。','剣と盾を持つ人型ボスとプレイヤーが赤い闘技場で対峙している') + '<p>プレイヤーの攻撃やガード・回避の傾向を参照し、攻め、防御、間合いの調整などの行動目的を選ぶ構成です。</p></article></div>' + Pending('各形態15～25秒の戦闘映像を、この2枚の下に配置。予兆から防御・回避、反撃までを見せる。') + '</section>'

TITLES = ['入力の予約と行動遷移','スキルのMotionWarp','人型ボスの判断と行動','演出の編集と試写']
SUMMARIES = ['先行入力を保存する区間と、行動を切り替える受付を分け、アニメーションと遷移表で調整します。','スキルの段ごとに補正区間と上限を指定。予定RootMotionと対象の停止距離から移動を補正します。','観測と履歴から行動目的を選び、グラフの条件と重みに反映。反応待ちと防御のコストを調整します。','登録したActionの時刻を編集し、本番と同じ経路で試写。停止時は試写前のSceneへ戻します。']
WORK += '<section id="strengths"><h2>戦闘を支える3つの仕組み</h2><div class="cards">'
for index_ in range(3):
    WORK += f'<article class="card"><span class="tech-label">0{index_+1}</span><h3>{TITLES[index_]}</h3><p>{SUMMARIES[index_]}</p><a href="Techniques.html#tech{index_+1}">仕組みと比較素材を見る →</a></article>'
WORK += '</div></section><section id="editor"><h2>制作を支える演出Editor</h2><p>' + SUMMARIES[3] + '</p>' + Diagram('editor-flow','下書き編集 → Scene退避 → 本番経路で試写 → 再評価 → 停止・復元') + Pending('時刻変更→試写→シーク→停止・復元を30～45秒で撮影。既存のゲーム画面をEditor画像として流用しない。') + '<a href="Techniques.html#tech4">編集・試写の設計を読む →</a></section>'
WORK += '<section><h2>設計上の課題と対応</h2>' + Markdown(Section('B7')) + '</section><section id="status"><h2>開発状況・利用技術</h2>' + Markdown(Section('B8')) + '</section>'
Page('作品紹介',WORK,'Work.html')

TECH = '<div class="hero"><span class="eyebrow">DESIGN / IMPLEMENTATION</span><h1>戦闘と制作を支える、<br>4つの仕組み。</h1><p class="lead">操作や調整の課題と、実装した仕組みを対応付けて紹介します。</p><p>掲載用本文の詳細を開いて読めます。完成度・操作感・改善率は静的調査だけでは確定していません。</p></div><div class="toc">' + ''.join(f'<a href="#tech{i_+1}">0{i_+1} {title_}</a>' for i_,title_ in enumerate(TITLES)) + '</div>'
for index_, (name_,labels_) in enumerate(DIAGRAMS):
    TECH += f'<section id="tech{index_+1}"><span class="eyebrow">0{index_+1}</span><h2>{TITLES[index_]}</h2><p class="lead">{SUMMARIES[index_]}</p>'
    TECH += Diagram(name_, ' → '.join(labels_))
    TECH += Pending(['受付前／受付中の入力と、ジャスト成功から反撃を25～40秒で比較。','同じスキル・同じ開始条件でOFF左／ON右の比較を20～30秒。','ゲーム画面とIntent・反応予算・待ち時間を25～40秒で同時表示。','Action時刻変更→試写→シーク→停止・復元を30～45秒。'][index_])
    detail_ = Section(f'C{index_+1}')
    for label_ in ['目的','仕組み','工夫と方式の理由','効果','見せる素材']:
        detail_ = detail_.replace('\n\n'+label_+'\n\n','\n\n### '+label_+'\n\n')
    if detail_.startswith('目的\n'):
        detail_ = '### '+detail_
    TECH += '<details><summary>詳しい説明を読む</summary><div class="details-body">' + Markdown(detail_) + '</div></details></section>'
Page('技術詳細',TECH,'Techniques.html')

GUIDE = '<div class="hero"><span class="eyebrow">EDITORIAL GUIDE</span><h1>Google Sitesへの反映</h1><p class="lead">完成原稿と素材の対応を確認してから、既存ページへ貼り付けてください。</p></div>'
for filename_ in ['01_調査と編集方針.md','03_素材配置と撮影仕様.md','05_GoogleSites反映手順.md']:
    GUIDE += '<section>' + Markdown((OUTPUT / filename_).read_text(UTF8)) + '</section>'
Page('調査・編集手順',GUIDE,'EditingGuide.html',True)


# 掲載根拠のファイルと記述の存在、および使用JSONの解析を静的に確認する。
CHECKS = [
('Code/Game/Charcter/Player/PlayerInputResolver.cpp','PlayerInputResolver::ResolveSkillSlot'),
('Code/Game/Charcter/Player/PlayerControllerComponent.cpp','PlayerControllerComponent::RequestAction'),
('Code/Game/Charcter/Player/PlayerControllerComponent.cpp','PlayerControllerComponent::SetCanActionBranch'),
('Code/Game/Charcter/Player/PlayerControllerComponent.cpp','PlayerControllerComponent::NotifyComboTransition'),
('Code/Game/Charcter/Player/PlayerControllerComponent.cpp','PlayerControllerComponent::RegisterPlayerAnimationActions'),
('Code/Game/Charcter/Player/PlayerControllerComponent.cpp','PlayerControllerComponent::PromoteBufferedAttackToCounterAttack'),
('Code/Game/Charcter/Player/PlayerControllerComponent.cpp','PlayerControllerComponent::ResolveSkillMotionWarpTarget'),
('Code/Game/Charcter/Player/PlayerDodgeState.cpp','PlayerDodgeState::HandleJustDodgeSuccess'),
('Code/Engine/GamePlay/System/JustGuardSystem.cpp','JustGuardSystem::HandleJustGuard'),
('Code/Game/Charcter/Player/PlayerSkillState.cpp','PlayerSkillState::StartPhaseMotionWarp'),
('Code/Engine/GamePlay/Core/Component/MotionWarpComponent.cpp','LimitRootMotionToStopDistance'),
('Code/Engine/GamePlay/Core/Component/MotionWarpComponent.cpp','MotionWarpComponent::ApplyRootMotionWarp'),
('Code/Engine/GamePlay/Core/Component/MotionWarpComponent.cpp','MotionWarpComponent::DrawComparisonGui'),
('Code/Engine/Graphics/Resource/Animation/AnimationControllerComponent.cpp','motionWarp_->ApplyRootMotionWarp'),
('Code/Game/AI/Human/HumanBossAIComponent.Mind.cpp','HumanBossAIComponent::SelectIntent'),
('Code/Game/AI/Human/HumanBossAIComponent.Mind.cpp','HumanBossAIComponent::GetIntentActionModifier'),
('Code/Game/AI/Human/HumanBossAIComponent.cpp','HumanBossAIComponent::TickBehaviorGraph'),
('Code/Engine/GamePlay/AI/AIBehaviorGraph.cpp','AIBehaviorGraph::TickComposite'),
('Code/Editor/Workspace/CinematicSequenceEditor.cpp','CinematicSequenceEditor::Bind'),
('Code/Editor/Workspace/CinematicSequenceEditor.cpp','CinematicSequenceEditor::Apply'),
('Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp','GameObjectManager::BeginCinematicSequencePreview'),
('Code/Engine/GamePlay/Core/Object/GameObjectManager.cpp','GameObjectManager::UpdateCinematicSequencePreview'),
('Code/Game/AI/Dragon/DragonAIComponent.cpp','DragonAIComponent::CanUseTurnTailSweep'),
('Code/Game/Charcter/Player/PlayerStateMachine.cpp','PlayerStateMachine::ChangeState'),
('Code/Engine/Graphics/Core/GraphicsDeviceManager.h','ID3D11Device'),
('BIGShine.vcxproj','v143'),
]
verification_ = {'source':[], 'json':[], 'images':[], 'format':[], 'links':[], 'buildRun':False, 'gameRun':False, 'published':False}
for path_, keyword_ in CHECKS:
    source_ = (ROOT / path_).read_text(UTF8)
    matches_ = [index_+1 for index_,line_ in enumerate(source_.splitlines()) if keyword_ in line_]
    verification_['source'].append({'path':path_,'keyword':keyword_,'lines':matches_})
    if not matches_:
        raise ValueError((path_,keyword_))
for path_ in ['Data/Skills/PlayerSkills.json','Data/AI/BehaviorGraph/HumanBoss.json','Data/AI/BehaviorGraph/DragonFirstForm.json','Data/Scenes/MainScene/Objects/HumanBoss.json','Data/Scenes/MainScene/Objects/PlayerModel.json','Data/Scenes/MainScene/Objects/PhaseTransition.json','Data/Animation/StateData/Player/States/SS_Attack1.json','Data/Animation/StateData/Player/States/SS_Attack2.json','Data/Animation/StateData/Player/States/SS_Attack3.json','Data/Animation/StateData/Player/States/SS_Attack4.json']:
    data_ = json.loads((ROOT / path_).read_text(UTF8))
    verification_['json'].append({'path':path_,'parsed':True})
for original_, destination_ in [('editor-11.jpg','hero.jpg'),('editor-03.jpg','dragon.jpg'),('editor-47.jpg','human.jpg'),('title.jpg','title.jpg')]:
    identical_ = (ROOT/'Docs/K2Presentation'/original_).read_bytes() == (ASSETS/destination_).read_bytes()
    verification_['images'].append({'original':original_,'copy':destination_,'identical':identical_})
    assert identical_
for path_ in OUTPUT.rglob('*'):
    if path_.suffix in ['.md','.html','.svg','.py']:
        SaveText(path_,path_.read_text(UTF8))
        raw_ = path_.read_bytes()
        raw_.decode(UTF8,errors='strict')
        valid_ = not raw_.startswith(b'\xef\xbb\xbf') and b'\n' not in raw_.replace(b'\r\n',b'') and b'\r' not in raw_.replace(b'\r\n',b'')
        verification_['format'].append({'path':str(path_.relative_to(OUTPUT)), 'utf8NoBomCrlf':valid_})
        assert valid_
    if path_.suffix == '.html':
        html_ = path_.read_text(UTF8)
        for target_ in re.findall(r'(?:src|href)="([^\"]+)"',html_):
            if target_.startswith(('http:','https:','mailto:')):
                continue
            file_, _, anchor_ = target_.partition('#')
            targetPath_ = path_.parent/file_ if file_ else path_
            valid_ = targetPath_.exists()
            if anchor_ and valid_:
                valid_ = f'id="{anchor_}"' in targetPath_.read_text(UTF8)
            verification_['links'].append({'page':path_.name,'target':target_,'valid':valid_})
            assert valid_, (path_,target_)
SaveText(OUTPUT/'verification.json',json.dumps(verification_,ensure_ascii=False,indent=2))
print(json.dumps({'sourceChecks':len(CHECKS),'jsonParsed':len(verification_['json']),'imageCopies':len(verification_['images']),'formats':len(verification_['format']),'localLinks':len(verification_['links'])},ensure_ascii=False))
