"""ゲームループと実行時参照を起点にDataを選別し、元データを変更せず配布先へ複製する。"""
import argparse
from collections import defaultdict, deque
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
from urllib.parse import unquote


MODEL_EXTENSIONS = {'.gltf', '.glb', '.fbx', '.obj', '.cereal', '.bin', '.mtl', '.blend'}
SCENES = ('TitleBackground', 'LoadingPresentation', 'MainScene')
PRELOAD_ROOTS = ('Particle', 'VFX', 'VFXSequence', 'ImpactEffect', 'AttackTemplate')
CONFIG_ROOTS = ('Startup', 'Config', 'Settings', 'Input', 'Presentation', 'HumanBoss')
ROOT = Path(__file__).resolve().parents[1]
PLAN_PATH = ROOT / 'Tool' / 'DistributionDataPlan.json'


def WriteJson(path_, value_):
    """一覧はUTF-8 BOMなし・CRLFで保存し、配布する実Assetのバイト列には触れない。"""
    path_.write_bytes((json.dumps(value_, ensure_ascii=False, indent=2) + '\n').replace('\n', '\r\n').encode('utf-8'))


def Strings(value_):
    """JSONの文字列値だけを列挙し、保存キーをAsset名と誤認しない。"""
    if isinstance(value_, str):
        yield value_
    elif isinstance(value_, dict):
        for child_ in value_.values():
            yield from Strings(child_)
    elif isinstance(value_, list):
        for child_ in value_:
            yield from Strings(child_)


def ReadMetadata(path_):
    """glTF/GLBのURIを含むJSONメタデータを読み、埋込み画像は外部PNGとして扱わない。"""
    if path_.suffix.lower() == '.glb':
        with path_.open('rb') as stream_:
            magic_, version_, size_ = struct.unpack('<III', stream_.read(12))
            if magic_ != 0x46546C67 or version_ != 2:
                raise ValueError('Unsupported GLB header')
            while stream_.tell() < size_:
                length_, kind_ = struct.unpack('<II', stream_.read(8))
                if kind_ == 0x4E4F534A:
                    return json.loads(stream_.read(length_).decode('utf-8').rstrip('\x00 '))
                stream_.seek(length_, 1)
        return {}
    return json.loads(path_.read_text(encoding='utf-8-sig'))


