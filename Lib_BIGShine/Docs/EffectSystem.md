# Effect System

## 目次

- [生成インターフェース](#生成インターフェース)
- [管理と再利用](#管理と再利用)
- [Particle と MeshVFX](#particle-と-meshvfx)
- [GPU Particle](#gpu-particle)
- [関連コード](#関連コード)

## 生成インターフェース

`GameObjectManager` は `EffectManager` を所有し、ゲーム側は各種 Spawn Request を渡して Particle、MeshVFX、Decal、Impact Effect などを生成します。リクエストには Asset、位置・法線、寿命等をまとめます。呼び出し側から個々の描画登録を直接行わず、Manager が生成・寿命管理・描画側との接続を担当します。

## 管理と再利用

EffectManager は管理対象 Effect、遅延生成予約、Asset キャッシュ、Asset 名解決用 Map、種別別 Object Pool を保持します。生成済み Effect には寿命と Pool 種別を持たせ、終了後に Pool へ戻す経路があります。公開 API は GameObject を返す場合がありますが、汎用 Handle 型は確認できません。Pool での所有確認には Object と lifetime owner の組み合わせを用いる API があります。

## Particle と MeshVFX

Particle は `ParticleComponent` と Emitter 設定を持ち、Asset の Emitter 単位で再生／停止できます。MeshVFX はメッシュを用いた効果の Component／Resource／RenderSystem を持ちます。用途や形状の違いは Asset と Shader の実装に依存します。VFX Sequence Asset は Particle と MeshVFX を時間 Track でまとめる構造です。

## GPU Particle

Shader 群には初期化、積分、Emit、描画用 Compute／Vertex／Geometry／Pixel Shader が存在します。これは GPU 上の Particle 処理経路が用意されていることを示します。全 Particle Asset が GPU 処理を使うとは限らないため、具体的な Asset／Emitter の使用条件は ParticleEmitter と ParticleRenderSystem の実装を参照してください。

### 関連コード

- [EffectManager.h](../Code/Engine/GamePlay/System/EffectManager.h)
- [EffectManager.cpp](../Code/Engine/GamePlay/System/EffectManager.cpp)
- [ParticleComponent.h](../Code/Engine/Graphics/Particle/ParticleComponent.h)
- [ParticleEmitter.h](../Code/Engine/Graphics/Particle/ParticleEmitter.h)
- [ParticleRenderSystem.cpp](../Code/Engine/Graphics/RenderSystem/ParticleRenderSystem.cpp)
- [MeshVFXComponent.cpp](../Code/Engine/Graphics/VFX/MeshVFXComponent.cpp)
- [MeshVFXRenderSystem.cpp](../Code/Engine/Graphics/VFX/MeshVFXRenderSystem.cpp)
- [integrate_particle_cs.hlsl](../Shader/GPUParticle/integrate_particle_cs.hlsl)
- [particle_gs.hlsl](../Shader/GPUParticle/particle_gs.hlsl)
- [ParticleEditorWindow.cpp](../Code/Editor/Workspace/ParticleEditorWindow.cpp)

紹介する API と管理処理は [CodeHighlights](CodeHighlights.md) を参照してください。
