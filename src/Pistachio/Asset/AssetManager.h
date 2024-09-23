#pragma once
#include "RefCountedObject.h"
#include "Pistachio/Renderer/Texture.h"
#include "Pistachio/Renderer/Model.h"
#include "Pistachio/Renderer/ShaderAsset.h"
#include "Asset.h"
namespace Pistachio
{
	template<typename T>
	concept is_resource = std::is_base_of_v<RefCountedObject, T>;
	class Material;
	class PISTACHIO_API AssetManager
	{
	public:
		[[nodiscard]] Result<Asset> CreateMaterialAsset(const std::string& filename);
		[[nodiscard]] Result<Asset> CreateTexture2DAsset(const std::string& filename);
		[[nodiscard]] Result<Asset> CreateModelAsset(const std::string& filename);
		[[nodiscard]] Result<Asset> CreateShaderAsset(const std::string& filename);
		[[nodiscard]] Result<Asset> CreateSkyboxAsset(const std::string& filename);
		[[nodiscard]] std::optional<Asset> GetAsset(const std::string& resource_name);
		[[nodiscard]] std::string GetAssetFileName(const Asset& asset);
		void ReportLiveObjects();
		template<is_resource Resource> [[nodiscard]] const Resource* GetResource(const Asset& asset) const;
		friend class Asset;
		//intended to only be used by engine developer. it will most likely leak memory otherwise
		[[nodiscard]] std::optional<Asset> FromResource(RefCountedObject* resource, const std::string& str_id, ResourceType type);
		~AssetManager() { ReportLiveObjects(); }
	private:
		friend class Renderer;
		Result<Asset> CreateAsset(const std::string& filename, ResourceType type);
	private:
		std::unordered_map<std::string, UUID> pathUUIDMap;
		std::unordered_map<UUID, RefCountedObject*> assetResourceMap;
	};

	template <is_resource Resource>
	const Resource* AssetManager::GetResource(const Asset& asset) const
	{
		if (const auto it = assetResourceMap.find(asset.m_uuid); it != assetResourceMap.end())
			return dynamic_cast<Resource*>(it->second);
		return nullptr;
	}

	PISTACHIO_API AssetManager* GetAssetManager();
}
