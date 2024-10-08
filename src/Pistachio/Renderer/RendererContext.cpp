#include "RendererContext.h"
#include "Buffer.h"
#include "Mesh.h"
#include "FormatsAndTypes.h"
#include "PipelineStateObject.h"
#include "Pistachio/Asset/AssetManager.h"
#include "Pistachio/Core/Application.h"
#include "Pistachio/Core/Log.h"
#include "Pistachio/Renderer/Shader.h"
#include "Pistachio/Renderer/ShaderAsset.h"
#include "Pistachio/Utils/RendererUtils.h"
#include "RootSignature.h"
#include <optional>
static const uint32_t VB_INITIAL_SIZE = 1024;
static const uint32_t IB_INITIAL_SIZE = 1024;
static const uint32_t INITIAL_NUM_OBJECTS = 20;

namespace Pistachio
{
    void MonolithicBufferAllocator::Initialize(uint32_t initialSize)
    {
        capacity = initialSize;
        freeSpace = initialSize;
        freeFastSpace = initialSize;
        freeList = FreeList(initialSize);
    }
	uint32_t MonolithicBufferAllocator::AssignHandle(std::uint32_t offset)
	{
		if (UnusedHandles.empty())
		{
			HandleOffsets.push_back(offset);
			return(uint32_t)( HandleOffsets.size() - 1);
		}
		uint32_t handle = UnusedHandles.back();
		HandleOffsets[handle] = offset;
		UnusedHandles.pop_back();
		return handle;
	}
	RendererVBHandle MonolithicBufferAllocator::Allocate(
		const std::function<void(uint32_t)>& grow_fn, const std::function<void()>& defrag_fn,
		uint32_t size,
		MonolithicBuffer* m_buffer, 
		const void* initialData)
	{
		RHI::Ptr<RHI::Buffer> buffer;
		if(m_buffer) buffer = m_buffer->buffer;
		RendererVBHandle handle;
		//check if we have immediate space available
		if (size < freeFastSpace)
		{
			//allocate to buffer end
			//fast space is always at the end
			PT_CORE_ASSERT(freeList.Allocate(capacity - freeFastSpace, size) == 0);
			if (initialData)
			{
				RendererBase::PushBufferUpdate(buffer, capacity - freeFastSpace, initialData, size);
				RendererBase::FlushStagingBuffer();
			}
			handle.handle = AssignHandle(capacity - freeFastSpace);
			handle.size = size;
			freeFastSpace -= size;
			freeSpace -= size;
			return handle;
		}
		//if not, check if we have space at all
		else if (size < freeSpace)
		{
			//if we have space, check the free list to see if space is continuos
			if (auto space = freeList.Find(size); space != UINT32_MAX)
			{
				//allocate
				PT_CORE_ASSERT(freeList.Allocate(space, size) == 0);
				if (initialData)
				{
					RendererBase::PushBufferUpdate(buffer, space, initialData, size);
					RendererBase::FlushStagingBuffer();
				}
				handle.handle = AssignHandle(space);
				handle.size = size;
				freeSpace -= size;
				return handle;
			}
			PT_CORE_WARN("Defragmenting Buffer");
			defrag_fn();
			if (initialData)
			{
				RendererBase::PushBufferUpdate(buffer, capacity - freeFastSpace, initialData, size);
				RendererBase::FlushStagingBuffer();
			}
			handle.handle = AssignHandle(capacity - freeFastSpace);
			handle.size = size;
			freeFastSpace -= size;
			freeSpace -= size;
			return handle;
			//allocate to buffer end
		}
		else
		{
			PT_CORE_WARN("Growing Buffer");
			grow_fn(size);
			//growth doesnt guarantee that the free space is "Fast" it just guarantees we'll have enough space for the op
			return Allocate(grow_fn,defrag_fn,size, m_buffer,initialData);
			//allocate to buffer end
		}
		return handle;
	}
	void MonolithicBufferAllocator::DeAllocate(RendererVBHandle handle)
	{
		freeSpace += handle.size;
		freeList.DeAllocate(HandleOffsets[handle.handle], handle.size);
		UnusedHandles.push_back(handle.handle);
	}
    void MonolithicBuffer::Initialize(uint32_t initialSize, RHI::BufferUsage usage)
    {

        RHI::BufferDesc desc;
        desc.size = initialSize;
        desc.usage = RHI::BufferUsage::CopyDst | RHI::BufferUsage::CopySrc | usage;
        RHI::AutomaticAllocationInfo info;
        info.access_mode = RHI::AutomaticAllocationCPUAccessMode::None;
        buffer = RendererBase::GetDevice()->CreateBuffer(desc, nullptr, nullptr, &info, 0, RHI::ResourceType::Automatic).value();
        allocator.Initialize(initialSize);
    }
    
