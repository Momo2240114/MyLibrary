#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "ComponentRegistry.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/Resource/Sprite.h"
#include "Engine/Physics/BoxCollider.h"
#include "Engine/Physics/CapsuleCollider.h"
#include "Engine/Physics/SphereCollider.h"
#include "Engine/Graphics/Decal/DecalComponent.h"
#include "Engine/UI/UIButton.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/GamePlay/Camera/CameraDirectorComponent.h"
#include "Engine/GamePlay/EventSystem/ActionSequence.h"
#include "Engine/GamePlay/Camera/FovDirector.h"
#include "Engine/GamePlay/Transform/TransformDirector.h"
#include "Engine/GamePlay/Graphics/LightBlinkerComponent.h"

#include "Engine/Math/EasingComponent.h"
#include "Engine/Math/Curve/CurveComponent.h"
#include "Engine/GamePlay/Transform/ModelAttachmentComponent.h"
#include "Engine/Graphics/PostEffect/ColorFilterDirector.h"
#include "Engine/UI/NumberSwitcher.h"
//#include "CurvePlayerComponent.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/GamePlay/Camera/FollowCameraComponent.h"
#include "Game/Charcter/Player/PlayerInput.h"
#include "Engine/GamePlay/Core/Component/Movement.h"


// Registerの実装。GameObject.h の定義を知っている必要がある。
template<typename T>
void ComponentRegistry::Register(const std::string& name, bool csllStart) {
    // [=] を追加して csllStart をキャプチャする
    creators[name] = [=](std::shared_ptr<GameObject> go, bool overrideStart) -> std::shared_ptr<Component> {

        // 1. Register時の設定(csllStart)とCreate時の引数(overrideStart)を組み合わせて判断
        // ここでは Create 呼び出し時の引数を優先する例
        bool finalStart = csllStart && overrideStart;

        // 2. AddComponent は <T> までの型指定。引数として bool を渡す
        auto comp = go->AddComponent<T>(finalStart);

        // 3. static_pointer_cast に第2引数は不要
        return std::static_pointer_cast<Component>(comp);
        };
}
   
void RegisterAllComponents()
{
    auto& registry = ComponentRegistry::Instance();

    // システム・レンダリング系
    registry.Register<CameraComponent>("CameraComponent");
    registry.Register<GltfModelCommponennt>("GltfModel");
    registry.Register<ModelAttachmentComponent>("ModelAttachment");
    registry.Register<AnimationControllerComponent>("AnimationControllerComponent");
    registry.Register<SpriteRenderer>("SpriteRenderer");

    // ライト・エフェクト系
    registry.Register<LightComponent>("LightComponent");
    registry.Register<DecalComponent>("DecalComponent");
    registry.Register<ParticleComponent>("ParticleComponent");


    // 物理・コリジョン系
    registry.Register<BoxCollider>("BoxCollider");
    registry.Register<CapsuleCollider>("CapsuleCollider");
    registry.Register<SphereCollider>("SphereCollider");

    //ゲームシステム
    registry.Register<UIButton>("UIButton");
    registry.Register<CameraDirector>("CameraDirector");
    registry.Register<ActionSequence>("ActionSequence");
    registry.Register<FovDirector>("FovDirector");
    registry.Register<TransformDirector>("TransformDirector");
    registry.Register<ColorFilterDirector>("ColorFilterDirector");
    registry.Register<LightBlinkerComponent>("LightBlinker");
    registry.Register<EasingComponent>("Easing");
    registry.Register<CurveComponent>("CurveComponent");
    registry.Register<NumberSwitcher>("NumberSwitcher");
    registry.Register<PlayerInput>("PlayerInput");
    registry.Register<Movement>("Movement");
    registry.Register<FollowCameraComponent>("FollowCameraComponent");
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

