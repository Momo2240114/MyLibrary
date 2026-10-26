"""ゲームを起動せず、入力素材・Scene参照・登録内容から静的確認用資料を作る。"""
from pathlib import Path
import json
import re
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / 'Tool/Hud'
SOURCE = (ROOT / 'Code/Engine/UI/InputIconAtlas.cpp').read_text(encoding='utf-8')
SHEETS = [
    ('Keyboard & Mouse', 'Data/Sprite/Input/KeyMouse/KeyMouseInputSheet.png', 64),
    ('Xbox', 'Data/Sprite/Input/GamePad/Xbox/XboxInputSheet.png', 64),
    ('PlayStation', 'Data/Sprite/Input/GamePad/PS/playstation-series_sheet_double.png', 128),
]
IMAGES = [Image.open(ROOT / path).convert('RGBA') for _, path, _ in SHEETS]
REPORT = {'kind': 'static inspection; no game execution', 'sheets': [], 'regions': []}

for (name, path, cell), image in zip(SHEETS, IMAGES):
    assert image.width % cell == 0 and image.height % cell == 0
    assert image.getchannel('A').getextrema() == (0, 255)
    REPORT['sheets'].append({'device': name, 'path': path, 'size': image.size,
                             'grid': [image.width // cell, image.height // cell], 'alpha': [0, 255]})

key_section = SOURCE.split('KEY_CELLS[] = {', 1)[1].split('};', 1)[0]
key_cells = {name: (int(column), int(row)) for name, column, row in
             re.findall(r"\{('[^']'|VK_\w+),(\d+),(\d+)\}", key_section)}
pad_section = SOURCE.split('PAD_CELLS[] = {', 1)[1].split('};', 1)[0]
pad_cells = {name: ((int(xc), int(xr)), (int(pc), int(pr))) for name, xc, xr, pc, pr in
             re.findall(r'\{PadCode::(\w+),(-?\d+),(-?\d+),(-?\d+),(-?\d+)\}', pad_section)}

def CheckCell(device, name, cell):
    column, row = cell
    if column < 0 or row < 0:
        return
    size = SHEETS[device][2]
    image = IMAGES[device]
    box = (column * size, row * size, (column + 1) * size, (row + 1) * size)
    assert box[2] <= image.width and box[3] <= image.height, (device, name, box)
    assert image.crop(box).getchannel('A').getbbox(), (device, name, 'empty cell')
    REPORT['regions'].append({'device': SHEETS[device][0], 'input': name,
                              'sourceRect': [box[0] + .5, box[1] + .5, size - 1, size - 1]})

for name, cell in key_cells.items():
    CheckCell(0, name, cell)
for digit in range(10):
    CheckCell(0, str(digit), (digit * 2 + 2, 15) if digit < 8 else ((digit - 8) * 2 + 1, 14))
for index, (column, row) in enumerate(zip([3,11,13,15,0,2,4,6,8,5,7,9], [9,9,9,9,8,8,8,8,8,9,9,9])):
    CheckCell(0, 'F' + str(index + 1), (column, row))
for name, cells in pad_cells.items():
    CheckCell(1, name, cells[0])
    CheckCell(2, name, cells[1])

scene = json.loads((ROOT / 'Data/Scenes/MainScene/SceneList.json').read_text(encoding='utf-8'))
names = scene['ObjectList']
assert len(names) == len(set(names))
widgets = set()
for name in names:
    path = ROOT / 'Data/Scenes/MainScene/Objects' / (name + '.json')
    assert path.exists(), str(path)
    if not name.startswith('HudGuide'):
        continue
    data = json.loads(path.read_text(encoding='utf-8'))
    assert data['Name'] == name
    for component in data['Components']:
        if component['TypeName'] == 'UIWidgetComponent':
            widget = component['Data']['WidgetId']
            assert widget not in widgets
            widgets.add(widget)
        if component['TypeName'] == 'SpriteRenderer':
            texture = component['Data']['TexturePath']
            assert not texture or (ROOT / texture).exists(), texture
for index in range(4):
    for prefix in [f'HudGuide{index}Button', f'HudGuideHint{index}Button', f'HudGuide{index}SkillButton']:
        assert all(prefix + (str(slot) if slot else '') in names for slot in range(4))
REPORT['scene'] = {'objects': len(names), 'guideWidgets': len(widgets), 'uniqueNames': True, 'references': 'resolved'}

namespace = {'ms': 'http://schemas.microsoft.com/developer/msbuild/2003'}
for filename in ['BIGShine.vcxproj', 'BIGShine.vcxproj.filters']:
    tree = ET.parse(ROOT / filename)
    for tag, extension in [('ClCompile', 'cpp'), ('ClInclude', 'h')]:
        target = 'Code\\Engine\\UI\\InputIconAtlas.' + extension
        entries = [entry for entry in tree.findall('.//ms:' + tag, namespace) if entry.get('Include') == target]
        assert len(entries) == 1, (filename, target)
        if filename.endswith('filters'):
            assert entries[0].find('ms:Filter', namespace).text == 'Code\\Engine\\GamePlay\\UI'
REPORT['visualStudio'] = 'source/header registered once; UI filter verified'

# 設定済みの主操作を元画像から描き、3解像度の配置計算を比較する。実機画面の代替とは扱わない。
font = ImageFont.truetype(str(ROOT / 'Data/Font/NotoSansJP-Regular.otf'), 16)
small = ImageFont.truetype(str(ROOT / 'Data/Font/NotoSansJP-Regular.otf'), 13)
actions = ['Player.StrongAttack', 'Player.WeakAttack', 'Player.JustGuard', 'Player.Jump']
labels = ['強攻撃', '攻撃', 'パリィ', 'ジャンプ']
config = json.loads((ROOT / 'Data/Input/KeyConfig.json').read_text(encoding='utf-8'))['actions']
pad_codes = {'Y': 0x8000, 'X': 0x4000, 'B': 0x2000, 'A': 0x1000, 'L1': 0x100, 'L2': 0x10000, 'R2': 0x20000, 'R3': 0x80}
virtual = {'VK_LBUTTON': 1, 'VK_RBUTTON': 2, 'VK_SHIFT': 16, 'VK_SPACE': 32}
key_lookup = {ord(name[1]): cell for name, cell in key_cells.items() if name.startswith("'")}
key_lookup.update({value: key_cells[name] for name, value in virtual.items()})
face_positions = {0x8000: (-100, -85), 0x4000: (-185, 0), 0x2000: (-15, 0), 0x1000: (-100, 85)}
canvas = Image.new('RGB', (1440, 1260), (18, 23, 30))
draw = ImageDraw.Draw(canvas)
draw.text((12, 8), 'STATIC MATERIAL / LAYOUT REFERENCE - not a game capture', font=font, fill='white')
REPORT['layoutBounds'] = []

def DrawIcon(device, code, center, size):
    if device == 0:
        cell = key_lookup.get(code)
    else:
        name = next((name for name, value in pad_codes.items() if value == code), None)
        cell = pad_cells[name][device - 1] if name else None
    if cell is None:
        return
    width = SHEETS[device][2]
    column, row = cell
    icon = IMAGES[device].crop((column * width, row * width, (column + 1) * width, (row + 1) * width))
    icon.thumbnail((size, size), Image.Resampling.LANCZOS)
    canvas.paste(icon, (round(center[0] - icon.width / 2), round(center[1] - icon.height / 2)), icon)

for row, resolution in enumerate([(1920,1080), (1600,900), (1280,720)]):
    # 各パネルは実際の相対スケールを示し、全画面の右下600x600相当を表示する。
    scale = resolution[0] / 1920
    for device in range(3):
        ox, oy = device * 480, 40 + row * 400
        draw.rectangle((ox+8, oy+8, ox+472, oy+392), outline=(60,80,95))
        draw.text((ox+18, oy+15), f'{SHEETS[device][0]} {resolution[0]}x{resolution[1]}', font=font, fill='white')
        center = (ox+265, oy+(300 if device == 0 else 215))
        relative_root = (-260, -310) if device == 0 else (-340, -250)
        for index, (action, label) in enumerate(zip(actions, labels)):
            combo = next((combo for combo in config[action] if all(unit['dev'] == (0 if device == 0 else 1) for unit in combo)), [])
            if not combo:
                continue
            code = combo[0]['code']
            position = (-160, -220 + index * 40) if device == 0 else face_positions[code]
            # 参考画像ではK&Mの縦型を左へ寄せ、右側に操作名を配置する。
            point = (center[0] + (position[0] + (80 if device == 0 else 100)) * scale,
                     center[1] + position[1] * scale)
            DrawIcon(device, code, point, round((32 if device == 0 else 42) * scale))
            label_point = (center[0] + 160 * scale, point[1]) if device == 0 else (point[0], point[1] + 40 * scale)
            draw.text(label_point, label, font=small, fill=(225,230,235), anchor='mm')
            left = resolution[0] + (relative_root[0] + position[0] - 21) * scale
            top = resolution[1] + (relative_root[1] + position[1] - 21) * scale
            assert left >= 0 and top >= 0
        bounds = (-450,-551,-70,-95) if device == 0 else (-637,-422,-60,-72)
        REPORT['layoutBounds'].append({'device': SHEETS[device][0], 'resolution': resolution,
                                       'referenceBoundsFromBottomRight': bounds, 'scale': scale})
        assert resolution[0] + bounds[0] * scale >= 0 and resolution[1] + bounds[1] * scale >= 0
        draw.text((ox+18, oy+360), 'Common skill art/cooldown retained; GPU result unverified', font=small, fill=(170,180,190))
canvas.save(OUTPUT / 'InputGuideStaticPreview.png')
(OUTPUT / 'InputGuideStaticReview.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2) + '\n', encoding='utf-8', newline='\r\n')
print(json.dumps({'regions': len(REPORT['regions']), 'scene': REPORT['scene'], 'layouts': len(REPORT['layoutBounds']), 'build': 'not performed'}, ensure_ascii=False))
