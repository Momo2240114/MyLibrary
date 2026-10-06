# プレイヤーのSkill Loadout

Skill定義は共有ライブラリ、装備はPlayerのScene、CooldownはPlayerの実行時情報へ分離しています。
新しいManagerやComponentは追加していません。

## 初期装備と入力

| 入力 | スロット | 初期装備 | Cooldown |
| --- | --- | --- | --- |
| L1 + □ | SkillSlot0 | PhantomRush | 3秒 |
| L1 + △ | SkillSlot1 | Breakthrough | 5秒 |
| L1 + ○ | SkillSlot2 | RisingJudgment | 8秒 |

PlayerInputResolver::ResolveSkillSlotは、優先順位が最も高いスロット番号だけを返します。
PlayerControllerComponent::RequestSkillSlotが既存Stateの識別子へ変換し、
GetSkillDefinitionで装備名からカタログの定義を取得します。
その後は既存のPlayerSkillState → SkillExecution → Animation / Action / Effectです。
内部のState番号は変更せず、Slot0はSKILL_B、Slot1はSKILL_A、Slot2はSKILL_Cへ接続します。
入力処理に具体的なSkill名の判定はありません。

空スロットや存在しない装備名では使用要求を拒否します。
その複合入力を通常攻撃へ流さない優先順位は維持しています。

## Editorでの変更

Player Inspector、またはSkill WorkspaceのPlayer / AIタブで、
Player Skill LoadoutのSkillSlot0 / 1 / 2のDropdownから装備を変更します。
選択は直ちにPlayerへ反映されます。Scene保存で永続化してください。
Skill実行中は全スロットの編集を禁止します。

Skill WorkspaceのEquip / Preview Slotで枠を選び、Save & Apply Bindingを押すと、
編集中のSkillをライブラリへ保存・再読込してその枠へ装備します。
Save & Playも同じスロットから使用要求を発行します。
ライブラリの保存とPlayerの装備のScene保存は別です。

例: SkillSlot1でHealを選択すると、次のL1 + △はBreakthroughではなくHealになります。
同じSkillを複数枠へ装備することもできます。カテゴリによる枠の制限はありません。

## 保存形式と互換性

PlayerControllerComponentのScene設定へ次を保存します。

```json
{
    "SkillLoadout": {
        "Slot0": "PhantomRush",
        "Slot1": "Breakthrough",
        "Slot2": "RisingJudgment"
    }
}
```

SkillライブラリのInputsは、ActionではなくSlotを持ちます。

```json
{
    "Slot": 1,
    "ModifierAction": "Player.SkillModifier",
    "TriggerAction": "Player.StrongAttack",
    "Priority": 100
}
```

旧InputsのAction=SkillA/B/Cも読み込めます。
旧SceneにSkillLoadoutがない場合だけ、旧DefinitionのActionを初期装備へ移行します。
新しい定義のActionは不要で、名前だけで全定義を装備候補へ登録します。
保存された装備名がカタログに存在しない場合は、その枠を使用不可にして参照切れを防ぎます。

## 定義・装備・実行時情報

- SkillDefinition: 名前、分類、使用場所、Cooldown、Phaseと演出の設定。
- PlayerSkillLoadout: 3枠へ装備したSkill名だけ。PlayerSkillDefinition.h内の小さなデータ型です。
- PlayerControllerComponent::skillCooldowns: Skill名ごとの残りCooldown。保存しません。
- SkillExecution: 開始時に複製した定義とPhaseの実行状況。

装備の変更、ライブラリ再読込、中断はCooldownをリセットしません。
他枠へ移しても同名Skillの残り時間を使います。同じSkillを複数枠へ装備しても待ち時間を共有します。
Scene開始時と既存のReset Skill Cooldownsボタンではリセットできます。

定義にCategoryを追加しました。ATTACK / RUSH / LAUNCHER / PROJECTILE / HEAL / BUFF / COUNTER / UTILITYです。
分類はEditor表示用で、実行内容を巨大なswitchで分岐させる用途には使用していません。
CanUseOnGround / CanUseInAirも保存し、PlayerのCanExecuteSkillで使用条件を確認します。
旧JSONの既定値は地上使用可・空中使用不可です。
攻撃、回復、Buff、Projectileなどの実処理は引き続き既存ActionとEventから呼び出します。

## 追加サンプル: Heal

- カテゴリ: HEAL
- State: Heal_1段目
- 実在クリップ: SS_Sp_Skill2（モデル内Index 7）
- Cooldown: 10秒
- 地上・空中で使用可能。初期装備には含めません。
- 0.35秒: 既存ParryStar Particleを仮演出として生成。
- 0.50秒: Health_Heal ActionでHPを25回復。最大HPを超えず、死亡中は回復しません。
- Animation終了後に既存の通常State復帰処理へ接続。空中では既存のJump復帰経路を使用します。
- Soundは空イベントです。回復音Actionを後から指定してください。

回復量はAnimation EventのHealth / HealAmount書き込みで調整します。
ParticleはSkillEffect / ParticleAsset、ParticleLifeTime、HeightOffsetの書き込みで調整します。
SkillEffect.PlayParticleは既存EffectManagerへの汎用Actionで、特定Skill名へ依存しません。
Health_HealもHealthComponentの汎用Actionです。
PowerBuffとMagicProjectileのサンプルは今回追加していません。
大規模なBuff・Projectileシステムを増設せず、ロードアウトとHealによる差し替えを実装範囲としています。
将来は登録済みのBuff／Projectile Actionを同じPhase・Event経路から使用できます。

## 将来のSkillSelect画面向けAPI

PlayerControllerComponentで次を利用できます。

- GetSkillCatalog(): 装備候補の一覧。
- GetSkillDefinitionByName(name): Skill名から定義を取得。
- SetEquippedSkill(slotIndex, name): 空枠、または登録済みSkillを装備。
- GetEquippedSkillName(slotIndex): 装備名を取得。
- CanEditSkillLoadout(): 実行中の編集禁止を確認。
- RequestSkillSlot(slotIndex): スロットから通常の使用要求を発行。
- GetRemainingSkillCooldown(name): 装備していないSkillの待ち時間も参照。
- GetRemainingSkillCooldown(action) / GetSkillCooldownDuration(action): 現在そのスロットに装備したSkillの時間。
- IsSkillCooldownReady(action) / CanExecuteSkill(action): Cooldownと使用条件を確認。

## 今回追加・変更したファイル

変更したソース:

- Code/Game/Charcter/Player/PlayerSkillDefinition.h / .cpp
- Code/Game/Charcter/Player/PlayerInputResolver.h / .cpp
- Code/Game/Charcter/Player/PlayerControllerComponent.h / .cpp
- Code/Game/Charcter/Player/PlayerSkillState.cpp
- Code/Engine/GamePlay/Combat/SkillDefinition.h / .cpp
- Code/Engine/GamePlay/Combat/HealthComponent.cpp
- Code/Editor/Workspace/SkillWorkspaceWindow.h / .cpp

変更したデータ・説明:

- Data/Skills/PlayerSkills.json
- Data/Scenes/MainScene/Objects/PlayerModel.json
- Data/Animation/GraphData/Player.json
- Data/Skills/README.md
- Data/Skills/SkillWorkspace.md
- Data/Skills/PrototypeSkills.md

追加:

- Data/Animation/StateData/Player/States/Heal_01.json
- Data/Skills/SkillLoadout.md

新規C++ファイルはなく、既存のVisual Studio登録とFilterを使用します。
Heal StateはGraphとPlayerModelのStateAssetsへ登録しています。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
