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

pktx = subproject('pktx')
pktx_dep = pktx.get_variable('pktx_dep')

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
    pktx_dep,
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
    'src/Pistachio/Core/SwapChain.cpp',
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
    'src/Pistachio/Renderer/Skybox.cpp',
    'src/Pistachio/Renderer/Cubemap.cpp',
    'src/Pistachio/Renderer/Material.cpp',
    'src/Pistachio/Asset/AssetManager.cpp',
    'src/Pistachio/Physics/Physics.cpp',
    'src/Pistachio/Allocators/FreeList.cpp',
    'src/Pistachio/Scripting/AngelScript/script_file.cpp',
    'src/Pistachio/Scripting/AngelScript/ScriptAPIBase.cpp',
    'src/Pistachio/Scripting/AngelScript/ScriptAPI_ECS.cpp',
    'src/Pistachio/Allocators/AtlasAllocator.cpp',
    'src/Pistachio/Utils/RendererUtils.cpp',
    'vendor/SimpleMath/SimpleMath.cpp', #we don't need the entire DXTK
    'vendor/stb_image/stb_image.cpp'
]
tests_src = [
    'tests/script_test.cpp'
]
shaders_dir = 'src/Pistachio/Renderer/shaders/'
vertex_shaders = [
    'background_vs.hlsl',
    'prefilter_vs.hlsl',
    'StandaloneVertexShader.hlsl',
    'vertex_shader_no_transform.hlsl',
    'equirectangular_to_cubemap_vs.hlsl',
    'Shadow_vs.hlsl',
    'VertexShader.hlsl'
]
pixel_shaders = [
    'background_ps.hlsl',
    'CFPBRShader_ps.hlsl',
    'equirectangular_to_cubemap_fs.hlsl',
    'irradiance_fs.hlsl',
    'prefilter_fs.hlsl',
    'Shadow_ps.hlsl',
]
compute_shaders = [
    'BRDF_LUT_cs.hlsl',
    'CFActiveClusters_cs.hlsl',
    'CFBuildClusters_cs.hlsl',
    'CFCullLights_cs.hlsl',
    'CFTightenList_cs.hlsl',
    'EqToCubeMap_cs.hlsl',
    #'src/Pistachio/Renderer/shaders/compute/Irradiance_cs.hlsl',
]

vs_args = [rhi_sc_exe, '-i', '@INPUT@', '-o', '@OUTPUT@', '-ONone', '-g', '-t', 'vertex']
ps_args = [rhi_sc_exe, '-i', '@INPUT@', '-o', '@OUTPUT@', '-ONone', '-g', '-t', 'pixel']
cs_args = [rhi_sc_exe, '-i', '@INPUT@', '-o', '@OUTPUT@', '-ONone', '-g', '-t', 'compute']

vs_output = []
vs_input = []

ps_output = []
ps_input = []

cs_output = []
cs_input = []

foreach shader : vertex_shaders
    vs_output += (shader.substring(0, -5) + '.rbc') #replace .hlsl with .rbc
    vs_input += shaders_dir / 'vertex' / shader
endforeach
foreach shader : pixel_shaders
    ps_output += (shader.substring(0, -5) + '.rbc')
    ps_input += shaders_dir / 'pixel' / shader
endforeach
foreach shader : compute_shaders
    cs_output += (shader.substring(0, -5) + '.rbc')
    cs_input += shaders_dir / 'compute' / shader
endforeach
custom_target('Vertex Shaders', command: vs_args, input: vs_input,output: vs_output)
custom_target('Pixel Shaders', command: ps_args, input: ps_input,output: ps_output)
custom_target('Compute Shaders', command: cs_args, input: cs_input, output: cs_output)

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
