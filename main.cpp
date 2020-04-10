
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#if D3D11_SUPPORTED
#include <EngineFactoryD3D11.h>
#endif
#if D3D12_SUPPORTED
#include <EngineFactoryD3D12.h>
#endif
#if GL_SUPPORTED || GLES_SUPPORTED
#include <EngineFactoryOpenGL.h>
#endif
#if VULKAN_SUPPORTED
#include <EngineFactoryVk.h>
#endif
#if METAL_SUPPORTED
#include <EngineFactoryMtl.h>
#endif
#include <Common/interface/RefCntAutoPtr.hpp>

#if PLATFORM_MACOS
#define GLFW_EXPOSE_NATIVE_COCOA
#endif
#if PLATFORM_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

using namespace Diligent;

void InitializeEngine(const NativeWindow* pWindow);
void InitializePipeline();
void Render();
void Present();

#if PLATFORM_WIN32
RENDER_DEVICE_TYPE m_DeviceType = RENDER_DEVICE_TYPE_D3D12;
#else
RENDER_DEVICE_TYPE m_DeviceType = RENDER_DEVICE_TYPE_GL;
#endif
RefCntAutoPtr<IPipelineState> m_pPSO;
RefCntAutoPtr<IEngineFactory> m_pEngineFactory;
RefCntAutoPtr<IRenderDevice> m_pDevice;
RefCntAutoPtr<IDeviceContext> m_pImmediateContext;
std::vector<RefCntAutoPtr<IDeviceContext>> m_pDeferredContexts;
RefCntAutoPtr<ISwapChain> m_pSwapChain;
AdapterAttribs m_AdapterAttribs;
std::vector<DisplayModeAttribs> m_DisplayModes;

int m_InitialWindowWidth = 0;
int m_InitialWindowHeight = 0;
int m_ValidationLevel = -1;
std::string m_AppTitle;
Uint32 m_AdapterId = 0;
ADAPTER_TYPE m_AdapterType = ADAPTER_TYPE_UNKNOWN;
std::string m_AdapterDetailsString;
int m_SelectedDisplayMode = 0;
bool m_bVSync = false;
bool m_bFullScreenMode = false;
bool m_bShowAdaptersDialog = true;
bool m_bShowUI = true;
double m_CurrentTime = 0;

static const char* VSSource = R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float3 Color : COLOR; 
};
void main(in  uint    VertId : SV_VertexID,
          out PSInput PSIn) 
{
    float4 Pos[3];
    Pos[0] = float4(-0.5, -0.5, 0.0, 1.0);
    Pos[1] = float4( 0.0, +0.5, 0.0, 1.0);
    Pos[2] = float4(+0.5, -0.5, 0.0, 1.0);
    float3 Col[3];
    Col[0] = float3(1.0, 0.0, 0.0); // red
    Col[1] = float3(0.0, 1.0, 0.0); // green
    Col[2] = float3(0.0, 0.0, 1.0); // blue
    PSIn.Pos   = Pos[VertId];
    PSIn.Color = Col[VertId];
}
)";

// Pixel shader simply outputs interpolated vertex color
static const char* PSSource = R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float3 Color : COLOR; 
};
struct PSOutput
{ 
    float4 Color : SV_TARGET; 
};
void main(in  PSInput  PSIn,
          out PSOutput PSOut)
{
    PSOut.Color = float4(PSIn.Color.rgb, 1.0);
}
)";

