#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "ComponentRegistry.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/Resource/Sprite.h"
#include "Engine/Physics/BoxCollider.h"
#include "Engine/Physics/CapsuleCollider.h"
#include "Engine/Physics/SphereCollider.h"
#include "Engine/Physics/CylinderCollider.h"
#include "Engine/Physics/MeshCollider.h"
#include "Engine/Physics/CharacterCollider.h"
#include "Engine/Physics/HitBox.h"
#include "Engine/Physics/HurtBox.h"
#include "Engine/Physics/JustDodgeCollider.h"
#include "Engine/Physics/ContainmentVolumeComponent.h"
#include "Engine/Graphics/Decal/DecalComponent.h"
#include "Engine/UI/UIButton.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Graphics/Resource/Animation/IKMapComponent.h"
#include "Engine/Graphics/Resource/Animation/SpringBoneSimulationComponent.h"
#include "Engine/GamePlay/Camera/CameraDirectorComponent.h"
#include "Engine/GamePlay/EventSystem/ActionSequence.h"
#include "Engine/GamePlay/Camera/FovDirector.h"
#include "Engine/GamePlay/Transform/TransformDirector.h"
#include "Engine/GamePlay/Graphics/LightBlinkerComponent.h"

#include "Engine/Math/EasingComponent.h"
#include "Engine/Math/Curve/CurveComponent.h"
#include "Engine/GamePlay/Transform/ModelAttachmentComponent.h"
#include "Engine/GamePlay/Transform/BoneSegmentAttachmentComponent.h"
#include "Engine/Graphics/PostEffect/ColorFilterDirector.h"
#include "Engine/Graphics/PostEffect/RadialBlurDirector.h"
#include "Engine/Graphics/PostEffect/VignetteDirector.h"
#include "Engine/GamePlay/EventSystem/TimeScaleDirector.h"
#include "Engine/UI/NumberSwitcher.h"
//#include "CurvePlayerComponent.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/GamePlay/Camera/FollowCameraComponent.h"
#include "Engine/GamePlay/Camera/ThirdPersonCameraComponent.h"
#include "Engine/GamePlay/Camera/CameraLockOnComponent.h"
#include "Engine/GamePlay/Camera/CameraCollisionComponent.h"
#include "Engine/GamePlay/Camera/LockOnTargetComponent.h"
#include "Engine/GamePlay/Core/Component/Movement.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Combat/HitReactionComponent.h"
#include "Engine/GamePlay/Combat/JustDodgeReactionComponent.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Game/Combat/DebugTools/DebugHitHistoryResetComponent.h"
#include "Engine/GamePlay/VFX/ImpactEffectComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/Graphics/VFX/MaterialOverlayComponent.h"
#include "Engine/Graphics/VFX/AfterimageComponent.h"
#include "Engine/Graphics/VFX/AfterimageTrailComponent.h"
#include "Engine/Audio/AudioComponent.h"


// Registerの実装。GameObject.h の定義を知っている必要がある。
template<typename T>
void ComponentRegistry::Register(const std::string& name_, bool callStart_) {
    auto createFunc_ = [=](std::shared_ptr<GameObject> go_, bool overrideStart_) -> std::shared_ptr<Component> {
        const bool finalStart_ = callStart_ && overrideStart_;
        auto comp_ = go_->AddComponent<T>(finalStart_);
        return std::static_pointer_cast<Component>(comp_);
        };

    AddCreator(name_, createFunc_);

    const std::string typeName_ = T().GetName();
    if (!typeName_.empty() && typeName_ != name_)
    {
        AddCreator(typeName_, createFunc_);
    }
}
   
void RegisterAllComponents()
{
    auto& registry = ComponentRegistry::Instance();

    // システム・レンダリング系
    registry.Register<CameraComponent>("CameraComponent");
    registry.Register<GltfModelCommponennt>("GltfModel");
    registry.Register<ModelAttachmentComponent>("ModelAttachment");
    registry.Register<BoneSegmentAttachmentComponent>("BoneSegmentAttachment");
    registry.Register<AnimationControllerComponent>("AnimationControllerComponent");
    registry.Register<IKMapComponent>("IKMapComponent");
    registry.Register<SpringBoneSimulationComponent>("SpringBoneSimulationComponent");
    registry.Register<SpriteRenderer>("SpriteRenderer");

    // ライト・エフェクト系
    registry.Register<LightComponent>("LightComponent");
    registry.Register<DecalComponent>("DecalComponent");
    registry.Register<ParticleComponent>("ParticleComponent");
    registry.Register<MeshVFXComponent>("MeshVFXComponent");
    registry.Register<MaterialOverlayComponent>("MaterialOverlayComponent");
    registry.Register<AfterimageComponent>("AfterimageComponent");
    registry.Register<AfterimageTrailComponent>("AfterimageTrailComponent");
    registry.Register<AudioComponent>("AudioComponent");
    registry.Register<ImpactEffectComponent>("ImpactEffectComponent");


    // 物理・コリジョン系
    registry.Register<BoxCollider>("BoxCollider");
    registry.Register<CapsuleCollider>("CapsuleCollider");
    registry.Register<SphereCollider>("SphereCollider");
    registry.Register<CylinderCollider>("CylinderCollider");
    registry.Register<MeshCollider>("MeshCollider");
    registry.Register<CharacterCollider>("CharacterCollider");
    registry.Register<HitBox>("HitBox");
    registry.Register<HurtBox>("HurtBox");
    registry.Register<JustDodgeCollider>("JustDodgeCollider");
    registry.Register<ContainmentVolumeComponent>("ContainmentVolumeComponent");

    //ゲームシステム
    registry.Register<UIButton>("UIButton");
    registry.Register<CameraDirector>("CameraDirector");
    registry.Register<ActionSequence>("ActionSequence");
    registry.Register<FovDirector>("FovDirector");
    registry.Register<TransformDirector>("TransformDirector");
    registry.Register<ColorFilterDirector>("ColorFilterDirector");
    registry.Register<RadialBlurDirector>("RadialBlurDirector");
    registry.Register<VignetteDirector>("VignetteDirector");
    registry.Register<TimeScaleDirector>("TimeScaleDirector");
    registry.Register<LightBlinkerComponent>("LightBlinker");
    registry.Register<EasingComponent>("Easing");
    registry.Register<CurveComponent>("CurveComponent");
    registry.Register<NumberSwitcher>("NumberSwitcher");
    registry.Register<Movement>("Movement");
    registry.Register<CharacterMovementComponent>("CharacterMovementComponent");
    registry.Register<BlackboardComponent>("BlackboardComponent");
    registry.Register<HitReactionComponent>("HitReactionComponent");
    registry.Register<JustDodgeReactionComponent>("JustDodgeReactionComponent");
    registry.Register<HealthComponent>("HealthComponent");
    registry.Register<DebugHitHistoryResetComponent>("DamageBoxComponent");
    registry.Register<FollowCameraComponent>("FollowCameraComponent");
    registry.Register<ThirdPersonCameraComponent>("ThirdPersonCameraComponent");
    registry.Register<CameraLockOnComponent>("CameraLockOnComponent");
    registry.Register<CameraCollisionComponent>("CameraCollisionComponent");
    registry.Register<LockOnTargetComponent>("LockOnTargetComponent");
}


std::shared_ptr<Component> ComponentRegistry::Create(const std::string& name, std::shared_ptr<GameObject> go,bool casllStart)
{
    auto it = creators.find(name);
    if (it == creators.end())
        return nullptr;

    auto existing = go->GetComponentByName(name);
    if (existing)
        return existing;

    return it->second(go,casllStart);
}


