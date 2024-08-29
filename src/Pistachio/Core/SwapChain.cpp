#include "DescriptorHeap.h"
#include "FormatsAndTypes.h"
#include "Pistachio/Utils/RendererUtils.h"
#include "Pistachio/Renderer/RendererBase.h"
#include "Application.h"
#include "Window.h"
#include <cstdint>

namespace Pistachio
{
    void SwapChain::BackBufferBarrier(RHI::PipelineStage before,RHI::PipelineStage after,RHI::ResourceLayout oldLayout, RHI::ResourceLayout newLayout,
				RHI::ResourceAcessFlags prevAccess, RHI::ResourceAcessFlags currAccess)
	{
        auto& base = Application::Get().GetRendererBase();
		RHI::TextureMemoryBarrier barr{};
		barr.oldLayout = oldLayout;
		barr.newLayout = newLayout;
		barr.AccessFlagsBefore = prevAccess;
		barr.AccessFlagsAfter = currAccess;
		barr.subresourceRange = 
		{	
			.imageAspect = RHI::Aspect::COLOR_BIT,
			.IndexOrFirstMipLevel = 0,
			.NumMipLevels = 1,
			.FirstArraySlice = 0,
			.NumArraySlices = 1,
		};
		barr.texture = swapTextures[swapchain->GetImageIndex()];
		barr.previousQueue = barr.nextQueue = RHI::QueueFamily::Ignored;
		base.mainCommandList->PipelineBarrier(before, after, {},{&barr,1});
	}
    void SwapChain::Initialize(uint32_t width, uint32_t height)
    {
        RHI::SwapChainDesc sDesc;
		sDesc.BufferCount = RendererUtils::SwapImageCount(RendererBase::GetInstance()->GetSwapChainMinMaxImageCount(RendererBase::GetPhysicalDevice(), &surface), 2);
		sDesc.Flags = 0;
		sDesc.Height = height;
		sDesc.Width = width;
		sDesc.OutputSurface = surface;
		sDesc.RefreshRate = { 60,1 };
		sDesc.SampleCount = 1; //disable multisampling for now, because RHI doesnt fully support it
		sDesc.SampleQuality = 0;
		sDesc.SwapChainFormat = RHI::Format::B8G8R8A8_UNORM;//todo add functionality to get supported formats in the RHI
		sDesc.Windowed = true;
		swapchain = RendererBase::GetInstance()->CreateSwapChain(sDesc, RendererBase::GetPhysicalDevice(), RendererBase::GetDevice(), RendererBase::GetDirectQueue()).value();
        swapTextures.resize(sDesc.BufferCount);
        uint32_t index = 0;
        for(auto& texture : swapTextures)
        {
            texture = RendererBase::GetDevice()->GetSwapChainImage(swapchain, index++).value();
            char name[20] = {"Back Buffer Image 0"};
			name[18] += index;
			texture->SetName(name);
        }
        auto& base = Application::Get().GetRendererBase();
        //create render target views
        RHI::PoolSize ps;
        ps.numDescriptors = sDesc.BufferCount;
        ps.type  = RHI::DescriptorType::RTV;
        RHI::DescriptorHeapDesc rtvHeapDesc;
        rtvHeapDesc.maxDescriptorSets = 1;
        rtvHeapDesc.numPoolSizes = 1;
        rtvHeapDesc.poolSizes = &ps;
		mainRTVheap = RendererBase::GetDevice()->CreateDescriptorHeap(rtvHeapDesc).value();
		PT_CORE_INFO("Created RTV descriptor heaps");
		for (int i = 0; i < sDesc.BufferCount; i++)
		{
			RHI::CPU_HANDLE handle;
			handle.val = mainRTVheap->GetCpuHandle().val + (i * base.device->GetDescriptorHeapIncrementSize(RHI::DescriptorType::RTV));
			RHI::RenderTargetViewDesc rtvDesc;
			rtvDesc.arraySlice = rtvDesc.TextureArray = rtvDesc.textureMipSlice = 0;
			rtvDesc.format = RHI::Format::B8G8R8A8_UNORM;
			base.device->CreateRenderTargetView(swapTextures[i], rtvDesc, handle);
		}
		BackBufferBarrier(
				RHI::PipelineStage::TOP_OF_PIPE_BIT,
				RHI::PipelineStage::TRANSFER_BIT, 

				RHI::ResourceLayout::UNDEFINED,
				RHI::ResourceLayout::TRANSFER_DST_OPTIMAL, 

				RHI::ResourceAcessFlags::NONE, 
				RHI::ResourceAcessFlags::TRANSFER_WRITE);
		PT_CORE_INFO("Created Tender Target Views");
    }
    void SwapChain::Update()
    {
        auto& base = Application::Get().GetRendererBase();
        BackBufferBarrier(
				RHI::PipelineStage::TRANSFER_BIT,
				RHI::PipelineStage::BOTTOM_OF_PIPE_BIT, 

				RHI::ResourceLayout::TRANSFER_DST_OPTIMAL,
				RHI::ResourceLayout::PRESENT, 

				RHI::ResourceAcessFlags::TRANSFER_WRITE, 
				RHI::ResourceAcessFlags::NONE);
        swapchain->Present(base.mainFence, base.fence_vals[(base.currentFrameIndex+2)%3]);
        BackBufferBarrier(
				RHI::PipelineStage::TOP_OF_PIPE_BIT,
				RHI::PipelineStage::TRANSFER_BIT, 

				RHI::ResourceLayout::UNDEFINED,
				RHI::ResourceLayout::TRANSFER_DST_OPTIMAL, 

				RHI::ResourceAcessFlags::NONE, 
				RHI::ResourceAcessFlags::TRANSFER_WRITE);
    }
}