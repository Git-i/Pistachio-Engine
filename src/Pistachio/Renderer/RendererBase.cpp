#include "Barrier.h"
#include "CommandList.h"
#include "CommandQueue.h"
#include "Device.h"
#include "FormatsAndTypes.h"
#include "Instance.h"
#include "PhysicalDevice.h"
#include "Pistachio/Core.h"
#include "Pistachio/Core/Application.h"
#include "Pistachio/Debug/Instrumentor.h"
#include "Ptr.h"
#include "Texture.h"
#include "TraceRHI.h"
#include "RendererBase.h"
#include "Pistachio/Core/Log.h"
#include "Util/FormatUtils.h"
#include <cstdint>
#include <vector>

static const uint32_t STAGING_BUFFER_INITIAL_SIZE = 80 * 1024 * 1024; //todo: reduce this
namespace Pistachio {
	static RHI::Device* s_device = nullptr;
	void exit_handler()
	{
		raise(SIGTRAP);
	}
	void RendererBase::Shutdown()
	{
		auto& base = Application::Get().GetRendererBase();
		base.mainFence->Wait(base.fence_vals[(base.currentFrameIndex+2)%3]);
	}
	
	void RendererBase::EndFrame()
	{
		auto& base = Application::Get().GetRendererBase();
		PT_PROFILE_FUNCTION();
		if(auto wnd = Application::Get().GetWindow())
		{
			wnd->GetSwapChain().BackBufferBarrier(
				RHI::PipelineStage::TRANSFER_BIT,
				RHI::PipelineStage::BOTTOM_OF_PIPE_BIT, 

				RHI::ResourceLayout::TRANSFER_DST_OPTIMAL,
				RHI::ResourceLayout::PRESENT, 

				RHI::ResourceAcessFlags::TRANSFER_WRITE, 
				RHI::ResourceAcessFlags::NONE);
		}
		base.mainCommandList->End();
		base.directQueue->ExecuteCommandLists(&base.mainCommandList->ID, 1);
		base.fence_vals[base.currentFrameIndex] = ++base.currentFenceVal;
		base.directQueue->SignalFence(base.mainFence, base.currentFenceVal); //todo add fence signaling together with queue
		
		base.currentFrameIndex = (base.currentFrameIndex + 1) % 3;
		//prep for next frame
		{
			PT_PROFILE_SCOPE("Wait For Past Frame To Complete");
			base.mainFence->Wait(base.fence_vals[base.currentFrameIndex]);
		}
		{
			PT_PROFILE_SCOPE("Prep Command List and Allocators for Next Frame");
			base.commandAllocators[base.currentFrameIndex]->Reset();
			if(base.computeQueue.IsValid()) base.computeCommandAllocators[base.currentFrameIndex]->Reset();
			base.mainCommandList->Begin(base.commandAllocators[base.currentFrameIndex]);
		}
	}
	uint32_t DeviceScore(RHI::PhysicalDevice* device)
	{
		uint32_t score = 1;
		auto desc = device->GetDesc();
		if(desc.type == RHI::DeviceType::Dedicated)
		{
			score += 5;
		}
		else if(desc.type == RHI::DeviceType::Integrated)
		{
			score += 2;
		}
		return score;
	}
	static RHI::PhysicalDevice* SelectPhysicalDevice(std::span<RHI::PhysicalDevice*> devices)
	{
		std::unordered_map<RHI::PhysicalDevice*, uint32_t> scores;
		for(auto& device: devices)
		{
			scores[device] = DeviceScore(device);
		}
		std::pair<RHI::PhysicalDevice*, uint32_t> device = {nullptr, 0};
		for(auto& score : scores)
		{
			if (score.second > device.second)
			{
				device = score;
			}
		}
		return device.first;
	}
	std::pair<std::vector<RHI::CommandQueueDesc>, bool> CreateCommandQueueDesc(const RHI::QueueInfo& info, bool force_single)
	{
		PT_CORE_ASSERT(info.graphicsSupported && info.computeSupported && info.copySupported, "Selected Device Doesn't Support Graphics, Compute and Transfer");
		std::vector<RHI::CommandQueueDesc> descs;
		bool separateCompute = false;
		if(info.gfxCountIndex == info.cmpCountIndex)
		{
			if(info.counts[info.gfxCountIndex] > 1) separateCompute = true;
		}
		else
			separateCompute = true;
		if(force_single) separateCompute = false;
		auto& gfxQueue = descs.emplace_back();
		gfxQueue.commandListType = RHI::CommandListType::Direct;
		gfxQueue.Priority = 1.f;
		if(separateCompute)
		{
			auto& cmpQueue = descs.emplace_back();
			cmpQueue.commandListType = RHI::CommandListType::Compute;
			cmpQueue.Priority = 1.f;
		}
		return {descs, separateCompute};
	}
	bool RendererBase::Init(InitOptions& options)
	{
		PT_PROFILE_FUNCTION();
		PT_CORE_ASSERT((options.custom_instance && options.custom_device) || (!options.custom_instance && !options.custom_device));
		if(options.custom_instance) instance = RHI::Instance::FromNativeHandle(options.custom_instance).value();
		else instance = RHI::Instance::Create().value();
		instance->SetLoggerCallback([](RHI::LogLevel l, std::string_view str)
		{
			using enum RHI::LogLevel;
			switch(l)
			{
				case Error: PT_CORE_ERROR("RHI: {0}", str); break;
				case Warn: PT_CORE_WARN("RHI: {0}", str); break;
				case Info: PT_CORE_INFO("RHI: {0}", str); break;
			}
		});
		//todo implement device selection
		PT_CORE_INFO("Initializing RendererBase");
		if(options.custom_device)
		{
			PT_CORE_INFO("Custom Device Provided");
			RHI::CommandQueueDesc desc;
			desc.commandListType = RHI::CommandListType::Direct;
			desc.Priority = 1.f;
			device = RHI::Device::FromNativeHandle(options.custom_device, options.custom_physical_device,options.custom_instance, options.indices).value();
			physicalDevice = RHI::PhysicalDevice::FromNativeHandle(options.custom_physical_device);
			directQueue = RHI::CommandQueue::FromNativeHandle(options.custom_direct_queue);
			if(options.custom_compute_queue)
			{
				computeQueue = RHI::CommandQueue::FromNativeHandle(options.custom_compute_queue);
			}
		}
		else {
			uint32_t num_devices =  instance->GetNumPhysicalDevices();
			PT_CORE_INFO("Found {0} physical devices: ", num_devices);
			std::vector<RHI::PhysicalDevice*> pdevices(num_devices);
			instance->GetAllPhysicalDevices(pdevices.data());
			physicalDevice = options.custom_fn ?  options.custom_fn(pdevices) : SelectPhysicalDevice(pdevices);
			for (auto pDevice : pdevices)
			{
				auto pDDesc = pDevice->GetDesc();
				if(options.useLuid) if (memcmp(options.luid.data, pDDesc.AdapterLuid.data, 8) == 0) physicalDevice = pDevice;
				PT_CORE_INFO("    {0}", pDDesc.Description);
			}
			

			PT_CORE_INFO("Creating Device");
			auto flag = options.exportTexture ? RHI::DeviceCreateFlags::ShareAutomaticMemory: RHI::DeviceCreateFlags::None;
			RHI::QueueInfo queue_config = physicalDevice->GetQueueInfo();
			auto [queue_infos, seperate_compute] = CreateCommandQueueDesc(queue_config, options.forceSingleQueue);
			auto [dev, queue] = RHI::Device::Create(physicalDevice, queue_infos, instance, flag).value();
			device = dev;
			directQueue = queue[0];
			if(seperate_compute) computeQueue = queue[1];
			else computeQueue = nullptr;
			PT_CORE_INFO("Device Created ID:{0}, Physical Device used [{1}]", device->ID, physicalDevice->GetDesc().Description);
		}


		device->SetName("RHI Device");

		commandAllocators[0] = device->CreateCommandAllocator(RHI::CommandListType::Direct).value();
		commandAllocators[1] = device->CreateCommandAllocator(RHI::CommandListType::Direct).value();
		commandAllocators[2] = device->CreateCommandAllocator(RHI::CommandListType::Direct).value();
		PT_CORE_INFO("Created main command allocators");
		if(computeQueue.IsValid())
		{
			computeCommandAllocators[0] = device->CreateCommandAllocator(RHI::CommandListType::Compute).value();
			computeCommandAllocators[1] = device->CreateCommandAllocator(RHI::CommandListType::Compute).value();
			computeCommandAllocators[2] = device->CreateCommandAllocator(RHI::CommandListType::Compute).value();
			PT_CORE_INFO("Created compute command allocators");
		}
		stagingCommandAllocator = device->CreateCommandAllocator(RHI::CommandListType::Direct).value();
		PT_CORE_INFO("Created staging command allocator(s)");
		stagingCommandList = device->CreateCommandList(RHI::CommandListType::Direct, stagingCommandAllocator).value();
		PT_DEBUG_REGION(stagingCommandList->SetName("Staging List"));
		PT_CORE_INFO("Created staging command list");
		stagingCommandList->Begin(stagingCommandAllocator);
		PT_CORE_INFO("Began staging command list");
		//create a main command list for now, multithreading will come later
		mainCommandList = device->CreateCommandList(RHI::CommandListType::Direct, commandAllocators[0]).value();
		PT_DEBUG_REGION(mainCommandList->SetName("Main Command List"));
		PT_CORE_INFO("Created main command list");
		mainFence = device->CreateFence(0).value();
		stagingFence = device->CreateFence(0).value();
		PT_CORE_INFO("Created fence(s)");

		PT_CORE_INFO("Creating Trace Context");
		traceRHICtx = TraceRHIContext(instance, physicalDevice, device, directQueue, mainCommandList);

		RHI::PoolSize HeapSizes[3];

		HeapSizes[0].numDescriptors = 40;
		HeapSizes[0].type = RHI::DescriptorType::ConstantBuffer;

		HeapSizes[1].numDescriptors = 40;
		HeapSizes[1].type = RHI::DescriptorType::SampledTexture;

		HeapSizes[2].numDescriptors = 40;
		HeapSizes[2].type = RHI::DescriptorType::StructuredBuffer;

		RHI::DescriptorHeapDesc DHdesc;
		DHdesc.maxDescriptorSets = 120;
		DHdesc.numPoolSizes = 3;
		DHdesc.poolSizes = HeapSizes;
		heap = device->CreateDescriptorHeap(DHdesc).value();
		PT_CORE_INFO("Created main descriptor heap");


		//initialize the staing buffer
		stagingBufferSize = STAGING_BUFFER_INITIAL_SIZE;
		RHI::BufferDesc stagingBufferDesc;
		stagingBufferDesc.size = stagingBufferSize;
		stagingBufferDesc.usage = RHI::BufferUsage::CopySrc;
		RHI::AutomaticAllocationInfo allocInfo;
		allocInfo.access_mode = RHI::AutomaticAllocationCPUAccessMode::Sequential;
		stagingBuffer = device->CreateBuffer(stagingBufferDesc, nullptr, nullptr, &allocInfo, 0, RHI::ResourceType::Automatic).value();

		

		uint8_t whiteData[4] = {255,255,255,255};
		uint8_t blackData[4] = {0,0,0,0};

		whiteTexture.CreateStack(1,1,RHI::Format::R8G8B8A8_UNORM,whiteData PT_DEBUG_REGION(, "RendererBase -> White Texture"));
		blackTexture.CreateStack(1,1,RHI::Format::R8G8B8A8_UNORM,blackData PT_DEBUG_REGION(, "RendererBase -> White Texture"));

		//prep for rendering
		mainCommandList->Begin(commandAllocators[0]);
		
		PT_CORE_INFO("Done Initializing RHI");
		s_device = device.Raw();
		std::atexit(exit_handler);
		return 0;
	}
	RHI::API RendererBase::GetAPI()
	{
		auto& base = Application::Get().GetRendererBase();
		return base.instance->GetInstanceAPI();
	}

