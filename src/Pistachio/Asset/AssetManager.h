#pragma once
#include "RefCountedObject.h"
#include "Pistachio/Renderer/Texture.h"
#include "Pistachio/Renderer/Model.h"
#include "Pistachio/Renderer/ShaderAsset.h"
#include "Asset.h"
namespace Pistachio
{
	class Material;
	class PISTACHIO_API AssetManager
	{
	public:
		Result<Asset> CreateMaterialAsset(const std::string& filename);
		Result<Asset> CreateTexture2DAsset(const std::string& filename);
		Result<Asset> CreateModelAsset(const std::string& filename);
		Result<Asset> CreateShaderAsset(const std::string& filename);
		std::optional<Asset> GetAsset(const std::string& resource_name);
		std::string GetAssetFileName(const Asset& asset);
		void ReportLiveObjects();
		Material* GetMaterialResource(Asset& a);
		Texture2D* GetTexture2DResource(Asset& a);
		Model* GetModelResource(Asset& a);
		ShaderAsset* GetShaderResource(const Asset& a);
		friend class Asset;
		//intended to only be used by engine developer. it will most likely leak memory otherwise
		Asset FromResource(RefCountedObject* resource, const std::string& str_id, ResourceType type);
	private:
		friend class Renderer;
		Result<Asset> CreateAsset(const std::string& filename, ResourceType type);
	private:
		std::unordered_map<std::string, UUID> pathUUIDMap;
		std::unordered_map<UUID, RefCountedObject*> assetResourceMap;
	};
	PISTACHIO_API AssetManager* GetAssetManager();
}
