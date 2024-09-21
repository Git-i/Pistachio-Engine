#pragma once
#include "Pistachio/Core/UUID.h"
namespace Pistachio
{
	enum class PISTACHIO_API ResourceType
	{
		Invalid,
		Texture,
		Skybox,
		Model,
		Material,
		Animation,
		Audio,
		Shader,
		Unknown // This means valid
	};
	class PISTACHIO_API Asset
	{
	public:
		friend class AssetManager;
		~Asset();
		Asset() = default;
		Asset(const Asset&);
		void operator=(const Asset& other);
		void operator=(Asset&& other) noexcept;
		bool operator==(const Asset& other) const { return m_uuid == other.m_uuid; };
		int ViewRefCount();
		[[nodiscard]] UUID GetUUID() const { return m_uuid; };
		[[nodiscard]] ResourceType GetType() const { return m_type; }
	private:
		Asset(UUID, ResourceType);
	private:
		ResourceType m_type = ResourceType::Invalid;
		UUID m_uuid = (UUID)0;
	};
}
