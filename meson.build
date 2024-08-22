project('Pistachio-Engine', 'cpp',
    default_options: ['cpp_std=c++20'])
deps_required = not get_option('fetch_dependencies')
args = ['-DPAL_STDCPP_COMPAT','-D_DEBUG']
cmake = import('cmake')


yaml_dep = dependency('yaml-cpp', required: deps_required)
if not yaml_dep.found()
    yaml_cpp = cmake.subproject('yaml-cpp')
    yaml_dep = yaml_cpp.dependency('yaml-cpp')
endif

assimp_dep = dependency('assimp')

glfw3_dep = dependency('glfw3')

angelscript = subproject('angelscript')
angelscript_dep = angelscript.get_variable('angelscript_dep')
as_addons_dep = angelscript.get_variable('as_addons_dep')

rhi = subproject('RHI')
rhi_dep = rhi.get_variable('rhi_dep')

rhi_sc = subproject('rhi_sc')
rhi_sc_dep = rhi_sc.get_variable('rhi_sc_dep')
rhi_sc_exe = rhi_sc.get_variable('rhi_sc_exe')

imgui_dep = dependency('imgui')

spdlog_dep = dependency('spdlog')

xxhash_dep = dependency('libxxhash')


# Normally we would depend on ezr but RHI provides it
# ezr = subproject('ezr')
# ezr_dep = ezr.get_variable('ezr_dep')
deps = [
    rhi_dep,
    yaml_dep,
    assimp_dep,
    imgui_dep,
    spdlog_dep,
    xxhash_dep,
    angelscript_dep,
    as_addons_dep,
    rhi_sc_dep,]

if(get_option('profiler_support') == 'tracy')
tracy = subproject('tracy')
tracy_dep = tracy.get_variable('tracy_dep')
trace_rhi = subproject('TraceRHI')
trace_rhi_dep = trace_rhi.get_variable('trace_rhi_dep')
deps += tracy_dep
deps += trace_rhi_dep
args += '-DPT_PROFILE_TRACY'
elif (get_option('profiler_support') == 'json')
args += '-DPT_PROFILE_JSON'
endif

inc_dirs = ['src/',
    'vendor/entt/include/',
    'vendor/sal',
    'vendor/SimpleMath',
    'vendor/stb_image/',
    'vendor/DirectXMath/',]