void InitializeEngine(const NativeWindow* pWindow) {
  SwapChainDesc SCDesc;

#if PLATFORM_MACOS
  // We need at least 3 buffers on Metal to avoid massive
  // peformance degradation in full screen mode.
  // https://github.com/KhronosGroup/MoltenVK/issues/808
  SCDesc.BufferCount = 3;
#endif

  std::vector<IDeviceContext*> ppContexts;
  switch (m_DeviceType) {
#if D3D11_SUPPORTED
    case RENDER_DEVICE_TYPE_D3D11: {
      EngineD3D11CreateInfo EngineCI;

      if (m_ValidationLevel >= 1) {
        EngineCI.DebugFlags =
            D3D11_DEBUG_FLAG_CREATE_DEBUG_DEVICE |
            D3D11_DEBUG_FLAG_VERIFY_COMMITTED_SHADER_RESOURCES |
            D3D11_DEBUG_FLAG_VERIFY_COMMITTED_RESOURCE_RELEVANCE;
      } else if (m_ValidationLevel == 0) {
        EngineCI.DebugFlags = D3D11_DEBUG_FLAG_NONE;
      }

#if ENGINE_DLL
      // Load the dll and import GetEngineFactoryD3D11() function
      auto GetEngineFactoryD3D11 = LoadGraphicsEngineD3D11();
#endif
      auto* pFactoryD3D11 = GetEngineFactoryD3D11();
      m_pEngineFactory = pFactoryD3D11;
      Uint32 NumAdapters = 0;
      pFactoryD3D11->EnumerateAdapters(EngineCI.MinimumFeatureLevel,
                                       NumAdapters, 0);
      std::vector<AdapterAttribs> Adapters(NumAdapters);
      if (NumAdapters > 0) {
        pFactoryD3D11->EnumerateAdapters(EngineCI.MinimumFeatureLevel,
                                         NumAdapters, Adapters.data());
      } else {
      }

      if (m_AdapterType == ADAPTER_TYPE_SOFTWARE) {
        for (Uint32 i = 0; i < Adapters.size(); ++i) {
          if (Adapters[i].AdapterType == m_AdapterType) {
            m_AdapterId = i;
            break;
          }
        }
      }

      m_AdapterAttribs = Adapters[m_AdapterId];
      if (m_AdapterType != ADAPTER_TYPE_SOFTWARE) {
        Uint32 NumDisplayModes = 0;
        pFactoryD3D11->EnumerateDisplayModes(
            EngineCI.MinimumFeatureLevel, m_AdapterId, 0,
            TEX_FORMAT_RGBA8_UNORM_SRGB, NumDisplayModes, nullptr);
        m_DisplayModes.resize(NumDisplayModes);
        pFactoryD3D11->EnumerateDisplayModes(
            EngineCI.MinimumFeatureLevel, m_AdapterId, 0,
            TEX_FORMAT_RGBA8_UNORM_SRGB, NumDisplayModes,
            m_DisplayModes.data());
      }

      EngineCI.AdapterId = m_AdapterId;
      ppContexts.resize(1 + EngineCI.NumDeferredContexts);
      pFactoryD3D11->CreateDeviceAndContextsD3D11(EngineCI, &m_pDevice,
                                                  ppContexts.data());

      if (pWindow != nullptr)
        pFactoryD3D11->CreateSwapChainD3D11(m_pDevice, ppContexts[0], SCDesc,
                                            FullScreenModeDesc{}, *pWindow,
                                            &m_pSwapChain);
    } break;
#endif

#if D3D12_SUPPORTED
    case RENDER_DEVICE_TYPE_D3D12: {
      EngineD3D12CreateInfo EngineCI;

      if (m_ValidationLevel >= 1) {
        EngineCI.EnableDebugLayer = true;
        if (m_ValidationLevel >= 2) EngineCI.EnableGPUBasedValidation = true;
      } else if (m_ValidationLevel == 0) {
        EngineCI.EnableDebugLayer = false;
      }

#if ENGINE_DLL
      // Load the dll and import GetEngineFactoryD3D12() function
      auto GetEngineFactoryD3D12 = LoadGraphicsEngineD3D12();
#endif
      auto* pFactoryD3D12 = GetEngineFactoryD3D12();
      if (!pFactoryD3D12->LoadD3D12()) {
      }

      m_pEngineFactory = pFactoryD3D12;
      Uint32 NumAdapters = 0;
      pFactoryD3D12->EnumerateAdapters(EngineCI.MinimumFeatureLevel,
                                       NumAdapters, 0);
      std::vector<AdapterAttribs> Adapters(NumAdapters);
      if (NumAdapters > 0) {
        pFactoryD3D12->EnumerateAdapters(EngineCI.MinimumFeatureLevel,
                                         NumAdapters, Adapters.data());
      } else {
#if D3D11_SUPPORTED
        m_DeviceType = RENDER_DEVICE_TYPE_D3D11;
        InitializeEngine(pWindow);
        return;
#endif
      }

      if (m_AdapterType == ADAPTER_TYPE_SOFTWARE) {
        for (Uint32 i = 0; i < Adapters.size(); ++i) {
          if (Adapters[i].AdapterType == m_AdapterType) {
            m_AdapterId = i;
            break;
          }
        }
      }

      m_AdapterAttribs = Adapters[m_AdapterId];
      if (m_AdapterType != ADAPTER_TYPE_SOFTWARE) {
        Uint32 NumDisplayModes = 0;
        pFactoryD3D12->EnumerateDisplayModes(
            EngineCI.MinimumFeatureLevel, m_AdapterId, 0,
            TEX_FORMAT_RGBA8_UNORM_SRGB, NumDisplayModes, nullptr);
        m_DisplayModes.resize(NumDisplayModes);
        pFactoryD3D12->EnumerateDisplayModes(
            EngineCI.MinimumFeatureLevel, m_AdapterId, 0,
            TEX_FORMAT_RGBA8_UNORM_SRGB, NumDisplayModes,
            m_DisplayModes.data());
      }

      EngineCI.AdapterId = m_AdapterId;
      ppContexts.resize(1 + EngineCI.NumDeferredContexts);
      pFactoryD3D12->CreateDeviceAndContextsD3D12(EngineCI, &m_pDevice,
                                                  ppContexts.data());

      if (!m_pSwapChain && pWindow != nullptr)
        pFactoryD3D12->CreateSwapChainD3D12(m_pDevice, ppContexts[0], SCDesc,
                                            FullScreenModeDesc{}, *pWindow,
                                            &m_pSwapChain);
    } break;
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
    case RENDER_DEVICE_TYPE_GL:
    case RENDER_DEVICE_TYPE_GLES: {
#if !PLATFORM_MACOS
      VERIFY_EXPR(pWindow != nullptr);
#endif
#if EXPLICITLY_LOAD_ENGINE_GL_DLL
      // Load the dll and import GetEngineFactoryOpenGL() function
      auto GetEngineFactoryOpenGL = LoadGraphicsEngineOpenGL();
#endif
      auto* pFactoryOpenGL = GetEngineFactoryOpenGL();
      m_pEngineFactory = pFactoryOpenGL;
      EngineGLCreateInfo CreationAttribs;
      CreationAttribs.Window = *pWindow;
      if (CreationAttribs.NumDeferredContexts != 0) {
        CreationAttribs.NumDeferredContexts = 0;
      }
      ppContexts.resize(1 + CreationAttribs.NumDeferredContexts);
      pFactoryOpenGL->CreateDeviceAndSwapChainGL(CreationAttribs, &m_pDevice,
                                                 ppContexts.data(), SCDesc,
                                                 &m_pSwapChain);
    } break;
#endif

#if VULKAN_SUPPORTED
    case RENDER_DEVICE_TYPE_VULKAN: {
#if EXPLICITLY_LOAD_ENGINE_VK_DLL
      // Load the dll and import GetEngineFactoryVk() function
      auto GetEngineFactoryVk = LoadGraphicsEngineVk();
#endif
      EngineVkCreateInfo EngVkAttribs;

      if (m_ValidationLevel >= 1) {
        EngVkAttribs.EnableValidation = true;
      } else if (m_ValidationLevel == 0) {
        EngVkAttribs.EnableValidation = false;
      }

      ppContexts.resize(1 + EngVkAttribs.NumDeferredContexts);
      auto* pFactoryVk = GetEngineFactoryVk();
      m_pEngineFactory = pFactoryVk;
      pFactoryVk->CreateDeviceAndContextsVk(EngVkAttribs, &m_pDevice,
                                            ppContexts.data());
      if (!m_pDevice) {
      }
      if (!m_pSwapChain && pWindow != nullptr)
        pFactoryVk->CreateSwapChainVk(m_pDevice, ppContexts[0], SCDesc,
                                      *pWindow, &m_pSwapChain);
    } break;
#endif

#if METAL_SUPPORTED
    case RENDER_DEVICE_TYPE_METAL: {
      EngineMtlCreateInfo MtlAttribs;

      ppContexts.resize(1 + MtlAttribs.NumDeferredContexts);
      auto* pFactoryMtl = GetEngineFactoryMtl();
      pFactoryMtl->CreateDeviceAndContextsMtl(MtlAttribs, &m_pDevice,
                                              ppContexts.data());

      if (!m_pSwapChain && pWindow != nullptr)
        pFactoryMtl->CreateSwapChainMtl(m_pDevice, ppContexts[0], SCDesc,
                                        *pWindow, &m_pSwapChain);
    } break;
#endif

    default:
      break;
  }

  m_pImmediateContext.Attach(ppContexts[0]);
  auto NumDeferredCtx = ppContexts.size() - 1;
  m_pDeferredContexts.resize(NumDeferredCtx);
  for (Uint32 ctx = 0; ctx < NumDeferredCtx; ++ctx)
    m_pDeferredContexts[ctx].Attach(ppContexts[1 + ctx]);
}

