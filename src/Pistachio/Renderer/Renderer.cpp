#include "FormatsAndTypes.h"
#include "Pistachio/Renderer/RenderGraph.h"
#include "Pistachio/Renderer/RendererBase.h"
#include "Pistachio/Renderer/RendererContext.h"
#include "Pistachio/Renderer/Shader.h"
#include "Pistachio/Utils/RendererUtils.h"
#include "Texture.h"
#include "client/TracyProfiler.hpp"
#include "common/TracyQueue.hpp"
#include "ptpch.h"
#include "Renderer.h"
#include "Material.h"
#include "../Scene/Scene.h"
#include "Pistachio/Core/Window.h"
#include "Pistachio/Core/Math.h"
#include <functional>
#include "Pistachio/Core/Application.h"

namespace Pistachio {
	inline static Renderer& Self()
	{
		return Application::Get().GetRenderer();
	}
	void Renderer::Init()
	{
		Self().ctx.Initailize();
	}
	void Renderer::ChangeRGTexture(RGTextureHandle& texture, RHI::ResourceLayout newLayout, RHI::ResourceAcessFlags newAccess,RHI::QueueFamily newFamily)
	{
		texture.originVector->at(texture.offset).current_layout = newLayout;
		texture.originVector->at(texture.offset).currentAccess = newAccess;
		texture.originVector->at(texture.offset).currentFamily = newFamily;
	}
	
