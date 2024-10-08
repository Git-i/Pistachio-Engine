#pragma once
#include "CommandQueue.h"
#include "DescriptorHeap.h"
#include "FormatsAndTypes.h"
#include "PhysicalDevice.h"
#include "Pistachio/Core.h"
#include "Buffer.h"
#include "../Core/Instance.h"
#include "Pistachio/Renderer/Texture.h"
#include "Ptr.h"
#include "TraceRHI.h"
namespace Pistachio {
	template<typename T>
	concept rendererbase_handle = std::is_trivially_copy_assignable_v<T> && requires(T a){
		{T::Invalid()} -> std::convertible_to<T>;
		{a == a} -> std::convertible_to<bool>;
	};
	template<rendererbase_handle T, void(*deleter)(T)>
	class UniqueHandle
	{
		T data;
	public:
		explicit UniqueHandle(T&& data) : data(data) {};
		UniqueHandle() : data(T::Invalid()) {}
		UniqueHandle(const UniqueHandle&) = delete;
		[[nodiscard]] const T* operator->() const
		{
			return &data;
		}
		[[nodiscard]] T* operator->()
		{
			return &data;
		}
		UniqueHandle(UniqueHandle&& other) noexcept
		{
			data = other.data;
			other.data = static_cast<T>(T::Invalid());
		}
		~UniqueHandle()
		{
			if(data == static_cast<T>(T::Invalid())) return;
			deleter(data);
		}
		[[nodiscard]] const T& Get() const { return data; }
		[[nodiscard]] T& Get() { return data; }

		UniqueHandle& operator=(UniqueHandle&& other) noexcept
		{
			data = other.data;
			other.data = static_cast<T>(T::Invalid());
			return *this;
		}
		UniqueHandle& operator=(const UniqueHandle&) = delete;
	};
	struct RTVHandle
	{
		uint32_t heapIndex;
		uint32_t heapOffset;
		bool operator==(const RTVHandle& other) const {return heapIndex == other.heapIndex && heapOffset == other.heapOffset;}
		constexpr static auto Invalid() { return RTVHandle{UINT32_MAX, UINT32_MAX};}
	};

	struct DSVHandle
	{
		uint32_t heapIndex;
		uint32_t heapOffset;
		bool operator==(const DSVHandle& other) const {return heapIndex == other.heapIndex && heapOffset == other.heapOffset;}
		constexpr static auto Invalid() { return DSVHandle{UINT32_MAX, UINT32_MAX};}
	};
	struct SamplerHandle
	{
		uint32_t heapIndex = UINT32_MAX;
		uint32_t heapOffset = UINT32_MAX;
		bool operator==(const SamplerHandle& other) const {return heapIndex == other.heapIndex && heapOffset == other.heapOffset;}
		constexpr static auto Invalid() {return SamplerHandle{UINT32_MAX, UINT32_MAX};}
	};
	struct TrackedDescriptorHeap
	{
		RHI::Ptr<RHI::DescriptorHeap> heap;
		uint32_t sizeLeft = 0;
		uint32_t freeOffset = 0;
	};

