#include "PrecompiledHeader.h"

#include "ComponentBase.h"

#include "Engine/GamePlay/Core/Object/GameObjectManager.h"

GameObjectManager& Component::GetManager()
{
    // 親であるGameObjectを経由して、マネージャーを呼び出す
    return GetGameObject()->GetManager();
}
