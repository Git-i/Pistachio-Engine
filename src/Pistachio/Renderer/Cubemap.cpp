#include "Cubemap.h"
#include "Pistachio/Renderer/RendererBase.h"
namespace Pistachio
{
    Result<CubeMap*> CubeMap::Create(std::string_view filepath)
    {
        auto res = std::make_unique<CubeMap>();
        auto err = res->Initialize(filepath);
        if(!err.Successful()) return ezr::err(err);
        return res.release();
    }
    Result<CubeMap*> CubeMap::Create(pktx::Texture& texture)
    {
        auto res = std::make_unique<CubeMap>();
        auto err = res->Initialize(texture);
        if(!err.Successful()) return ezr::err(err);
        return res.release();
    }
    Result<CubeMap*> CubeMap::Create(uint32_t width, uint32_t height, RHI::Format format)
    {
        auto res = std::make_unique<CubeMap>();
        return res.release();
    };
    Error CubeMap::Initialize(std::string_view filepath)
    {
        auto tex_res = pktx::Texture::CreateFromNamedFile(filepath, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT);
        if(tex_res.is_err()) return Error::FromKTXError(tex_res.err());
        auto tex = std::move(tex_res).value();
        return Initialize(tex);
    }
    ktx_transcode_fmt_e GetOptimalTranscodeFormat()
    {
        //todo
        return KTX_TTF_BC7_RGBA;
    }
    Error CubeMap::Initialize(pktx::Texture& texture)
    {
        if(!texture.IsCubemap()) return Error(ErrorType::InvalidFile, "Texture passed in is not a cubemap");
        RHI::TextureType type;
        if(texture.NumDimensions() == 1) type = RHI::TextureType::Texture1D;
        else if(texture.NumDimensions() == 2) type = RHI::TextureType::Texture2D;
        else type = RHI::TextureType::Texture3D;
        RHI::TextureTilingMode mode = RHI::TextureTilingMode::Optimal;
        if(texture.NeedsTranscode())
        {
            auto err = texture.Transcode(GetOptimalTranscodeFormat(), 0);
            if(err != KTX_SUCCESS) return Error::FromKTXError(err);
        }
        RHI::TextureDesc td {
            .type = type,
            .width = texture.BaseWidth(),
            .height = texture.BaseHeight(),
            .depthOrArraySize = texture.NumFaces() * texture.NumLayers(),
            .format = RHI::FormatFromVk(texture.GetVkFormat()),
            .mipLevels = texture.NumLevels(),
            .sampleCount = 1,
            .mode = mode,
            .optimizedClearValue = nullptr,
            .usage = RHI::TextureUsage::CopyDst | RHI::TextureUsage::SampledImage | RHI::TextureUsage::CubeMap
        };
        RHI::AutomaticAllocationInfo alloc_info;
        alloc_info.access_mode = RHI::AutomaticAllocationCPUAccessMode::None;
        RHI::Ptr<RHI::Texture> tex;
        RHI::Ptr<RHI::TextureView> tex_view;
        auto err = RendererBase::GetDevice()->CreateTexture(td, nullptr, nullptr, &alloc_info, 0, RHI::ResourceType::Automatic).handle(
            [&tex](auto&& texture) {
                tex = texture; return Error(ErrorType::Success);
            },
            [](auto&& err) {return Error::FromRHIError(err);}
        );
        if(!err.Successful()) return err;
        err = RendererBase::GetDevice()->CreateTextureView(RHI::TextureViewDesc {
            .type = texture.NumLayers() > 1 ? RHI::TextureViewType::TextureCubeArray : RHI::TextureViewType::TextureCube,
            .format = td.format,
            .texture = tex,
            .range {
                .imageAspect = RHI::Aspect::COLOR_BIT,
                .IndexOrFirstMipLevel = 0,
                .NumMipLevels = td.mipLevels,
                .FirstArraySlice = 0,
                .NumArraySlices = td.depthOrArraySize
            }
        }).handle([&tex_view](auto&& view) {tex_view = view; return Error(ErrorType::Success);},
            [](auto&& err){return Error::FromRHIError(err);});
        if(!err.Successful()) return err;
        ID = tex;
        view = tex_view;
        return Error(ErrorType::Success);
    }
}