	RHI::Ptr<RHI::Instance>& RendererBase::GetInstance()
	{
		auto& base = Application::Get().GetRendererBase();
		return base.instance;
	}
	void RendererBase::PushBufferUpdate(RHI::Weak<RHI::Buffer> buffer, uint32_t offsetFromBufferStart, const void* data, uint32_t size)
	{
		auto& base = Application::Get().GetRendererBase();
		//check if we have enough space to queue the write
		if ((base.stagingBufferSize - base.staginBufferPortionUsed) < size)
		{
			FlushStagingBuffer();
		}
		//check if resource is bigger than the entire buffer
		if(size > base.stagingBufferSize)
		{
			return;
			//split updates into portions and return from function
			// or make a larger buffer temporarily, or better expand the staging buffer
		}
		void* stagingBufferPointer;
		base.stagingBuffer->Map(&stagingBufferPointer);
		stagingBufferPointer = ((std::uint8_t*)stagingBufferPointer) + base.staginBufferPortionUsed; //offset by the amount of bytes already used
		memcpy(stagingBufferPointer, data, size);
		base.stagingBuffer->UnMap();
		base.stagingCommandList->CopyBufferRegion(base.staginBufferPortionUsed, offsetFromBufferStart, size, base.stagingBuffer, buffer);
		base.staginBufferPortionUsed += size;
		base.outstandingResourceUpdate = true;
	}

