#include "MaterialOverlayComponent.h"

MaterialOverlayComponent::MaterialOverlayComponent()
{
    MeshVFXPart part_;
    part_.renderEnabled = true;
    part_.useMaterialBaseTexture = true;
    part_.useMaterialNormalTexture = true;
    GetAsset().parts.push_back(part_);
}
