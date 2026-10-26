"""HUD資料から素材を抽出し、透過PNG・由来一覧・背景別の確認画像を生成する。"""
from pathlib import Path
import json
import math
from PIL import Image, ImageDraw, ImageFilter, ImageFont
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / 'Data/Sprite/UI/Hud'
SOURCE = ROOT / 'Data/Sprite/UI/幻想アクションゲームHUD設計図.png'
RECORDS = []


def Save(image_, name_, method_, rect_=None):
    """再生成可能な由来情報とともに、ストレートアルファのPNGを保存する。"""
    path_ = OUTPUT / (name_ + '.png')
    path_.parent.mkdir(parents=True, exist_ok=True)
    image_.save(path_)
    alpha_ = image_.getchannel('A')
    RECORDS.append(dict(file=name_ + '.png', size=list(image_.size), alpha=alpha_.getextrema(),
                        method=method_, sourceRect=rect_))


def Extract(source_, name_, rect_, shape_='rect', polygon_=None):
    """素材の輪郭だけを残し、資料の背景と文字を含む領域を除く。"""
    image_ = source_.crop(rect_).convert('RGBA')
    mask_ = Image.new('L', image_.size)
    draw_ = ImageDraw.Draw(mask_)
    if shape_ == 'circle':
        draw_.ellipse((1, 1, image_.width - 2, image_.height - 2), fill=255)
    elif polygon_:
        draw_.polygon(polygon_, fill=255)
        mask_ = mask_.filter(ImageFilter.GaussianBlur(0.45))
    else:
        draw_.rounded_rectangle((1, 1, image_.width - 2, image_.height - 2), radius=3, fill=255)
    pixels_ = np.array(image_).astype(np.float32)
    alpha_ = np.array(mask_).astype(np.float32) / 255
    if shape_ == 'metal':
        # 資料の黒背景と合成済みの細い棘は、輝度を透過率へ戻して黒い矩形を除去する。
        matte_ = np.array([8, 12, 15], dtype=np.float32)
        # 黒い金属の内部は保持する。輪郭の数画素だけを黒マットから復元し、穴だらけになる輝度キーを避ける。
        core_ = np.array(mask_.filter(ImageFilter.MinFilter(7))).astype(np.float32)/255
        strength_ = np.max(np.maximum(pixels_[:, :, :3]-matte_,0),axis=2)
        alpha_ = np.maximum(core_, alpha_*np.clip((strength_-3)/34,0,1))
        pixels_[:, :, :3] = np.clip((pixels_[:, :, :3] - matte_ * (1-alpha_[:, :, None])) /
                                   np.maximum(alpha_[:, :, None], 0.001), 0, 255)
    pixels_[:, :, 3] = np.round(alpha_ * 255)
    pixels_[alpha_ == 0, :3] = 0
    Save(Image.fromarray(pixels_.astype(np.uint8)), name_,
         'source contour / dark-matte reconstruction' if shape_ == 'metal' else 'source contour', list(rect_))


def Surface(name_, size_, kind_, color_):
    """隠れたゲージ内部など、資料から分離不能な形状を透明面へ再構成する。"""
    factor_ = 4
    image_ = Image.new('RGBA', (size_[0]*factor_, size_[1]*factor_))
    draw_ = ImageDraw.Draw(image_)
    width_, height_ = image_.size
    if kind_ == 'fill':
        # 左右方向の模様を避け、UV切り詰め後も変形しない縦方向の陰影にする。
        for y_ in range(height_):
            value_ = 0.55 + 0.45 * math.sin(math.pi * y_ / max(1, height_-1))
            draw_.line((0, y_, width_, y_), fill=tuple(round(c_*value_) for c_ in color_[:3]) + (color_[3],))
    elif kind_ == 'frame':
        points_ = [(0,height_*0.25),(width_*0.018,0),(width_*0.98,0),(width_-1,height_*0.45),
                   (width_*0.98,height_-1),(width_*0.018,height_-1),(0,height_*0.25)]
        draw_.line(points_, fill=(28,24,23,255), width=12)
        draw_.line(points_, fill=color_, width=4)
        draw_.line((width_*0.02,height_*0.82,width_*0.975,height_*0.82), fill=(88,73,51,255), width=4)
    elif kind_ == 'ring':
        draw_.ellipse((8,8,width_-9,height_-9), outline=color_, width=8)
    elif kind_ == 'empty':
        draw_.ellipse((8,8,width_-9,height_-9), fill=(15,19,25,235), outline=(125,136,148,255), width=6)
        draw_.line((width_*0.3,height_*0.5,width_*0.7,height_*0.5), fill=color_, width=14)
        draw_.line((width_*0.5,height_*0.3,width_*0.5,height_*0.7), fill=color_, width=14)
    else:
        draw_.rounded_rectangle((2,2,width_-3,height_-3), radius=12, fill=color_, outline=(84,105,120,210), width=3)
    Save(image_.resize(size_, Image.Resampling.LANCZOS), name_, 'reconstructed geometry (not extracted)')


