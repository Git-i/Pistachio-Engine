#include "Barrier.h"
#include "Pistachio/Core/Log.h"
#include "ptpch.h"
#include "RendererBase.h"
#include "RenderTexture.h"

namespace Pistachio
{
    Result<RenderTexture*> RenderTexture::Create(uint32_t width, uint32_t height, uint32_t mipLevels, RHI::Format format, const char* name)
    {
        auto returnVal = std::make_unique<RenderTexture>();
        if(auto err = returnVal->CreateStack(width,height,mipLevels,format, name); !err.Successful())
            return ezr::err(std::move(err));
        return returnVal.release();
    }
    Error RenderTexture::CreateStack(uint32_t width, uint32_t height, uint32_t mipLevels, RHI::Format format, const char* name)
	{
        m_width = width;
        m_height = height;
        m_mipLevels = mipLevels;
        m_format = format;
        RHI::TextureDesc desc{};
        desc.depthOrArraySize = 1;
        desc.height = height;
        desc.width = width;
        desc.mipLevels = mipLevels;
        desc.mode = RHI::TextureTilingMode::Optimal;
        desc.optimizedClearValue = nullptr;
        desc.format = format;
        desc.sampleCount = 1;
        desc.type = RHI::TextureType::Texture2D;
        desc.usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::SampledImage | RHI::TextureUsage::CopySrc;
        RHI::AutomaticAllocationInfo allocInfo {.access_mode = RHI::AutomaticAllocationCPUAccessMode::None};
        RHI::Ptr<RHI::Texture> texture;
        RHI::Ptr<RHI::TextureView> textureView;
        auto err = RendererBase::GetDevice()->CreateTexture(desc, nullptr, nullptr, &allocInfo, 0, RHI::ResourceType::Automatic).handle(
            [&texture](auto&& tex) { texture = tex; return Error(ErrorType::Success);},
            [](auto&& err) {return Error::FromRHIError(err);});
        if(!err.Successful()) return err;
        PT_DEBUG_REGION(texture->SetName(name));
        RHI::SubResourceRange range{
            .imageAspect = RHI::Aspect::COLOR_BIT,
            .IndexOrFirstMipLevel = 0,
            .NumMipLevels = mipLevels,
            .FirstArraySlice = 0,
            .NumArraySlices = 1,
        };

        RHI::TextureViewDesc viewDesc{
            .type = RHI::TextureViewType::Texture2D,
            .format = format,
            .texture = texture,
            .range = range,
        };
        err = RendererBase::GetDevice()->CreateTextureView(viewDesc).handle(
            [&textureView](auto&& view) {textureView = view; return Error(ErrorType::Success);},
            [](auto&& err) {return Error::FromRHIError(err);});
        if(!err.Successful()) return err;
        
        RHI::RenderTargetViewDesc rtDesc{
            .TextureArray = false,
            .arraySlice = 0,
            .format = format,
            .textureMipSlice = 0,
        };
        m_ID = texture;
        m_view = textureView;
        RTView = RendererBase::CreateRenderTargetView(m_ID, rtDesc);
        return {};
	}
    RHI::Format RenderTexture::GetFormat() const{return m_format;}
    uint32_t RenderTexture::GetWidth()  const{ return m_width; }
    uint32_t RenderTexture::GetHeight() const{ return m_height; }
    Result<RenderCubeMap*> RenderCubeMap::Create(uint32_t size, uint32_t mipLevels, RHI::Format format, const char* name, RHI::TextureUsage extraUsage)
    {
        auto returnVal = std::make_unique<RenderCubeMap>();
        if(auto err = returnVal->CreateStack(size, mipLevels, format, name,extraUsage); !err.Successful())
            return ezr::err(std::move(err));
        return returnVal.release();
    }
    Error RenderCubeMap::CreateStack(uint32_t size, uint32_t mipLevels, RHI::Format format PT_DEBUG_REGION(, const char* name), RHI::TextureUsage extraUsage)
    {
        m_size = size;
        m_mipLevels = mipLevels;
        m_format = format;
        RHI::TextureDesc desc{};
        desc.depthOrArraySize = 6;
        desc.height = size;
        desc.width = size;
        desc.mipLevels = mipLevels;
        desc.mode = RHI::TextureTilingMode::Optimal;
        desc.optimizedClearValue = nullptr;
        desc.format = format;
        desc.sampleCount = 1;
        desc.type = RHI::TextureType::Texture2D;
        desc.usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::SampledImage | RHI::TextureUsage::CubeMap | extraUsage;
        RHI::AutomaticAllocationInfo allocInfo{.access_mode = RHI::AutomaticAllocationCPUAccessMode::None};
        RHI::Ptr<RHI::Texture> texture;
        RHI::Ptr<RHI::TextureView> textureView;
        auto err = RendererBase::GetDevice()->CreateTexture(desc, nullptr, nullptr, &allocInfo, 0, RHI::ResourceType::Automatic).handle(
            [&texture](auto&& tex){texture = tex; return Error(ErrorType::Success);},
            Error::FromRHIError);
        if(!err.Successful()) return err;
        PT_DEBUG_REGION(m_ID->SetName(name));
        RHI::SubResourceRange range{
            .imageAspect = RHI::Aspect::COLOR_BIT,
            .IndexOrFirstMipLevel = 0,
            .NumMipLevels = mipLevels,
            .FirstArraySlice = 0,
            .NumArraySlices = 6
        };

        err = RendererBase::GetDevice()->CreateTextureView(RHI::TextureViewDesc
        {
            .type = RHI::TextureViewType::TextureCube,
            .format = format,
            .texture = texture,
            .range = range
        }).handle(
            [&textureView](auto&& view) {textureView = view; return Error(ErrorType::Success);},
            Error::FromRHIError);
        if(!err.Successful()) return err;

        for (uint32_t i = 0; i < 6; i++)
        {
            RHI::RenderTargetViewDesc rtDesc{
                .TextureArray = true,
                .arraySlice = i,
                .format = format,
                .textureMipSlice = 0,
            };
            RTViews[i] = RendererBase::CreateRenderTargetView(texture, rtDesc);
        }
        m_ID = texture;
        m_view = textureView;
        return {};
    }
    void RenderCubeMap::SwitchToRenderTargetMode(RHI::GraphicsCommandList* list)
    {
        //todo
    }
    void RenderCubeMap::SwitchToShaderUsageMode(RHI::GraphicsCommandList* list)
    {
        RHI::TextureMemoryBarrier barrier;
        barrier.AccessFlagsBefore = RHI::ResourceAcessFlags::NONE;
        barrier.AccessFlagsAfter = RHI::ResourceAcessFlags::SHADER_READ;
        barrier.oldLayout = RHI::ResourceLayout::UNDEFINED;
        barrier.newLayout = RHI::ResourceLayout::SHADER_READ_ONLY_OPTIMAL;
        barrier.previousQueue = barrier.nextQueue = RHI::QueueFamily::Ignored;
        RHI::SubResourceRange range;
        range.FirstArraySlice = 0;
        range.imageAspect = RHI::Aspect::COLOR_BIT;
        range.IndexOrFirstMipLevel = 0;
        range.NumArraySlices = 6;
        range.NumMipLevels = m_mipLevels;
        barrier.subresourceRange = range;
        barrier.texture = m_ID;
        if(list)
        list->PipelineBarrier(RHI::PipelineStage::TOP_OF_PIPE_BIT, RHI::PipelineStage::ALL_GRAPHICS_BIT, {}, {&barrier,1});
        else
            RendererBase::GetMainCommandList()->PipelineBarrier(RHI::PipelineStage::TOP_OF_PIPE_BIT, RHI::PipelineStage::ALL_GRAPHICS_BIT, {}, {&barrier,1});
    }
    Result<DepthTexture*> DepthTexture::Create(uint32_t width, uint32_t height, uint32_t mipLevels, RHI::Format format,const char* name)
    {
        auto returnVal = std::make_unique<DepthTexture>();
        if(auto err = returnVal->CreateStack(width, height, mipLevels, format, name); !err.Successful())
            return ezr::err(std::move(err));
        return returnVal.release();
    }
    Error DepthTexture::CreateStack(uint32_t width, uint32_t height, uint32_t mipLevels, RHI::Format format,const char* name)
    {
        m_width = width;
        m_height = height;
        m_mipLevels = mipLevels;
        m_format = format;
        RHI::TextureDesc desc{};
        desc.depthOrArraySize = 1;
        desc.height = height;
        desc.width = width;
        desc.mipLevels = mipLevels;
        desc.mode = RHI::TextureTilingMode::Optimal;
        desc.optimizedClearValue = nullptr;
        desc.format = format;
        desc.sampleCount = 1;
        desc.type = RHI::TextureType::Texture2D;
        desc.usage = RHI::TextureUsage::DepthStencilAttachment | RHI::TextureUsage::SampledImage;
        RHI::AutomaticAllocationInfo allocInfo{.access_mode = RHI::AutomaticAllocationCPUAccessMode::None};
        RHI::Ptr<RHI::Texture> texture;
        RHI::Ptr<RHI::TextureView> textureView;
        auto err = RendererBase::GetDevice()->CreateTexture(desc, nullptr, nullptr, &allocInfo, 0, RHI::ResourceType::Automatic).handle(
            [&texture](auto&& tex){texture = tex; return Error(ErrorType::Success);},
            Error::FromRHIError);
        if(!err.Successful()) return err;
        PT_DEBUG_REGION(texture->SetName(name));
        RHI::SubResourceRange range{
            .imageAspect = RHI::Aspect::DEPTH_BIT,
            .IndexOrFirstMipLevel = 0,
            .NumMipLevels = mipLevels,
            .FirstArraySlice = 0,
            .NumArraySlices = 1,
        };

        err = RendererBase::GetDevice()->CreateTextureView(RHI::TextureViewDesc{
            .type = RHI::TextureViewType::Texture2D,
            .format = format,
            .texture = texture,
            .range = range
        }).handle([&textureView](auto&& texView){textureView = texView; return Error(ErrorType::Success);}, Error::FromRHIError);

        if(!err.Successful()) return err;
        m_ID = texture;
        m_view = textureView;
        DSView = RendererBase::CreateDepthStencilView(m_ID, RHI::DepthStencilViewDesc{.TextureArray = false, .arraySlice = 0, .format = format, .textureMipSlice = 0});
        return {};
    }
    RHI::Format DepthTexture::GetFormat() const
    {
        return m_format;
    }
    uint32_t DepthTexture::GetWidth() const
    {
        return m_width;
    }
    uint32_t DepthTexture::GetHeight() const
    {
        return  m_height;
    }
}