	class PISTACHIO_API RendererBase
	{
	public:
		struct InitOptions
		{
			RHI::LUID luid;
			bool useLuid = false;
			bool exportTexture;
			bool forceSingleQueue;
			Internal_ID custom_device;
			Internal_ID custom_instance;
			Internal_ID custom_physical_device;
			Internal_ID custom_direct_queue;//required
			Internal_ID custom_compute_queue;//optional
			RHI::QueueFamilyIndices indices;
			std::function<RHI::PhysicalDevice*(std::span<RHI::PhysicalDevice*>)> custom_fn;
		};
		static void Shutdown();
		static void EndFrame();
		static RHI::API GetAPI();
		static void DestroyRenderTargetView(RTVHandle handle);
		static void DestroyDepthStencilView(DSVHandle handle);
		static void DestroySampler(SamplerHandle handle);
		static auto CreateRenderTargetView(RHI::Weak<RHI::Texture> texture, const RHI::RenderTargetViewDesc& viewDesc) -> UniqueHandle<RTVHandle, DestroyRenderTargetView>;
		static auto CreateDepthStencilView(RHI::Weak<RHI::Texture> texture, const RHI::DepthStencilViewDesc& viewDesc) -> UniqueHandle<DSVHandle, DestroyDepthStencilView>;
		static auto CreateSampler(const RHI::SamplerDesc& viewDesc) -> UniqueHandle<SamplerHandle, DestroySampler>;
		static RHI::CPU_HANDLE GetCPUHandle(RTVHandle handle);
		static RHI::CPU_HANDLE GetCPUHandle(DSVHandle handle);
		static RHI::CPU_HANDLE GetCPUHandle(SamplerHandle handle);
		static void PushBufferUpdate(RHI::Weak<RHI::Buffer> buffer, uint32_t offsetFromBufferStart,const void* data, uint32_t size);
		static void PushTextureUpdate(RHI::Weak<RHI::Texture> texture, uint32_t imgByteSize,const void* data,const RHI::SubResourceLayers& range, RHI::Extent3D imageExtent, RHI::Offset3D imageOffset,RHI::Format format);
		static RHI::Ptr<RHI::DescriptorSet> CreateDescriptorSet(RHI::Ptr<RHI::DescriptorSetLayout> layout);
		static void FlushStagingBuffer();
		static void FlushGPU();
		static void DrawIndexed(uint32_t indexCount);
		bool Init(InitOptions& options);
		static RHI::Ptr<RHI::Device>& GetDevice();
		static RHI::Ptr<RHI::Instance>& GetInstance();
		static RHI::Ptr<RHI::GraphicsCommandList>& GetMainCommandList();
		//To be used when creating new stuff to transition to optimal layout
		static RHI::Ptr<RHI::GraphicsCommandList>& GetStagingCommandList();
		static RHI::Ptr<RHI::DescriptorHeap>& GetMainDescriptorHeap();
		static RHI::Ptr<RHI::Texture>& GetBackBufferTexture(uint32_t index);
		static RHI::PhysicalDevice* GetPhysicalDevice();
		static RHI::Ptr<RHI::CommandQueue>& GetDirectQueue();
		static RHI::Ptr<RHI::CommandQueue>& GetComputeQueue(); ///<-Returns Invalid Ptr if using single queue
		static Texture2D& GetWhiteTexture();
		static Texture2D& GetBlackTexture();
		static uint32_t GetCurrentFrameIndex();
		static TraceRHI::Context& TraceContext();
		static RendererBase& Get();
		static const constexpr uint32_t numFramesInFlight = 3;
	private:
		friend class Renderer;
		friend class RendererContext;
		friend class Texture2D;
		friend class RenderGraph;
		friend class DepthTexture;
		friend class FrameComposer;
		friend class Scene;
		friend class SwapChain;
		friend class SamplerHandle;
		TraceRHI::Context traceRHICtx;
		RHI::Ptr<RHI::Device> device;
		RHI::Ptr<RHI::GraphicsCommandList> mainCommandList;
		RHI::Ptr<RHI::GraphicsCommandList> stagingCommandList;
		// using one of the frame's allocator would mean that we might reset the staging command list
		// thereby limiting the ability to queue updates effectively
		RHI::Ptr<RHI::CommandAllocator> stagingCommandAllocator;
		RHI::Ptr<RHI::CommandAllocator> commandAllocators[3];
		RHI::Ptr<RHI::CommandAllocator> computeCommandAllocators[3];
		RHI::PhysicalDevice* physicalDevice;
		RHI::Ptr<RHI::CommandQueue> directQueue;
		RHI::Ptr<RHI::CommandQueue> computeQueue;
		RHI::Ptr<RHI::Instance> instance;
		std::vector<TrackedDescriptorHeap> rtvHeaps;
		std::vector<RTVHandle> freeRTVs;
		std::vector<TrackedDescriptorHeap> dsvHeaps;
		std::vector<DSVHandle> freeDSVs;
		std::vector<TrackedDescriptorHeap> samplerHeaps;
		std::vector<SamplerHandle> freeSamplers;
		std::uint64_t fence_vals[3]; //managing sync across allocators
		std::uint64_t currentFenceVal; //managing sync across allocators
		RHI::Ptr<RHI::Fence> mainFence;
		RHI::Ptr<RHI::Fence> stagingFence;
		RHI::Ptr<RHI::DescriptorHeap> heap;



		Texture2D whiteTexture;
		Texture2D blackTexture;
		//Staging buffer to manage GPU resource updates, default size probably 2mb
		RHI::Ptr<RHI::Buffer> stagingBuffer;
		//because staging buffer updates won't happen immediately, we need the number of used bytes
		//staging buffer size will probably never cross 4gb so no need for uint64
		uint32_t staginBufferPortionUsed;
		uint32_t stagingBufferSize;
		bool outstandingResourceUpdate;
		uint32_t currentFrameIndex;
	};
	using UniqueRTVHandle = UniqueHandle<RTVHandle, RendererBase::DestroyRenderTargetView>;
	using UniqueDSVHandle = UniqueHandle<DSVHandle, RendererBase::DestroyDepthStencilView>;
	using UniqueSamplerHandle = UniqueHandle<SamplerHandle, RendererBase::DestroySampler>;
}