    void FrameResource::Initialize(uint32_t cbCapacity)
    {
        transformBuffer.CreateStack(nullptr, cbCapacity);
        transformBufferDesc = RendererBase::GetDevice()->CreateDynamicDescriptor(
				RendererBase::GetMainDescriptorHeap(),
				RHI::DescriptorType::ConstantBufferDynamic,
				RHI::ShaderStage::Vertex,
				transformBuffer.GetID(),
				0,
				256).value();
	    transformBufferDescPS = RendererBase::GetDevice()->CreateDynamicDescriptor(
				RendererBase::GetMainDescriptorHeap(),
				RHI::DescriptorType::ConstantBufferDynamic,
				RHI::ShaderStage::Pixel,
				transformBuffer.GetID(),
				0,
				256).value();
    }


    void RendererContext::Initailize()
    {
		auto& shader_dir = Application::Get().GetShaderDir();
        //Create constant and structured buffers needed for each frame in flight
		PT_CORE_INFO("Initializing 3D Renderer Context");
		//vertex buffer
		PT_CORE_INFO("Creating Vertex Buffer");
		meshVertices.Initialize(VB_INITIAL_SIZE, RHI::BufferUsage::VertexBuffer);
		//index buffer
		PT_CORE_INFO("Creating Index Buffer");
		meshIndices.Initialize(IB_INITIAL_SIZE, RHI::BufferUsage::IndexBuffer);
		//Create Free lists
        uint32_t cbCapacity = RendererUtils::ConstantBufferElementSize(sizeof(TransformData)) * INITIAL_NUM_OBJECTS;
        constantBufferAllocator.Initialize(cbCapacity);
		for (uint32_t i = 0; i < RendererBase::numFramesInFlight; i++)
		{
			PT_CORE_INFO("Initializing Frame Resources {0} of {1}", i + 1, RendererBase::numFramesInFlight);
			PT_CORE_INFO("Creating Constant Buffer and Descriptors");
			resources[i].Initialize(cbCapacity);
		}
		
		PT_CORE_INFO("Creating Samplers");
		{
			RHI::SamplerDesc sampler;
			sampler.AddressU = RHI::AddressMode::Clamp;
			sampler.AddressV = RHI::AddressMode::Clamp;
			sampler.AddressW = RHI::AddressMode::Clamp;
			sampler.anisotropyEnable = false;
			sampler.compareEnable = false;
			sampler.magFilter = RHI::Filter::Linear;
			sampler.minFilter = RHI::Filter::Linear;
			sampler.mipFilter = RHI::Filter::Linear;
			sampler.maxLOD = std::numeric_limits<float>::max();
			sampler.minLOD = 0;
			sampler.mipLODBias = 0;
			defaultSampler = RendererBase::CreateSampler(sampler);
			brdfSampler = RendererBase::CreateSampler(sampler);
			sampler.compareEnable = true;
			sampler.compareFunc = RHI::ComparisonFunc::LessEqual;
			shadowSampler = RendererBase::CreateSampler(sampler);
		}

		

		RHI::BlendMode blendMode{};

		RHI::DepthStencilMode dsMode{};

		RHI::RasterizerMode rsMode{};


		PT_CORE_INFO("Creating Compute Shaders...");
		PT_CORE_INFO("Build Clusters");
		computeShaders["Build Clusters"] = std::unique_ptr<ComputeShader>{ComputeShader::Create(   {shader_dir + "CFBuildClusters_cs.rbc"} , RHI::ShaderMode::File)};
		PT_CORE_INFO("Filter Clusters");
		computeShaders["Filter Clusters"] = std::unique_ptr<ComputeShader>{ComputeShader::Create({shader_dir + "CFActiveClusters_cs.rbc"}, RHI::ShaderMode::File)};
		PT_CORE_INFO("Tighten Clusters");
		computeShaders["Tighten Clusters"] = std::unique_ptr<ComputeShader>{ComputeShader::Create({ shader_dir + "CFTightenList_cs.rbc"}, RHI::ShaderMode::File)};
		PT_CORE_INFO("Cull Lights");
		computeShaders["Cull Lights"] = std::unique_ptr<ComputeShader>{ComputeShader::Create({ shader_dir+ "CFCullLights_cs.rbc"}, RHI::ShaderMode::File)};
		
		

		Helpers::FillDepthStencilMode(dsMode, true, RHI::DepthWriteMask::None);
		Helpers::BlendModeDisabledBlend(blendMode);
		Helpers::FillRaseterizerMode(rsMode);
		ShaderCreateDesc ShaderDesc{};
		std::string vs = shader_dir + "VertexShader.rbc";
		std::string ps = shader_dir + "CFPBRShader_ps.rbc";
		Pistachio::Helpers::ZeroAndFillShaderDesc(ShaderDesc, vs,ps, 1, 1, &dsMode,1, &blendMode,1,&rsMode);
		ShaderDesc.RTVFormats[0] = RHI::Format::R16G16B16A16_FLOAT;
		ShaderDesc.DSVFormat = RHI::Format::D32_FLOAT;
		ShaderDesc.InputDescription = Mesh::GetLayout();
		ShaderDesc.numInputs = Mesh::GetLayoutSize();
		PT_CORE_INFO("Creating Default Forward Shader");
		auto fwdShader = new ShaderAsset();
		fwdShader->GetShader().CreateStack(ShaderDesc, {{0,4}}, std::nullopt);
		fwdShader->paramBufferSize = 12;
		fwdShader->parametersMap["Diffuse"] = ParamInfo{ 0,ParamType::Float };
		fwdShader->parametersMap["Metallic"] = ParamInfo{ 4,ParamType::Float };
		fwdShader->parametersMap["Roughness"] = ParamInfo{ 8,ParamType::Float };
		fwdShader->bindingsMap["Diffuse Texture"] = 0;
		fwdShader->bindingsMap["Metallic Texture"] = 1;
		fwdShader->bindingsMap["Roughness Texture"] = 2;
		fwdShader->bindingsMap["Normal Texture"] = 3;
		defaultShader = GetAssetManager()->FromResource(fwdShader, "Default Shader", Pistachio::ResourceType::Shader).value();

		//Z-Prepass
		Helpers::FillDepthStencilMode(dsMode);
		Helpers::BlendModeDisabledBlend(blendMode);
		Helpers::FillRaseterizerMode(rsMode);
		vs = shader_dir + "StandaloneVertexShader.rbc";
		Helpers::ZeroAndFillShaderDesc(ShaderDesc,
			vs,std::string_view(), 0, 1, &dsMode, 1, &blendMode, 1, &rsMode);
		ShaderDesc.DSVFormat = RHI::Format::D32_FLOAT;
		ShaderDesc.InputDescription = Mesh::GetLayout();
		ShaderDesc.numInputs = Mesh::GetLayoutSize();
		PT_CORE_INFO("Creating Z-Prepass Shader");
		shaders["Z-Prepass"] = std::unique_ptr<Shader>{Shader::Create(ShaderDesc, {{0u}}, std::nullopt)};

		//shadow shaders
		vs = shader_dir + "Shadow_vs.rbc";
		ShaderDesc.VS = RHI::ShaderCode{ vs };
		ShaderDesc.RasterizerModes->cullMode = RHI::CullMode::Front;
		PT_CORE_INFO("Creating Shadow Shader");
		shaders["Shadow Shader"] = std::unique_ptr<Shader>{Shader::Create(ShaderDesc, {{0u}}, 1)};


		BrdfTex.CreateStack(512, 512, RHI::Format::R16G16_FLOAT, nullptr PT_DEBUG_REGION(,"Renderer -> White Texture"),TextureFlags::Compute);
		auto brdfShader = std::unique_ptr<ComputeShader>(ComputeShader::Create({shader_dir + "BRDF_LUT_cs.rbc" },RHI::ShaderMode::File));

		ShaderDesc.DepthStencilModes->DepthWriteMask = RHI::DepthWriteMask::None;
		ShaderDesc.RasterizerModes->cullMode = RHI::CullMode::None;
		vs = shader_dir + "background_vs.rbc";
		ps = shader_dir + "background_ps.rbc";
		ShaderDesc.VS = { vs};
		ShaderDesc.PS = { ps};
		ShaderDesc.NumRenderTargets = 1;
		ShaderDesc.RTVFormats[0] = RHI::Format::R16G16B16A16_FLOAT;
		PT_CORE_INFO("Creating Background Shader");
		shaders["Background Shader"] = std::unique_ptr<Shader>{Shader::Create(ShaderDesc, {})};

		ResourceSet brdfTexInfo;
		brdfShader->GetShaderBinding(brdfTexInfo, 0);
		brdfTexInfo.UpdateTextureBinding(BrdfTex.GetView(), 0, RHI::DescriptorType::CSTexture);
		RHI::TextureMemoryBarrier barr;
		barr.AccessFlagsAfter = RHI::ResourceAcessFlags::SHADER_WRITE;barr.AccessFlagsBefore = RHI::ResourceAcessFlags::NONE;
		barr.newLayout = RHI::ResourceLayout::GENERAL;
		barr.oldLayout = RHI::ResourceLayout::UNDEFINED;
		barr.previousQueue = barr.nextQueue = RHI::QueueFamily::Ignored;
		barr.texture = BrdfTex.GetID();
		RHI::SubResourceRange range;
		range.FirstArraySlice = 0;
		range.NumArraySlices = 1;
		range.IndexOrFirstMipLevel = 0;
		range.NumMipLevels = 1;
		range.imageAspect = RHI::Aspect::COLOR_BIT;
		barr.subresourceRange = range;
		RendererBase::GetMainCommandList()->PipelineBarrier(RHI::PipelineStage::TOP_OF_PIPE_BIT, RHI::PipelineStage::COMPUTE_SHADER_BIT, {}, {&barr,1});
		brdfShader->Bind(RendererBase::GetMainCommandList());
		brdfShader->ApplyShaderBinding(RendererBase::GetMainCommandList(), brdfTexInfo);
		PT_CORE_INFO("Generating BRDF Look up Texture");
		RendererBase::GetMainCommandList()->Dispatch(512,512,1);
		barr.oldLayout = RHI::ResourceLayout::GENERAL;
		barr.newLayout = RHI::ResourceLayout::SHADER_READ_ONLY_OPTIMAL;
		barr.AccessFlagsBefore = RHI::ResourceAcessFlags::SHADER_WRITE;
		barr.AccessFlagsAfter = RHI::ResourceAcessFlags::SHADER_READ;
		RendererBase::GetMainCommandList()->PipelineBarrier(RHI::PipelineStage::COMPUTE_SHADER_BIT, RHI::PipelineStage::FRAGMENT_SHADER_BIT, {}, {&barr,1});
    	RendererBase::EndFrame();
    	RendererBase::Get().mainFence->Wait(RendererBase::Get().currentFenceVal);

		auto err = defaultCubemap.Initialize(1, 1, RHI::Format::R8G8B8A8_UNORM);
		if(!err.Successful())
		{
			PT_CORE_ERROR("Could not create default cubemap: ");
			Error::LogErrorToConsole(err);
			PT_DEBUG_BREAK;
		}
    }
}