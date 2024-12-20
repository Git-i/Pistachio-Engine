#include "Components.h"
namespace Pistachio
{
    MeshRendererComponent::MeshRendererComponent(const char* path)
    {
        model = GetAssetManager()->CreateModelAsset(path).value_or(Asset{});
        if(model.GetType() == ResourceType::Invalid)
            PT_CORE_ERROR("Error loading mesh at {}", path);
    }
}