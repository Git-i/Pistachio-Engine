#include "Pistachio/Asset/Asset.h"
#include "Pistachio/Asset/RefCountedObject.h"
#include "Pistachio/Core/Error.h"
#include "Pistachio/Renderer/Texture.h"
#include "ptpch.h"
#include "Pistachio/Renderer/Material.h"
#include "Pistachio/Renderer/Skybox.h"
#include <cstdint>
#include <optional>
#include <type_traits>
#include "AssetManager.h"

#include <Pistachio/Core/Application.h>

namespace Pistachio
{
	AssetManager* GetAssetManager()
	{
		return &Application::Get().GetAssetManager();
	}
	Asset::Asset(const Asset& other)
	{
		auto assetMan = GetAssetManager();
		m_type = other.m_type;
		m_uuid = other.m_uuid;
		if (static_cast<uint64_t>(m_uuid) != 0)
			assetMan->assetResourceMap[m_uuid]->hold();
	}
	void Asset::operator=(const Asset& other)
	{
		auto assetMan = GetAssetManager();
		if (m_uuid) {
			auto res = assetMan->assetResourceMap[m_uuid];
			if (res->release() == 0)
			{
				delete res;
				assetMan->assetResourceMap.erase(m_uuid);
				auto it = std::find_if(assetMan->pathUUIDMap.begin(), assetMan->pathUUIDMap.end(), [this](auto&& p) { return p.second == m_uuid; });
				assetMan->pathUUIDMap.erase(it->first);
			}
		}
		m_type = other.m_type;
		m_uuid = other.m_uuid;
		if (static_cast<uint64_t>(m_uuid) != 0)
			assetMan->assetResourceMap[m_uuid]->hold();
	}
	void Asset::operator=(Asset&& other) noexcept
	{
		auto assetMan = GetAssetManager();
		if (m_uuid) {
			auto res = assetMan->assetResourceMap[m_uuid];
			if (res->release() == 0)
			{
				delete res;
				assetMan->assetResourceMap.erase(m_uuid);
				auto it = std::find_if(assetMan->pathUUIDMap.begin(), assetMan->pathUUIDMap.end(), [this](auto&& p) { return p.second == m_uuid; });
				assetMan->pathUUIDMap.erase(it->first);
			}
		}
		m_type = other.m_type;
		m_uuid = other.m_uuid;
		if (static_cast<uint64_t>(m_uuid) != 0)
			assetMan->assetResourceMap[m_uuid]->hold();
		other.~Asset();
	}
	Asset::Asset(UUID uuid, ResourceType type)
	{
		GetAssetManager()->assetResourceMap[uuid]->hold();
		m_uuid = uuid;
		m_type = type;
	}
	int Asset::ViewRefCount()
	{
		return GetAssetManager()->assetResourceMap[m_uuid]->count();
	}
	Asset::~Asset()
	{
		if (m_uuid == UUID(0)) return;
		auto assetMan = GetAssetManager();
		auto res = assetMan->assetResourceMap[m_uuid];
		if (m_uuid) {
			if (res->release() == 0)
			{
				delete res;
				assetMan->assetResourceMap.erase(m_uuid);
				const auto it = std::find_if(assetMan->pathUUIDMap.begin(), assetMan->pathUUIDMap.end(), [this](auto&& p) { return p.second == m_uuid; });
				assetMan->pathUUIDMap.erase(it->first);
			}
		}
		m_uuid = 0;
		m_type = ResourceType::Invalid;
	}
	inline int Pistachio::RefCountedObject::hold() const
	{
		m_count_++;
		return m_count_;
	}
	inline int RefCountedObject::release() const
	{
		assert(m_count_ > 0);
		m_count_--;
		return m_count_;
	}
	inline int RefCountedObject::count()
	{
		return m_count_;
	}
	Result<Asset> AssetManager::CreateMaterialAsset(const std::string& filename)
	{
		return CreateAsset(filename, ResourceType::Material);
	}
	Result<Asset> AssetManager::CreateTexture2DAsset(const std::string& filename)
	{
		return CreateAsset(filename, ResourceType::Texture);
	}
	Result<Asset> AssetManager::CreateSkyboxAsset(const std::string& filename)
	{
		return CreateAsset(filename, ResourceType::Skybox);
	}
	Result<Asset> AssetManager::CreateModelAsset(const std::string& filename)
	{
		return CreateAsset(filename, ResourceType::Model);
	}
	Result<Asset> AssetManager::CreateShaderAsset(const std::string& filename)
	{
		return CreateAsset(filename, ResourceType::Shader);
	}
	std::string AssetManager::GetAssetFileName(const Asset& asset)
	{
		auto it = std::find_if(pathUUIDMap.begin(), pathUUIDMap.end(), [asset](auto&& p) { return p.second == asset.m_uuid; });
		if (it == pathUUIDMap.end())
			return "None";
		return it->first;
	}
	void AssetManager::ReportLiveObjects()
	{
		for (auto [uuid, res] : assetResourceMap)
		{
			PT_CORE_INFO("Live object with id: {0} at memory location {1}", (uint64_t)uuid, (void*)res);
		}
	}



	std::optional<Asset> AssetManager::GetAsset(const std::string& filename)
	{
		if(pathUUIDMap.contains(filename)) return Asset(pathUUIDMap[filename], ResourceType::Unknown);
		return std::nullopt;
	}
	Result<Asset> AssetManager::CreateAsset(const std::string& filename, ResourceType type)
	{
		if (auto it = pathUUIDMap.find(filename); it != pathUUIDMap.end())
		{
			return ezr::ok(Asset(it->second, type));
		}
		else
		{
			UUID uuid = UUID();
			Result<RefCountedObject*> obj;
			auto result_to_ref_obj = [](auto&& result) -> RefCountedObject* { return static_cast<RefCountedObject*>(result); };
			if (type == ResourceType::Texture) obj = Texture2D::Create(filename.c_str(), filename.c_str()).transform(result_to_ref_obj);
			else if (type == ResourceType::Material) obj = Material::Create(filename.c_str()).transform(result_to_ref_obj);
			else if (type == ResourceType::Shader) obj = ShaderAsset::Create(filename.c_str()).transform(result_to_ref_obj);
			else if (type == ResourceType::Model) obj = Model::Create(filename.c_str()).transform(result_to_ref_obj);
			else if (type == ResourceType::Skybox) obj = Skybox::Create(filename).transform(result_to_ref_obj);
			else obj = ezr::err(Error(ErrorType::InvalidResourceType, PT_PRETTY_FUNCTION));

			if(!obj) return ezr::err(std::move(obj).err());
			assetResourceMap[uuid] = obj.value();
			pathUUIDMap[filename] = uuid;
			assetResourceMap.at(uuid)->release();
			return ezr::ok(Asset(uuid, type));
		}
	}
	std::optional<Asset> AssetManager::FromResource(RefCountedObject* resource,const std::string& in, ResourceType type)
	{
		if (pathUUIDMap.contains(in))
		{
			return std::nullopt;
		}
		else
		{
			UUID uuid = UUID();
			assetResourceMap[uuid] = resource;
			pathUUIDMap[in] = uuid;
			assetResourceMap.at(uuid)->release();
			return Asset(uuid, type);
		}
	}
}
