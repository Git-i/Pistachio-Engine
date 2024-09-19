#include "Cubemap.h"
#include "Pistachio/Renderer/RendererBase.h"
#include <ranges>
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
        constexpr std::array preferred_transcode_formats = {
            RHI::Format::BC7_UNORM, //todo add more formats
        };
        auto to_ktx_transcode_fmt = [](RHI::Format format)
        {
            switch (format)
            {
            case RHI::Format::BC7_UNORM: return KTX_TTF_BC7_RGBA;
            default: return KTX_TTF_NOSELECTION;
            }
        };
        for(const auto format : preferred_transcode_formats)
        {
            const auto support = RendererBase::GetPhysicalDevice()->GetFormatSupportInfo(format, RHI::TextureTilingMode::Optimal);
            if((support & RHI::FormatSupport::SampledImage) != RHI::FormatSupport::None) return to_ktx_transcode_fmt(format);
        }
        return KTX_TTF_BC7_RGBA;
    }
    Error CubeMap::Initialize(pktx::Texture& texture)
    {
        if (!texture.IsCubemap()) return {ErrorType::InvalidFile, "Texture passed in is not a cube map"};

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

        RHI::AutomaticAllocationInfo alloc_info{};
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
        
        RHI::TextureMemoryBarrier barr{
            .AccessFlagsBefore = RHI::ResourceAcessFlags::NONE,
            .AccessFlagsAfter = RHI::ResourceAcessFlags::TRANSFER_WRITE,
            .oldLayout = RHI::ResourceLayout::UNDEFINED,
            .newLayout = RHI::ResourceLayout::TRANSFER_DST_OPTIMAL,
            .texture = ID,
            .previousQueue = RHI::QueueFamily::Ignored,
            .nextQueue = RHI::QueueFamily::Ignored,
            .subresourceRange{
                .imageAspect = RHI::Aspect::COLOR_BIT,
                .IndexOrFirstMipLevel = 0,
                .NumMipLevels = td.mipLevels,
                .FirstArraySlice = 0,
                .NumArraySlices = td.depthOrArraySize
            }
        };
        RendererBase::GetStagingCommandList()->PipelineBarrier(RHI::PipelineStage::TOP_OF_PIPE_BIT, RHI::PipelineStage::TRANSFER_BIT, {}, {&barr,1});
        for(auto mip : std::views::iota(0u, texture.NumLevels()))
        {
            for(auto layer: std::views::iota(0u, texture.NumLayers()))
            {
                for(auto face : std::views::iota(0u, texture.NumFaces()))
                {
                    auto slice = layer * texture.NumFaces() + face;
                    RHI::SubResourceRange range{
                        .imageAspect = RHI::Aspect::COLOR_BIT,
                        .IndexOrFirstMipLevel = mip,
                        .NumMipLevels = 1,
                        .FirstArraySlice = slice,
                        .NumArraySlices = 1
                    };
                    RendererBase::PushTextureUpdate(tex, texture.ImageSize(mip), texture.Data(mip, layer, face).value(), &range, {
                        static_cast<uint32_t>(texture.BaseWidth()/std::pow(2, mip)),
                        static_cast<uint32_t>(texture.BaseHeight()/std::pow(2, mip)),
                        1
                    }, {0,0,0}, td.format);
                }
            }
        }

        barr.AccessFlagsBefore = RHI::ResourceAcessFlags::NONE;
        barr.AccessFlagsAfter = RHI::ResourceAcessFlags::TRANSFER_WRITE;
        barr.oldLayout = RHI::ResourceLayout::UNDEFINED;
        barr.newLayout = RHI::ResourceLayout::TRANSFER_DST_OPTIMAL;
        RendererBase::GetStagingCommandList()->PipelineBarrier(RHI::PipelineStage::TRANSFER_BIT, RHI::PipelineStage::FRAGMENT_SHADER_BIT, {}, {&barr,1});

        ID = tex;
        view = tex_view;
        return {ErrorType::Success};
    }
    Error CubeMap::Initialize(uint32_t width, uint32_t height, RHI::Format format)
    {
        RHI::Ptr<RHI::Texture> tex;
        RHI::Ptr<RHI::TextureView> tex_view;
        RHI::AutomaticAllocationInfo alloc_info{RHI::AutomaticAllocationCPUAccessMode::None};
        auto err = RendererBase::GetDevice()->CreateTexture(RHI::TextureDesc{
            .type = RHI::TextureType::Texture2D,
            .width = width,
            .height = height,
            .depthOrArraySize = 6,
            .format = format,
            .mipLevels = 1,
            .sampleCount = 1,
            .mode = RHI::TextureTilingMode::Optimal,
            .optimizedClearValue = nullptr,
            .usage = RHI::TextureUsage::CopyDst | RHI::TextureUsage::SampledImage | RHI::TextureUsage::CubeMap
        }, nullptr, nullptr, &alloc_info, 0, RHI::ResourceType::Automatic)
        .handle(
            [&tex](auto&& texture) {
                tex = texture; return Error(ErrorType::Success);
            },
            [](auto&& err) {return Error::FromRHIError(err);}
        );
        if(!err.Successful()) return err;
        err = RendererBase::GetDevice()->CreateTextureView(RHI::TextureViewDesc {
            .type = RHI::TextureViewType::TextureCube,
            .format = format,
            .texture = tex,
            .range {
                .imageAspect = RHI::Aspect::COLOR_BIT,
                .IndexOrFirstMipLevel = 0,
                .NumMipLevels = 1,
                .FirstArraySlice = 0,
                .NumArraySlices = 6
            }
        }).handle([&tex_view](auto&& view) {tex_view = view; return Error(ErrorType::Success);},
            [](auto&& err){return Error::FromRHIError(err);});
        if(!err.Successful()) return err;
        ID = tex;
        view = tex_view;
        RHI::TextureMemoryBarrier barr{
            .AccessFlagsBefore = RHI::ResourceAcessFlags::NONE,
            .AccessFlagsAfter = RHI::ResourceAcessFlags::SHADER_READ,
            .oldLayout = RHI::ResourceLayout::UNDEFINED,
            .newLayout = RHI::ResourceLayout::SHADER_READ_ONLY_OPTIMAL,
            .texture = ID,
            .previousQueue = RHI::QueueFamily::Ignored,
            .nextQueue = RHI::QueueFamily::Ignored,
            .subresourceRange{
                .imageAspect = RHI::Aspect::COLOR_BIT,
                .IndexOrFirstMipLevel = 0,
                .NumMipLevels = 1,
                .FirstArraySlice = 0,
                .NumArraySlices = 6
            }
        };
        RendererBase::GetStagingCommandList()->PipelineBarrier(RHI::PipelineStage::TOP_OF_PIPE_BIT, RHI::PipelineStage::FRAGMENT_SHADER_BIT, {}, {&barr,1});
        return Error(ErrorType::Success);
    }
}