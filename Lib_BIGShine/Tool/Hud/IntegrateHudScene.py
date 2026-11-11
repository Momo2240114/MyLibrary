"""既存MainSceneのHP表示を再利用し、表示専用WidgetとHUD設定を追加する。"""
from pathlib import Path
import json
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SCENE = ROOT / 'Data/Scenes/MainScene'
OBJECTS = SCENE / 'Objects'
ASSETS = ROOT / 'Data/Sprite/UI/Hud'


def Write(path_, text_):
    """UTF-8 BOMなし・CRLFで保存する。"""
    path_.write_bytes(text_.replace('\r\n','\n').replace('\n','\r\n').encode('utf-8'))


def Load(name_):
    """既存Objectを読み、依頼と無関係なComponent設定を保持する。"""
    return json.loads((OBJECTS/(name_+'.json')).read_text(encoding='utf-8-sig'))


def Save(object_):
    """Object名とSceneListの名前を同一に保つ。"""
    Write(OBJECTS/(object_['Name']+'.json'),json.dumps(object_,ensure_ascii=False,indent=4)+'\n')


def Component(type_, data_):
    """Sceneの標準Component形式を生成する。"""
    return dict(TypeName=type_,Data=data_,Enabled=True,UpdateEnabled=True,LateUpdateEnabled=True,
                UpdatePriority=0,LateUpdatePriority=0,RequiredForSceneStart=False)


def Data(object_, type_):
    """指定Componentの保存項目を参照する。"""
    return next(c_['Data'] for c_ in object_['Components'] if c_['TypeName']==type_)


def Widget(name_, parent_, anchor_, position_, size_, pivot_=(0.5,0.5), z_=30):
    """既存AnchorとBoard計算へ渡すRectを作る。"""
    return Component('UIWidgetComponent',dict(Version=1,WidgetId=name_,ParentWidgetId=parent_,ZOrder=z_,Visible=True,
        InputEnabled=False,RaycastTarget=False,RectTransform=dict(AnchorMin=list(anchor_),AnchorMax=list(anchor_),
        AnchoredPosition=list(position_),SizeDelta=list(size_),Pivot=list(pivot_),LocalScale=[1,1],LocalRotation=0)))


def Sprite(path_, size_, visible_=True):
    """動的生成を避け、Scene読込時にPNGを読み込む。"""
    textureSize_ = Image.open(ASSETS/(path_+'.png')).size if path_ else (1,1)
    return Component('SpriteRenderer',dict(TexturePath='Data/Sprite/UI/Hud/'+path_+'.png' if path_ else '',
        Color=[1,1,1,1],LocalSize=list(size_),CatPos=[0,0],CatSize=list(textureSize_),Visible=visible_,ShaderID=0,pivotType=0,LinearSampling=True,SdrTexture=True))


def Object(name_, parent_, anchor_, position_, size_, path_=None, text_=None, fontSize_=20, pivot_=(0.5,0.5), z_=30):
    """画像やTextを、既存SpriteRendererとUITextの組合せで配置する。"""
    object_ = dict(Name=name_,ParentName=parent_,Layer=16,CollisionMask=0,RequiredForSceneStart=False,
        Transform=dict(Pos=[0,0,0],Rot=[0,0,0],Scale=[1,1,1]),Components=[Widget(name_,parent_,anchor_,position_,size_,pivot_,z_)])
    if path_ is not None or text_ is not None:
        object_['Components'].append(Sprite(path_ or '',size_))
    if text_ is not None:
        object_['Components'].append(Component('UITextComponent',dict(Text=text_,FontPath='Data/Font/NotoSansJP-Regular.otf',
            FontSize=fontSize_,Color=[1,1,1,1],MaxWidth=size_[0],Padding=3,AutoSizeWidget=False,
            HorizontalAlign='CENTER',VerticalAlign='CENTER')))
    return object_


def SetRect(object_, parent_, anchor_, position_, size_, pivot_=(0.5,0.5), z_=None):
    """既存Widgetの識別名を維持したまま配置を更新する。"""
    object_['ParentName']=parent_
    widget_ = Data(object_,'UIWidgetComponent')
    widget_['ParentWidgetId']=parent_
    widget_['RectTransform'].update(AnchorMin=list(anchor_),AnchorMax=list(anchor_),AnchoredPosition=list(position_),SizeDelta=list(size_),Pivot=list(pivot_))
    if z_ is not None:
        widget_['ZOrder']=z_