	void RendererBase::PushTextureUpdate(RHI::Weak<RHI::Texture> texture, uint32_t size ,const void* data,RHI::SubResourceRange* range, RHI::Extent3D imageExtent, RHI::Offset3D imageOffset,RHI::Format format)
	{
		auto& base = Application::Get().GetRendererBase();
		PT_CORE_ASSERT((size % (imageExtent.width * imageExtent.height)) == 0);
		//check required offset
		uint32_t offsetFactor = RHI::Util::GetFormatBPP(format);
		uint32_t currentOffset = base.staginBufferPortionUsed;
		uint32_t requiredOffset = 0;
		if (currentOffset==0) requiredOffset = 0;
		else
		{
			uint32_t remainder = currentOffset % offsetFactor;
			if (remainder == 0) requiredOffset = currentOffset;
			else requiredOffset = currentOffset + offsetFactor - remainder;
		}
		//check if we have enough space to queue the write
		if ((base.stagingBufferSize - base.staginBufferPortionUsed) < size+requiredOffset)
		{
			FlushStagingBuffer();
			requiredOffset = 0;
		}
		//check if resource is bigger than the entire buffer
		if (size > base.stagingBufferSize)
		{
			//split updates into portions and return from function
			// or make a larger buffer temporarily, or better expand the staging buffer
			return;
		}
		void* stagingBufferPointer;
		base.stagingBuffer->Map(&stagingBufferPointer);
		stagingBufferPointer = ((std::uint8_t*)stagingBufferPointer) + requiredOffset; //offset by the amount of bytes already used
		memcpy(stagingBufferPointer, data, size);
		base.stagingBuffer->UnMap();
		base.stagingCommandList->CopyBufferToImage(requiredOffset, *range, imageOffset, imageExtent, base.stagingBuffer, texture);
		base.staginBufferPortionUsed = size + requiredOffset;
		base.outstandingResourceUpdate = true;
	}