	void Renderer::EndScene()
	{
		RendererBase::EndFrame();
		PT_PROFILE_FRAME_MARK;
	}
	void Renderer::Shutdown() {
		
		RendererBase::Shutdown();
	}
	const RendererVBHandle Renderer::AllocateVertexBuffer(uint32_t size,const void* initialData)
	{
		return Self().ctx.meshVertices.allocator.Allocate(std::bind(Renderer::GrowMeshBuffer, std::placeholders::_1, 
			RHI::BufferUsage::VertexBuffer|RHI::BufferUsage::CopySrc|RHI::BufferUsage::CopyDst,
			std::ref(Self().ctx.meshVertices)),
			std::bind(Renderer::DefragmentMeshBuffer, std::ref(Self().ctx.meshIndices)),size,&Self().ctx.meshVertices, initialData);
	}
	const RendererIBHandle Renderer::AllocateIndexBuffer(uint32_t size, const void* initialData)
	{
		auto [a,b] = Self().ctx.meshIndices.allocator.Allocate(std::bind(Renderer::GrowMeshBuffer, std::placeholders::_1, 
			RHI::BufferUsage::IndexBuffer|RHI::BufferUsage::CopySrc|RHI::BufferUsage::CopyDst,
			std::ref(Self().ctx.meshIndices)), 
			std::bind(Renderer::DefragmentMeshBuffer, std::ref(Self().ctx.meshIndices)), size,&Self().ctx.meshIndices, initialData);
		return { a,b };
	}
	ComputeShader* Renderer::GetBuiltinComputeShader(const std::string& name)
	{
		if (auto it = Self().ctx.computeShaders.find(name); it != Self().ctx.computeShaders.end())
		{
			return it->second;
		}
		return nullptr;
	}
	Shader* Renderer::GetBuiltinShader(const std::string& name)
	{
		if (auto it = Self().ctx.shaders.find(name); it != Self().ctx.shaders.end())
		{
			return it->second;
		}
		return nullptr;
	}
	const RendererCBHandle Renderer::AllocateConstantBuffer(uint32_t size)
	{
		
		auto [a,b] = Self().ctx.constantBufferAllocator.Allocate(&Renderer::GrowConstantBuffer, &Renderer::DefragmentConstantBuffer,
			RendererUtils::ConstantBufferElementSize(size));
		return { a,size, b };
	}
	void Renderer::GrowMeshBuffer(uint32_t minExtraSize,
			RHI::BufferUsage usage,
			MonolithicBuffer& buffer)
	{
		uint32_t capacity = buffer.allocator.capacity;
		const uint32_t GROW_FACTOR = 20; //probably use a better, more size dependent method to determing this
		uint32_t new_size = capacity + minExtraSize + GROW_FACTOR;
		RHI::BufferDesc desc;
		desc.size = new_size;
		desc.usage = usage;
		RHI::AutomaticAllocationInfo allocInfo;
		allocInfo.access_mode = RHI::AutomaticAllocationCPUAccessMode::None;
		RHI::Ptr<RHI::Buffer> newBuffer = RendererBase::GetDevice()->CreateBuffer(desc, 0, 0, &allocInfo, 0, RHI::ResourceType::Automatic).value();
		RHI::BufferMemoryBarrier barr;
		barr.AccessFlagsBefore = RHI::ResourceAcessFlags::TRANSFER_WRITE;
		barr.AccessFlagsAfter = RHI::ResourceAcessFlags::TRANSFER_READ;
		barr.buffer = buffer.buffer;
		barr.nextQueue = barr.previousQueue = RHI::QueueFamily::Ignored;
		barr.size = capacity;
		barr.offset = 0;
		RendererBase::Get().stagingCommandList->PipelineBarrier(RHI::PipelineStage::TRANSFER_BIT, RHI::PipelineStage::TRANSFER_BIT, {&barr,1},{});
		//Queue it with staging stuff
		RendererBase::Get().stagingCommandList->CopyBufferRegion(0, 0, capacity, buffer.buffer, newBuffer);
		barr.AccessFlagsAfter = RHI::ResourceAcessFlags::TRANSFER_WRITE;
		barr.buffer = newBuffer;
		RendererBase::Get().stagingCommandList->PipelineBarrier(RHI::PipelineStage::TRANSFER_BIT, RHI::PipelineStage::TRANSFER_BIT, {&barr,1},{});
		//wait until copy is finished?
		RendererBase::FlushStagingBuffer();
		//before destroying old buffer, wait for old frames to render
		RendererBase::Get().mainFence->Wait(RendererBase::Get().currentFenceVal);
		buffer.buffer = newBuffer;
		buffer.allocator.freeSpace += minExtraSize + GROW_FACTOR;
		buffer.allocator.freeFastSpace += minExtraSize + GROW_FACTOR;
		buffer.allocator.capacity = new_size;
		buffer.allocator.freeList.Grow(new_size);
	}
	void Pistachio::Renderer::GrowConstantBuffer(uint32_t minExtraSize)
	{

		uint32_t capacity = Self().ctx.constantBufferAllocator.capacity;
		const uint32_t GROW_FACTOR = 20; //probably use a better, more size dependent method to determing this
		uint32_t new_size = capacity + minExtraSize + GROW_FACTOR;
		RHI::BufferDesc desc;
		desc.size = new_size;
		desc.usage = RHI::BufferUsage::ConstantBuffer;
		RendererBase::Get().mainFence->Wait(RendererBase::Get().currentFenceVal);
		for (uint32_t i = 0; i < 3; i++)
		{
			
			RHI::AutomaticAllocationInfo allocInfo;
			allocInfo.access_mode = RHI::AutomaticAllocationCPUAccessMode::Sequential;
			RHI::Ptr<RHI::Buffer> newCB = RendererBase::GetDevice()->CreateBuffer(desc, 0, 0, &allocInfo, 0, RHI::ResourceType::Automatic).value();
			void* writePtr;
			void* readPtr;
			newCB->Map(&writePtr);
			Self().ctx.resources[i].transformBuffer.ID->Map(&readPtr);
			memcpy(writePtr, readPtr, capacity);
			Self().ctx.resources[i].transformBuffer.ID->UnMap();
			newCB->UnMap();
			Self().ctx.resources[i].transformBuffer.ID = newCB;
		}
		Self().ctx.constantBufferAllocator.freeSpace += minExtraSize + GROW_FACTOR;
		Self().ctx.constantBufferAllocator.freeFastSpace += minExtraSize + GROW_FACTOR;
		Self().ctx.constantBufferAllocator.capacity = new_size;
		Self().ctx.constantBufferAllocator.freeList.Grow(new_size);
	}
	void Pistachio::Renderer::FreeVertexBuffer(const RendererVBHandle handle)
	{
		Self().ctx.meshVertices.allocator.DeAllocate(handle);
	}
	void Renderer::FreeIndexBuffer(const RendererIBHandle handle)
	{
		Self().ctx.meshIndices.allocator.DeAllocate({handle.handle, handle.size});
	}
	RHI::Ptr<RHI::Buffer> Renderer::GetVertexBuffer()
	{
		return Self().ctx.meshVertices.buffer;
	}
	RHI::Ptr<RHI::Buffer> Renderer::GetIndexBuffer()
	{
		return Self().ctx.meshIndices.buffer;
	}
	void Renderer::DefragmentMeshBuffer(MonolithicBuffer& buffer)
	{


		auto block = buffer.allocator.freeList.GetBlockPtr();
		uint32_t nextFreeOffset = 0;
		while (block)
		{
			if (block->offset > nextFreeOffset)
			{
				RendererBase::Get().stagingCommandList->CopyBufferRegion(block->offset, nextFreeOffset, block->size, buffer.buffer, buffer.buffer);
				nextFreeOffset += block->size;
			}
			block = block->next;
		}
		buffer.allocator.freeList.Reset();
		//fill up the beginning of the free list with the memory size we just copied
		buffer.allocator.freeList.Allocate(0, nextFreeOffset);
		buffer.allocator.freeFastSpace = buffer.allocator.freeSpace;//after defrag all free space is fast space
		/*
		 *Is that a safe assumption?
		 *we dont flush for every copy, because we asssume copies are done in order, so memory won't get overritten
		 */
		RendererBase::FlushStagingBuffer();
		
	}
	void Renderer::DefragmentConstantBuffer()
	{

		RendererBase::Get().mainFence->Wait(RendererBase::Get().currentFenceVal);
		auto block = Self().ctx.constantBufferAllocator.freeList.GetBlockPtr();
		uint32_t nextFreeOffset = 0;
		void *ptr1, *ptr2, *ptr3;
		Self().ctx.resources[0].transformBuffer.ID->Map(&ptr1);
		Self().ctx.resources[1].transformBuffer.ID->Map(&ptr2);
		Self().ctx.resources[2].transformBuffer.ID->Map(&ptr3);
		while (block)
		{
			//if block has gap from last block
			if (block->offset > nextFreeOffset)
			{
				//we use memmove to handle overlap possibility
				memmove((uint8_t*)ptr1 + nextFreeOffset, (uint8_t*)ptr1 + block->offset, block->size);
				memmove((uint8_t*)ptr2 + nextFreeOffset, (uint8_t*)ptr2 + block->offset, block->size);
				memmove((uint8_t*)ptr3 + nextFreeOffset, (uint8_t*)ptr3 + block->offset, block->size);
				nextFreeOffset += block->size;
			}
			block = block->next;
		}
		Self().ctx.resources[0].transformBuffer.ID->UnMap();
		Self().ctx.resources[1].transformBuffer.ID->UnMap();
		Self().ctx.resources[2].transformBuffer.ID->UnMap();
	}
	const uint32_t Pistachio::Renderer::GetIBOffset(const RendererIBHandle handle)
	{
		return Self().ctx.meshIndices.allocator.HandleOffsets[handle.handle];
	}
	const uint32_t Pistachio::Renderer::GetVBOffset(const RendererVBHandle handle)
	{
		return Self().ctx.meshVertices.allocator.HandleOffsets[handle.handle];
	}
	const uint32_t Pistachio::Renderer::GetCBOffset(const RendererCBHandle handle)
	{
		return Self().ctx.constantBufferAllocator.HandleOffsets[handle.handle];
	}
	void Pistachio::Renderer::PartialCBUpdate(RendererCBHandle handle, void* data, uint32_t offset, uint32_t size)
	{
		PT_CORE_ASSERT(offset+size <= handle.size);
		Self().ctx.resources[RendererBase::Get().currentFrameIndex].transformBuffer.Update(data, size, GetCBOffset(handle) + offset);
	}
	void Pistachio::Renderer::FullCBUpdate(RendererCBHandle handle, void* data)
	{
		Self().ctx.resources[RendererBase::Get().currentFrameIndex].transformBuffer.Update(data, handle.actual_size, GetCBOffset(handle));
	}
	RHI::Ptr<RHI::Buffer> Pistachio::Renderer::GetConstantBuffer()
	{
		return Self().ctx.resources[RendererBase::Get().currentFrameIndex].transformBuffer.ID;
	}
	const RHI::Ptr<RHI::DynamicDescriptor> Pistachio::Renderer::GetCBDesc()
	{
		return Self().ctx.resources[RendererBase::Get().currentFrameIndex].transformBufferDesc;
	}
	const RHI::Ptr<RHI::DynamicDescriptor> Renderer::GetCBDescPS()
	{
		return Self().ctx.resources[RendererBase::Get().currentFrameIndex].transformBufferDescPS;
	}
	void Pistachio::Renderer::Submit(RHI::Weak<RHI::GraphicsCommandList> list,const RendererVBHandle vb, const RendererIBHandle ib, uint32_t vertexStride)
	{
		list->DrawIndexed(ib.size / sizeof(uint32_t),
			1,
			GetIBOffset(ib) / sizeof(uint32_t),
			GetVBOffset(vb) / vertexStride, 0);
	}
	SamplerHandle Pistachio::Renderer::GetDefaultSampler()
	{
		return Self().ctx.defaultSampler;
	}
	SamplerHandle Renderer::GetShadowSampler()
	{
		return Self().ctx.shadowSampler;
	}
	Texture2D& Renderer::GetBrdfTexture()
	{
		return Self().ctx.BrdfTex;
	}
}