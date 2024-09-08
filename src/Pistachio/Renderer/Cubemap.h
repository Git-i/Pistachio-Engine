#pragma once
#include "Pistachio/Asset/RefCountedObject.h"
#include "Pistachio/Core/Error.h"
#include "pktx/texture.h"
namespace Pistachio
{
    class CubeMap : public RefCountedObject
    {
    public:
        [[nodiscard]] static Result<CubeMap*> Create(std::string_view path);
        [[nodiscard]] static Result<CubeMap*> Create(const pktx::Texture& texture);
        [[nodiscard]] Error Initialize(std::string_view path);
        [[nodiscard]] Error Initialize(const pktx::Texture& texture);
    private:
        uint32_t width, height;
        RHI::Ptr<RHI::Texture> ID;
        RHI::Ptr<RHI::TextureView> view;
    };
}