	RHI::Ptr<RHI::DescriptorSet> RendererBase::CreateDescriptorSet(RHI::Ptr<RHI::DescriptorSetLayout> layout)
	{
		//TODO make this dynamic
		auto& base = Application::Get().GetRendererBase();
		return base.device->CreateDescriptorSets(base.heap, 1, &layout).value()[0];
	}
	void RendererBase::FlushGPU()
	{
		auto& base = Get();
		auto f = base.device->CreateFence(0).value();
		base.directQueue->SignalFence(f, 1);
		f->Wait(1);
		if(base.computeQueue.IsValid())
		{
			base.computeQueue->SignalFence(f, 2);
			f->Wait(2);
		}
	}

	void RendererBase::FlushStagingBuffer()
	{
		auto& base = Get();
		if(!base.outstandingResourceUpdate) return;
		static uint64_t stagingFenceVal = 0;
		stagingFenceVal++;
		base.stagingCommandList->End();
		base.directQueue->ExecuteCommandLists(&base.stagingCommandList->ID, 1); //todo look into dedicated transfer queue ??
		base.directQueue->SignalFence(base.stagingFence, stagingFenceVal);
		base.stagingFence->Wait(stagingFenceVal); // consider doing this async perhaps;
		base.stagingCommandAllocator->Reset();
		base.stagingCommandList->Begin(base.stagingCommandAllocator);
		base.staginBufferPortionUsed = 0;
		base.outstandingResourceUpdate = false;
	}


