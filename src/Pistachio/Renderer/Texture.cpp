#include "Pistachio/Core/Error.h"
#include "Ptr.h"
#include "Texture.h"
#include "Util/FormatUtils.h"
#include "stb_image.h"
#include "RendererBase.h"
#include <filesystem>
namespace Pistachio
{
    Error Texture2D::CreateTexture(void* data, TextureFlags flags)
    {
        Error e;
        PT_PROFILE_FUNCTION();
        RHI::TextureDesc desc{};
        desc.depthOrArraySize = 1;
        desc.height = m_Height;
        desc.width = m_Width;
        desc.mipLevels = 1; //add mip-mapping ??
        desc.mode = RHI::TextureTilingMode::Optimal;
        desc.optimizedClearValue = nullptr;
        desc.format = m_format;
        desc.sampleCount = 1;
        desc.type = RHI::TextureType::Texture2D;
        desc.usage = RHI::TextureUsage::CopyDst | RHI::TextureUsage::SampledImage;
        desc.usage |= ((flags & TextureFlags::Compute) != TextureFlags::None) ? RHI::TextureUsage::StorageImage : RHI::TextureUsage::None;
        RHI::AutomaticAllocationInfo allocInfo;
        allocInfo.access_mode = RHI::AutomaticAllocationCPUAccessMode::None;
        e = RendererBase::GetDevice()->CreateTexture(desc, nullptr, nullptr, &allocInfo, 0, RHI::ResourceType::Automatic)
            .handle([this](auto&& texture){
                m_ID = texture;
                return Error(ErrorType::Success);
            }, [](auto&& e){
                return Error::FromRHIError(e);
            });
        if(!e.Successful()) return e; 
        
        RHI::SubResourceRange range;
        range.FirstArraySlice = 0;
        range.imageAspect = RHI::Aspect::COLOR_BIT;
        range.IndexOrFirstMipLevel = 0;
        range.NumArraySlices = 1;
        range.NumMipLevels = 1;
        
        if (data)
        {
            RHI::TextureMemoryBarrier barrier;
            barrier.AccessFlagsAfter = RHI::ResourceAcessFlags::TRANSFER_WRITE;
            barrier.AccessFlagsBefore = RHI::ResourceAcessFlags::NONE;
            barrier.newLayout = RHI::ResourceLayout::TRANSFER_DST_OPTIMAL;
            barrier.oldLayout = RHI::ResourceLayout::UNDEFINED;
            barrier.previousQueue = RHI::QueueFamily::Graphics;
            barrier.nextQueue = RHI::QueueFamily::Graphics;
            barrier.subresourceRange = range;
            barrier.texture = m_ID;
            RendererBase::Get().stagingCommandList->PipelineBarrier(
                RHI::PipelineStage::TOP_OF_PIPE_BIT,
                RHI::PipelineStage::TRANSFER_BIT,
                {},
                {&barrier,1});
            RendererBase::PushTextureUpdate(m_ID, m_Width * m_Height * RHI::Util::GetFormatBPP(m_format), data, &range, {m_Width, m_Height,1}, {0,0,0},m_format); //todo image sizes
            barrier.AccessFlagsBefore = RHI::ResourceAcessFlags::TRANSFER_WRITE;
            barrier.AccessFlagsAfter = RHI::ResourceAcessFlags::SHADER_READ;
            barrier.newLayout = RHI::ResourceLayout::SHADER_READ_ONLY_OPTIMAL;
            barrier.oldLayout = RHI::ResourceLayout::TRANSFER_DST_OPTIMAL;
            RendererBase::Get().stagingCommandList->PipelineBarrier(
                RHI::PipelineStage::TRANSFER_BIT,
                RHI::PipelineStage::FRAGMENT_SHADER_BIT,
                {},
                {&barrier, 1});
        }
        RHI::TextureViewDesc viewDesc;
        viewDesc.format = m_format;
        viewDesc.range = range;
        viewDesc.texture = m_ID;
        viewDesc.type = RHI::TextureViewType::Texture2D;
        e = RendererBase::GetDevice()->CreateTextureView(viewDesc).handle([this](auto&& view){
            m_view = view;
            return Error(ErrorType::Success);
        }, [this](auto&& error){
            m_ID = nullptr;
            return Error::FromRHIError(error);
        });
        return e;
    }
    RHI::Format Texture2D::GetFormat() const
    {
        PT_PROFILE_FUNCTION();
        return m_format;
    }
    unsigned int Texture2D::GetHeight() const
    {
        PT_PROFILE_FUNCTION();
        return m_Height;
    }

    unsigned int Texture2D::GetWidth() const
    {
        PT_PROFILE_FUNCTION();
        return m_Width;
    }

    void Texture2D::Bind(int slot) const
    {
        PT_PROFILE_FUNCTION();
    }

    Result<Texture2D*> Texture2D::Create(const char* path , const char* name, RHI::Format format, TextureFlags flags)
    {
        PT_PROFILE_FUNCTION();
        Texture2D* result = new Texture2D;
        auto e = result->CreateStack(path, format , name);
        if(!e.Successful())
        {
            delete result;
            return Result<Texture2D*>(e);
        }
        return Result<Texture2D*>(result);
    }
    Error Texture2D::CreateStack(const char* path, RHI::Format format , const char* name, TextureFlags flags )
    {
        PT_PROFILE_FUNCTION();
        if(!std::filesystem::exists(path))
        {
            return Error(ErrorType::NonExistentFile, __FUNCTION__);
        }
        int Width, Height, nChannels;
        void* data = nullptr;
        if (format == RHI::Format::R16G16B16A16_FLOAT || format == RHI::Format::R32G32B32A32_FLOAT )
        {
            data = stbi_loadf(path, &Width, &Height, &nChannels, 4);
            if(!data) return Error(ErrorType::ProvidedInString, stbi_failure_reason());
        }
        else
        {
            data = stbi_load(path, &Width, &Height, &nChannels, 4);
            if(!data) return Error(ErrorType::ProvidedInString, stbi_failure_reason());
        }
        m_Width = Width;
        m_Height = Height;
        m_format = format;
        auto e = CreateTexture(data, flags);
        if(m_ID.IsValid() && name) m_ID->SetName(name); 
        stbi_image_free(data);
        return e;
    }
    Error Texture2D::CreateStack(uint32_t width, uint32_t height, RHI::Format format, void* data , const char* name,TextureFlags flags)
    {
        PT_PROFILE_FUNCTION();
        m_Width = width;
        m_Height = height;
        m_format = format;
        auto e = CreateTexture(data, flags);
        if(e.Successful() && name) m_ID->SetName(name);
        return e;
    }
    Result<Texture2D*> Texture2D::Create(uint32_t width, uint32_t height, RHI::Format format, void* data , const char* name, TextureFlags flags)
    {
        PT_PROFILE_FUNCTION();
        Texture2D* result = new Texture2D;
        result->m_Width = width;
        result->m_Height = height;
        auto e = result->CreateStack(width, height, format, data , name, flags);
        if(!e.Successful()) 
        {
            delete result;
            return Result<Texture2D*>(e);
        }
        return Result<Texture2D*>(result);
    }
    bool Texture2D::operator==(const Texture2D& texture) const
    {
        PT_PROFILE_FUNCTION();
        return (m_ID == texture.m_ID);
    }
}
