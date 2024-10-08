#pragma once
#include "../Core.h"
#include "Pistachio/Core/Error.h"
#include "../Asset/RefCountedObject.h"
#include "FormatsAndTypes.h"
#include "../Core/TextureView.h"
namespace Pistachio {
	class PISTACHIO_API Texture : public RefCountedObject
	{
	protected:
		Texture() = default;
		friend class Renderer;
		friend class RenderGraph;
		RHI::Ptr<RHI::Texture> m_ID;
	public:
		RHI::Ptr<RHI::Texture> GetID() const
		{
			return m_ID;
		}
		virtual RHI::Format GetFormat() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual uint32_t GetWidth() const = 0;
	};
	enum class TextureFlags
	{
		None = 0,
		Compute = 1,
	};
	ENUM_FLAGS(TextureFlags);
	class PISTACHIO_API Texture2D final : public Texture
	{
	public:
		RHI::Format GetFormat() const override;
		uint32_t GetHeight() const override;
		uint32_t GetWidth() const override;
		Texture2D() : m_Width(0), m_Height(0), m_format(RHI::Format::UNKNOWN){};
		void Bind(int slot = 0) const;
		static Result<Texture2D*> Create(const char* path ,const char* name, RHI::Format format = RHI::Format::R8G8B8A8_UNORM, TextureFlags flags = TextureFlags::None);
		static Result<Texture2D*> Create(uint32_t width, uint32_t height, RHI::Format format,void* data ,const char* name, TextureFlags flags = TextureFlags::None);
		Error CreateStack(const char* path, RHI::Format format ,const char* name, TextureFlags flags = TextureFlags::None);
		Error CreateStack(uint32_t width, uint32_t height, RHI::Format format,void* data ,const char* name, TextureFlags flags = TextureFlags::None);
		RHI::Ptr<RHI::TextureView> GetView()const { return m_view; }
		//TODO: Asset Management
		bool operator==(const Texture2D& texture) const;
		friend class RenderTexture;
		friend class RenderCubeMap;
	private:
		unsigned int m_Width, m_Height, m_MipLevels;
		RHI::Format m_format;
		RHI::Ptr<RHI::TextureView> m_view;
	private:
		Error CreateTexture(void* data, TextureFlags flags);
	};
}

