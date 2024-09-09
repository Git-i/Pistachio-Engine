#pragma once
#include "Pistachio/Asset/RefCountedObject.h"
#include "Pistachio/Core/Error.h"
#include "Pistachio/Renderer/Cubemap.h"
namespace Pistachio
{
    /**
     * @brief Used by scenes for skybox, and can be loaded from a skybox file
     * 
     */
    class Skybox : public RefCountedObject
    {
    public:
        [[nodiscard]] static Result<Skybox*> Create(std::string_view path);
        [[nodiscard]] static Result<Skybox*> Create(const void* skybox_memory, size_t size);
        [[nodiscard]] Error Initialize(std::string_view path);
        [[nodiscard]] Error Initialize(const void* skybox_memory, size_t size);
        ~Skybox();
    private:
        CubeMap* base{};
        CubeMap* ir{}; 
        CubeMap* pf{};
    private:
        Error InitializeCubeMaps(pktx::Texture&, pktx::Texture&, pktx::Texture&);
    };
}