def BuildPlan():
    """静的シーン、動的ロード、先読み、モデルの相対URIを閉包として追跡する。"""
    dataRoot_ = ROOT / 'Data'
    files_ = sorted(path_ for path_ in dataRoot_.rglob('*') if path_.is_file())
    fileSet_ = set(files_)
    for path_ in dataRoot_.rglob('*'):
        if path_.is_symlink() or (hasattr(path_, 'is_junction') and path_.is_junction()):
            raise RuntimeError('Data contains a link: ' + str(path_))
    byPath_ = {path_.relative_to(ROOT).as_posix().casefold(): path_ for path_ in files_}
    suffixes_ = defaultdict(set)
    aliases_ = defaultdict(set)
    for path_ in files_:
        parts_ = path_.relative_to(ROOT).as_posix().split('/')
        for index_ in range(len(parts_) - 1):
            suffixes_['/'.join(parts_[index_:]).casefold()].add(path_)
        if path_.suffix.lower() == '.json' and not any(part_ in {'Scenes', 'Editor', 'Cache'} for part_ in parts_[1:-1]):
            aliases_[path_.stem.casefold()].add(path_)
    reached_ = set()
    reasons_ = {}
    queue_ = deque()
    unresolved_ = set()
    errors_ = []

    def Add(path_, reason_):
        if path_ not in reached_:
            reached_.add(path_)
            reasons_[path_.relative_to(ROOT).as_posix()] = reason_
            queue_.append(path_)

    def Resolve(value_, origin_):
        if not value_ or value_.startswith(('data:', 'http:', 'https:', '__')):
            return
        value_ = unquote(value_).replace('\\\\', '/').replace('\\', '/').strip()
        if not value_:
            return
        candidates_ = set()
        normalized_ = os.path.normpath(value_).replace('\\', '/').removeprefix('./')
        # ボーン名・イベント名など大量の非パス文字列では、Windowsの実パス照会を行わない。
        if '/' not in normalized_ and Path(normalized_).suffix.lower() not in MODEL_EXTENSIONS | {'.png', '.json', '.vfxmat'}:
            for candidate_ in aliases_.get(normalized_.casefold(), ()):
                Add(candidate_, str(origin_) + ' -> ' + value_)
            return
        if normalized_.casefold().startswith('data/'):
            direct_ = byPath_.get(normalized_.casefold())
            if direct_:
                candidates_.add(direct_)
            elif Path(normalized_).suffix.lower() in MODEL_EXTENSIONS | {'.png', '.json', '.vfxmat'}:
                unresolved_.add((str(origin_), value_))
        if origin_ != 'runtime-code':
            parent_ = Path(origin_).parent
            relative_ = Path(os.path.normpath(str(ROOT / parent_ / value_)))
            try:
                relativeKey_ = relative_.relative_to(ROOT).as_posix().casefold()
                if relativeKey_ in byPath_:
                    # glTF内の同名テクスチャは、そのモデルからの相対参照を優先する。
                    Add(byPath_[relativeKey_], str(origin_) + ' -> ' + value_)
                    return
            except ValueError:
                pass
        for key_ in (normalized_, normalized_ + '.json', normalized_ + '.png', normalized_ + '.vfxmat'):
            if '/' in key_:
                candidates_.update(suffixes_.get(key_.casefold(), ()))
        if '/' not in normalized_:
            candidates_.update(aliases_.get(Path(normalized_).stem.casefold(), ()))
        for candidate_ in candidates_:
            Add(candidate_, str(origin_) + ' -> ' + value_)

    for scene_ in SCENES:
        path_ = dataRoot_ / 'Scenes' / scene_ / 'SceneList.json'
        if not path_.is_file():
            raise FileNotFoundError(path_)
        Add(path_, 'game-loop scene')
        sceneData_ = ReadMetadata(path_)
        for name_ in sceneData_.get('ObjectList', []):
            object_ = path_.parent / 'Objects' / (name_ + '.json')
            if not object_.is_file():
                raise FileNotFoundError(object_)
            Add(object_, 'scene ObjectList: ' + scene_)
    # エンジンが一括先読みするAssetは、未描画でも参照先を必要とするため保持する。
    for directory_ in PRELOAD_ROOTS + CONFIG_ROOTS:
        for path_ in (dataRoot_ / directory_).rglob('*'):
            if path_.is_file() and path_.suffix.lower() in {'.json', '.vfxmat'}:
                Add(path_, 'runtime preload/config: ' + directory_)
    # コードからの直接ロードと、HUD等のフォルダー相対名の組合せも拾う。
    for path_ in (ROOT / 'Code').rglob('*'):
        if path_.suffix.lower() not in {'.cpp', '.h'} or 'Editor' in path_.parts or '.Gui.' in path_.name:
            continue
        source_ = path_.read_text(encoding='utf-8-sig', errors='replace')
        for literal_ in re.findall(r'"([^"\r\n]*)"', source_):
            if '\\n' not in literal_ and len(literal_) < 1000:
                Resolve(literal_, 'runtime-code')
    while queue_:
        path_ = queue_.popleft()
        if len(reached_) % 200 == 0:
            print(f'Tracing dependencies: {len(reached_)} reached / {len(queue_)} queued', flush=True)
        extension_ = path_.suffix.lower()
        if extension_ in {'.gltf', '.glb', '.cereal'}:
            for suffix_ in ('.gltf', '.glb', '.cereal'):
                sibling_ = byPath_.get(path_.with_suffix(suffix_).relative_to(ROOT).as_posix().casefold())
                if sibling_:
                    Add(sibling_, 'model source/cache: ' + path_.relative_to(ROOT).as_posix())
        if extension_ in {'.json', '.gltf', '.glb', '.vfxmat'}:
            try:
                metadata_ = ReadMetadata(path_)
                for value_ in Strings(metadata_):
                    Resolve(value_, path_.relative_to(ROOT).as_posix())
                if extension_ in {'.gltf', '.glb'}:
                    for section_ in ('buffers', 'images'):
                        for entry_ in metadata_.get(section_, []):
                            uri_ = entry_.get('uri', '')
                            if uri_ and not uri_.startswith('data:'):
                                target_ = (path_.parent / unquote(uri_).replace('\\', '/')).resolve()
                                if target_ in fileSet_:
                                    Add(target_, 'model URI: ' + path_.relative_to(ROOT).as_posix())
                                else:
                                    unresolved_.add((path_.relative_to(ROOT).as_posix(), uri_))
            except (ValueError, OSError, struct.error) as error_:
                errors_.append({'path': path_.relative_to(ROOT).as_posix(), 'error': str(error_)})
        elif extension_ in {'.obj', '.mtl'}:
            for line_ in path_.read_text(encoding='utf-8-sig', errors='replace').splitlines():
                if line_.startswith(('mtllib ', 'map_', 'bump ')):
                    Resolve(line_.split(' ', 1)[-1].strip(), path_.relative_to(ROOT).as_posix())

    kept_, omitted_ = [], []
    for path_ in files_:
        candidate_ = path_.suffix.lower() == '.png' or (
            path_.suffix.lower() in MODEL_EXTENSIONS and 'Model' in path_.relative_to(dataRoot_).parts)
        record_ = {'path': path_.relative_to(ROOT).as_posix(), 'bytes': path_.stat().st_size,
                   'mtimeNs': path_.stat().st_mtime_ns}
        if candidate_ and path_ not in reached_:
            omitted_.append(record_)
        else:
            kept_.append(record_)
    result_ = {'createdUtc': datetime.now(timezone.utc).isoformat(), 'sourceRoot': str(ROOT),
               'scenes': list(SCENES), 'policy': 'Exclude unreferenced model files under Data/Model and PNG; retain other Data files unchanged. Runtime preloads are dependencies.',
               'kept': kept_, 'omitted': omitted_, 'reasons': reasons_,
               'sourceMissingReferences': [{'origin': origin_, 'reference': reference_} for origin_, reference_ in sorted(unresolved_)],
               'metadataErrors': errors_, 'keptBytes': sum(item_['bytes'] for item_ in kept_),
               'omittedBytes': sum(item_['bytes'] for item_ in omitted_)}
    WriteJson(PLAN_PATH, result_)
    print(json.dumps({key_: value_ for key_, value_ in result_.items() if key_ in {'scenes', 'keptBytes', 'omittedBytes'}}, ensure_ascii=True), flush=True)
    print(f"Keep {len(kept_)} files; omit {len(omitted_)} files; metadata errors {len(errors_)}; source missing references {len(unresolved_)}", flush=True)
    print('Plan: ' + str(PLAN_PATH), flush=True)
    return result_


