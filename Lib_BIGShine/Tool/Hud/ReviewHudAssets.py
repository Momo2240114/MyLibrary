"""ゲームを起動せず、PNG・Scene参照・VS登録と解像度別の静的配置を確認する。"""
from pathlib import Path
import json
import math
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
SCENE = ROOT/'Data/Scenes/MainScene'
OUTPUT = ROOT/'Data/Sprite/UI/Hud'
FONT = ROOT/'Data/Font/NotoSansJP-Regular.otf'
if not FONT.exists():
    FONT = Path('C:/Windows/Fonts/YuGothM.ttc')


def Read(path_):
    """JSONを厳密なUTF-8として読む。"""
    return json.loads(path_.read_text(encoding='utf-8'))


def Data(object_, type_):
    """ComponentがないObjectも、空の設定として扱う。"""
    return next((c_['Data'] for c_ in object_.get('Components',[]) if c_['TypeName']==type_),{})


def Rect(name_, objects_, resolution_, cache_):
    """既存UIWidgetのAnchor計算を静的データ上で再現する。"""
    if name_ in cache_:
        return cache_[name_]
    object_ = objects_[name_]
    widget_ = Data(object_,'UIWidgetComponent')
    rect_ = widget_['RectTransform']
    parent_ = object_.get('ParentName')
    parentRect_ = Rect(parent_,objects_,resolution_,cache_) if parent_ in objects_ and Data(objects_[parent_],'UIWidgetComponent') else (0,0,*resolution_)
    scale_ = min(resolution_[0]/1920,resolution_[1]/1080)
    anchorMin_,anchorMax_,pivot_ = rect_['AnchorMin'],rect_['AnchorMax'],rect_['Pivot']
    anchorSize_ = [(anchorMax_[i_]-anchorMin_[i_])*parentRect_[i_+2] for i_ in range(2)]
    size_ = [max(1,(anchorSize_[i_]+rect_['SizeDelta'][i_]*scale_)*rect_['LocalScale'][i_]) for i_ in range(2)]
    position_ = [parentRect_[i_]+parentRect_[i_+2]*anchorMin_[i_]+anchorSize_[i_]*pivot_[i_]+rect_['AnchoredPosition'][i_]*scale_-size_[i_]*pivot_[i_] for i_ in range(2)]
    cache_[name_]=(*position_,*size_)
    return cache_[name_]


def Preview(objects_, resolution_, skill_):
    """実行画面と混同しないよう注記した、素材と配置だけの参考画像を描く。"""
    canvas_=Image.new('RGBA',resolution_,(24,33,43,255))
    draw_=ImageDraw.Draw(canvas_)
    scale_=min(resolution_[0]/1920,resolution_[1]/1080)
    font_=ImageFont.truetype(str(FONT),max(10,round(18*scale_)))
    draw_.text((round(32*scale_),round(180*scale_)),'静的レイアウト参考 / ゲーム実行・実背景の検証画像ではありません',font=font_,fill='#9db3c7')
    draw_.text((round(32*scale_),round(214*scale_)),str(resolution_)+' / '+('スキル表示の参考' if skill_ else '通常表示の参考'),font=font_,fill='#9db3c7')
    cache_={}
    drawing_=[]
    for name_,object_ in objects_.items():
        widget_=Data(object_,'UIWidgetComponent')
        sprite_=Data(object_,'SpriteRenderer')
        if not widget_ or not sprite_ or not widget_.get('Visible',True) or not sprite_.get('Visible',True):
            continue
        if not (name_.startswith(('BossHealth','PlayerHp','PlayerFace','HudGuide')) or name_=='BossNamePlate'):
            continue
        if any(s_ in name_ for s_ in ['Flash','EdgeHighlight','HealDelay','Awakened','Highlight']):
            continue
        if name_.startswith('HudGuide') and not 'Hint' in name_:
            isSkill_ = any(s_ in name_ for s_ in ['SkillIcon','SkillLabel','CooldownMask','CooldownText','ReadyFrame'])
            if isSkill_ != skill_:
                continue
        drawing_.append((widget_.get('ZOrder',0),name_,object_))
    for _,name_,object_ in sorted(drawing_):
        sprite_=Data(object_,'SpriteRenderer')
        text_=Data(object_,'UITextComponent')
        x_,y_,w_,h_=Rect(name_,objects_,resolution_,cache_)
        position_=(round(x_),round(y_))
        size_=(max(1,round(w_)),max(1,round(h_)))
        textValue_=text_.get('Text','')
        if name_=='BossHealthValue': textValue_='5000 / 5000'
        if name_=='PlayerHpValue': textValue_='500 / 500'
        if name_.startswith('HudGuideHint') and name_.endswith('Text'):
            index_=int(name_[12])
            textValue_=['R2 回避','L2 ガード','L1 スキル','R3 ロックオン切替'][index_]
        if name_.startswith('HudGuide') and name_[8:9].isdigit():
            index_=int(name_[8])
            if name_.endswith('Label'):
                textValue_=(['L1+△ Breakthrough','L1+□ PhantomRush','L1+○ RisingJudgment','未設定'] if skill_ else ['△ 強攻撃','□ 攻撃','○ パリィ','× ジャンプ'])[index_]
            if name_.endswith('CooldownText'): textValue_=''
            if name_.endswith('CooldownMask') or name_.endswith('ReadyFrame'):
                continue
        if text_:
            font_=ImageFont.truetype(str(FONT),max(8,round(text_.get('FontSize',18)*scale_)))
            draw_.text((x_+w_/2,y_+h_/2),textValue_,anchor='mm',font=font_,fill='white')
            continue
        path_=sprite_.get('TexturePath','')
        if skill_ and name_.endswith('SkillIcon'):
            index_=int(name_[8])
            path_='Data/Sprite/UI/Hud/Skills/Skill_'+['ChargeRound','ComboRound','LauncherRound','Empty'][index_]+'.png'
        if name_=='HudGuideHint3Button':
            continue
        if not path_:
            continue
        image_=Image.open(ROOT/path_).convert('RGBA')
        if name_=='PlayerFaceIcon':
            image_=image_.crop((0,0,464,464))
        canvas_.alpha_composite(image_.resize(size_,Image.Resampling.LANCZOS),position_)
    canvas_.convert('RGB').save(OUTPUT/('Layout_'+str(resolution_[0])+'x'+str(resolution_[1])+('_Skill' if skill_ else '_Normal')+'.png'))