sources = [
    'src/Pistachio/Core/UUID.cpp',
    'src/Pistachio/Core/Layer.cpp',
    'src/Pistachio/Core/LayerStack.cpp',
    'src/Pistachio/Core/FileSystem.cpp',
    'src/Pistachio/Core/Application.cpp',
    'src/Pistachio/Core/Log.cpp',
    'src/Pistachio/Core/Input.cpp',
    'src/Pistachio/Core/Transform.cpp',
    'src/Pistachio/Core/InputCallbacks.cpp',
    'src/Pistachio/Scene/CullingManager.cpp',
    'src/Pistachio/Scene/Scene.cpp',
    'src/Pistachio/Scene/Entity.cpp',
    'src/Pistachio/Scene/SceneSerializer.cpp',
    'src/Pistachio/Renderer/ShaderAssetCompiler.cpp',
    'src/Pistachio/Renderer/FrameComposer.cpp',
    'src/Pistachio/Renderer/RenderGraph.cpp',
    'src/Pistachio/Renderer/Renderer2D.cpp',
    'src/Pistachio/Renderer/RendererContext.cpp',
    'src/Pistachio/Renderer/Model.cpp',
    'src/Pistachio/Renderer/ShaderAsset.cpp',
    'src/Pistachio/Renderer/Renderer.cpp',
    'src/Pistachio/Renderer/Camera.cpp',
    'src/Pistachio/Renderer/Shader.cpp',
    'src/Pistachio/Renderer/Buffer.cpp',
    'src/Pistachio/Renderer/Texture.cpp',
    'src/Pistachio/Renderer/EditorCamera.cpp',
    'src/Pistachio/Renderer/MeshFactory.cpp',
    'src/Pistachio/Renderer/RenderTexture.cpp',
    'src/Pistachio/Renderer/RendererBase.cpp',
    'src/Pistachio/Renderer/Mesh.cpp',
    'src/Pistachio/Renderer/Material.cpp',
    'src/Pistachio/Asset/AssetManager.cpp',
    'src/Pistachio/Physics/Physics.cpp',
    'src/Pistachio/Allocators/FreeList.cpp',
    'src/Pistachio/Scripting/AngelScript/script_file.cpp',
    'src/Pistachio/Scripting/AngelScript/ScriptAPIBase.cpp',
    'src/Pistachio/Scripting/AngelScript/ScriptAPI_ECS.cpp',
    'src/Pistachio/Allocators/AtlasAllocator.cpp',
    'vendor/SimpleMath/SimpleMath.cpp', #we don't need the entire DXTK
    'vendor/stb_image/stb_image.cpp'
]
tests_src = [
    'tests/script_test.cpp'
]
vertex_shaders = [
    'src/Pistachio/Renderer/shaders/vertex/prefilter_vs.hlsl',
    'src/Pistachio/Renderer/shaders/vertex/StandaloneVertexShader.hlsl',
    'src/Pistachio/Renderer/shaders/vertex/vertex_shader_no_transform.hlsl',
    'src/Pistachio/Renderer/shaders/vertex/equirectangular_to_cubemap_vs.hlsl',
    'src/Pistachio/Renderer/shaders/vertex/Shadow_vs.hlsl',
    'src/Pistachio/Renderer/shaders/vertex/VertexShader.hlsl'
]
pixel_shaders = [
    'src/Pistachio/Renderer/shaders/pixel/background_ps.hlsl',
    'src/Pistachio/Renderer/shaders/pixel/CFPBRShader_ps.hlsl',
    'src/Pistachio/Renderer/shaders/pixel/equirectangular_to_cubemap_fs.hlsl',
    'src/Pistachio/Renderer/shaders/pixel/irradiance_fs.hlsl',
    'src/Pistachio/Renderer/shaders/pixel/prefilter_fs.hlsl',
    'src/Pistachio/Renderer/shaders/pixel/Shadow_ps.hlsl',
]
compute_shaders = [
    'src/Pistachio/Renderer/shaders/compute/BRDF_LUT_cs.hlsl',
    'src/Pistachio/Renderer/shaders/compute/CFActiveClusters_cs.hlsl',
    'src/Pistachio/Renderer/shaders/compute/CFBuildClusters_cs.hlsl',
    'src/Pistachio/Renderer/shaders/compute/CFCullLights_cs.hlsl',
    'src/Pistachio/Renderer/shaders/compute/CFTightenList_cs.hlsl',
    'src/Pistachio/Renderer/shaders/compute/EqToCubeMap_cs.hlsl',
    #'src/Pistachio/Renderer/shaders/compute/Irradiance_cs.hlsl',
]
shader_gen = generator(rhi_sc_exe, arguments: ['@INPUT@', '-o', '@OUTPUT@', '-ONone', '-g', '@EXTRA_ARGS@'],  output: ['@BASENAME@.rbc'])

vs_compiled = shader_gen.process(vertex_shaders, extra_args: ['-t', 'vertex'])
ps_compiled = shader_gen.process(pixel_shaders, extra_args: ['-t', 'pixel'])
cs_compiled = shader_gen.process(compute_shaders, extra_args: ['-t', 'compute'])

custom_target('Shaders', command:['cd'], input: [vs_compiled, ps_compiled, cs_compiled], output: ['shaders'])
if(build_machine.system() == 'linux')
    deps += glfw3_dep
    args += '-DUSE_GLFW'
    args += '-DPT_PLATFORM_LINUX'
    sources += 'src/Pistachio/Platform/Linux/LinuxWindow.cpp'
    sources += 'src/Pistachio/Platform/Linux/LinuxInputPoll.cpp'
endif
if(get_option('verbose_logging') == true)
args += '-DPT_VERBOSE'
endif
if(get_option('use_magic_enum') == true)
deps += dependency('magic_enum')
args += '-DPT_USE_MENUM'
endif
add_project_arguments(args,language: 'cpp',)
inc = include_directories(inc_dirs)
lib = shared_library('Pistachio',sources,include_directories:inc,dependencies:deps,
    override_options: ['cpp_std=c++20'])
pistachio_dep = declare_dependency(include_directories:inc, link_with:lib, dependencies: deps)
executable('Pistachio-Tests', tests_src,dependencies: pistachio_dep)
