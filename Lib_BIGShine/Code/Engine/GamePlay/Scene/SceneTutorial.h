#pragma once

#include "SceneBase.h"

class SceneTutorial : public SceneBase
{
public:
    SceneTutorial();

    void Initialize() override;
    bool RequiresFreeCursor() const override;
};
