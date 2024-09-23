#include "CommandList.h"
#include "Pistachio/Core/Error.h"
#include "ptpch.h"
#include "Buffer.h"
#include "RendererBase.h"

namespace Pistachio {
	void VertexBuffer::Bind(RHI::GraphicsCommandList* list, uint32_t slot) const
	{
		PT_PROFILE_FUNCTION();
		list->BindVertexBuffers(slot, 1, &ID->ID);//id->id is a little confusing
	}
	Result<VertexBuffer*> VertexBuffer::Create(const void* verts, unsigned int size, unsigned int stride)
	{
		PT_PROFILE_FUNCTION();
		std::unique_ptr<VertexBuffer> result = std::make_unique<VertexBuffer>();
		auto res = result->CreateStack(verts, size, stride);
		if(res.GetErrorType() != ErrorType::Success)
			return ezr::err(std::move(res));
		return ezr::ok(result.release());
	}
	Error VertexBuffer::CreateStack(const void* verts, unsigned int size, unsigned int Stride)
	{
		PT_PROFILE_FUNCTION();
		stride = Stride;
		RHI::BufferDesc vbDesc {
			.size = size,
			.usage = RHI::BufferUsage::VertexBuffer | RHI::BufferUsage::CopyDst
		};
		RHI::AutomaticAllocationInfo info {.access_mode = RHI::AutomaticAllocationCPUAccessMode::None};
		auto res = RendererBase::GetDevice()->CreateBuffer(vbDesc, nullptr, nullptr,&info, 0, RHI::ResourceType::Automatic);
		if(res.is_err())
		{
			return Error::FromRHIError(res.err());
		}
		ID = std::move(res).value();
		if(verts) RendererBase::PushBufferUpdate(ID, 0, verts, size);
		return {};
	}
	void VertexBuffer::SetData(const void* data, unsigned int size)
	{
		PT_PROFILE_FUNCTION();
		RendererBase::PushBufferUpdate(ID, 0, data, size);
	}

	void IndexBuffer::Bind(RHI::GraphicsCommandList* list) const
	{
		PT_PROFILE_FUNCTION();
		list->BindIndexBuffer(ID, 0);
	}
	Error IndexBuffer::CreateStack(const void* indices, unsigned int size, unsigned int stride)
	{
		PT_PROFILE_FUNCTION();
		count = size / stride;
		RHI::BufferDesc desc{
			.size = size,
			.usage = RHI::BufferUsage::IndexBuffer | RHI::BufferUsage::CopyDst
		};
		RHI::AutomaticAllocationInfo info {.access_mode = RHI::AutomaticAllocationCPUAccessMode::None};
		auto res = RendererBase::GetDevice()->CreateBuffer(desc, nullptr, nullptr, &info,0, RHI::ResourceType::Automatic);
		if(res.is_err()) return Error::FromRHIError(res.err());
		ID = std::move(res).value();
		if (indices)
		{
			RendererBase::PushBufferUpdate(ID, 0, indices, size);
		}
		return {};
	}
	Result<IndexBuffer*> IndexBuffer::Create(const void* indices, std::uint32_t size, std::uint32_t stride)
	{
		PT_PROFILE_FUNCTION();
		std::unique_ptr<IndexBuffer> result = std::make_unique<IndexBuffer>();
		auto res = result->CreateStack(indices, size,stride);
		if(res.GetErrorType() != ErrorType::Success)
			return ezr::err(std::move(res));
		return ezr::ok(result.release());
	}
	Error StructuredBuffer::CreateStack(const void* data, std::uint32_t size, SBCreateFlags flags)
	{
		PT_PROFILE_FUNCTION();
		RHI::BufferDesc bufferDesc{
			.size = size,
			.usage = RHI::BufferUsage::StructuredBuffer | RHI::BufferUsage::CopyDst
		};
		RHI::AutomaticAllocationInfo info{};
		info.access_mode = ((flags & SBCreateFlags::AllowCPUAccess)==SBCreateFlags::None) ? 
			RHI::AutomaticAllocationCPUAccessMode::None :
			RHI::AutomaticAllocationCPUAccessMode::Sequential;
		auto res = RendererBase::GetDevice()->CreateBuffer(bufferDesc, nullptr, nullptr, &info, 0, RHI::ResourceType::Automatic);
		if(res.is_err()) return Error::FromRHIError(res.err());
		ID = std::move(res).value();
		if (data)
		{
			void* writePointer = ID->Map().value();
			memcpy(writePointer, data, size);
			ID->UnMap();
		}
		return {};
		
	}
	void StructuredBuffer::Update(const void* data, std::uint32_t size, std::uint32_t offset)
	{
		void* writePointer = ID->Map().value();
		memcpy(static_cast<char*>(writePointer) + offset, data, size);
		ID->UnMap();
	}
	Result<StructuredBuffer*> StructuredBuffer::Create(const void* data, std::uint32_t size, SBCreateFlags flags)
	{
		PT_PROFILE_FUNCTION();
		std::unique_ptr<StructuredBuffer> result = std::make_unique<StructuredBuffer>();
		auto res = result->CreateStack(data, size);
		if(res.GetErrorType() != ErrorType::Success)
			return ezr::err(std::move(res));
		return ezr::ok(result.release());
	}
	void ConstantBuffer::Update(const void* data, std::uint32_t size, std::uint32_t offset)
	{
		PT_PROFILE_FUNCTION();
		void* writePointer = ID->Map().value();
		memcpy(static_cast<char*>(writePointer) + offset, data, size);
		ID->UnMap();
	}
	Error ConstantBuffer::CreateStack(void* data, std::uint32_t size)
	{
		PT_PROFILE_FUNCTION();
		RHI::BufferDesc bufferDesc{
			.size = size,
			.usage = RHI::BufferUsage::ConstantBuffer
		};
		RHI::AutomaticAllocationInfo info {.access_mode = RHI::AutomaticAllocationCPUAccessMode::Sequential};
		auto res = RendererBase::GetDevice()->CreateBuffer(bufferDesc, nullptr, nullptr, &info, 0, RHI::ResourceType::Automatic);
		if(res.is_err()) return Error::FromRHIError(res.err());
		ID = std::move(res).value();
		if (data)
		{
			void* writePointer = ID->Map().value();
			memcpy(writePointer, data, size);
			ID->UnMap();
		}
		return {};
	}
	Result<ConstantBuffer*> ConstantBuffer::Create(void* data, std::uint32_t size)
	{
		PT_PROFILE_FUNCTION();
		std::unique_ptr<ConstantBuffer> retVal = std::make_unique<ConstantBuffer>();
		auto res  = retVal->CreateStack(data, size);
		if(res.GetErrorType() != ErrorType::Success)
			return ezr::err(std::move(res));
		return Result<ConstantBuffer*>::ok(retVal.release());
	}
}