	void RendererBase::DrawIndexed(uint32_t indexCount)
	{
		auto& base = Application::Get().GetRendererBase();
		base.mainCommandList->DrawIndexed(indexCount, 1, 0, 0, 0);
	}



	RHI::Ptr<RHI::Device>& RendererBase::GetDevice()
	{
		auto& base = Application::Get().GetRendererBase();
		return base.device;
	}
	RHI::Ptr<RHI::GraphicsCommandList>& RendererBase::GetMainCommandList()
	{
		auto& base = Application::Get().GetRendererBase();
		return base.mainCommandList;
	}
	UniqueRTVHandle RendererBase::CreateRenderTargetView(RHI::Weak<RHI::Texture> texture, const RHI::RenderTargetViewDesc& desc)
	{
		using return_t = UniqueHandle<RTVHandle, DestroyRenderTargetView>;
		auto& base = Application::Get().GetRendererBase();
		if (base.freeRTVs.size())
		{
			auto handle = base.freeRTVs[base.freeRTVs.size() - 1];
			RHI::CPU_HANDLE CPUhandle{};
			CPUhandle.val = base.rtvHeaps[handle.heapIndex].heap->GetCpuHandle().val +
				base.device->GetDescriptorHeapIncrementSize(RHI::DescriptorType::RTV) * handle.heapOffset;
			base.device->CreateRenderTargetView(texture, desc, CPUhandle);
			base.freeRTVs.pop_back();
			return return_t(std::move(handle));
		}
		for (uint32_t i = 0; i < base.rtvHeaps.size();i++)
		{
			auto& heap = base.rtvHeaps[i];
			if (heap.sizeLeft)
			{
				RHI::CPU_HANDLE CPUhandle{};
				CPUhandle.val = heap.heap->GetCpuHandle().val +
					base.device->GetDescriptorHeapIncrementSize(RHI::DescriptorType::RTV) * heap.freeOffset;
				base.device->CreateRenderTargetView(texture, desc, CPUhandle);
				heap.freeOffset++;
				heap.sizeLeft--;
				return return_t(RTVHandle{ i, heap.freeOffset - 1 });
			}
		}
		//no space in all heaps
		auto& heap = base.rtvHeaps.emplace_back();
		RHI::PoolSize pSize;
		pSize.numDescriptors = 10;
		pSize.type = RHI::DescriptorType::RTV;
		RHI::DescriptorHeapDesc hDesc;
		hDesc.maxDescriptorSets = 10;//?
		hDesc.numPoolSizes = 1;
		hDesc.poolSizes = &pSize;
		heap.heap = base.device->CreateDescriptorHeap(hDesc).value();
		heap.sizeLeft = 10;
		heap.freeOffset = 0;
		return CreateRenderTargetView(texture, desc);

	}
	UniqueDSVHandle RendererBase::CreateDepthStencilView(RHI::Weak<RHI::Texture> texture, const RHI::DepthStencilViewDesc& desc)
	{
		using return_t = UniqueHandle<DSVHandle, DestroyDepthStencilView>;
		auto& base = Application::Get().GetRendererBase();
		if (base.freeDSVs.size())
		{
			auto handle = base.freeDSVs[base.freeDSVs.size() - 1];
			RHI::CPU_HANDLE CPUhandle{};
			CPUhandle.val = base.dsvHeaps[handle.heapIndex].heap->GetCpuHandle().val +
				base.device->GetDescriptorHeapIncrementSize(RHI::DescriptorType::DSV) * handle.heapOffset;
			base.device->CreateDepthStencilView(texture, desc, CPUhandle);
			base.freeDSVs.pop_back();
			return return_t(std::move(handle));
		}
		for (uint32_t i = 0; i < base.dsvHeaps.size(); i++)
		{
			auto& heap = base.dsvHeaps[i];
			if (heap.sizeLeft)
			{
				RHI::CPU_HANDLE CPUhandle{};
				CPUhandle.val = heap.heap->GetCpuHandle().val +
					base.device->GetDescriptorHeapIncrementSize(RHI::DescriptorType::DSV) * heap.freeOffset;
				base.device->CreateDepthStencilView(texture, desc, CPUhandle);
				heap.freeOffset++;
				heap.sizeLeft--;
				return return_t(DSVHandle{ i, heap.freeOffset - 1 });
			}
		}
		//no space in all heaps
		auto& heap = base.dsvHeaps.emplace_back();
		RHI::PoolSize pSize{
			.type = RHI::DescriptorType::DSV,
			.numDescriptors = 10,
		};
		RHI::DescriptorHeapDesc hDesc{
		.maxDescriptorSets = 10,
			.numPoolSizes = 1,
			.poolSizes = &pSize,
		};
		heap.heap = base.device->CreateDescriptorHeap(hDesc).value();
		heap.sizeLeft = 10;
		heap.freeOffset = 0;
		return CreateDepthStencilView(texture, desc);
	}
	UniqueSamplerHandle RendererBase::CreateSampler(const RHI::SamplerDesc& viewDesc)
	{
		using return_t = UniqueHandle<SamplerHandle, DestroySampler>;
		auto& base = Application::Get().GetRendererBase();
		if (!base.freeSamplers.empty())
		{
			auto handle = base.freeSamplers[base.freeSamplers.size() - 1];
			RHI::CPU_HANDLE CPUhandle{};
			CPUhandle.val = base.samplerHeaps[handle.heapIndex].heap->GetCpuHandle().val +
				base.device->GetDescriptorHeapIncrementSize(RHI::DescriptorType::Sampler) * handle.heapOffset;
			base.device->CreateSampler(viewDesc, CPUhandle);
			base.freeSamplers.pop_back();
			return return_t(std::move(handle));
		}
		for (uint32_t i = 0; i < base.samplerHeaps.size(); i++)
		{
			auto& heap = base.samplerHeaps[i];
			if (heap.sizeLeft)
			{
				RHI::CPU_HANDLE CPUhandle{};
				CPUhandle.val = heap.heap->GetCpuHandle().val +
					base.device->GetDescriptorHeapIncrementSize(RHI::DescriptorType::Sampler) * heap.freeOffset;
				base.device->CreateSampler(viewDesc, CPUhandle);
				heap.freeOffset++;
				heap.sizeLeft--;
				return return_t(SamplerHandle{ i, heap.freeOffset - 1 });
			}
		}
		//no space in all heaps
		auto& heap = base.samplerHeaps.emplace_back();
		RHI::PoolSize pSize {
			.type = RHI::DescriptorType::Sampler,
			.numDescriptors = 10,
		};
		RHI::DescriptorHeapDesc hDesc{
			.maxDescriptorSets = 10,
			.numPoolSizes = 1,
			.poolSizes = &pSize,
		};
		heap.heap = base.device->CreateDescriptorHeap(hDesc).value();
		heap.sizeLeft = 10;
		heap.freeOffset = 0;
		return CreateSampler(viewDesc);
	}