void InitializePipeline() {
  PipelineStateCreateInfo PSOCreateInfo;
  PipelineStateDesc& PSODesc = PSOCreateInfo.PSODesc;

  // Pipeline state name is used by the engine to report issues.
  // It is always a good idea to give objects descriptive names.
  PSODesc.Name = "Simple triangle PSO";

  // This is a graphics pipeline
  PSODesc.IsComputePipeline = false;

  // clang-format off
  // This tutorial will render to a single render target
  PSODesc.GraphicsPipeline.NumRenderTargets             = 1;
  // Set render target format which is the format of the swap chain's color buffer
  PSODesc.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
  // Use the depth buffer format from the swap chain
  PSODesc.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
  // Primitive topology defines what kind of primitives will be rendered by this pipeline state
  PSODesc.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  // No back face culling for this tutorial
  PSODesc.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
  // Disable depth testing
  PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
  // clang-format on

  ShaderCreateInfo ShaderCI;
  // Tell the system that the shader source code is in HLSL.
  // For OpenGL, the engine will convert this into GLSL under the hood.
  ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture
  // + g_Texture_sampler combination)
  ShaderCI.UseCombinedTextureSamplers = true;
  // Create a vertex shader
  RefCntAutoPtr<IShader> pVS;
  {
    ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
    ShaderCI.EntryPoint = "main";
    ShaderCI.Desc.Name = "Triangle vertex shader";
    ShaderCI.Source = VSSource;
    m_pDevice->CreateShader(ShaderCI, &pVS);
  }

  // Create a pixel shader
  RefCntAutoPtr<IShader> pPS;
  {
    ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
    ShaderCI.EntryPoint = "main";
    ShaderCI.Desc.Name = "Triangle pixel shader";
    ShaderCI.Source = PSSource;
    m_pDevice->CreateShader(ShaderCI, &pPS);
  }

  // Finally, create the pipeline state
  PSODesc.GraphicsPipeline.pVS = pVS;
  PSODesc.GraphicsPipeline.pPS = pPS;
  m_pDevice->CreatePipelineState(PSOCreateInfo, &m_pPSO);
}

