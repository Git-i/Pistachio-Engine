#include "SwapChain.h"
#include "DescriptorHeap.h"
#include "Device.h"
#include "FormatsAndTypes.h"
#include "Pistachio/Core/Log.h"
#include "Pistachio/Utils/RendererUtils.h"
#include "Pistachio/Renderer/RendererBase.h"
#include "Application.h"
#include "Surface.h"
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
	static RHI::SwapChainDesc MakeDesc(RHI::Surface& srf, uint32_t width, uint32_t height)
	{
		static uint32_t size  = RendererUtils::SwapImageCount(RendererBase::GetInstance()->GetSwapChainMinMaxImageCount(RendererBase::GetPhysicalDevice(), &srf), 2);
		RHI::SwapChainDesc sDesc;
		sDesc.BufferCount = size;
		sDesc.Flags = 0;
		sDesc.Height = height;
		sDesc.Width = width;
		sDesc.OutputSurface = srf;
		sDesc.RefreshRate = { 60,1 };
		sDesc.SampleCount = 1; //disable multisampling for now, because RHI doesnt fully support it
		sDesc.SampleQuality = 0;
		sDesc.SwapChainFormat = RHI::Format::B8G8R8A8_UNORM;//todo add functionality to get supported formats in the RHI
		sDesc.Windowed = true;
		return sDesc;
	}
    void SwapChain::Initialize(uint32_t width, uint32_t height)
    {
        auto sDesc = MakeDesc(surface, width, height);
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
        
        auto res = swapchain->Present(base.mainFence, base.fence_vals[(base.currentFrameIndex+2)%3]);
		if(res == RHI::SwapChainError::OutOfDate || res == RHI::SwapChainError::SubOptimal)
		{
			PT_CORE_WARN("SwapChain Out of Date");
			auto wnd = Application::Get().GetWindow();
			Resize(wnd->GetWidth(), wnd->GetHeight());
		}
        BackBufferBarrier(
				RHI::PipelineStage::TOP_OF_PIPE_BIT,
				RHI::PipelineStage::TRANSFER_BIT, 

				RHI::ResourceLayout::UNDEFINED,
				RHI::ResourceLayout::TRANSFER_DST_OPTIMAL, 

				RHI::ResourceAcessFlags::NONE, 
				RHI::ResourceAcessFlags::TRANSFER_WRITE);
    }
	void SwapChain::Resize(uint32_t width, uint32_t height)
	{
		RendererBase::Get().mainFence->Wait(RendererBase::Get().fence_vals[(RendererBase::GetCurrentFrameIndex() + 2) % 3]);
		auto sDesc = MakeDesc(surface, width, height);
		swapchain->Release();
		swapchain = nullptr;
		swapchain = RendererBase::GetInstance()->CreateSwapChain(sDesc, RendererBase::GetPhysicalDevice(), RendererBase::GetDevice(), RendererBase::GetDirectQueue()).value();
		uint32_t index = 0;
        for(auto& texture : swapTextures)
        {
            texture = RendererBase::GetDevice()->GetSwapChainImage(swapchain, index++).value();
            char name[20] = {"Back Buffer Image 0"};
			name[18] += index;
			texture->SetName(name);
        }
		BackBufferBarrier(
				RHI::PipelineStage::TOP_OF_PIPE_BIT,
				RHI::PipelineStage::TRANSFER_BIT, 

				RHI::ResourceLayout::UNDEFINED,
				RHI::ResourceLayout::TRANSFER_DST_OPTIMAL, 

				RHI::ResourceAcessFlags::NONE, 
				RHI::ResourceAcessFlags::TRANSFER_WRITE);
	}
}