	void RendererBase::DestroyRenderTargetView(RTVHandle handle)
	{
		auto& base = Application::Get().GetRendererBase();
		base.device->DestroyRenderTargetView(GetCPUHandle(handle));
		base.freeRTVs.push_back(handle);
	}
	void RendererBase::DestroyDepthStencilView(DSVHandle handle)
	{
		auto& base = Application::Get().GetRendererBase();
		base.device->DestroyDepthStencilView(GetCPUHandle(handle));
		base.freeDSVs.push_back(handle);
	}
	void RendererBase::DestroySampler(SamplerHandle handle)
	{
		auto& base = Application::Get().GetRendererBase();
		base.device->DestroySampler(GetCPUHandle(handle));
		base.freeSamplers.push_back(handle);
	}
	RHI::CPU_HANDLE RendererBase::GetCPUHandle(RTVHandle handle)
	{
		auto& base = Application::Get().GetRendererBase();
		RHI::CPU_HANDLE retVal;
		retVal.val = base.rtvHeaps[handle.heapIndex].heap->GetCpuHandle().val +
			base.device->GetDescriptorHeapIncrementSize(RHI::DescriptorType::RTV) * handle.heapOffset;
		return retVal;
	}
	RHI::CPU_HANDLE RendererBase::GetCPUHandle(DSVHandle handle)
	{
		auto& base = Application::Get().GetRendererBase();
		RHI::CPU_HANDLE retVal;
		retVal.val = base.dsvHeaps[handle.heapIndex].heap->GetCpuHandle().val +
			base.device->GetDescriptorHeapIncrementSize(RHI::DescriptorType::DSV) * handle.heapOffset;
		return retVal;
	}
	RHI::CPU_HANDLE RendererBase::GetCPUHandle(SamplerHandle handle)
	{
		auto& base = Application::Get().GetRendererBase();
		RHI::CPU_HANDLE retVal;
		retVal.val = base.samplerHeaps[handle.heapIndex].heap->GetCpuHandle().val +
			base.device->GetDescriptorHeapIncrementSize(RHI::DescriptorType::Sampler) * handle.heapOffset;
		return retVal;
	}
	TraceRHI::Context& RendererBase::TraceContext()
	{
		auto& base = Application::Get().GetRendererBase();
		return base.traceRHICtx;
	}
	RHI::Ptr<RHI::DescriptorHeap>& RendererBase::GetMainDescriptorHeap()
	{ 
		auto& base = Application::Get().GetRendererBase();
		return base.heap;
	}
	uint32_t RendererBase::GetCurrentFrameIndex(){ 
		auto& base = Application::Get().GetRendererBase();
		return base.currentFrameIndex;
	}
	
	RHI::Ptr<RHI::CommandQueue>& RendererBase::GetDirectQueue(){return Get().directQueue;}
	RHI::Ptr<RHI::CommandQueue>& RendererBase::GetComputeQueue(){return Get().computeQueue;}
	Texture2D& RendererBase::GetWhiteTexture()
	{ 
		auto& base = Application::Get().GetRendererBase();
		return base.whiteTexture;
	}
	RendererBase& RendererBase::Get()
	{
		return Application::Get().GetRendererBase();
	}
	Texture2D& RendererBase::GetBlackTexture()
	{ 
		auto& base = Application::Get().GetRendererBase();
		return base.blackTexture;
	}
	RHI::PhysicalDevice* RendererBase::GetPhysicalDevice()
	{
		return Get().physicalDevice;
	}
	RendererBase& Get()
	{
		return Application::Get().GetRendererBase();
	}
	RHI::Ptr<RHI::GraphicsCommandList>& RendererBase::GetStagingCommandList()
	{
		return Get().stagingCommandList;
	}
}