def SetSprite(object_, path_):
    """既存Spriteの色・可視設定を更新し、正しい画像全体のUVを設定する。"""
    sprite_ = Data(object_,'SpriteRenderer')
    sprite_.update(Sprite(path_,(1,1))['Data'])


def Main():
    """HPの旧Objectと新規ガイドを、単一のScene保存データへ統合する。"""
    listPath_ = SCENE/'SceneList.json'
    scene_ = json.loads(listPath_.read_text(encoding='utf-8-sig'))
    added_ = []
    def Add(object_):
        Save(object_)
        added_.append(object_['Name'])

    root_ = Load('UIRoot')
    Data(root_,'WidgetBoardComponent').update(BoardId='integrated_battle_hud',BoardName='Integrated Battle HUD',ScaleMode='EXPAND')
    widget_ = Data(root_,'UIWidgetComponent')
    widget_.update(WidgetId='BattleHudRoot',ParentWidgetId='')
    widget_['RectTransform'].update(AnchorMin=[0,0],AnchorMax=[1,1],SizeDelta=[0,0],AnchoredPosition=[0,0])
    if not any(c_['TypeName']=='BattleHudComponent' for c_ in root_['Components']):
        root_['Components'].append(Component('BattleHudComponent',{}))
    Save(root_)
    added_.append('UIRoot')
    # UIRootは旧資料に存在するが、SceneList未登録だった。UIの既存Actionを残したまま独立Boardでフェードする。
    for prefix_, rootName_, anchor_, position_, size_, source_ in [
        ('BossHealth','BossHealthBar',(0.5,0),(0,55),(850,12),'Boss/BossBar'),
        ('PlayerHp','PlayerStatusRoot',(0,1),(60,-120),(560,116),'Player/PlayerBar')]:
        base_ = Load(rootName_)
        SetRect(base_,'UIRoot',anchor_,position_,size_,(0.5,0.5) if prefix_=='BossHealth' else (0,0))
        Data(base_,'UIWidgetComponent')['ParentWidgetId']='BattleHudRoot'
        Save(base_)
        barName_ = prefix_+'Bar'
        bar_ = Load(barName_)
        if prefix_=='BossHealth':
            bar_=base_
        else:
            SetRect(bar_,'PlayerStatusRoot',(0,0),(94,44),(440,12),(0,0),34)
        Data(bar_,'UIProgressBarComponent').update(Source='MANUAL',CropFillUV=True,DamageDelayDuration=0.3,DecreaseDelaySpeed=0.45,
            IncreaseDelaySpeed=1.5,TargetRate=1,CurrentRate=1,DamageDelayRate=1,HealDelayRate=1)
        for component_ in bar_['Components']:
            if component_['TypeName']=='UIProgressBarComponent':
                component_['UpdateEnabled']=False
        Save(bar_)
        for suffix_, path_, z_ in [('Background',source_+'_Background',31),('DamageDelay',source_+'_DamageTrail',32),
                ('HealDelay',source_+'_Fill' if prefix_=='BossHealth' else source_+'_HpFill',33),
                ('Current',source_+'_Fill' if prefix_=='BossHealth' else source_+'_HpFill',34)]:
            object_=Load(prefix_+suffix_)
            SetRect(object_,barName_,(0,0),(0,0),(0,0),(0,0),z_)
            rect_=Data(object_,'UIWidgetComponent')['RectTransform']
            rect_['AnchorMax']=[1,1]
            SetSprite(object_,path_)
            Save(object_)
        # 既存Flashと端部Highlightも保持し、太くなりすぎない寸法へ合わせる。
        for suffix_ in ['Flash','EdgeHighlight']:
            object_=Load(prefix_+suffix_)
            SetRect(object_,barName_,(0,0),(0,0),(0,0),(0,0),35)
            Data(object_,'UIWidgetComponent')['RectTransform']['AnchorMax']=[1,1]
            Save(object_)
        frame_=Load(prefix_+'Frame')
        SetRect(frame_,barName_,(0.5,0.5),(0,0),(890,28) if prefix_=='BossHealth' else (468,28),(0.5,0.5),36)
        SetSprite(frame_,source_+'_Frame')
        Save(frame_)
    name_=Load('BossNamePlate')
    SetRect(name_,'BossHealthBar',(0,0),(0,-26),(420,32),(0,0.5),39)
    Data(name_,'UITextComponent').update(AutoSizeWidget=False,HorizontalAlign='LEFT',FontSize=24)
    Save(name_)
    Add(Object('BossHealthValue','BossHealthBar',(1,0),(0,-22),(230,28),text_='',fontSize_=18,pivot_=(1,0.5),z_=39))
    # モデルと異なる顔画像は再生成時も表示しない。既存Widgetの参照は維持する。
    decoration_ = Object('BossHealthDecoration','BossHealthBar',(0,0.5),(-48,0),(108,88),'Boss/BossBar_DragonDecoration',z_=38)
    Data(decoration_,'SpriteRenderer')['Visible'] = False
    Add(decoration_)
    Add(Object('BossHealthAwakened','BossHealthBar',(0.5,0.5),(0,0),(890,28),'Boss/BossBar_AwakenedDecoration',z_=37))
    Add(Object('PlayerHpValue','PlayerHpBar',(1,0),(0,-20),(210,26),text_='',fontSize_=18,pivot_=(1,0.5),z_=39))
    face_=Load('PlayerFaceFrame')
    SetRect(face_,'PlayerStatusRoot',(0,0),(44,46),(96,84),(0.5,0.5),37)
    SetSprite(face_,'Player/PlayerBar_Decoration')
    Save(face_)
    face_=Load('PlayerFaceIcon')
    SetRect(face_,'PlayerStatusRoot',(0,0),(44,46),(40,40),(0.5,0.5),38)
    Save(face_)
    for index_, name_ in enumerate(['Guard','JustGuard','Invincible']):
        Add(Object('HudStatus'+name_,'PlayerStatusRoot',(0,0),(110+index_*42,84),(32,32),'Status/Status_'+name_,z_=39))

    Add(Object('HudGuideRoot','UIRoot',(1,1),(-340,-250),(0,0),z_=40))
    guideRoot_=Load('HudGuideRoot')
    Data(guideRoot_,'UIWidgetComponent')['ParentWidgetId']='BattleHudRoot'
    Save(guideRoot_)
    positions_=[(-80,-72),(-152,0),(-8,0),(-80,72)]
    labelPositions_=[(-80,-126),(-185,52),(0,52),(-80,116)]
    for index_, (button_,position_,labelPosition_) in enumerate(zip(['Triangle','Square','Circle','Cross'],positions_,labelPositions_)):
        prefix_='HudGuide'+str(index_)
        Add(Object(prefix_+'Button','HudGuideRoot',(0.5,0.5),position_,(54,56),'Buttons/Button_'+button_,z_=43))
        Add(Object(prefix_+'Highlight','HudGuideRoot',(0.5,0.5),position_,(70,70),'ActionGuide/ActionGuide_Highlight',z_=47))
        Add(Object(prefix_+'Label','HudGuideRoot',(0.5,0.5),labelPosition_,(148,30),text_='',fontSize_=18,z_=48))
        Add(Object(prefix_+'SkillIcon','HudGuideRoot',(0.5,0.5),position_,(64,64),'Skills/Skill_Empty',z_=44))
        Add(Object(prefix_+'CooldownMask','HudGuideRoot',(0.5,0.5),position_,(68,68),'Skills/Skill_CooldownOverlay',z_=45))
        Add(Object(prefix_+'ReadyFrame','HudGuideRoot',(0.5,0.5),position_,(76,76),'Skills/Skill_ReadyFrame',z_=46))
        Add(Object(prefix_+'SkillLabel','HudGuideRoot',(0.5,0.5),labelPosition_,(180,30),text_='',fontSize_=14,z_=48))
        Add(Object(prefix_+'CooldownText','HudGuideRoot',(0.5,0.5),position_,(112,26),text_='',fontSize_=14,z_=48))
    for index_ in range(4):
        position_=(210,-70+index_*48)
        Add(Object('HudGuideHint'+str(index_)+'Background','HudGuideRoot',(0.5,0.5),position_,(220,42),'ActionGuide/ActionGuide_LabelBackground',z_=41))
        Add(Object('HudGuideHint'+str(index_)+'Button','HudGuideRoot',(0.5,0.5),(125,position_[1]),(42,24),'Buttons/Button_'+['R2','L2','L1','R1'][index_],z_=42))
        Add(Object('HudGuideHint'+str(index_)+'Text','HudGuideRoot',(0.5,0.5),(236,position_[1]),(160,38),text_='',fontSize_=16,z_=48))
    for name_ in added_:
        if name_ not in scene_['ObjectList']:
            scene_['ObjectList'].append(name_)
    Write(listPath_,json.dumps(scene_,ensure_ascii=False,indent=4)+'\n')
    print('Integrated HUD using existing HP Objects;',len(added_),'SceneList additions checked.')


if __name__=='__main__':
    Main()
