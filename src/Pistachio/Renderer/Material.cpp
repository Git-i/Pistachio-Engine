#include "Pistachio/Core/Error.h"
#include "Ptr.h"
#include "ptpch.h"
#include "Material.h"
#include "../Renderer/Renderer.h"
#include "yaml-cpp/yaml.h"
#include "../Asset/AssetManager.h"
namespace YAML {
	template<>
	struct convert<DirectX::XMVECTOR>
	{
		static Node encode(const DirectX::XMVECTOR& v)
		{
			Node node;
			node.push_back(DirectX::XMVectorGetX(v));
			node.push_back(DirectX::XMVectorGetY(v));
			node.push_back(DirectX::XMVectorGetZ(v));
			node.push_back(DirectX::XMVectorGetW(v));
			return node;
		}

		static bool decode(const Node& node, DirectX::XMVECTOR& v)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;
			v = DirectX::XMVectorSet(node[0].as<float>(), node[1].as<float>(), node[2].as<float>(), node[3].as<float>());
			return true;
		}

	};
	template<>
	struct convert<DirectX::XMFLOAT3>
	{
		static Node encode(const DirectX::XMFLOAT3& v)
		{
			Node node;
			node.push_back(v.x);
			node.push_back(v.y);
			node.push_back(v.z);
			return node;
		}

		static bool decode(const Node& node, DirectX::XMFLOAT3& v)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;
			v = { node[0].as<float>(), node[1].as<float>(), node[2].as<float>() };
			return true;
		}

	};
	template<>
	struct convert<DirectX::XMFLOAT4>
	{
		static Node encode(const DirectX::XMFLOAT4& v)
		{
			Node node;
			node.push_back(v.x);
			node.push_back(v.y);
			node.push_back(v.z);
			node.push_back(v.w);
			return node;
		}

		static bool decode(const Node& node, DirectX::XMFLOAT4& v)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;
			v = { node[0].as<float>(), node[1].as<float>(), node[2].as<float>(), node[3].as<float>() };
			return true;
		}

	};

}
namespace Pistachio
{
	YAML::Emitter& operator<<(YAML::Emitter& out, const DirectX::XMFLOAT3& v);
	YAML::Emitter& operator<<(YAML::Emitter& out, const DirectX::XMFLOAT4& v);
	void MaterialSerializer::Serialize(const std::string& filepath, const Material& mat)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Comment("Pistachio Material File");
		out << YAML::Key << "Material" << YAML::Value << "Unnamed Materail";
		out << YAML::Key << "Num Textures" << YAML::Value << (uint32_t)mat.m_textures.size();
		for (uint32_t i = 0; i < mat.m_textures.size();i++)
		{
			out << YAML::Key << i << YAML::Value << GetAssetManager()->GetAssetFileName(mat.m_textures[i]);
		}
		out << YAML::EndMap;

		std::ofstream fout(filepath, std::ios::binary | std::ios::out);
		fout.write(out.c_str(), out.size());
		fout.close();
	}
	
	Result<Material*> Material::Create(const char* filepath)
	{
		Material* mat = new Material;
		std::ifstream stream(filepath);
		std::stringstream strStream;
		strStream << stream.rdbuf();
		YAML::Node data = YAML::Load(strStream.str());
		if (!data["Material"])
		{
			delete mat;
			return ezr::err(Error(ErrorType::InvalidFile, PT_PRETTY_FUNCTION));
		}
		uint32_t numTextures = data["Num Textures"].as<uint32_t>();
		for (uint32_t i = 0; i < numTextures; i++)
		{
			auto asset = GetAssetManager()->CreateTexture2DAsset(data[std::to_string(i)].as<std::string>());
			if(asset.is_err())
			{
				delete mat;
				return asset.transform([](auto a) -> Material* {return nullptr;});
			}
			mat->m_textures.push_back(asset.value());
		}
		std::string shader_name = data["Shader Asset"].as<std::string>();
		auto e = GetAssetManager()->CreateShaderAsset(shader_name)
			.handle([&mat](auto&& asset){
				mat->shader = asset;
				return Error(ErrorType::Success);
			}, [](auto&& error){
				return error;
			});
		if(!e.Successful()) return ezr::err(std::move(e));
		const ShaderAsset* shader = GetAssetManager()->GetResource<ShaderAsset>(mat->shader);
		shader->GetShader().GetShaderBinding(mat->mtlInfo, 3);
		Renderer::AllocateConstantBuffer(shader->GetParamBufferSize());
		
		return ezr::ok(mat);
	}
	void Material::SetShader(Asset _shader)
	{
		shader = _shader;
	}
	void Material::Bind(RHI::Weak<RHI::GraphicsCommandList> list) const
	{
		const ShaderAsset* shader_asset = GetAssetManager()->GetResource<ShaderAsset>(shader);
		const Shader& shd = shader_asset->GetShader();
		shd.Bind(list);
		shd.ApplyBinding(list, mtlInfo);
		list->BindDynamicDescriptor(Renderer::GetCBDescPS(), 4, Renderer::GetCBOffset(parametersBuffer));
	}
}
