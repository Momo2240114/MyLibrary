#pragma once

#include "MeshVFXComponent.h"

class MaterialOverlayComponent : public MeshVFXComponent
{
public:
    MaterialOverlayComponent();

    const std::string& GetName() const override
    {
        static std::string name = "MaterialOverlayComponent";
        return name;
    }

    bool UsesOwnerModel() const override { return true; }
};