def CopyPlan(destination_):
    """承認済みの一覧だけを新規Dataフォルダーへ複製し、全ファイルをSHA-256照合する。"""
    plan_ = json.loads(PLAN_PATH.read_text(encoding='utf-8'))
    if Path(plan_['sourceRoot']).resolve() != ROOT:
        raise RuntimeError('Plan source root mismatch')
    if plan_['metadataErrors']:
        raise RuntimeError('Resolve metadata errors before copying')
    destination_ = destination_.resolve()
    if destination_ == ROOT or ROOT in destination_.parents or destination_ in ROOT.parents:
        raise RuntimeError('Distribution must be outside the source project')
    targetRoot_ = destination_ / 'Data'
    if targetRoot_.exists():
        raise FileExistsError('Existing Data will not be overwritten: ' + str(targetRoot_))
    if (destination_ / 'DataCopyManifest.json').exists():
        raise FileExistsError('Existing manifest will not be overwritten')
    # 調査後にAssetやSceneが増えていれば、古い一覧のまま配布しない。
    currentFiles_ = {path_.relative_to(ROOT).as_posix() for path_ in (ROOT / 'Data').rglob('*') if path_.is_file()}
    if currentFiles_ != {item_['path'] for item_ in plan_['kept'] + plan_['omitted']}:
        raise RuntimeError('Data file list changed; regenerate plan')
    for item_ in plan_['kept']:
        relative_ = Path(item_['path'])
        if relative_.is_absolute() or '..' in relative_.parts or relative_.parts[0] != 'Data':
            raise RuntimeError('Invalid relative copy path')
        source_ = (ROOT / item_['path']).resolve()
        if ROOT / 'Data' not in source_.parents or source_.is_symlink():
            raise RuntimeError('Invalid source file: ' + str(source_))
        if source_.stat().st_size != item_['bytes'] or source_.stat().st_mtime_ns != item_['mtimeNs']:
            raise RuntimeError('Source changed; regenerate plan: ' + str(source_))
    if shutil.disk_usage(destination_.parent).free < plan_['keptBytes'] + 128 * 1024**2:
        raise RuntimeError('Insufficient space')
    targetRoot_.mkdir(parents=True, exist_ok=False)
    verified_ = []
    for index_, item_ in enumerate(plan_['kept'], 1):
        source_ = ROOT / item_['path']
        target_ = destination_ / item_['path']
        target_.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_, target_)
        with source_.open('rb') as stream_:
            sourceHash_ = hashlib.file_digest(stream_, 'sha256').hexdigest()
        with target_.open('rb') as stream_:
            targetHash_ = hashlib.file_digest(stream_, 'sha256').hexdigest()
        if sourceHash_ != targetHash_:
            raise RuntimeError('Copy verification failed: ' + str(target_))
        verified_.append({'path': item_['path'], 'bytes': item_['bytes'], 'sha256': targetHash_})
        if index_ % 100 == 0:
            print(f'Copied and verified {index_}/{len(plan_["kept"])} files', flush=True)
    actual_ = {path_.relative_to(destination_).as_posix() for path_ in targetRoot_.rglob('*') if path_.is_file()}
    if actual_ != {item_['path'] for item_ in verified_}:
        raise RuntimeError('Destination file list differs from plan')
    WriteJson(destination_ / 'DataCopyManifest.json', {'sourceRoot': str(ROOT), 'scenes': plan_['scenes'],
              'verifiedFiles': verified_, 'omitted': plan_['omitted'], 'sourceMissingReferences': plan_['sourceMissingReferences'],
              'keptBytes': plan_['keptBytes'], 'omittedBytes': plan_['omittedBytes']})
    print('Completed: ' + str(targetRoot_) + f' ({len(verified_)} SHA-256 verified files)', flush=True)


if __name__ == '__main__':
    parser_ = argparse.ArgumentParser(description=__doc__)
    parser_.add_argument('--copy-to', type=Path)
    arguments_ = parser_.parse_args()
    if arguments_.copy_to:
        CopyPlan(arguments_.copy_to)
    else:
        BuildPlan()