def Main():
    """静的な資産整合性を確認し、実行検証とは分けた結果を保存する。"""
    manifest_=Read(OUTPUT/'AssetManifest.json')
    for asset_ in manifest_['assets']:
        image_=Image.open(OUTPUT/asset_['file'])
        if image_.mode!='RGBA' or list(image_.size)!=asset_['size']:
            raise ValueError('Invalid image: '+asset_['file'])
    scene_=Read(SCENE/'SceneList.json')
    names_=scene_['ObjectList']
    if len(names_)!=len(set(names_)):
        raise ValueError('Duplicate SceneList names')
    objects_={name_:Read(SCENE/'Objects'/(name_+'.json')) for name_ in names_}
    hudNames_=[name_ for name_ in names_ if name_.startswith(('BossHealth','PlayerHp','PlayerFace','HudGuide','HudStatus')) or name_ in ['UIRoot','PlayerStatusRoot','BossNamePlate']]
    widgetIds_={Data(objects_[name_],'UIWidgetComponent').get('WidgetId') for name_ in names_}
    for name_ in hudNames_:
        object_=objects_[name_]
        parent_=object_.get('ParentName')
        if parent_ and parent_ not in objects_:
            raise ValueError('Unresolved parent: '+name_)
        widget_=Data(object_,'UIWidgetComponent')
        if widget_.get('ParentWidgetId') and widget_['ParentWidgetId'] not in widgetIds_:
            raise ValueError('Unresolved parent widget: '+name_)
        sprite_=Data(object_,'SpriteRenderer')
        if sprite_.get('TexturePath') and not (ROOT/sprite_['TexturePath']).exists():
            raise ValueError('Missing texture: '+name_)
    namespace_={'m':'http://schemas.microsoft.com/developer/msbuild/2003'}
    for file_ in ['BIGShine.vcxproj','BIGShine.vcxproj.filters']:
        document_=ET.parse(ROOT/file_)
        for type_,suffix_ in [('ClCompile','cpp'),('ClInclude','h')]:
            expected_='Code\\Engine\\UI\\BattleHudComponent.'+suffix_
            entries_=[item_ for item_ in document_.findall('.//m:'+type_,namespace_) if item_.get('Include')==expected_]
            if len(entries_)!=1:
                raise ValueError('VS registration: '+file_)
    bounds_={}
    for resolution_ in [(1920,1080),(1600,900),(1280,720),(2560,1080),(1024,768)]:
        cache_={}
        outside_=[]
        for name_ in hudNames_:
            if name_=='UIRoot':
                continue
            x_,y_,w_,h_=Rect(name_,objects_,resolution_,cache_)
            if x_<-1 or y_<-1 or x_+w_>resolution_[0]+1 or y_+h_>resolution_[1]+1:
                outside_.append(name_)
        if outside_:
            raise ValueError('Outside screen '+str(resolution_)+': '+str(outside_))
        bounds_[str(resolution_)]=dict(checked=len(hudNames_)-1,outside=outside_)
        Preview(objects_,resolution_,False)
        Preview(objects_,resolution_,True)
    report_=dict(assets=len(manifest_['assets']),sceneHudObjects=len(hudNames_),resolutions=bounds_,
                 visualOnly=True,gameExecuted=False,buildExecuted=False,actualBackgroundVerified=False)
    (OUTPUT/'StaticReview.json').write_bytes((json.dumps(report_,ensure_ascii=False,indent=2)+'\n').replace('\n','\r\n').encode('utf-8'))
    print(json.dumps(report_,ensure_ascii=False))


if __name__=='__main__':
    Main()
