#include "Barrier.h"
#include "CommandList.h"
#include "FormatsAndTypes.h"
#include "Pistachio/Core.h"
#include "Pistachio/Core/Log.h"
#include "Pistachio/Debug/Instrumentor.h"
#include "Pistachio/Renderer/RendererBase.h"
#include "Ptr.h"
#include "Texture.h"
#include "TraceRHI.h"
#include "ptpch.h"
#include "Core/Device.h"
#include "RenderGraph.h"
#include "Renderer.h"
#include "Util/FormatUtils.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <type_traits>
namespace Pistachio
{
    const RGTextureInstance RGTextureInstance::Invalid = { UINT32_MAX,UINT32_MAX };
    const RGBufferInstance RGBufferInstance::Invalid = { UINT32_MAX,UINT32_MAX };
    RenderGraph::~RenderGraph()
    {
        fence->Wait(maxFence);
    }
    RenderGraph::RenderGraph()
    {
        fence = RendererBase::GetDevice()->CreateFence(0).value();
        dbgBufferGFX = RendererBase::GetDevice()->CreateDebugBuffer().value();
        dbgBufferCMP = RendererBase::GetDevice()->CreateDebugBuffer().value();
    }
    void RenderGraph::SubmitToQueue()
    {
        RendererBase::FlushStagingBuffer();
        bool computeLast = true;
        //uint32_t maxFenceDiff = 0;
        uint32_t gfxIndex = 0;
        uint32_t cmpIndex = 0;
        //if(levelTransitionIndices.size()) RendererBase::GetDirectQueue()->WaitForFence(fence, passesSortedAndFence[0].second + maxFence);
        //if(computeLevelTransitionIndices.size()) RendererBase::GetComputeQueue()->WaitForFence(fence, computePassesSortedAndFence[0].second + maxFence);
        uint32_t count = textures.size();
        std::vector<Internal_ID> ids;
        for(uint32_t i = 0; i < count; i++)
        {
            ids.push_back(textures[i].texture->ID);
        }
        if (!RendererBase::GetComputeQueue().IsValid())
        {
            if (cmdLists.size())
            {
                cmdLists[0]->End();
                RendererBase::GetDirectQueue()->ExecuteCommandLists(&cmdLists[0]->ID, 1);
                RendererBase::GetDirectQueue()->SignalFence(fence, ++maxFence);
            }
            //RESULT res = RendererBase::device->QueueWaitIdle(RendererBase::GetDirectQueue());
            return;
        }

        while (gfxIndex < levelTransitionIndices.size() && cmpIndex < computeLevelTransitionIndices.size())
        {
            uint32_t gfx = gfxIndex == 0 ? 0 : levelTransitionIndices[gfxIndex - 1].first;
            uint32_t cmp = cmpIndex == 0 ? 0 : computeLevelTransitionIndices[cmpIndex - 1].first;

            if (passesSortedAndFence[gfx].second <= computePassesSortedAndFence[cmp].second)
            {
                cmdLists[gfxIndex]->End();
                RendererBase::GetDirectQueue()->ExecuteCommandLists(&cmdLists[gfxIndex]->ID, 1);
                uint32_t j = gfxIndex == 0 ? 0 : levelTransitionIndices[gfxIndex - 1].first;
                PT_CORE_VERBOSE("GFX Group {0}", gfxIndex);
                if ((levelTransitionIndices[gfxIndex].second & PassAction::Signal) != (PassAction)0)
                {
                    RendererBase::GetDirectQueue()->SignalFence(fence, passesSortedAndFence[j].second + 1 + maxFence);
                    PT_CORE_VERBOSE("    Signal Fence to {0}", passesSortedAndFence[j].second + 1);
                }
                if ((levelTransitionIndices[gfxIndex].second & PassAction::Wait) != (PassAction)0)
                {
                    uint64_t waitVal = passesSortedAndFence[levelTransitionIndices[gfxIndex].first].second;
                    RendererBase::GetDirectQueue()->WaitForFence(fence, waitVal + maxFence);
                    PT_CORE_VERBOSE("    Wait for fence to reach {0}", waitVal);
                }
                gfxIndex++;
            }
            else
            {
                uint32_t j = cmpIndex == 0 ? 0 : computeLevelTransitionIndices[cmpIndex - 1].first;
                computeCmdLists[cmpIndex]->End();
                RendererBase::GetComputeQueue()->ExecuteCommandLists(&computeCmdLists[cmpIndex]->ID, 1);
                PT_CORE_VERBOSE("CMP Group {0}", cmpIndex);
                if ((computeLevelTransitionIndices[cmpIndex].second & PassAction::Signal) != (PassAction)0)
                {
                    RendererBase::GetComputeQueue()->SignalFence(fence, computePassesSortedAndFence[j].second + 1 + maxFence);
                    PT_CORE_VERBOSE("    Signal Fence to {0}", computePassesSortedAndFence[j].second + 1);
                }
                if ((computeLevelTransitionIndices[cmpIndex].second & PassAction::Wait) != (PassAction)0)
                {
                    uint64_t waitVal = computePassesSortedAndFence[computeLevelTransitionIndices[cmpIndex].first].second;
                    RendererBase::GetComputeQueue()->WaitForFence(fence, waitVal + maxFence);
                    PT_CORE_VERBOSE("Wait for fence to reach {0}", waitVal);
                }
                cmpIndex++;
            }
        }
        while (gfxIndex < levelTransitionIndices.size())
        {
            computeLast = false;
            cmdLists[gfxIndex]->End();
            RendererBase::GetDirectQueue()->ExecuteCommandLists(&cmdLists[gfxIndex]->ID, 1);
            uint32_t j = gfxIndex == 0 ? 0 : levelTransitionIndices[gfxIndex - 1].first;
            PT_CORE_VERBOSE("GFX Group ", gfxIndex);
            if ((levelTransitionIndices[gfxIndex].second & PassAction::Signal) != (PassAction)0)
            {
                RendererBase::GetDirectQueue()->SignalFence(fence, passesSortedAndFence[j].second + 1 + maxFence);
                PT_CORE_VERBOSE("Signal Fence to ", passesSortedAndFence[j].second + 1);
            }
            if ((levelTransitionIndices[gfxIndex].second & PassAction::Wait) != (PassAction)0)
            {
                uint64_t waitVal = passesSortedAndFence[levelTransitionIndices[gfxIndex].first].second;
                
                RendererBase::GetDirectQueue()->WaitForFence(fence, waitVal + maxFence);
                PT_CORE_VERBOSE("Wait for fence to reach ", waitVal);
            }
            gfxIndex++;
        }
        while (cmpIndex < computeLevelTransitionIndices.size())
        {
            uint32_t j = cmpIndex == 0 ? 0 : computeLevelTransitionIndices[cmpIndex - 1].first;
            computeCmdLists[cmpIndex]->End();
            
            RendererBase::GetComputeQueue()->ExecuteCommandLists(&computeCmdLists[cmpIndex]->ID, 1);
            PT_CORE_VERBOSE("CMP Group {0}", cmpIndex);
            if ((computeLevelTransitionIndices[cmpIndex].second & PassAction::Signal) != (PassAction)0)
            {
                RendererBase::GetComputeQueue()->SignalFence(fence, computePassesSortedAndFence[j].second + 1 + maxFence);
                PT_CORE_VERBOSE("    Signal Fence to {0}", computePassesSortedAndFence[j].second + 1);
            }
            if ((computeLevelTransitionIndices[cmpIndex].second & PassAction::Wait) != (PassAction)0)
            {
                uint64_t waitVal = computePassesSortedAndFence[computeLevelTransitionIndices[cmpIndex].first].second;
                RendererBase::GetComputeQueue()->WaitForFence(fence, waitVal + maxFence) ;
                PT_CORE_VERBOSE("    Wait for fence to reach {0}", waitVal);
            }
            cmpIndex++;
        }

        if (computeLast)
        {
            maxFence += computePassesSortedAndFence[computePassesSortedAndFence.size() - 1].second + 1;
            RendererBase::GetComputeQueue()->SignalFence(fence, maxFence);
        }
        else
        {
            maxFence += passesSortedAndFence[passesSortedAndFence.size() - 1].second + 1;
            RendererBase::GetDirectQueue()->SignalFence(fence, maxFence);
        }
        //auto res = RendererBase::device->QueueWaitIdle(RendererBase::GetDirectQueue());
        //if (res) {
        //    uint32_t gfxPoint = dbgBufferGFX->GetValue();
        //    uint32_t cmpPoint = dbgBufferCMP->GetValue();
        //    __debugbreak();
        //}
        //RendererBase::device->QueueWaitIdle(RendererBase::GetComputeQueue());
        //fence->Wait(maxFence);

    }
    void RenderGraph::NewFrame()
    {
        uint32_t numGFXCmdLists = cmdLists.size(), numComputeCmdLists = computeCmdLists.size();
        for (uint32_t i = 0; i < numGFXCmdLists; i++)
        {
            cmdLists[i]->Begin(RendererBase::Get().commandAllocators[RendererBase::GetCurrentFrameIndex()]);
        }
        for (uint32_t i = 0; i < numComputeCmdLists; i++)
        {
            computeCmdLists[i]->Begin(RendererBase::Get().computeCommandAllocators[RendererBase::GetCurrentFrameIndex()]);
        }
    }
    RGTextureHandle RenderGraph::CreateTexture(RenderTexture* texture)
    {
        auto& tex = textures.emplace_back(RGTexture(texture->m_ID, RHI::ResourceLayout::UNDEFINED, RHI::QueueFamily::Graphics, 0, false, 0,1,texture->m_mipLevels,RHI::ResourceAcessFlags::NONE));
        tex.rtvHandle = UniqueRTVHandle({texture->RTView->heapIndex, texture->RTView->heapOffset});
        tex.ownsRTV = false;
        return RGTextureHandle{ &textures, static_cast<uint32_t>(textures.size() - 1) };
    }
    RGTextureHandle RenderGraph::CreateTexture(DepthTexture* texture)
    {
        auto& tex = textures.emplace_back(RGTexture(texture->m_ID, RHI::ResourceLayout::UNDEFINED, RHI::QueueFamily::Graphics,0, false, 0,1,texture->m_mipLevels,RHI::ResourceAcessFlags::NONE));
        tex.dsvHandle = UniqueDSVHandle({texture->DSView->heapIndex, texture->DSView->heapOffset});
        tex.ownsDSV = false;
        return RGTextureHandle{ &textures, static_cast<uint32_t>(textures.size() - 1)  };
    }
    RGTextureHandle RenderGraph::CreateTexture(RenderCubeMap* texture, uint32_t cubeIndex, uint32_t numSlices)
    {

        auto& tex = textures.emplace_back(RGTexture(texture->m_ID, RHI::ResourceLayout::UNDEFINED, RHI::QueueFamily::Graphics, 0, true, cubeIndex, numSlices,texture->m_mipLevels, RHI::ResourceAcessFlags::NONE));
        tex.rtvHandle = UniqueRTVHandle({texture->RTViews[cubeIndex]->heapIndex, texture->RTViews[cubeIndex]->heapOffset});
        return RGTextureHandle{ &textures, static_cast<uint32_t>(textures.size() - 1) };
    }
    RGTextureInstance RenderGraph::MakeUniqueInstance(RGTextureHandle texture)
    {
        return RGTextureInstance{ texture.offset, textures[texture.offset].numInstances++ };
    }
    RGBufferInstance RenderGraph::MakeUniqueInstance(RGBufferHandle buffer)
    {
        return RGBufferInstance{ buffer.offset, buffers[buffer.offset].numInstances++ };
    }
    RGBufferHandle RenderGraph::CreateBuffer(const RHI::Ptr<RHI::Buffer>& buffer, uint32_t offset, uint32_t size, RHI::QueueFamily family)
    {
        auto& buff = buffers.emplace_back(RGBuffer(buffer, offset, size, family,RHI::ResourceAcessFlags::NONE));
        return RGBufferHandle{ &buffers, static_cast<uint32_t>(buffers.size() - 1) };
    }
    RenderPass& RenderGraph::AddPass(RHI::PipelineStage stage, const char* name)
    {
        auto& pass = passes.emplace_back();
        pass.stage = stage;
        pass.name = name;
        dirty = true;
        return pass;
    }
    ComputePass& RenderGraph::AddComputePass(const char* passName)
    {
        auto& pass = computePasses.emplace_back();
        pass.name = passName;
        dirty = true;
        return pass;
    }
    RGTextureHandle RenderGraph::CreateTexture(Pistachio::Texture* texture, uint32_t mipSlice, bool isArray , uint32_t arraySlice, uint32_t numSlices,uint32_t numMips, RHI::ResourceLayout layout, RHI::QueueFamily family)
    {
        auto& tex = textures.emplace_back(RGTexture(texture->m_ID, layout, family,mipSlice, isArray, arraySlice,numSlices,numMips,RHI::ResourceAcessFlags::NONE));
        return RGTextureHandle{ &textures, static_cast<uint32_t>(textures.size() - 1) };
    }
    RGTextureHandle RenderGraph::CreateTexture(RHI::Ptr<RHI::Texture> texture, uint32_t mipSlice, bool isArray, uint32_t arraySlice, uint32_t numSlices, uint32_t numMips, RHI::ResourceLayout layout, RHI::QueueFamily family)
    {
        auto& tex = textures.emplace_back(RGTexture(texture, layout,  family, mipSlice,isArray,arraySlice,numSlices,numMips,RHI::ResourceAcessFlags::NONE));
        return RGTextureHandle{ &textures, static_cast<uint32_t>(textures.size() - 1)};
    }
    void RenderPass::SetShader(Shader* shader)
    {
        pso = shader->GetCurrentPipeline();
        rsig = shader->GetRootSignature();
        pso->Hold();
        rsig->Hold();
    }
    RenderPass::~RenderPass()
    {
    }
    void RenderPass::SetPassArea(const RHI::Area2D& _area)
    {
        area = _area;
    }
    void RenderPass::AddColorInput(AttachmentInfo* info)
    {
        inputs.push_back(*info);
    }
    void RenderPass::AddColorOutput(AttachmentInfo* info)
    {
        outputs.push_back(*info);
    }
    void RenderPass::AddBufferInput(BufferAttachmentInfo* buffer)
    {
        bufferInputs.push_back(*buffer);
    }
    void RenderPass::AddBufferOutput(BufferAttachmentInfo* buffer)
    {
        bufferOutputs.push_back(*buffer);
    }
    void ComputePass::AddColorInput(AttachmentInfo* info)
    {
        inputs.push_back(*info);
    }
    void ComputePass::AddColorOutput(AttachmentInfo* info)
    {
        outputs.push_back(*info);
    }
    void ComputePass::AddBufferInput(BufferAttachmentInfo* buffer)
    {
        bufferInputs.push_back(*buffer);
    }
    void ComputePass::AddBufferOutput(BufferAttachmentInfo* buffer)
    {
        bufferOutputs.push_back(*buffer);
    }
    void ComputePass::SetShader(ComputeShader* shader)
    {
        computePipeline = shader->pipeline;
        rsig = shader->rSig;
        rsig->Hold();
        computePipeline->Hold();
    }
    void ComputePass::SetShader(const RHI::Ptr<RHI::ComputePipeline>& pipeline)
    {
        computePipeline = pipeline;
    }
    void RenderPass::SetDepthStencilOutput(AttachmentInfo* info) { dsOutput = *info; }
    RHI::Aspect FormatAspect(const RHI::Format format)
    {
        const auto info = RHI::Util::GetFormatInfo(format);
        return info == RHI::Util::FormatInfo::Color ? RHI::Aspect::COLOR_BIT : RHI::Aspect::DEPTH_BIT;
    }
    RHI::LoadOp LoadOpFrom(const AttachmentAccess access)
    {
        using enum AttachmentAccess;
        switch (access)
        {
            case Read: return RHI::LoadOp::Load;
            case Write: return RHI::LoadOp::Clear;
            case ReadWrite: return RHI::LoadOp::Load;
            default: return RHI::LoadOp::DontCare;
        }
    }
    RHI::ResourceLayout InputLayout(const AttachmentUsage usage)
    {
        switch (usage)
        {
        case AttachmentUsage::Unspec: [[fallthrough]];
        case AttachmentUsage::Graphics: return RHI::ResourceLayout::SHADER_READ_ONLY_OPTIMAL;
            break;
        case AttachmentUsage::Copy: return RHI::ResourceLayout::TRANSFER_SRC_OPTIMAL;
            break;
        case AttachmentUsage::Blit: return RHI::ResourceLayout::GENERAL;
            break;
        case AttachmentUsage::Compute: return RHI::ResourceLayout::SHADER_READ_ONLY_OPTIMAL;
        default: return RHI::ResourceLayout::UNDEFINED;
            break;
        }
    }
    RHI::ResourceLayout OutputLayout(AttachmentUsage usage)
    {
        switch (usage)
        {
        case AttachmentUsage::Unspec: [[fallthrough]];
        case Pistachio::AttachmentUsage::Graphics: return RHI::ResourceLayout::COLOR_ATTACHMENT_OPTIMAL;
            break;
        case Pistachio::AttachmentUsage::Copy: return RHI::ResourceLayout::TRANSFER_DST_OPTIMAL;
        break;
        case Pistachio::AttachmentUsage::Blit: return RHI::ResourceLayout::TRANSFER_DST_OPTIMAL;
            break;
        case Pistachio::AttachmentUsage::Compute: return RHI::ResourceLayout::GENERAL;
            break;
        default: return RHI::ResourceLayout::UNDEFINED;
            break;
        }
    }
    RHI::ResourceAcessFlags InputDstAccess(AttachmentUsage usage)
    {
        switch (usage)
        {
        case AttachmentUsage::Unspec: [[fallthrough]];
        case AttachmentUsage::Graphics: return RHI::ResourceAcessFlags::SHADER_READ;
        case AttachmentUsage::Copy: return RHI::ResourceAcessFlags::TRANSFER_READ;
        case AttachmentUsage::Blit: return RHI::ResourceAcessFlags::TRANSFER_READ;
        case AttachmentUsage::Compute: return RHI::ResourceAcessFlags::SHADER_READ | RHI::ResourceAcessFlags::SHADER_WRITE;
        default: return RHI::ResourceAcessFlags::NONE;
        }
    }
    RHI::ResourceAcessFlags OutputDstAccess(AttachmentUsage usage)
    {
        switch (usage)
        {
        case AttachmentUsage::Unspec: [[fallthrough]];
        case AttachmentUsage::Graphics: return RHI::ResourceAcessFlags::COLOR_ATTACHMENT_WRITE;
        case AttachmentUsage::Copy: return RHI::ResourceAcessFlags::TRANSFER_WRITE;
        case AttachmentUsage::Blit: return RHI::ResourceAcessFlags::TRANSFER_WRITE;
        case AttachmentUsage::Compute: return RHI::ResourceAcessFlags::SHADER_WRITE | RHI::ResourceAcessFlags::SHADER_READ;
        default: return RHI::ResourceAcessFlags::NONE;
        }
    }
    RHI::Ptr<RHI::GraphicsCommandList> RenderGraph::GetFirstList()
    {
        return cmdLists[0];
    }
    void RenderGraph::SetName(std::string_view _name)
    {
        name = _name;
    }
    void RenderGraph::Execute()
    {
        PT_PROFILE_FUNCTION();
        if (dirty) Compile();
        NewFrame();
        RHI::PipelineStage GFXstage = RHI::PipelineStage::TOP_OF_PIPE_BIT;
        RHI::PipelineStage CMPstage = RHI::PipelineStage::TOP_OF_PIPE_BIT;
        uint32_t gfxIndex = 0;
        uint32_t cmpIndex = 0;
        RHI::PipelineStage* gfxStage = 0, * cmpStage = 0;
        const bool MQ = RendererBase::GetComputeQueue().IsValid();
        if (!MQ) { gfxStage = &GFXstage; cmpStage = &GFXstage; }
        else {gfxStage = &GFXstage; cmpStage = &CMPstage; }
        //we execute in order of fence vals with the guarantee that if passes overlap they do not share dependencies
        //this shouldn't be necessary as fence vals should alternate, but well do it anyway
        
        auto gfxQueue = RHI::QueueFamily::Graphics;
        auto cmpQueue = MQ ? RHI::QueueFamily::Compute : RHI::QueueFamily::Graphics;
        while (gfxIndex < levelTransitionIndices.size() && cmpIndex < computeLevelTransitionIndices.size())
        {
            const uint32_t gfx = gfxIndex == 0 ? 0 : levelTransitionIndices[gfxIndex - 1].first;
            const uint32_t cmp = cmpIndex == 0 ? 0 : computeLevelTransitionIndices[cmpIndex - 1].first;
            RHI::Weak<RHI::GraphicsCommandList> cmpList = nullptr;
            RHI::Weak<RHI::GraphicsCommandList> gfxList = nullptr;
            
            if (MQ)
            {
                cmpList = cmpIndex ? computeCmdLists[cmpIndex - 1] : computeCmdLists[0];
                gfxList = gfxIndex ? cmdLists[gfxIndex - 1] : cmdLists[0];
            }
            else
            {
                cmpList = cmdLists[0];
                gfxList = cmdLists[0];
            }
            if (passesSortedAndFence[gfx].second < computePassesSortedAndFence[cmp].second)
                ExecuteGFXLevel(gfxIndex++, *gfxStage, cmpList, gfxQueue);
            else
                ExecuteCMPLevel(cmpIndex++, *cmpStage, gfxList, cmpQueue);
        }
        RHI::Weak<RHI::GraphicsCommandList> cmpList = nullptr;
        RHI::Weak<RHI::GraphicsCommandList> gfxList = nullptr;
        if (MQ)
        {
            cmpList = !computeLevelTransitionIndices.empty() && cmpIndex ? computeCmdLists[cmpIndex - 1] : nullptr;
            gfxList = !levelTransitionIndices.empty() && gfxIndex ? cmdLists[gfxIndex - 1] : nullptr;
        }
        else
        {
            cmpList = cmdLists[0];
            gfxList = cmdLists[0];
        }
        while (gfxIndex < levelTransitionIndices.size()) ExecuteGFXLevel(gfxIndex++, *gfxStage, cmpList, gfxQueue);
        while (cmpIndex < computeLevelTransitionIndices.size()) ExecuteCMPLevel(cmpIndex++, *cmpStage, gfxList,cmpQueue);

        
    }
    bool FillTextureBarrier(RGTexture& tex, AttachmentInfo& info, 
        std::vector<RHI::TextureMemoryBarrier>& barriers,
        std::vector<RHI::TextureMemoryBarrier>& release,
        RHI::ResourceLayout layout_fn(AttachmentUsage),
        RHI::ResourceAcessFlags access_fn(AttachmentUsage),
        RHI::QueueFamily srcQueue,
        RHI::PipelineStage stg,
        bool isDepth = false)
    {
        if(info.usage == AttachmentUsage::PassThrough) return false;
        auto& barrier = barriers.emplace_back();
        barrier.newLayout = isDepth ? RHI::ResourceLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL : layout_fn(info.usage);
        barrier.AccessFlagsAfter = isDepth ? RHI::ResourceAcessFlags::DEPTH_STENCIL_ATTACHMENT_WRITE : access_fn(info.usage);
        barrier.AccessFlagsBefore = tex.currentAccess;
        //temporary
        const RHI::SubResourceRange range{
            .imageAspect = FormatAspect(info.format),
            .IndexOrFirstMipLevel = tex.mipSlice,
            .NumMipLevels = tex.mipSliceCount,
            .FirstArraySlice = tex.IsArray ? tex.arraySlice : 0,
            .NumArraySlices = tex.IsArray ? tex.sliceCount : 1,
        };
        if (tex.currentFamily != srcQueue)
        {
            barrier.AccessFlagsBefore = RHI::ResourceAcessFlags::NONE;
            auto& barr = release.emplace_back();
            barr.AccessFlagsAfter = barr.AccessFlagsBefore = RHI::ResourceAcessFlags::NONE;
            barr.previousQueue = tex.currentFamily;
            barr.nextQueue = srcQueue;
            barr.texture = tex.texture;
            barr.subresourceRange = range;
            barr.oldLayout = tex.current_layout;
            barr.newLayout = barrier.newLayout;
        }
        if (
            tex.current_layout == barrier.newLayout &&
            tex.currentFamily == srcQueue
            )
        {
            barriers.pop_back();
            return false;
        }
        
        //transition
        barrier.oldLayout = tex.current_layout;
        barrier.texture = tex.texture;
        barrier.subresourceRange = range;
        barrier.previousQueue = tex.currentFamily == srcQueue ? RHI::QueueFamily::Ignored : tex.currentFamily;
        barrier.nextQueue = tex.currentFamily == srcQueue ? RHI::QueueFamily::Ignored : srcQueue;
        tex.currentFamily = srcQueue;
        tex.current_layout = barrier.newLayout;
        tex.currentAccess = barrier.AccessFlagsAfter;
        tex.stage = stg;
        return true;
    }
    void FillBufferBarrier(RGBuffer& buff, BufferAttachmentInfo& info,
        std::vector<RHI::BufferMemoryBarrier>& barriers,
        std::vector<RHI::BufferMemoryBarrier>& release,
        RHI::ResourceAcessFlags access_fn(AttachmentUsage),
        RHI::QueueFamily srcQueue,
        RHI::PipelineStage stg)
    {
        if(info.usage == AttachmentUsage::PassThrough) return;
        auto& barrier = barriers.emplace_back();
        if (buff.currentFamily != srcQueue)
        {
            auto& barr = release.emplace_back();
            barr.AccessFlagsAfter = barr.AccessFlagsBefore = RHI::ResourceAcessFlags::NONE;
            barr.previousQueue = buff.currentFamily;
            barr.nextQueue = srcQueue;
            barr.offset = buff.offset;
            barr.size = buff.size;
            barr.buffer = buff.buffer;
        }
        barrier.AccessFlagsAfter = access_fn(info.usage);
        //transition
        if(buff.currentFamily == srcQueue)
        {
            barriers.pop_back();
            return;
        }
        barrier.AccessFlagsBefore = buff.currentAccess;
        barrier.buffer = buff.buffer;
        barrier.previousQueue = buff.currentFamily;
        barrier.nextQueue = srcQueue;
        barrier.offset = buff.offset;
        barrier.size = buff.size;
        buff.currentAccess = barrier.AccessFlagsAfter;
        buff.currentFamily = srcQueue;
        buff.stage = stg;
    }
    enum AttachmentType
    {
        AttachRT, AttachDS
    };
    template<AttachmentType type>
    struct AttachmentType_t {};
    template<>
    struct AttachmentType_t<AttachRT>
    {
        using desc_type = RHI::RenderTargetViewDesc;
        using handle_type = UniqueRTVHandle;
    };
    template<>
    struct AttachmentType_t<AttachDS>
    {
        using desc_type = RHI::DepthStencilViewDesc;
        using handle_type = UniqueDSVHandle;
    };
    template<int type>
    void RenderGraph::FillAttachment(AttachmentInfo& info, std::vector<RHI::RenderingAttachmentDesc>& desc, RGTexture& tex)
    {
        auto& attachment = desc.emplace_back();
        if constexpr (type == AttachRT) attachment.clearColor = { 0,0,0,0 };
        if constexpr (type == AttachDS) attachment.clearColor = { 1,0,0,0 };
        attachment.loadOp = LoadOpFrom(info.access);
        attachment.storeOp = RHI::StoreOp::Store;
        typename AttachmentType_t<static_cast<AttachmentType>(type)>::handle_type* handle;
        if constexpr (type == AttachRT) handle = &tex.rtvHandle;
        if constexpr (type == AttachDS) handle = &tex.dsvHandle;
        if ((*handle)->heapIndex == UINT32_MAX)
        {
            typename AttachmentType_t<static_cast<AttachmentType>(type)>::desc_type Desc;
            Desc.arraySlice = tex.arraySlice;
            Desc.format = info.format;
            Desc.TextureArray = tex.IsArray;
            Desc.textureMipSlice = tex.mipSlice;
           if constexpr (type == AttachRT) *handle = RendererBase::CreateRenderTargetView(tex.texture, Desc);
           if constexpr (type == AttachDS) *handle = RendererBase::CreateDepthStencilView(tex.texture, Desc);
        }
        attachment.ImageView = RendererBase::GetCPUHandle(handle->Get());
    }
    void RenderGraph::LogAttachmentHeader(const AttachmentInfo& att)
    {
        PT_CORE_VERBOSE("Texture Attachment: (format: {0}, load_op: {1}, usage: {2})",
            ENUM_FMT(att.format), 
            ENUM_FMT(LoadOpFrom(att.access)), 
            ENUM_FMT(att.usage));
    } 
    void RenderGraph::LogAttachmentBody(const AttachmentInfo& att, std::vector<RGTexture>& textures)
    {
        auto& texture = textures[att.texture.texOffset];
        PT_CORE_VERBOSE("  Texture -> instance: {0}, layout: {1}, access: {2}, family: {3}, name: {4}",
            att.texture.instID,
            ENUM_FMT(texture.current_layout),
            ENUM_FMT(texture.currentAccess),
            ENUM_FMT(texture.currentFamily),
            rhi_tex->name ? rhi_tex->name : std::string_view()
            );
    }
    bool RenderGraph::LogTextureBarrier(bool value, std::vector<RHI::TextureMemoryBarrier>& barrier)
    {
        if(value)
        {
            auto& barr = barrier.back();
            PT_CORE_VERBOSE(
                "  Pipeline Barrier:\n"
                "\t\tacces src: {0} dst: {1}\n"
                "\t\tlayout from: {2} to: {3}\n"
                "\t\tqueue from: {4} to: {5}",
                ENUM_FMT(barr.AccessFlagsBefore),
                ENUM_FMT(barr.AccessFlagsAfter),
                ENUM_FMT(barr.oldLayout),
                ENUM_FMT(barr.newLayout),
                ENUM_FMT(barr.previousQueue),
                ENUM_FMT(barr.nextQueue)
            );
        }
        else
            PT_CORE_VERBOSE("Barrier Not Needed");
        return value;
    }
    template<typename PassTy>
    void RenderGraph::ExecLevel(std::vector<std::pair<uint32_t, PassAction>>& levelTransitionIndices, uint32_t levelInd,
        RHI::Weak<RHI::GraphicsCommandList> currentList,
        RHI::Weak<RHI::GraphicsCommandList> prevList,
        std::vector<std::pair<PassTy*, uint32_t>>& passes,
        std::vector<RGTexture>& textures,
        std::vector<RGBuffer>& buffers,
        RHI::QueueFamily srcQueue,
        RHI::PipelineStage& stage)
    {
        PT_PROFILE_FUNCTION();
        std::vector<RHI::TextureMemoryBarrier> textureRelease;
        std::vector<RHI::BufferMemoryBarrier> bufferRelease;
        uint32_t j = levelInd == 0 ? 0 : levelTransitionIndices[levelInd - 1].first;
        PT_CORE_VERBOSE("Level contains {0} Passes", levelTransitionIndices[levelInd].first - j);
        for (; j < levelTransitionIndices[levelInd].first; j++)
        {
            //transition all inputs to good state
            PassTy* pass = passes[j].first;
            RHI::PipelineStage pass_stg;
            if constexpr(std::is_same_v<RenderPass, PassTy>) pass_stg = pass->stage; else pass_stg = RHI::PipelineStage::COMPUTE_SHADER_BIT;
            std::unordered_map<RHI::PipelineStage, std::vector<RHI::TextureMemoryBarrier>> barriers;
            std::unordered_map<RHI::PipelineStage, std::vector<RHI::BufferMemoryBarrier>> bufferBarriers;
            barriers.reserve(pass->inputs.size() + pass->outputs.size());
            bufferBarriers.reserve(pass->bufferInputs.size() + pass->bufferOutputs.size());
            RHI::RenderingBeginDesc rbDesc{};
            if constexpr (std::is_same_v<PassTy, RenderPass>) rbDesc.renderingArea = pass->area;
            std::vector<RHI::RenderingAttachmentDesc> attachments;
            attachments.reserve(pass->outputs.size());
            
            for (auto& input : pass->inputs)
            {
                RGTexture& tex = textures[input.texture.texOffset];
                FillTextureBarrier(tex, input, barriers[tex.stage], textureRelease, InputLayout, InputDstAccess, srcQueue, pass_stg);
                
                LogAttachmentBody(input, textures);
            }
            PT_CORE_VERBOSE("Texture Outputs:");
            for (auto& output : pass->outputs)
            {
                RGTexture& tex = textures[output.texture.texOffset];
                if (output.usage == AttachmentUsage::Graphics) FillAttachment<AttachRT>(output, attachments, tex);
                FillTextureBarrier(tex, output, barriers[tex.stage], textureRelease, OutputLayout, OutputDstAccess, srcQueue, pass_stg);
            }
            if constexpr (std::is_same_v<PassTy, RenderPass>)
            {
                if (pass->dsOutput.texture != RGTextureInstance::Invalid)
                {
                    RGTexture& tex = textures[pass->dsOutput.texture.texOffset];
                    if (pass->dsOutput.usage == AttachmentUsage::Graphics)
                    {
                        FillAttachment<AttachDS>(pass->dsOutput, attachments, textures[pass->dsOutput.texture.texOffset]);
                        rbDesc.pDepthStencilAttachment = &attachments.back();
                    }
                    FillTextureBarrier(tex, pass->dsOutput, barriers[tex.stage], textureRelease, OutputLayout, OutputDstAccess, srcQueue,pass_stg, true);
                }
            }
            
            for (auto& input : pass->bufferInputs)
            {
                RGBuffer& buff = buffers[input.buffer.buffOffset];
                FillBufferBarrier(buff, input, bufferBarriers[buff.stage], bufferRelease, InputDstAccess, srcQueue, pass_stg);
            }
            for (auto& output : pass->bufferOutputs)
            {
                RGBuffer& buff = buffers[output.buffer.buffOffset];
                FillBufferBarrier(buff, output, bufferBarriers[buff.stage], bufferRelease, OutputDstAccess, srcQueue, pass_stg);
            }
            rbDesc.pColorAttachments = attachments.data();
            rbDesc.numColorAttachments = attachments.size() - (rbDesc.pDepthStencilAttachment ? 1 : 0);
            
            for(auto&[curr_stage, barrier] : barriers)
            {
                std::span textures_span = barrier;
                std::span<RHI::BufferMemoryBarrier> buffers_span = {};
                if(bufferBarriers.contains(curr_stage)) buffers_span = bufferBarriers[curr_stage];
                if(textures_span.size() || buffers_span.size())
                {
                    currentList->PipelineBarrier(curr_stage, pass_stg, buffers_span, textures_span);
                }
                if(buffers_span.size()) bufferBarriers.erase(curr_stage);
            }

            for(auto&[curr_stage, barrier] : bufferBarriers)
                if(barrier.size())
                    currentList->PipelineBarrier(curr_stage, pass_stg, barrier, {});
            
            if constexpr (std::is_same_v<PassTy, RenderPass>) if (pass->pso.IsValid()) currentList->SetPipelineState(pass->pso);
            if constexpr (std::is_same_v<PassTy, ComputePass>) if (pass->computePipeline.IsValid()) currentList->SetComputePipeline(pass->computePipeline);
            if (pass->rsig.IsValid()) currentList->SetRootSignature(pass->rsig);

            {
                if (std::is_same_v<PassTy, RenderPass> && attachments.size()) currentList->BeginRendering(rbDesc);
                TraceRHIZone("RenderGraph", currentList, RendererBase::TraceContext());
                pass->pass_fn(currentList);
                stage = pass_stg;
            }

            if (std::is_same_v<PassTy, RenderPass> && attachments.size()) currentList->EndRendering();
        }
        constexpr auto stg = std::is_same_v<PassTy, RenderPass> ?  RHI::PipelineStage::COMPUTE_SHADER_BIT : RHI::PipelineStage::ALL_GRAPHICS_BIT;
        if(bufferRelease.size() + textureRelease.size())
        prevList->ReleaseBarrier(stg, RHI::PipelineStage::TOP_OF_PIPE_BIT, bufferRelease, textureRelease);
    }
    inline void RenderGraph::ExecuteGFXLevel(uint32_t levelInd, RHI::PipelineStage& stage, RHI::Weak<RHI::GraphicsCommandList> prevList,
    RHI::QueueFamily srcQueue)
    {
        RHI::Weak<RHI::GraphicsCommandList> currentList = cmdLists[RendererBase::GetComputeQueue().IsValid() ? levelInd : 0];
        ExecLevel<RenderPass>(levelTransitionIndices, levelInd, currentList, prevList, passesSortedAndFence, textures, buffers, srcQueue, stage);
    }