void Render() {
  auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
  auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
  m_pImmediateContext->SetRenderTargets(
      1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Clear the back buffer
  const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
  // Let the engine perform required state transitions
  m_pImmediateContext->ClearRenderTarget(
      pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  m_pImmediateContext->ClearDepthStencil(
      pDSV, CLEAR_DEPTH_FLAG, 1.f, 0,
      RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Set pipeline state in the immediate context
  m_pImmediateContext->SetPipelineState(m_pPSO);
  // We need to commit shader resource. Even though in this example
  // we don't really have any resources, this call also sets the shaders
  m_pImmediateContext->CommitShaderResources(
      nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  DrawAttribs drawAttrs;
  drawAttrs.NumVertices = 3;  // We will render 3 vertices
  m_pImmediateContext->Draw(drawAttrs);
}

void Present() {
  if (!m_pSwapChain) {
    return;
  }
  m_pSwapChain->Present();
}

int main(void) {
  GLFWwindow* window;

  /* Initialize the library */
  if (!glfwInit()) {
    return -1;
  }

#if PLATFORM_WIN32
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#else
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

  /* Create a windowed mode window and its OpenGL context */
  window = glfwCreateWindow(640, 480, "GLFW CMake starter", NULL, NULL);

  if (!window) {
    glfwTerminate();
    return -1;
  }

  void* window_handle = NULL;
#if PLATFORM_WIN32
  window_handle = static_cast<void*>(glfwGetWin32Window(window));
#elif PLATFORM_MACOS
  glfwMakeContextCurrent(window);
  window_handle = static_cast<void*>(glfwGetCocoaWindow(window));
#elif PLATFORM_LINUX
  // FIXME: Get x11 or wayland window handle using glfw
  glfwMakeContextCurrent(window);
  throw std::runtime_error("Missing window handle");
#endif

  NativeWindow nw{window_handle};
  InitializeEngine(&nw);
  InitializePipeline();

  /* Loop until the user closes the window */
  while (!glfwWindowShouldClose(window)) {
    if (m_pImmediateContext) {
      Render();

#if PLATFORM_MACOS
      glfwSwapBuffers(window);
#else
      Present();
#endif
    }

    /* Poll for and process events */
    glfwPollEvents();
  }

  glfwTerminate();
  return 0;
}