def CooldownAtlas():
    """残り時間の割合に対応する65段階の円形マスクを作り、毎フレームの画像生成をなくす。"""
    atlas_ = Image.new('RGBA', (9*128, 8*128))
    for index_ in range(65):
        tile_ = Image.new('RGBA', (512,512))
        draw_ = ImageDraw.Draw(tile_)
        if index_:
            draw_.pieslice((16,16,495,495), -90, -90 + index_*360/64, fill=(4,9,17,175))
            draw_.arc((10,10,501,501), -90, -90 + index_*360/64, fill=(97,207,255,255), width=14)
        atlas_.paste(tile_.resize((128,128), Image.Resampling.LANCZOS), ((index_%9)*128,(index_//9)*128))
    Save(atlas_, 'Skills/Skill_CooldownOverlay', 'generated 65-frame radial atlas; 9 columns, 128px cells')


def ExtractFrame(source_, name_, rect_, top_, bottom_):
    """元の金属レールと両端を残し、重なっているHP塗りを透明にする。"""
    image_ = source_.crop(rect_).convert('RGBA')
    pixels_ = np.array(image_).astype(np.float32)
    brightness_ = np.max(pixels_[:,:,:3],axis=2)
    alpha_ = np.clip((brightness_-22)/72,0,1)
    # 塗りの内側は消し、レール側の暗い背景もアルファへ戻す。
    alpha_[top_:bottom_,20:image_.width-38]=0
    redFill_ = (pixels_[:,:,0]>pixels_[:,:,1]*2) & (pixels_[:,:,0]>pixels_[:,:,2]*1.6)
    alpha_[top_-1:bottom_+1][redFill_[top_-1:bottom_+1]]=0
    matte_=np.array([8,12,15],dtype=np.float32)
    pixels_[:,:,:3]=np.clip((pixels_[:,:,:3]-matte_*(1-alpha_[:,:,None]))/np.maximum(alpha_[:,:,None],0.001),0,255)
    pixels_[:,:,3]=alpha_*255
    pixels_[alpha_==0,:3]=0
    Save(Image.fromarray(pixels_.astype(np.uint8)),name_,'source rails; covered gauge interior removed',list(rect_))


def ContactSheets():
    """全素材を黒・白・市松背景で並べ、ファイル名を含む一覧を作る。"""
    font_ = ImageFont.truetype('C:/Windows/Fonts/consola.ttf', 12)
    for background_ in ['Checker', 'Black', 'White']:
        sheet_ = Image.new('RGB', (1120, math.ceil(len(RECORDS)/4)*150), '#182029')
        draw_ = ImageDraw.Draw(sheet_)
        for index_, record_ in enumerate(RECORDS):
            x_, y_ = (index_%4)*280, (index_//4)*150
            tile_ = Image.new('RGBA', (268,112), 'white' if background_ == 'White' else 'black')
            if background_ == 'Checker':
                tileDraw_ = ImageDraw.Draw(tile_)
                for row_ in range(0,112,12):
                    for column_ in range(0,268,12):
                        tileDraw_.rectangle((column_,row_,column_+11,row_+11), fill='#8a8f94' if (row_//12+column_//12)%2 else '#c1c5c9')
            asset_ = Image.open(OUTPUT / record_['file']).convert('RGBA')
            if 'CooldownOverlay' in record_['file']:
                asset_ = asset_.crop((5*128,3*128,6*128,4*128))
            asset_.thumbnail((260,104), Image.Resampling.LANCZOS)
            tile_.alpha_composite(asset_, ((268-asset_.width)//2,(112-asset_.height)//2))
            sheet_.paste(tile_.convert('RGB'), (x_+6,y_+4))
            draw_.text((x_+6,y_+119), record_['file'], font=font_, fill='white')
            draw_.text((x_+6,y_+134), str(record_['size']), font=font_, fill='#a9bbce')
        sheet_.save(OUTPUT / ('ContactSheet_'+background_+'.png'))


def Main():
    """元画像の画素座標から個別素材を出力し、検証用の台帳を保存する。"""
    source_ = Image.open(SOURCE)
    if source_.size != (1536,1024):
        raise ValueError('Source resolution changed; re-check extraction coordinates.')
    Extract(source_, 'Boss/BossBar_DragonDecoration', (35,55,185,175), 'metal',
            [(4,73),(13,58),(19,48),(25,20),(29,5),(30,31),(48,16),(55,7),(51,28),
             (72,11),(86,4),(73,23),(107,0),(96,22),(85,36),(108,36),(119,48),
             (127,60),(149,73),(136,81),(123,82),(112,97),(98,102),(92,118),
             (76,117),(67,107),(51,105),(38,93),(25,89),(15,91),(6,85)])
    Extract(source_, 'Boss/BossBar_HumanDecoration', (34,190,184,294), 'metal',
            [(6,90),(15,76),(12,65),(24,53),(18,36),(25,19),(42,13),(39,2),
             (59,7),(72,0),(80,10),(96,7),(117,1),(107,20),(119,33),(120,48),
             (135,59),(148,74),(135,83),(116,86),(104,98),(79,103),(57,96),(35,101),(20,95)])
    Extract(source_, 'Player/PlayerBar_Decoration', (49,354,175,458), 'metal',
            [(0,53),(34,29),(58,0),(76,30),(125,48),(86,68),(66,103),(44,73)])
    for name_, x_ in [('Buff',842),('Debuff',970),('Guard',1097),('Invincible',1223),('JustGuard',1357)]:
        Extract(source_, 'Status/Status_'+name_, (x_,362,x_+62,425))
    for name_, x_ in [('Combo',28),('Charge',132),('Launcher',236),('Heal',340),('Buff',445),('Magic',549)]:
        Extract(source_, 'Skills/Skill_'+name_, (x_,844,x_+89,935))
        # 操作パレット用には円形の輪郭を別途持たせ、元の四角いアイコンも素材として残す。
        icon_=source_.crop((x_+2,846,x_+87,933)).resize((384,384),Image.Resampling.LANCZOS).convert('RGBA')
        mask_=Image.new('L',icon_.size)
        ImageDraw.Draw(mask_).ellipse((7,7,376,376),fill=255)
        icon_.putalpha(mask_)
        ImageDraw.Draw(icon_).ellipse((7,7,376,376),outline=(163,184,202,255),width=6)
        Save(icon_.resize((96,96),Image.Resampling.LANCZOS),'Skills/Skill_'+name_+'Round','source icon with reconstructed circular contour',[x_,844,x_+89,935])
    for name_, x_ in [('Triangle',1260),('Circle',1326),('Cross',1391),('Square',1457)]:
        Extract(source_, 'Buttons/Button_'+name_, (x_,831,x_+54,887), 'circle')
    for name_, rect_ in [('L1',(1294,901,1372,941)),('R1',(1399,901,1478,941)),
                         ('L2',(1294,949,1372,991)),('R2',(1399,949,1478,991))]:
        Extract(source_, 'Buttons/Button_'+name_, rect_)
    for prefix_, size_ in [('Boss/BossBar',(900,36)),('Player/PlayerBar',(540,30))]:
        if prefix_.startswith('Boss'):
            ExtractFrame(source_,prefix_+'_Frame',(165,104,1090,142),7,21)
        else:
            ExtractFrame(source_,prefix_+'_Frame',(162,387,711,410),3,17)
        Surface(prefix_+'_Background', (256,16), 'fill', (20,23,29,245))
        Surface(prefix_+('_Fill' if prefix_.startswith('Boss') else '_HpFill'), (256,16), 'fill', (224,15,37,255))
        Surface(prefix_+'_DamageTrail', (256,16), 'fill', (244,175,120,230))
    Surface('Player/PlayerBar_ResourceFill', (256,16), 'fill', (67,207,245,255))
    ExtractFrame(source_,'Boss/BossBar_HumanFrame',(165,235,1090,276),8,28)
    Surface('Boss/BossBar_AwakenedDecoration', (900,36), 'frame', (255,76,38,170))
    Surface('ActionGuide/ActionGuide_Background', (300,270), 'panel', (5,11,18,115))
    Surface('ActionGuide/ActionGuide_LabelBackground', (220,44), 'panel', (5,11,18,190))
    Surface('ActionGuide/ActionGuide_ButtonFrame', (100,100), 'ring', (157,170,182,255))
    Surface('ActionGuide/ActionGuide_Highlight', (100,100), 'ring', (85,234,224,235))
    Surface('Skills/Skill_ReadyFrame', (100,100), 'ring', (98,222,252,255))
    Surface('Skills/Skill_Empty', (100,100), 'empty', (137,150,164,255))
    CooldownAtlas()
    ContactSheets()
    with (OUTPUT/'AssetManifest.json').open('w',encoding='utf-8',newline='') as file_:
        file_.write(json.dumps(dict(source=str(SOURCE.relative_to(ROOT)), assets=RECORDS),ensure_ascii=False,indent=2).replace('\n','\r\n')+'\r\n')
    print('Generated', len(RECORDS), 'RGBA assets and 3 contact sheets at', OUTPUT)


if __name__ == '__main__':
    Main()