    inline void RenderGraph::ExecuteCMPLevel(uint32_t levelInd, RHI::PipelineStage& stage, RHI::Weak<RHI::GraphicsCommandList> prevList,
    RHI::QueueFamily srcQueue)
    {
        RHI::Weak<RHI::GraphicsCommandList> currentList =RendererBase::GetComputeQueue().IsValid() ? computeCmdLists[levelInd] : cmdLists[0];
        ExecLevel<ComputePass>(computeLevelTransitionIndices, levelInd, currentList, prevList, computePassesSortedAndFence, textures, buffers, srcQueue, stage);
    }

    void RenderGraph::SortPasses()
    {
        PT_PROFILE_FUNCTION();
        std::vector<RenderPass*> passesLeft;
        std::vector<ComputePass*> computePassesLeft;
        for (auto& pass : passes) { passesLeft.push_back(&pass); }
        for (auto& pass : computePasses) { computePassesLeft.push_back(&pass); }
        std::vector<std::tuple<RGTextureInstance, PassType, uint64_t, void*>> readyOutputs;
        std::vector<std::tuple<RGBufferInstance, PassType, uint64_t, void*>> readyBufferOutputs;
        computePassesSortedAndFence.clear();
        passesSortedAndFence.clear();
        uint32_t readyIndex = 0;
        uint32_t readyBufferIndex = 0;
        while (passesLeft.size() + computePassesLeft.size())
        {
            std::vector<RenderPass*> passesStillLeft;
            std::vector<ComputePass*> computePassesStillLeft;

            //gfx passes
            for (uint32_t i = 0; i < passesLeft.size(); i++)
            {
                RenderPass* pass = passesLeft[i];
                //if all a pass inputs are in the ready Outputs,pass belong to current level
                bool currLevel = true;
                uint64_t fenceVal = 0;
                for (auto& input : pass->inputs)
                {
                    if (auto pos = std::find_if(readyOutputs.begin(), readyOutputs.end(),
                        [input, &fenceVal](const std::tuple<RGTextureInstance, PassType, uint64_t, void*>& output)
                        {
                            bool found = input.texture == std::get<0>(output);
                            bool diffFamily = (std::get<1>(output) != PassType::Graphics);
                            if (found)
                            {
                                fenceVal = std::max(fenceVal, std::get<2>(output) + diffFamily);
                                if (diffFamily)
                                {
                                    if (std::get<1>(output) == PassType::Graphics)
                                        ((RenderPass*)std::get<3>(output))->signal = true;
                                    else
                                        ((ComputePass*)std::get<3>(output))->signal = true;
                                }
                            }
                            return found;
                        });
                    pos == readyOutputs.end())
                    {
                        passesStillLeft.push_back(pass);
                        currLevel = false;
                        break;
                    }
                }
                for (auto& input : pass->bufferInputs)
                {
                    if (auto pos = std::find_if(readyBufferOutputs.begin(), readyBufferOutputs.end(),
                        [input, &fenceVal](const std::tuple<RGBufferInstance, PassType, uint64_t, void*>& output)
                        {
                            bool found = input.buffer == std::get<0>(output);
                            bool diffFamily = (std::get<1>(output) != PassType::Graphics);
                            if (found)
                            {
                                fenceVal = std::max(fenceVal, std::get<2>(output) + diffFamily);
                                if (diffFamily)
                                {
                                    if (std::get<1>(output) == PassType::Graphics)
                                        ((RenderPass*)std::get<3>(output))->signal = true;
                                    else
                                        ((ComputePass*)std::get<3>(output))->signal = true;
                                }
                            }
                            return found;
                        });
                    pos == readyBufferOutputs.end())
                    {
                        passesStillLeft.push_back(pass);
                        currLevel = false;
                        break;
                    }
                }
                if (currLevel)
                {
                    passesSortedAndFence.push_back({ pass,fenceVal });
                    for (auto output : pass->outputs)
                    {
                        readyOutputs.push_back({ output.texture ,PassType::Graphics,fenceVal, pass });
                    }
                    for (auto output : pass->bufferOutputs)
                    {
                        readyBufferOutputs.push_back({ output.buffer, PassType::Graphics, fenceVal,pass });
                    }
                    if (pass->dsOutput.texture != RGTextureInstance::Invalid)
                    {
                        readyOutputs.push_back({ pass->dsOutput.texture,PassType::Graphics,fenceVal,pass });
                    };
                }
            }
            //compute passes
            for (uint32_t i = 0; i < computePassesLeft.size(); i++)
            {
                ComputePass* pass = computePassesLeft[i];
                //if all a pass inputs are in the ready Outputs,pass belong to current level
                bool currLevel = true;
                uint64_t fenceVal = 0;
                for (auto& input : pass->inputs)
                {
                    if (auto pos = std::find_if(readyOutputs.begin(), readyOutputs.begin() + readyIndex,
                        [input, &fenceVal](const std::tuple<RGTextureInstance, PassType, uint64_t, void*>& output)
                        {
                            bool found = input.texture == std::get<0>(output);
                            bool diffFamily = (std::get<1>(output) != PassType::Compute);
                            if (found)
                            {
                                fenceVal = std::max(fenceVal, std::get<2>(output) + diffFamily);
                                if (diffFamily)
                                {
                                    if (std::get<1>(output) == PassType::Graphics)
                                        ((RenderPass*)std::get<3>(output))->signal = true;
                                    else
                                        ((ComputePass*)std::get<3>(output))->signal = true;
                                }
                            }
                            return found;
                        });
                    pos == readyOutputs.begin() +readyIndex)
                    {
                        computePassesStillLeft.push_back(pass);
                        currLevel = false;
                        break;
                    }
                }
                for (auto& input : pass->bufferInputs)
                {
                    if (auto pos = std::find_if(readyBufferOutputs.begin(), readyBufferOutputs.begin() + readyBufferIndex,
                        [input, &fenceVal](const std::tuple<RGBufferInstance, PassType, uint64_t, void*>& output)
                        {
                            bool found = input.buffer == std::get<0>(output);
                            bool diffFamily = (std::get<1>(output) != PassType::Compute);
                            if (found)
                            {
                                fenceVal = std::max(fenceVal, std::get<2>(output) + diffFamily);
                                if (diffFamily)
                                {
                                    if (std::get<1>(output) == PassType::Graphics)
                                        ((RenderPass*)std::get<3>(output))->signal = true;
                                    else
                                        ((ComputePass*)std::get<3>(output))->signal = true;
                                }
                            }
                            return found;
                        });
                    pos == readyBufferOutputs.begin() + readyBufferIndex)
                    {
                        computePassesStillLeft.push_back(pass);
                        currLevel = false;
                        break;
                    }
                }
                if (currLevel)
                {
                    computePassesSortedAndFence.push_back({ pass,fenceVal });
                    for (auto output : pass->outputs)
                    {
                        readyOutputs.push_back({ output.texture ,PassType::Compute,fenceVal,pass });
                        readyIndex++;
                    }
                    for (auto output : pass->bufferOutputs)
                    {
                        readyBufferOutputs.push_back({ output.buffer, PassType::Compute, fenceVal,pass });
                        readyBufferIndex++;
                    }
                }
            }
            readyIndex = readyOutputs.size();
            readyBufferIndex = readyBufferOutputs.size();
            passesLeft = std::move(passesStillLeft);
            computePassesLeft = std::move(computePassesStillLeft);
            if (passesSortedAndFence.size())
            {
                if (!(levelTransitionIndices.size() && (levelTransitionIndices.end() - 1)->first == passesSortedAndFence.size()))
                    levelTransitionIndices.push_back({ passesSortedAndFence.size(),PassAction::Wait });
            }
            if (computePassesSortedAndFence.size())
            {
                if (!(computeLevelTransitionIndices.size() && (computeLevelTransitionIndices.end() - 1)->first == computePassesSortedAndFence.size()))
                    computeLevelTransitionIndices.push_back({ computePassesSortedAndFence.size(),PassAction::Wait });
            }
        }
        if(levelTransitionIndices.size())(levelTransitionIndices.end() - 1)->second = (PassAction)0;
        if(computeLevelTransitionIndices.size())(computeLevelTransitionIndices.end() - 1)->second = (PassAction)0;
        //Go through every level
        for (uint32_t i = 0; i < levelTransitionIndices.size(); i++)
        {
            uint32_t farthestSignal = 0;
            bool signal = false;
            uint32_t j = i == 0 ? 0 : levelTransitionIndices[i - 1].first;
            for (; j < levelTransitionIndices[i].first; j++)
            {
                if (passesSortedAndFence[j].first->signal)
                {
                    signal = true;
                    farthestSignal = std::max(farthestSignal, j + 1);
                }
            }
            if (signal)
            {
                if (farthestSignal == levelTransitionIndices[i].first)
                {
                    levelTransitionIndices[i].second |= PassAction::Signal;
                }
                else
                {
                    levelTransitionIndices.insert(levelTransitionIndices.begin() + i, { farthestSignal, PassAction::Signal });
                    i++;
                }
            }
        }
        //Go through every Compute level
        for (uint32_t i = 0; i < computeLevelTransitionIndices.size(); i++)
        {
            uint32_t farthestSignal = 0;
            bool signal = false;
            uint32_t j = i == 0 ? 0 : computeLevelTransitionIndices[i - 1].first;
            for (; j < computeLevelTransitionIndices[i].first; j++)
            {
                if (computePassesSortedAndFence[j].first->signal)
                {
                    signal = true;
                    farthestSignal = std::max(farthestSignal, j + 1);
                }
            }
            if (signal)
            {
                if (farthestSignal == computeLevelTransitionIndices[i].first)
                {
                    computeLevelTransitionIndices[i].second |= PassAction::Signal;
                }
                else
                {
                    computeLevelTransitionIndices.insert(computeLevelTransitionIndices.begin() + i, { farthestSignal, PassAction::Signal });
                    i++;
                }
            }
        }


    }
    void RenderGraph::Compile()
    {
        PT_PROFILE_FUNCTION();  
        SortPasses();
        for(auto& buf : this->buffers)
            PT_CORE_ASSERT(buf.buffer.IsValid());
        for(auto& tex : this->textures)
            PT_CORE_ASSERT(tex.texture.IsValid());
        cmdLists.clear();
        computeCmdLists.clear();
        size_t numGFXCmdLists = levelTransitionIndices.size();
        const bool MQ = RendererBase::GetComputeQueue().IsValid();
        size_t numComputeCmdLists = MQ ? computeLevelTransitionIndices.size() : 0;
        if (!MQ) numGFXCmdLists = (levelTransitionIndices.size() + computeLevelTransitionIndices.size()) ? 1 : 0;
        cmdLists.resize(numGFXCmdLists);
        computeCmdLists.resize(numComputeCmdLists);
        for (uint32_t i = 0; i < numGFXCmdLists; i++)
        {
            cmdLists[i] = RendererBase::GetDevice()->CreateCommandList(RHI::CommandListType::Direct,
                RendererBase::Get().commandAllocators[RendererBase::GetCurrentFrameIndex()]).value();
            std::string name = "Render Graph Direct List" + std::to_string(i);
            cmdLists[i]->SetName(name.c_str());
        }
        for (uint32_t i = 0; i < numComputeCmdLists; i++)
        {
            computeCmdLists[i] = RendererBase::GetDevice()->CreateCommandList(RHI::CommandListType::Compute,
                RendererBase::Get().computeCommandAllocators[RendererBase::GetCurrentFrameIndex()]).value();
            std::string name = "Render Graph Compute List" + std::to_string(i);
            cmdLists[i]->SetName(name.c_str());
        }
        dirty = false;
    }

}
