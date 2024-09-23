#pragma once
#include "CommandList.h"
#include "Core/Buffer.h"
#include "Pistachio/Core/Error.h"
namespace Pistachio {
	class PISTACHIO_API VertexBuffer
	{
	public:
		void Bind(RHI::GraphicsCommandList* list, uint32_t slot = 0) const;

		VertexBuffer() = default;
		void SetData(const void* data, unsigned int size);
		[[nodiscard]] static Result<VertexBuffer*> Create(const void* vertices, unsigned int size, unsigned int stride);
		[[nodiscard]] Error CreateStack(const void* vertices, unsigned int size, unsigned int stride);
		
		[[nodiscard]] uint32_t GetStride() const {return stride;}
		[[nodiscard]] RHI::Ptr<RHI::Buffer> GetID() const {return ID;}
	private:
		friend class Renderer;
		uint32_t stride = 0;
		RHI::Ptr<RHI::Buffer> ID;
	};
	class PISTACHIO_API IndexBuffer
	{
	public:
		void Bind(RHI::GraphicsCommandList* list) const;

		IndexBuffer() = default;
		[[nodiscard]] static Result<IndexBuffer*> Create(const void* indices, unsigned int size, unsigned int stride);
		[[nodiscard]] Error CreateStack(const void* indices, unsigned int size, unsigned int stride);
		
		[[nodiscard]] uint32_t GetCount() const{ return count; }
		[[nodiscard]] RHI::Ptr<RHI::Buffer> GetID() const {return ID;}
	private:
		friend class Renderer;
		uint32_t count = 0;
		RHI::Ptr<RHI::Buffer> ID;
	};
	enum class SBCreateFlags
	{
		None = 0,
		AllowCPUAccess = 1,
	};
	ENUM_FLAGS(SBCreateFlags);
	struct PISTACHIO_API StructuredBuffer
	{
		void Bind(std::uint32_t slot) const;
		void Update(const void* data, std::uint32_t size, std::uint32_t offset);
		[[nodiscard]] static Result<StructuredBuffer*> Create(const void* data, std::uint32_t size, SBCreateFlags flags = SBCreateFlags::None);
		
		[[nodiscard]] Error CreateStack(const void* data, std::uint32_t size, SBCreateFlags flags = SBCreateFlags::None);
		[[nodiscard]] RHI::Ptr<RHI::Buffer> GetID() const { return ID; }
	private:
		friend class Renderer;
		RHI::Ptr<RHI::Buffer> ID;
	};
	class PISTACHIO_API ConstantBuffer {
	public:
		void Update(const void* data, std::uint32_t size, std::uint32_t offset);
		[[nodiscard]] Error CreateStack(void* data, std::uint32_t size);
		[[nodiscard]] static Result<ConstantBuffer*> Create(void* data, std::uint32_t size);
		
		[[nodiscard]] RHI::Ptr<RHI::Buffer> GetID() const { return ID; }
	private:
		friend class Renderer;
		RHI::Ptr<RHI::Buffer> ID;
	};
}
