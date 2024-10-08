#include "CommandList.h"
#include "DirectXMath.h"
#include "FormatsAndTypes.h"
#include "Pistachio/Debug/Instrumentor.h"
#include "Pistachio/Renderer/BufferHandles.h"
#include "Pistachio/Renderer/Mesh.h"
#include "Pistachio/Renderer/RenderGraph.h"
#include "Pistachio/Renderer/Renderer.h"
#include "Pistachio/Renderer/RendererBase.h"
#include "Pistachio/Renderer/RendererContext.h"
#include "ptpch.h"
#include "Pistachio/Core/Math.h"
#include <algorithm>
#include <cstdint>
#include "Scene.h"
#include "Components.h"
#include "Pistachio/Renderer/Renderer2D.h"

#include "Entity.h"
#ifdef IMGUI
#include "imgui.h"
#include "ImGuizmo.h"

#endif // IMGUI

#include <Pistachio/Renderer/Skybox.h>

#include "ScriptableComponent.h"
#include "Pistachio/Physics/Physics.h"
#include "Pistachio/Renderer/MeshFactory.h"
#include "CullingManager.h"
#include "../Renderer/Material.h"

static void getFrustumCornersWorldSpace(const DirectX::XMMATRIX& proj, const DirectX::XMMATRIX& view, DirectX::XMVECTOR* corners)
{
	PT_PROFILE_FUNCTION();
	const auto inv = DirectX::XMMatrixInverse(nullptr, view * proj);
	int index = 0;
	for (unsigned int x = 0; x < 2; ++x)
	{
		for (unsigned int y = 0; y < 2; ++y)
		{
			for (unsigned int z = 0; z < 2; ++z)
			{
				auto pt = DirectX::XMVector4Transform(DirectX::XMVectorSet(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f), inv);
				corners[index] = DirectX::XMVectorScale(pt, 1.f/DirectX::XMVectorGetW(pt));
				index++;
			}
		}
	}
}
static DirectX::XMMATRIX GetLightMatrixFromCamera(const DirectX::XMMATRIX& camView, const DirectX::XMMATRIX& camProj, const Pistachio::Light& light, float zMult)
{
	PT_PROFILE_FUNCTION();
	DirectX::XMVECTOR corners[8];
	getFrustumCornersWorldSpace(camProj, camView, corners);
	DirectX::XMVECTOR center = DirectX::XMVectorZero();
	for (const auto& v : corners)
	{
		center = DirectX::XMVectorAdd(v, center);
	}
	center = DirectX::XMVectorScale(center, 1.f / 8.f);

	const auto lightView = DirectX::XMMatrixLookAtLH(DirectX::XMVectorAdd(center, DirectX::XMLoadFloat4(&light.rotation)),
		center,
		DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1.f));
	float minX = std::numeric_limits<float>::max();
	float maxX = std::numeric_limits<float>::lowest();
	float minY = std::numeric_limits<float>::max();
	float maxY = std::numeric_limits<float>::lowest();
	float minZ = std::numeric_limits<float>::max();
	float maxZ = std::numeric_limits<float>::lowest();
	for (const auto& v : corners)
	{
		DirectX::XMVECTOR trf_xmv = DirectX::XMVector4Transform(v, lightView);
		DirectX::XMFLOAT3 trf;
		DirectX::XMStoreFloat3(&trf, trf_xmv);
		minX = std::min(minX, trf.x);
		maxX = std::max(maxX, trf.x);
		minY = std::min(minY, trf.y);
		maxY = std::max(maxY, trf.y);
		minZ = std::min(minZ, trf.z);
		maxZ = std::max(maxZ, trf.z);
	}
	if (minZ < 0)
	{
		minZ *= zMult;
	}
	else
	{
		minZ /= zMult;
	}
	if (maxZ < 0)
	{
		maxZ /= zMult;
	}
	else
	{
		maxZ *= zMult;
	}

	const DirectX::XMMATRIX lightProjection = DirectX::XMMatrixOrthographicOffCenterLH(minX, maxX, minY, maxY, minZ, maxZ);
	return DirectX::XMMatrixMultiplyTranspose(lightView, lightProjection);
}
static const uint32_t clusterAABBsize = ((sizeof(float) * 4) * 2);
namespace Pistachio {
	void OnMeshRendererAdded(entt::registry & reg, entt::entity e)
	{
		MeshRendererComponent& component = reg.get<MeshRendererComponent>(e);
		TransformData td;
		td.transform = Matrix4::Identity;
		td.normal = Matrix4::Identity;
		component.handle =  Renderer::AllocateConstantBuffer(sizeof(TransformData));
		Renderer::FullCBUpdate(component.handle, &td);
	}
	Scene::Scene(SceneDesc desc) : sm_allocator({ 4096, 4096 }, { 256, 256 })
	{
		MeshFactory::CreateCube();
		PT_PROFILE_FUNCTION();
		using namespace DirectX;
		AssetManager* assetMan = GetAssetManager();
		root = CreateRootEntity(UUID());

		m_Registry.on_construct<MeshRendererComponent>().connect<&OnMeshRendererAdded>();
		RHI::UVector2D resolution = { (uint32_t)desc.Resolution.x, (uint32_t)desc.Resolution.y };
		sceneResolution[0] = resolution.x;
		sceneResolution[1] = resolution.y;
		clustersDim[0] = desc.clusterX;
		clustersDim[1] = desc.clusterY;
		clustersDim[2] = desc.clusterZ;

		uint32_t numClusters = desc.clusterX * desc.clusterY * desc.clusterZ;
		uint32_t clusterBufferSize = clusterAABBsize * numClusters;

		clusterAABB.CreateStack(nullptr, clusterBufferSize);
		sparseActiveClustersBuffer_lightIndices.CreateStack(nullptr, sizeof(uint32_t) * numClusters * 50);//assuming a cluster can have 50 lights
		activeClustersBuffer.CreateStack(nullptr, numClusters * sizeof(uint32_t));
		lightListSize = ((sizeof(float) * 4) * 100);
		lightList.CreateStack(nullptr, lightListSize, SBCreateFlags::AllowCPUAccess);
		lightGrid.CreateStack(nullptr, numClusters * sizeof(uint32_t) * 4);
		zPrepass.CreateStack(resolution.x, resolution.y, 1, RHI::Format::D32_FLOAT PT_DEBUG_REGION(, "Scene -> ZPrepass"));
		finalRender.CreateStack(resolution.x, resolution.y, 1, RHI::Format::R16G16B16A16_FLOAT PT_DEBUG_REGION(, "Scene -> Final Render"));
		shadowMarker.CreateStack(nullptr, sizeof(uint32_t));
		shadowMapAtlas.CreateStack(4096, 4096,1, RHI::Format::D32_FLOAT PT_DEBUG_REGION(, "Scene -> Shadow Map"));
		computeShaderMiscBuffer.CreateStack(nullptr, sizeof(uint32_t) * 2, SBCreateFlags::None);
		
		ComputeShader* shd_buildClusters = Renderer::GetBuiltinComputeShader("Build Clusters");
		ComputeShader* shd_activeClusters = Renderer::GetBuiltinComputeShader("Filter Clusters");
		ComputeShader* shd_tightenList = Renderer::GetBuiltinComputeShader("Tighten Clusters");
		ComputeShader* shd_cullLights = Renderer::GetBuiltinComputeShader("Cull Lights");
		Shader* shd_prepass = Renderer::GetBuiltinShader("Z-Prepass");
		const Shader* shd_fwd = &assetMan->GetResource<ShaderAsset>(*assetMan->GetAsset("Default Shader"))->GetShader();
		Shader* shd_Shadow = Renderer::GetBuiltinShader("Shadow Shader");
		Shader* shd_background = Renderer::GetBuiltinShader("Background Shader");
		for (uint32_t i = 0; i < RendererBase::numFramesInFlight; i++)
		{
			passCB[i].CreateStack(nullptr, sizeof(PassConstants));
			shd_buildClusters->GetShaderBinding(passCBinfoCMP[i], 1);
			shd_prepass->GetShaderBinding(passCBinfoGFX[i], 1);
			shd_fwd->GetShaderBinding(passCBinfoVS_PS[i], 1);

			BufferBindingUpdateDesc bbud;
			bbud.buffer = passCB[i].GetID();
			bbud.size = sizeof(PassConstants);
			bbud.type = RHI::DescriptorType::ConstantBuffer;
			bbud.offset = 0;
			passCBinfoCMP[i].UpdateBufferBinding(bbud, 0);
			passCBinfoGFX[i].UpdateBufferBinding(bbud, 0);
			passCBinfoVS_PS[i].UpdateBufferBinding(bbud, 0);
			
		}
		shd_fwd->GetShaderBinding(sceneInfo, 2);
		sceneInfo.UpdateTextureBinding(Renderer::GetBrdfTexture().GetView(), 0);
		sceneInfo.UpdateTextureBinding(Renderer::GetDefaultCubeMap().GetView(), 1);
		sceneInfo.UpdateTextureBinding(Renderer::GetDefaultCubeMap().GetView(), 2);
		sceneInfo.UpdateTextureBinding(shadowMapAtlas.GetView(), 3);

		sceneInfo.UpdateBufferBinding(lightGrid.GetID(), 0, numClusters * sizeof(uint32_t) * 4, RHI::DescriptorType::StructuredBuffer, 4);
		sceneInfo.UpdateBufferBinding(lightList.GetID(), 0, lightListSize, RHI::DescriptorType::StructuredBuffer, 5);
		sceneInfo.UpdateBufferBinding(sparseActiveClustersBuffer_lightIndices.GetID(), 0, sizeof(uint32_t) * numClusters * 50, RHI::DescriptorType::StructuredBuffer, 6);

		sceneInfo.UpdateSamplerBinding(Renderer::GetDefaultSampler(), 7);
		sceneInfo.UpdateSamplerBinding(Renderer::GetDefaultSampler(), 8);
		sceneInfo.UpdateSamplerBinding(Renderer::GetShadowSampler(), 9);

		shd_background->GetShaderBinding(backgroundInfo, 1);
		backgroundInfo.UpdateTextureBinding(Renderer::GetDefaultCubeMap().GetView(), 0);
		backgroundInfo.UpdateSamplerBinding(Renderer::GetDefaultSampler(), 1);

		shd_Shadow->GetShaderBinding(shadowSetInfo, 1);
		shadowSetInfo.UpdateBufferBinding(lightList.GetID(), 0, lightListSize, RHI::DescriptorType::StructuredBuffer, 0);

		shd_buildClusters->GetShaderBinding(buildClusterInfo, 0);
		buildClusterInfo.UpdateBufferBinding(clusterAABB.GetID(), 0, clusterBufferSize, RHI::DescriptorType::CSBuffer, 0);
		shd_activeClusters->GetShaderBinding(activeClusterInfo, 0);
		activeClusterInfo.UpdateTextureBinding(zPrepass.GetView(), 0);
		activeClusterInfo.UpdateBufferBinding(sparseActiveClustersBuffer_lightIndices.GetID(), 0, sizeof(uint32_t) * numClusters, RHI::DescriptorType::CSBuffer, 1);
		shd_tightenList->GetShaderBinding(tightenListInfo, 0);
		tightenListInfo.UpdateBufferBinding(sparseActiveClustersBuffer_lightIndices.GetID(), 0, sizeof(uint32_t) * numClusters, RHI::DescriptorType::StructuredBuffer, 0);
		tightenListInfo.UpdateBufferBinding(computeShaderMiscBuffer.GetID(), 0, sizeof(uint32_t), RHI::DescriptorType::CSBuffer, 1);
		tightenListInfo.UpdateBufferBinding(activeClustersBuffer.GetID(), 0, sizeof(uint32_t) * numClusters, RHI::DescriptorType::CSBuffer, 2);
		shd_cullLights->GetShaderBinding(cullLightsInfo, 0);
		cullLightsInfo.UpdateBufferBinding(clusterAABB.GetID(), 0, clusterAABBsize, RHI::DescriptorType::StructuredBuffer, 0);
		cullLightsInfo.UpdateBufferBinding(activeClustersBuffer.GetID(), 0, sizeof(uint32_t) * numClusters, RHI::DescriptorType::StructuredBuffer, 1);
		cullLightsInfo.UpdateBufferBinding(lightList.GetID(), 0, lightListSize, RHI::DescriptorType::StructuredBuffer, 2);
		cullLightsInfo.UpdateBufferBinding(computeShaderMiscBuffer.GetID(), 0, sizeof(uint32_t)*2, RHI::DescriptorType::CSBuffer, 3);
		cullLightsInfo.UpdateBufferBinding(sparseActiveClustersBuffer_lightIndices.GetID(), 0, sizeof(uint32_t) * numClusters * 50, RHI::DescriptorType::CSBuffer, 4);
		cullLightsInfo.UpdateBufferBinding(lightGrid.GetID(), 0, numClusters * sizeof(uint32_t) * 4, RHI::DescriptorType::CSBuffer, 5);

		RGTextureHandle depthTex = graph.CreateTexture(&zPrepass);
		finalRenderTex = graph.CreateTexture(&finalRender);
		RGTextureHandle shadowMap = graph.CreateTexture(&shadowMapAtlas);
		RGBufferHandle clustersBuffer = graph.CreateBuffer(clusterAABB.GetID(), 0, clusterBufferSize);
		RGBufferHandle sparseActiveClusterBuffer = graph.CreateBuffer(sparseActiveClustersBuffer_lightIndices.GetID(), 0, sizeof(uint32_t) * numClusters * 50);
		RGBufferHandle ActiveClusterBuffer = graph.CreateBuffer(activeClustersBuffer.GetID(), 0, numClusters * sizeof(uint32_t));
		RGBufferInstance LightIndices = graph.MakeUniqueInstance(sparseActiveClusterBuffer);//we alias the sparse buffer
		RGBufferHandle LightList = graph.CreateBuffer(lightList.GetID(), 0, lightListSize);//light list is transient as it switches queue families
		RGBufferHandle LightGrid = graph.CreateBuffer(lightGrid.GetID(), 0, numClusters * 4 * sizeof(uint32_t));
		RGTextureInstance finalRenderWithBackground = graph.MakeUniqueInstance(finalRenderTex);

		AttachmentInfo a_info;
		BufferAttachmentInfo b_info;
		Matrix4 view = Matrix4::CreateLookAt({ 0,3,-10 }, { 0,0,0 }, { 0,1,0 });;
		Matrix4 proj = Matrix4::CreatePerspectiveFieldOfView(Math::ToRadians(45.f), 16.f / 9.f, 0.1, 50.f);
		PassConstants pc;
		XMStoreFloat4x4(&pc.View, XMMatrixTranspose(view));
		XMStoreFloat4x4(&pc.Proj, XMMatrixTranspose(proj));
		XMStoreFloat4x4(&pc.ViewProj, XMMatrixMultiplyTranspose(view, proj));
		XMStoreFloat4x4(&pc.InvProj, XMMatrixTranspose(XMMatrixInverse(nullptr, proj)));
		XMStoreFloat4x4(&pc.InvView, XMMatrixTranspose(XMMatrixInverse(nullptr, view)));
		XMStoreFloat4x4(&pc.InvViewProj, XMMatrixTranspose(XMMatrixInverse(nullptr, view * proj)));
		pc.numClusters = { 16,9,24 };
		pc.RenderTargetSize = { 1920,1080 };
		pc.NearZ = 0.1f;
		pc.FarZ = 50.f;
		pc.InvRenderTargetSize = { 1.f / 1920.f, 1.f / 1080.f };
		pc.bias = (24.f * log10f(0.1f)) / (log10f(50.f / 0.1f));
		pc.scale = 24.f / (log10f(50.f / 0.1f));
		pc.numClusters = { 16,9,24 };
		pc.numDirectionalRegularLights = 0;
		pc.numDirectionalShadowLights = 0;
		pc.numRegularLights = 0;
		pc.numShadowLights = 0;
		pc.EyePosW = { 0,3,-10 };
		for (uint32_t i = 0; i < 3; i++)
		{
			passCB[i].Update(&pc, sizeof(PassConstants), 0);
		}

		RenderPass& zprepass = graph.AddPass(RHI::PipelineStage::LATE_FRAGMENT_TESTS_BIT, "Z-Prepass");
		{
			a_info.format = RHI::Format::D32_FLOAT;//?
			a_info.access = AttachmentAccess::Write;
			a_info.usage = AttachmentUsage::Graphics;
			a_info.texture = depthTex;
			zprepass.SetDepthStencilOutput(&a_info);
			zprepass.SetPassArea({ {0,0}, resolution });
			zprepass.SetShader(shd_prepass);
			zprepass.pass_fn = [this](RHI::Weak<RHI::GraphicsCommandList> list) 
				{
					RHI::Viewport vp;
					vp.height = sceneResolution[1];
					vp.width = sceneResolution[0];
					vp.minDepth = 0;
					vp.maxDepth = 1;
					vp.x = vp.y = 0;
					list->SetViewports(1, &vp);
					RHI::Area2D rect = { {0,0},{sceneResolution[0],sceneResolution[1]}};
					list->SetScissorRects(1, &rect);
					Shader* shd = Renderer::GetBuiltinShader("Z-Prepass");
					list->SetRootSignature(shd->GetRootSignature());
					shd->ApplyBinding(list, passCBinfoGFX[RendererBase::GetCurrentFrameIndex()]);
					AssetManager* assetMan = GetAssetManager();
					list->BindVertexBuffers(0, 1, &Renderer::GetVertexBuffer()->ID);
					list->BindIndexBuffer(Renderer::GetIndexBuffer(), 0);
					for (auto entity : meshesToDraw)
					{
						auto& meshc = m_Registry.get<MeshRendererComponent>(entity);
						const Model* model = assetMan->GetResource<Model>(meshc.Model);
						const Mesh& mesh = model->meshes[meshc.modelIndex];
						list->BindDynamicDescriptor(Renderer::GetCBDesc(), 0, Renderer::GetCBOffset(meshc.handle));
						Renderer::Submit(list, mesh.GetVBHandle(), mesh.GetIBHandle(), sizeof(Vertex));
					}
				};
		}
		RenderPass& dirShadow = graph.AddPass(RHI::PipelineStage::LATE_FRAGMENT_TESTS_BIT, "Directional Shadow");
		{
			a_info.format = RHI::Format::D32_FLOAT;//?
			a_info.access = AttachmentAccess::ReadWrite;
			a_info.usage = AttachmentUsage::Unspec;
			a_info.texture = shadowMap;
			b_info.buffer = LightList;
			b_info.usage = AttachmentUsage::Graphics;
			dirShadow.SetDepthStencilOutput(&a_info);
			dirShadow.AddBufferOutput(&b_info);
			dirShadow.SetPassArea({ {0,0}, {shadowMapAtlas.GetWidth(), shadowMapAtlas.GetHeight()} });
			dirShadow.SetShader(shd_Shadow);
			dirShadow.pass_fn = [this](RHI::Weak<RHI::GraphicsCommandList> list) 
				{
					RHI::RenderingAttachmentDesc attachDesc{};
					attachDesc.clearColor = { 1,1,1,1 };
					attachDesc.ImageView = RendererBase::GetCPUHandle(shadowMapAtlas.DSView.Get());
					attachDesc.loadOp = RHI::LoadOp::Clear;
					attachDesc.storeOp = RHI::StoreOp::Store;
					RHI::RenderingBeginDesc rbDesc{};
					rbDesc.pDepthStencilAttachment = &attachDesc;

					AssetManager* assetMan = GetAssetManager();
					uint32_t baseOffset = (regularLights.size() * sizeof(RegularLight)) / (sizeof(float) * 4);
					uint32_t offsetMul = sizeof(ShadowCastingLight) / (sizeof(float) * 4);
					uint32_t index = 0;
					Shader* shd = Renderer::GetBuiltinShader("Shadow Shader");
					for (auto& light : shadowLights)
					{
						if ((light.light.type != LightType::Directional)) 
						{ index++; continue; }

						RHI::Viewport vp[4];
						vp[0].height = light.shadowMap.size.y/2; vp[0].width = light.shadowMap.size.x/2;
						vp[0].minDepth = 0; vp[0].maxDepth = 1;
						vp[0].x = light.shadowMap.offset.x; vp[0].y = light.shadowMap.offset.y;
						vp[1] = vp[2] = vp[3] = vp[0];

						vp[1].x += vp[1].width; vp[2].y += vp[2].height; vp[3].y += vp[3].height; vp[3].x += vp[3].width;

						RHI::Area2D rect = { {(int)vp[0].x,(int)vp[0].y},{(uint32_t)vp[0].width*2, (uint32_t)vp[0].height*2 }};
						
						shd->ApplyBinding(list, shadowSetInfo);
						list->SetScissorRects(1, &rect);
						auto meshes = m_Registry.view<MeshRendererComponent>();
						rbDesc.renderingArea = { {(int)light.shadowMap.offset.x,(int)light.shadowMap.offset.y},
							{light.shadowMap.size.x, light.shadowMap.size.y} };
						list->BeginRendering(rbDesc);
						uint32_t offset_cascade[2] = {baseOffset + (index * offsetMul),0};
						for (auto entity : meshes)
						{
							auto& meshc = meshes.get<MeshRendererComponent>(entity);
							const Model* model = assetMan->GetResource<Model>(meshc.Model);
							if(!model) continue;
							const Mesh& mesh = model->meshes[meshc.modelIndex];
							list->BindDynamicDescriptor(Renderer::GetCBDesc(), 0, Renderer::GetCBOffset(meshc.handle));
							for(uint32_t i = 0; i < 4; i++)
							{
								list->SetViewports(1, &vp[i]);
								offset_cascade[1] = i;
								list->PushConstants(1,2,offset_cascade,0);
								Renderer::Submit(list, mesh.GetVBHandle(), mesh.GetIBHandle(), sizeof(Vertex));
							}
						}
						list->EndRendering();
						index++;
					}
				};
		}
		RenderPass& pntShadow = graph.AddPass(RHI::PipelineStage::LATE_FRAGMENT_TESTS_BIT, "Point Shadow");
		{
			a_info.format = RHI::Format::D32_FLOAT;//?
			a_info.access = AttachmentAccess::ReadWrite;//dont clear the shadow map
			a_info.usage = AttachmentUsage::Graphics;
			a_info.texture = shadowMap;
			pntShadow.SetDepthStencilOutput(&a_info);
			pntShadow.SetPassArea({ {0,0}, {shadowMapAtlas.GetWidth(), shadowMapAtlas.GetHeight()} });;
			//sptShadow.SetShader(nullptr);
			pntShadow.pass_fn = [this](RHI::Weak<RHI::GraphicsCommandList> list) {
				};
		}
		RenderPass& sptShadow = graph.AddPass(RHI::PipelineStage::ALL_GRAPHICS_BIT, "Spot Shadow");
		{
			a_info.format = RHI::Format::D32_FLOAT;//?
			a_info.access = AttachmentAccess::Write;
			a_info.usage = AttachmentUsage::Unspec;
			a_info.texture = shadowMap;
			b_info.buffer = LightList;
			b_info.usage = AttachmentUsage::Graphics;
			sptShadow.SetDepthStencilOutput(&a_info);
			sptShadow.AddBufferOutput(&b_info);
			sptShadow.SetShader(shd_Shadow);
			sptShadow.pass_fn = [this](RHI::Weak<RHI::GraphicsCommandList> list)
				{
					RHI::RenderingAttachmentDesc attachDesc{};
					attachDesc.clearColor = { 1,1,1,1 };
					attachDesc.ImageView = RendererBase::GetCPUHandle(shadowMapAtlas.DSView.Get());
					attachDesc.loadOp = RHI::LoadOp::Clear;
					attachDesc.storeOp = RHI::StoreOp::Store;
					RHI::RenderingBeginDesc rbDesc{};
					rbDesc.pDepthStencilAttachment = &attachDesc;

					AssetManager* assetMan = GetAssetManager();
					uint32_t baseOffset = (regularLights.size() * sizeof(RegularLight)) / (sizeof(float) * 4);
					uint32_t offsetMul = sizeof(ShadowCastingLight) / (sizeof(float) * 4);
					uint32_t index = 0;
					Shader* shd = Renderer::GetBuiltinShader("Shadow Shader");
					for (auto& light : shadowLights)
					{
						if ((light.light.type != LightType::Spot)) 
						{ index++; continue; }
						RHI::Viewport vp;
						vp.height = light.shadowMap.size.y; vp.width = light.shadowMap.size.x;
						vp.minDepth = 0; vp.maxDepth = 1;
						vp.x = light.shadowMap.offset.x; vp.y = light.shadowMap.offset.y;
						list->SetViewports(1, &vp);
						RHI::Area2D rect = { {(int)vp.x,(int)vp.y},{(uint32_t)vp.width, (uint32_t)vp.height }};
						shd->ApplyBinding(list, shadowSetInfo);
						list->SetScissorRects(1, &rect);
						uint32_t offset[2] = {baseOffset + (index * offsetMul),0};
						list->PushConstants(1,2,offset,0);
						auto meshes = m_Registry.view<MeshRendererComponent>();
						rbDesc.renderingArea = { {(int)light.shadowMap.offset.x,(int)light.shadowMap.offset.y},
							{light.shadowMap.size.x, light.shadowMap.size.y} };
						list->BeginRendering(rbDesc);
						for (auto entity : meshes)
						{
							auto& meshc = meshes.get<MeshRendererComponent>(entity);
							const auto* model = assetMan->GetResource<Model>(meshc.Model);
							const Mesh& mesh = model->meshes[meshc.modelIndex];
							list->BindDynamicDescriptor(Renderer::GetCBDesc(), 0, Renderer::GetCBOffset(meshc.handle));
							Renderer::Submit(list, mesh.GetVBHandle(), mesh.GetIBHandle(), sizeof(Vertex));
						}
						list->EndRendering();
						index++;
					}
				};
		}
		ComputePass& buildClusters = graph.AddComputePass("Build Clusters");
		{
			b_info.usage = AttachmentUsage::Compute;
			b_info.buffer = clustersBuffer;
			buildClusters.AddBufferOutput(&b_info);
			buildClusters.SetShader(Renderer::GetBuiltinComputeShader("Build Clusters"));
			buildClusters.pass_fn = [this](RHI::Weak<RHI::GraphicsCommandList> list) 
				{
					ComputeShader* shd = Renderer::GetBuiltinComputeShader("Build Clusters");
					shd->ApplyShaderBinding(list, passCBinfoCMP[RendererBase::GetCurrentFrameIndex()]);
					shd->ApplyShaderBinding(list, buildClusterInfo);
					list->Dispatch(clustersDim[0], clustersDim[1], clustersDim[2]);
					list->MarkBuffer(graph.dbgBufferCMP, 3);
				};
		}
		ComputePass& filterClusters = graph.AddComputePass("Filter Clusters");
		{
			a_info.format = RHI::Format::D32_FLOAT;
			a_info.access = AttachmentAccess::ReadWrite;
			a_info.texture = depthTex;
			a_info.usage = AttachmentUsage::Compute;
			filterClusters.AddColorInput(&a_info);
			b_info.usage = AttachmentUsage::Compute;
			b_info.buffer = sparseActiveClusterBuffer;
			filterClusters.AddBufferOutput(&b_info);
			filterClusters.SetShader(Renderer::GetBuiltinComputeShader("Filter Clusters"));
			filterClusters.pass_fn = [this](RHI::Weak<RHI::GraphicsCommandList> list)
				{
					ComputeShader* shd = Renderer::GetBuiltinComputeShader("Filter Clusters");
					shd->ApplyShaderBinding(list, passCBinfoCMP[RendererBase::GetCurrentFrameIndex()]);
					shd->ApplyShaderBinding(list, activeClusterInfo);
					list->Dispatch(sceneResolution[0], sceneResolution[1], 1);
					list->MarkBuffer(graph.dbgBufferCMP,4);
				};
		}
		ComputePass& tightenCluster = graph.AddComputePass("Tighten Cluster List");
		{
			b_info.buffer = sparseActiveClusterBuffer;
			b_info.usage = AttachmentUsage::Compute;
			BufferAttachmentInfo b_info2;
			b_info2.buffer = ActiveClusterBuffer;
			b_info2.usage = AttachmentUsage::Compute;
			tightenCluster.AddBufferInput(&b_info);
			tightenCluster.AddBufferOutput(&b_info2);
			tightenCluster.SetShader(Renderer::GetBuiltinComputeShader("Tighten Clusters"));
			tightenCluster.pass_fn = [this](RHI::Weak<RHI::GraphicsCommandList> list)
				{
					ComputeShader* shd = Renderer::GetBuiltinComputeShader("Tighten Clusters");
					shd->ApplyShaderBinding(list, passCBinfoCMP[RendererBase::GetCurrentFrameIndex()]);
					shd->ApplyShaderBinding(list, tightenListInfo);
					list->Dispatch(clustersDim[0], clustersDim[1], clustersDim[2]);
					list->MarkBuffer(graph.dbgBufferCMP, 5);
				};
		}
		ComputePass& cullLights = graph.AddComputePass("Light Culling");
		{
			b_info.buffer = LightIndices;
			b_info.usage = AttachmentUsage::Compute;
			BufferAttachmentInfo b_info2{ LightList, AttachmentUsage::Compute };
			BufferAttachmentInfo b_info3{ LightGrid, AttachmentUsage::Compute };
			BufferAttachmentInfo b_info4{ ActiveClusterBuffer, AttachmentUsage::Compute };
			cullLights.AddBufferInput(&b_info4);
			cullLights.AddBufferOutput(&b_info);
			cullLights.AddBufferInput(&b_info2);
			cullLights.AddBufferOutput(&b_info3);
			cullLights.SetShader(Renderer::GetBuiltinComputeShader("Cull Lights"));
			cullLights.pass_fn = [this](RHI::Weak<RHI::GraphicsCommandList> list)
				{
					ComputeShader* shd = Renderer::GetBuiltinComputeShader("Cull Lights");
					shd->ApplyShaderBinding(list, passCBinfoCMP[RendererBase::GetCurrentFrameIndex()]);
					shd->ApplyShaderBinding(list, cullLightsInfo);
					list->Dispatch(clustersDim[0], clustersDim[1], clustersDim[2]);
					list->MarkBuffer(graph.dbgBufferCMP, 6);
					list->MarkBuffer(computeShaderMiscBuffer.GetID(), 0, 0);
					list->MarkBuffer(computeShaderMiscBuffer.GetID(), 4, 0);
				};
		}
		RenderPass& fwdShading = graph.AddPass(RHI::PipelineStage::ALL_GRAPHICS_BIT, "Forward Shading");
		{
			b_info.buffer = LightIndices;
			b_info.usage = AttachmentUsage::Graphics;
			BufferAttachmentInfo b_info2{ LightList, AttachmentUsage::Graphics };
			BufferAttachmentInfo b_info3{ LightGrid, AttachmentUsage::Graphics };
			fwdShading.AddBufferInput(&b_info);
			fwdShading.AddBufferInput(&b_info2);
			fwdShading.AddBufferInput(&b_info3);
			a_info.format = RHI::Format::D32_FLOAT;
			a_info.texture = shadowMap;
			a_info.access = AttachmentAccess::Read;
			a_info.usage = AttachmentUsage::Graphics;
			fwdShading.AddColorInput(&a_info);
			a_info.format = RHI::Format::R16G16B16A16_FLOAT;
			a_info.texture = finalRenderTex;
			a_info.access = AttachmentAccess::Write;
			fwdShading.AddColorOutput(&a_info);
			a_info.format = RHI::Format::D32_FLOAT;
			a_info.texture = depthTex;
			a_info.access = AttachmentAccess::Read;
			a_info.usage = AttachmentUsage::Graphics;
			fwdShading.SetDepthStencilOutput(&a_info);
			//fwdShading.SetShader(); we dont set shader because of custom shader support
			fwdShading.SetPassArea({ { 0,0},resolution });
			fwdShading.pass_fn = [this](RHI::Weak<RHI::GraphicsCommandList> list)
				{
					RHI::Viewport vp;
					vp.height = sceneResolution[1];
					vp.width = sceneResolution[0];
					vp.minDepth = 0;
					vp.maxDepth = 1;
					vp.x = vp.y = 0;
					list->SetViewports(1, &vp);
					RHI::Area2D rect = { 0,0,sceneResolution[0],sceneResolution[1] };
					list->SetScissorRects(1, &rect);
					AssetManager* assetMan = GetAssetManager();
					list->BindVertexBuffers(0, 1, &Renderer::GetVertexBuffer()->ID);
					list->BindIndexBuffer(Renderer::GetIndexBuffer(), 0);
					
					for (auto entity : meshesToDraw)
					{
						auto& meshc = m_Registry.get<MeshRendererComponent>(entity);
						const auto* mtl = assetMan->GetResource<Material>(meshc.material);
						mtl->Bind(list);
						Renderer::FullCBUpdate(mtl->parametersBuffer, mtl->parametersBufferCPU);
						const Shader& shd = assetMan->GetResource<ShaderAsset>(mtl->GetShader())->GetShader();
						const auto* model = assetMan->GetResource<Model>(meshc.Model);
						const Mesh& mesh = model->meshes[meshc.modelIndex];
						shd.ApplyBinding(list, passCBinfoVS_PS[RendererBase::GetCurrentFrameIndex()]);
						shd.ApplyBinding(list, sceneInfo);
						list->BindDynamicDescriptor(Renderer::GetCBDesc(), 0, Renderer::GetCBOffset(meshc.handle));
						Renderer::Submit(list, mesh.GetVBHandle(), mesh.GetIBHandle(), sizeof(Vertex));
					}
				};
			
		}
		RenderPass& backgroundPass = graph.AddPass(RHI::PipelineStage::ALL_GRAPHICS_BIT, "Background Pass");
		{
			a_info.format = RHI::Format::R16G16B16A16_FLOAT;
			a_info.texture = finalRenderTex;
			a_info.access = AttachmentAccess::Read;
			backgroundPass.AddColorInput(&a_info);
			a_info.texture = finalRenderWithBackground;
			backgroundPass.AddColorOutput(&a_info);
			a_info.format = RHI::Format::D32_FLOAT;
			a_info.texture = depthTex;
			a_info.access = AttachmentAccess::Read;
			a_info.usage = AttachmentUsage::Graphics;
			backgroundPass.SetDepthStencilOutput(&a_info);
			backgroundPass.SetShader(shd_background);
			backgroundPass.SetPassArea({ { 0,0},resolution });
			backgroundPass.pass_fn = [this](RHI::Weak<RHI::GraphicsCommandList> list)
			{
				RHI::Viewport vp;
				vp.height = sceneResolution[1];
				vp.width = sceneResolution[0];
				vp.minDepth = 0;
				vp.maxDepth = 1;
				vp.x = vp.y = 0;
				list->SetViewports(1, &vp);
				RHI::Area2D rect = { 0,0,sceneResolution[0],sceneResolution[1] };
				list->SetScissorRects(1, &rect);
				auto* shader = Renderer::GetBuiltinShader("Background Shader");
				auto& cb_info = passCBinfoGFX[RendererBase::GetCurrentFrameIndex()];
				uint32_t old_ind = cb_info.setIndex;
				cb_info.setIndex = 0;
				shader->ApplyBinding(list, passCBinfoGFX[RendererBase::GetCurrentFrameIndex()]);
				cb_info.setIndex = old_ind;
				shader->ApplyBinding(list, backgroundInfo);
				Renderer::Submit(list, Renderer::UnitCube()->GetVBHandle(), Renderer::UnitCube()->GetIBHandle(), sizeof(Vertex));
			};
		}
		graph.Compile();

		//graph.Execute();
		//graph.SubmitToQueue();
	}
	Scene::~Scene()
	{
	}
	Entity Scene::CreateRootEntity(UUID ID)
	{
		Entity entity = { m_Registry.create(), this };
		entity.AddComponent<IDComponent>(ID);
		auto& hierarchy = entity.AddComponent<HierarchyComponent>();
		hierarchy.parentID = entt::null;
		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = "Root";
		return entity;
	}
	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithUUID(UUID(), name);
	}
	Entity Scene::DuplicateEntity(Entity entity)
	{
		if (entity.GetComponent<HierarchyComponent>().parentID == entt::null)
			return entity;
		Entity newEntity = CreateEntity(entity.GetComponent<TagComponent>().Tag + "-Copy");
		newEntity.GetComponent<TransformComponent>() = entity.GetComponent<TransformComponent>();
		newEntity.GetComponent<HierarchyComponent>() = entity.GetComponent<HierarchyComponent>();
		if (entity.HasComponent<LightComponent>()) newEntity.AddComponent<LightComponent>(entity.GetComponent<LightComponent>());
		if (entity.HasComponent<SpriteRendererComponent>()) newEntity.AddComponent<SpriteRendererComponent>() = entity.GetComponent<SpriteRendererComponent>();
		if (entity.HasComponent<MeshRendererComponent>()) newEntity.AddComponent<MeshRendererComponent>() = entity.GetComponent<MeshRendererComponent>();
		if (entity.HasComponent<RigidBodyComponent>()) newEntity.AddComponent<RigidBodyComponent>() = entity.GetComponent<RigidBodyComponent>();
		if (entity.HasComponent<BoxColliderComponent>()) newEntity.AddComponent<BoxColliderComponent>() = entity.GetComponent<BoxColliderComponent>();
		if (entity.HasComponent<SphereColliderComponent>()) newEntity.AddComponent<SphereColliderComponent>() = entity.GetComponent<SphereColliderComponent>();
		return newEntity;
	}
	void Scene::ReparentEntity(Entity e, Entity new_parent)
	{
		if(e == root) return;
		auto view = m_Registry.view<HierarchyComponent>();
		auto& child = view.get<HierarchyComponent>(e);
		auto& old_tree = view.get<HierarchyComponent>(child.parentID);
		auto& new_tree = view.get<HierarchyComponent>(new_parent);
		//if new parent is a child of e return
		entt::entity iter = new_tree.parentID;
		while(iter != entt::null)
		{
			if(iter == e) return;
			iter = view.get<HierarchyComponent>(iter).parentID;
		}
		auto it = std::find(old_tree.chilren.begin(), old_tree.chilren.end(), e);
		old_tree.chilren.erase(it);

		child.parentID = new_parent;
		new_tree.chilren.push_back(e);
	}

	void Scene::SyncSkybox()
	{
		auto& env = m_Registry.get<EnvironmentComponent>(root);
		auto skybox = GetAssetManager()->GetResource<Skybox>(env.skybox);
		sceneInfo.UpdateTextureBinding(skybox->Irradiance()->GetView(), 1);
		sceneInfo.UpdateTextureBinding(skybox->SpecularPrefiltered()->GetView(), 2);

		backgroundInfo.UpdateTextureBinding(skybox->Base()->GetView(), 0);
	}

	Entity Scene::CreateEntityWithUUID(UUID ID, const std::string& name)
	{
		Entity entity = { m_Registry.create(), this };
		entity.AddComponent<IDComponent>(ID);
		entity.AddComponent<HierarchyComponent>((entt::entity)0);
		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		char id[100] = {'E','n','t','i','t', 'y', '0', '0', '0', '\0'};
		sprintf(id+6,"%u",((uint32_t)entity));
		tag.Tag = name.empty() ? id : name;

		auto& root_children = m_Registry.get<HierarchyComponent>(root);
		root_children.chilren.push_back(entity);
		return entity;
	}
	void Scene::OnRuntimeStop()
	{
	}
	void Scene::DestroyEntity(Entity entity)
	{
		auto view = m_Registry.view<HierarchyComponent>();
		for (auto child : view)
		{
			if (view.get<HierarchyComponent>(child).parentID == entity)
				DestroyEntity(Entity(child, this));
		}
		m_Registry.destroy(entity);
	}
	void Scene::DefferedDelete(Entity entity)
	{
		auto view = m_Registry.view<HierarchyComponent>();
		for (auto child : view)
		{
			if (view.get<HierarchyComponent>(child).parentID == entity)
				DefferedDelete(Entity(child, this));
		}
		deletionQueue.push_back(entity);
	}
	Entity Scene::GetRootEntity()
	{
		return Entity(root, this);
	}
	
	Entity Scene::GetPrimaryCameraEntity()
	{
		PT_PROFILE_FUNCTION();
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			const auto& camera = view.get<CameraComponent>(entity);
			if (camera.Primary)
				return Entity(entity, this);
		}
		return Entity();
	}
	Entity Scene::GetEntityByUUID(UUID id)
	{
		auto view = m_Registry.view<IDComponent>();
		Entity retVal = Entity(entt::null, this);
		for(auto entity : view)
		{
			auto [in_id] = view.get(entity);
			if(in_id.uuid == id)
			{
				retVal = Entity(entity, this);
				break;
			}
		}
		return retVal;
	}
	Entity Scene::GetEntityByName(const std::string& name)
	{
		auto view = m_Registry.view<TagComponent>();
		Entity retVal = Entity(entt::null, this);
		for(auto entity : view)
		{
			auto [tag] = view.get(entity);
			if(tag.Tag == name)
			{
				retVal = Entity(entity, this);
				break;
			}
		}
		return retVal;
	}
	std::vector<Entity> Scene::GetAllEntitesWithName(const std::string& name)
	{
		auto view = m_Registry.view<TagComponent>();
		std::vector<Entity> retVal;
		for(auto entity : view)
		{
			auto [tag] = view.get(entity);
			if(tag.Tag == name) retVal.push_back(Entity(entity, this));
		}
		return retVal;
	}
	void Scene::UpdatePassConstants(const Matrix4& view, const SceneCamera& cam, const Vector3& camPos,float delta)
	{
		const Matrix4& proj = cam.GetProjection();
		Matrix4 viewProj = view * proj;
		XMStoreFloat4x4(&passConstants.View, XMMatrixTranspose(view));
		XMStoreFloat4x4(&passConstants.Proj, XMMatrixTranspose(proj));
		XMStoreFloat4x4(&passConstants.ViewProj, XMMatrixTranspose(viewProj));
		XMStoreFloat4x4(&passConstants.InvProj, XMMatrixTranspose(XMMatrixInverse(nullptr, proj)));
		XMStoreFloat4x4(&passConstants.InvView, XMMatrixTranspose(XMMatrixInverse(nullptr, view)));
		XMStoreFloat4x4(&passConstants.InvViewProj, XMMatrixTranspose(XMMatrixInverse(nullptr, viewProj)));
		passConstants.numClusters = { (float)clustersDim[0],(float)clustersDim[1],(float)clustersDim[2]};
		passConstants.RenderTargetSize = { (float)sceneResolution[0],(float)sceneResolution[1]};
		passConstants.NearZ = cam.GetNear();
		passConstants.FarZ = cam.GetFar();
		passConstants.InvRenderTargetSize = { 1.f / (float)sceneResolution[0], 1.f / (float)sceneResolution[1]};
		passConstants.bias = ((float)clustersDim[2] * log10f(passConstants.NearZ)) / (log10f(passConstants.FarZ / passConstants.NearZ));
		passConstants.scale = (float)clustersDim[2] / (log10f(passConstants.FarZ / passConstants.NearZ));
		passConstants.numDirectionalRegularLights = numRegularDirLights;
		passConstants.numDirectionalShadowLights = numShadowDirLights;
		passConstants.numRegularLights = regularLights.size();
		passConstants.numShadowLights = shadowLights.size();
		passConstants.EyePosW = camPos;
		passConstants.DeltaTime = delta;
		passCB[RendererBase::GetCurrentFrameIndex()].Update(&passConstants, sizeof(PassConstants), 0);
	}
	void Scene::UpdatePassConstants(const EditorCamera& cam, float delta)
	{
		const Matrix4& proj = cam.GetProjection();
		const Matrix4& view = cam.GetViewMatrix();
		Matrix4 viewProj = cam.GetViewProjection();
		XMStoreFloat4x4(&passConstants.View, XMMatrixTranspose(view));
		XMStoreFloat4x4(&passConstants.Proj, XMMatrixTranspose(proj));
		XMStoreFloat4x4(&passConstants.ViewProj, XMMatrixTranspose(viewProj));
		XMStoreFloat4x4(&passConstants.InvProj, XMMatrixTranspose(XMMatrixInverse(nullptr, proj)));
		XMStoreFloat4x4(&passConstants.InvView, XMMatrixTranspose(XMMatrixInverse(nullptr, view)));
		XMStoreFloat4x4(&passConstants.InvViewProj, XMMatrixTranspose(XMMatrixInverse(nullptr, viewProj)));
		passConstants.numClusters = { (float)clustersDim[0],(float)clustersDim[1],(float)clustersDim[2] };
		passConstants.RenderTargetSize = { (float)sceneResolution[0],(float)sceneResolution[1] };
		passConstants.NearZ = cam.GetNearClip();
		passConstants.FarZ = cam.GetFarClip();
		passConstants.InvRenderTargetSize = { 1.f / (float)sceneResolution[0], 1.f / (float)sceneResolution[1] };
		passConstants.bias = ((float)clustersDim[2] * log10f(passConstants.NearZ)) / (log10f(passConstants.FarZ / passConstants.NearZ));
		passConstants.scale = (float)clustersDim[2] / (log10f(passConstants.FarZ / passConstants.NearZ));
		passConstants.numDirectionalRegularLights = numRegularDirLights;
		passConstants.numDirectionalShadowLights = numShadowDirLights;
		passConstants.numRegularLights = regularLights.size();
		passConstants.numShadowLights = shadowLights.size();
		passConstants.EyePosW = cam.GetPosition();
		passConstants.DeltaTime = delta;
		passCB[RendererBase::GetCurrentFrameIndex()].Update(&passConstants, sizeof(PassConstants), 0);
	}
	const RenderTexture& Scene::GetFinalRender()
	{
		return finalRender;
	}
	void Scene::UpdateTransforms(entt::entity e, const Matrix4& mat)
	{
		auto& tc = m_Registry.get<TransformComponent>(e);
		auto& hierc = m_Registry.get<HierarchyComponent>(e);
		tc.worldSpaceTransform = tc.GetLocalTransform() * ((DirectX::XMMATRIX)mat);
		for(auto child: hierc.chilren)
		{
			UpdateTransforms(child, tc.worldSpaceTransform);
		}
	}
	void Scene::OnUpdateEditor(float delta, EditorCamera& camera)
	{
		/*
		* Todo each parent should store a list of thier children, so we can
		* traverse a scene from the root entity and dirty/update the world space transfrom for the
		* remainder of the frame. then after the frame everything is "undirtied"
		*/
		PT_PROFILE_FUNCTION();
		meshesToDraw.clear();
		shadowLights.clear();
		regularLights.clear();
		numShadowDirLights = 0;
		numRegularDirLights = 0;
		//auto transform_parent_view = m_Registry.view<TransformComponent, HierarchyComponent>();
		UpdateTransforms(root, Matrix4::Identity);
		FrustumCull(camera.GetViewMatrix(), camera.GetProjection(),Math::ToRadians(camera.GetFOVdeg()),camera.GetNearClip(), camera.GetFarClip(), camera.GetAspectRatio());
		UpdateObjectCBs();
		UpdatePassConstants(camera, delta);
		UpdateLightsBuffer();
		
		graph.Execute();
		graph.SubmitToQueue();
		RHI::SubResourceRange range;
		range.FirstArraySlice = 0;
		range.imageAspect = RHI::Aspect::COLOR_BIT;
		range.IndexOrFirstMipLevel = 0;
		range.NumArraySlices = 1;
		range.NumMipLevels = 1;
		RHI::TextureMemoryBarrier barr[1];
		barr[0].AccessFlagsBefore = RHI::ResourceAcessFlags::NONE;
		barr[0].AccessFlagsAfter = RHI::ResourceAcessFlags::NONE;
		barr[0].newLayout = RHI::ResourceLayout::GENERAL;
		barr[0].nextQueue = barr[0].previousQueue = RHI::QueueFamily::Ignored;
		barr[0].oldLayout = RHI::ResourceLayout::COLOR_ATTACHMENT_OPTIMAL;
		barr[0].texture = finalRender.GetID();
		barr[0].subresourceRange = range;

		RendererBase::GetMainCommandList()->PipelineBarrier(RHI::PipelineStage::COLOR_ATTACHMENT_OUTPUT_BIT,
			RHI::PipelineStage::TRANSFER_BIT, {}, {barr,1});

		//notify render graph of layout change
		finalRenderTex.originVector->at(finalRenderTex.offset).current_layout = RHI::ResourceLayout::GENERAL;
		m_Registry.destroy(deletionQueue.begin(), deletionQueue.end());
		deletionQueue.clear();
	}
	void Scene::OnUpdateRuntime(float delta)
	{

	}
	void Scene::OnViewportResize(unsigned int width, unsigned int height)
	{
		PT_PROFILE_FUNCTION();
		//m_viewportWidth  = width;
		//m_ViewportHeight = height;
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			auto& camera = view.get<CameraComponent>(entity);
			if (!camera.FixedAspectRatio)
			{
				camera.camera.SetViewportSize(width, height);
			}
		}
	}
	void Scene::UpdateObjectCBs()
	{
		PT_PROFILE_FUNCTION();
		auto view = m_Registry.view<TransformComponent, MeshRendererComponent>();
		TransformData td;
		for (auto entity : view)
		{
			auto [transform,mesh] = view.get(entity);
			DirectX::XMStoreFloat4x4(&td.transform, DirectX::XMMatrixTranspose(transform.worldSpaceTransform));
			DirectX::XMStoreFloat4x4(&td.normal,DirectX::XMMatrixInverse(nullptr, transform.worldSpaceTransform));
			Renderer::FullCBUpdate(mesh.handle, &td);
		}
	}
	void Scene::SortMeshComponents()
	{
		bool dirtyMats = false;
		auto view = m_Registry.view<MeshRendererComponent>();
		for (auto entity : view)
		{
			auto& mesh = view.get<MeshRendererComponent>(entity);
			if (mesh.bMaterialDirty)
			{
				mesh.bMaterialDirty = false;
				dirtyMats = true;
			}
		}
		if (dirtyMats)
		{
			PT_CORE_WARN("Actually sorted");
			m_Registry.sort<MeshRendererComponent>([](const MeshRendererComponent& lhs, const MeshRendererComponent& rhs) {return lhs.material.GetUUID() < rhs.material.GetUUID(); });
		}
	}
	void Scene::UpdateLightsBuffer()
	{
		uint32_t requiredSize = sizeof(Light) * regularLights.size() + sizeof(ShadowCastingLight) * shadowLights.size();
		if (requiredSize <= lightListSize)
		{
			std::vector<uint8_t> joinedBuffer(requiredSize);
			if (regularLights.size()) memcpy(joinedBuffer.data(), regularLights.data(), sizeof(Light) * regularLights.size());
			if (shadowLights.size()) memcpy(joinedBuffer.data() + sizeof(Light) * regularLights.size(), shadowLights.data(), sizeof(ShadowCastingLight) * shadowLights.size());
			if (requiredSize) lightList.Update(joinedBuffer.data(), requiredSize, 0);
			return;
		}
		DEBUG_BREAK;
	}
	//Pratical Subdivision Scheme, i(index), n(num cascades), lambda(number btw 0 and 1)
	static float pss(int i, int n, float lambda, float near, float far)
	{
    	float uni = ((far-near)/((float)n)) * ((float)i) + near;
    	float log = near * powf((far/near), ((float)i)/((float)n));
    
    	return lambda*uni + (1-lambda)*log;
	}
	constexpr const uint32_t shadow_size = 512;
	void Scene::FrustumCull(const Matrix4& view, const Matrix4& proj, float fovRad, float nearClip, float farClip, float aspect)
	{
		auto mesh_transform = m_Registry.view<MeshRendererComponent, TransformComponent>();
		BoundingFrustum cameraFrustum(proj);
		cameraFrustum.Transform(cameraFrustum, view.Invert());
		for (auto entity : mesh_transform)
		{
			auto [mesh, transform] = mesh_transform.get(entity);
			const auto* model = GetAssetManager()->GetResource<Model>(mesh.Model);
			if (model)
			{
				BoundingBox box = model->aabbs[mesh.modelIndex];
				box.Transform(box, transform.worldSpaceTransform);
				if (CullingManager::FrustumCull(box, cameraFrustum)) //we passed the test
				{
					meshesToDraw.emplace_back(entity);
				}
			}
		}

		auto light_transform = m_Registry.view<LightComponent, TransformComponent>();
		for (auto& entity : light_transform)
		{
			auto [lightcomponent,tc] = light_transform.get(entity);
			//free space in the shadow map from non visible lights
			if (lightcomponent.Type == LightType::Point || lightcomponent.Type == LightType::Spot)
			{
				bool visible = CullingManager::FrustumCull(BoundingSphere(tc.Translation, lightcomponent.exData.z), cameraFrustum);
				if (!visible)
				{
					if (lightcomponent.shadowMap)
					{
						sm_allocator.DeAllocate(lightcomponent.shadowMap);
						lightcomponent.shadowMap = 0;
					}
					continue;
				}
			}
			Light light;
			light.position = tc.Translation;
			light.type = lightcomponent.Type;
			float z = 1.f;
			if (light.type == LightType::Directional) z = -1.f;
			DirectX::XMVECTOR lightTransform = DirectX::XMVector3Rotate(DirectX::XMVectorSet(0.f, 0.f, z, 1.f), tc.Rotation);
			DirectX::XMStoreFloat4(&light.rotation, lightTransform);
			light.exData = { lightcomponent.exData.x , lightcomponent.exData.y, lightcomponent.exData.z, (float)lightcomponent.shadow };
			light.color = { lightcomponent.color.x, lightcomponent.color.y, lightcomponent.color.z };
			light.intensity = lightcomponent.Intensity;
			if (lightcomponent.shadow)
			{
				iVector2 allocation_size;
				ShadowCastingLight* sclight = 0;
				if (lightcomponent.Type == LightType::Directional)
				{
					sclight = &*shadowLights.insert(shadowLights.begin(),ShadowCastingLight());
					sclight->light = light;
					numShadowDirLights++;
					allocation_size = { shadow_size * 4, shadow_size * 4 };
					float pss_vals[3];
					for(uint32_t i = 0; i < 3; i++) pss_vals[i] = pss(i+1,4,0.3,nearClip,farClip);
					/*
						Massive Hack, But for a directional light, the translation doesn't matter, so we store the cascade plane distances there
					*/
					sclight->light.position = Vector3(pss_vals);
					DirectX::XMStoreFloat4x4(&sclight->projection[0],GetLightMatrixFromCamera(view, DirectX::XMMatrixPerspectiveFovLH(fovRad, aspect, nearClip, pss_vals[0]), light, 1.05f));
					DirectX::XMStoreFloat4x4(&sclight->projection[1],GetLightMatrixFromCamera(view, DirectX::XMMatrixPerspectiveFovLH(fovRad, aspect, pss_vals[0], pss_vals[1]), light, 1.2f));
					DirectX::XMStoreFloat4x4(&sclight->projection[2],GetLightMatrixFromCamera(view, DirectX::XMMatrixPerspectiveFovLH(fovRad, aspect, pss_vals[1], pss_vals[2]), light, 1.2f));
					DirectX::XMStoreFloat4x4(&sclight->projection[3],GetLightMatrixFromCamera(view, DirectX::XMMatrixPerspectiveFovLH(fovRad, aspect, pss_vals[2], farClip), light, 1.2f));
				}
				else if (lightcomponent.Type == LightType::Spot)
				{
					sclight = &shadowLights.emplace_back();
					sclight->light = light;
					allocation_size = { shadow_size, shadow_size };
					DirectX::XMStoreFloat4x4(&sclight->projection[0],DirectX::XMMatrixTranspose(DirectX::XMMatrixLookAtLH(tc.Translation, DirectX::XMVectorAdd(tc.Translation, DirectX::XMLoadFloat4(&light.rotation)), DirectX::XMVectorSet(0, 1, 0, 0)) * DirectX::XMMatrixPerspectiveFovLH(DirectX::XMScalarACos(lightcomponent.exData.x) * 2, 1, 0.1f, lightcomponent.exData.z)));
				}
				else if (lightcomponent.Type == LightType::Point)
				{
					sclight = &shadowLights.emplace_back();
					sclight->light = light;
					allocation_size = { 256 * 3, 256 * 2 };
					//todo : Handle point light matrices;
				}
				
				if (lightcomponent.shadowMap != 0) // if there was a shadow map dont allocate a new one unnecessarily
				{
					//todo camera cascades for varying shadow map sizes at different distance levels
					sclight->shadowMap = sm_allocator.GetRegion(lightcomponent.shadowMap);
				}
				else
				{
					lightcomponent.shadowMap = sm_allocator.Allocate(allocation_size, AllocatorFlags::None); // todo render settings to control allocation size
					sclight->shadowMap = sm_allocator.GetRegion(lightcomponent.shadowMap);
				}
			}
			else
			{
				if (lightcomponent.shadowMap != 0)
				{
					sm_allocator.DeAllocate(lightcomponent.shadowMap);
					lightcomponent.shadowMap = 0;
				}
				regularLights.push_back(light);
			}
		}
	}


}