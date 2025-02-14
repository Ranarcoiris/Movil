#include "Tutorial03_Texturing.hpp"
#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "ColorConversion.h"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial03_Texturing();
}

void Tutorial03_Texturing::CreatePipelineState()
{
    // Pipeline state object encompasses configuration of all GPU stages

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSOCreateInfo.PSODesc.Name = "Cube PSO";

    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off
    // This tutorial will render to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // Cull back faces
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
    // Enable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    // clang-format on

    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.Desc.UseCombinedTextureSamplers = true;

    // Pack matrices in row-major order
    ShaderCI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    // Presentation engine always expects input in gamma space. Normally, pixel shader output is
    // converted from linear to gamma space by the GPU. However, some platforms (e.g. Android in GLES mode,
    // or Emscripten in WebGL mode) do not support gamma-correction. In this case the application
    // has to do the conversion manually.
    ShaderMacro Macros[] = {{"CONVERT_PS_OUTPUT_TO_GAMMA", m_ConvertPSOutputToGamma ? "1" : "0"}};
    ShaderCI.Macros      = {Macros, _countof(Macros)};

    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube VS";
        ShaderCI.FilePath        = "cube.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
        // Create dynamic uniform buffer that will store our transformation matrix
        // Dynamic buffers can be frequently updated by the CPU
        CreateUniformBuffer(m_pDevice, sizeof(float4x4), "VS constants CB", &m_VSConstants);
    }

    // Create a pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube PS";
        ShaderCI.FilePath        = "cube.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    // clang-format off
    // Define vertex shader input layout
    LayoutElement LayoutElems[] =
    {
        // Attribute 0 - vertex position
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        // Attribute 1 - texture coordinates
        LayoutElement{1, 0, 2, VT_FLOAT32, False}
    };
    // clang-format on

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);

    // Define variable type that will be used by default
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    // clang-format off
    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    ShaderResourceVariableDesc Vars[] = 
    {
        {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };
    // clang-format on
    PSOCreateInfo.PSODesc.ResourceLayout.Variables    = Vars;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    // clang-format off
    // Define immutable sampler for g_Texture. Immutable samplers should be used whenever possible
    SamplerDesc SamLinearClampDesc
    {
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, 
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
    };
    ImmutableSamplerDesc ImtblSamplers[] = 
    {
        {SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}
    };
    // clang-format on
    PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);

    // Since we did not explicitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables
    // never change and are bound directly through the pipeline state object.
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);

    // Since we are using mutable variable, we must create a shader resource binding object
    // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    m_pPSO->CreateShaderResourceBinding(&m_SRB, true);
}

void Tutorial03_Texturing::CreateVertexBuffer()
{
    // Layout of this structure matches the one we defined in the pipeline state
    struct Vertex
    {
        float3 pos;
        float2 uv;
    };

    // Cube vertices

    //      (-1,+1,+1)________________(+1,+1,+1)
    //               /|              /|
    //              / |             / |
    //             /  |            /  |
    //            /   |           /   |
    //(-1,-1,+1) /____|__________/(+1,-1,+1)
    //           |    |__________|____|
    //           |   /(-1,+1,-1) |    /(+1,+1,-1)
    //           |  /            |   /
    //           | /             |  /
    //           |/              | /
    //           /_______________|/
    //        (-1,-1,-1)       (+1,-1,-1)
    //

    // This time we have to duplicate verices because texture coordinates cannot
    // be shared
    constexpr Vertex CubeVerts[] =
        {
            {float3{-1, -1, -1}, float2{0, 1}},
            {float3{-1, +1, -1}, float2{0, 0}},
            {float3{+1, +1, -1}, float2{1, 0}},
            {float3{+1, -1, -1}, float2{1, 1}},

            {float3{-1, -1, -1}, float2{0, 1}},
            {float3{-1, -1, +1}, float2{0, 0}},
            {float3{+1, -1, +1}, float2{1, 0}},
            {float3{+1, -1, -1}, float2{1, 1}},

            {float3{+1, -1, -1}, float2{0, 1}},
            {float3{+1, -1, +1}, float2{1, 1}},
            {float3{+1, +1, +1}, float2{1, 0}},
            {float3{+1, +1, -1}, float2{0, 0}},

            {float3{+1, +1, -1}, float2{0, 1}},
            {float3{+1, +1, +1}, float2{0, 0}},
            {float3{-1, +1, +1}, float2{1, 0}},
            {float3{-1, +1, -1}, float2{1, 1}},

            {float3{-1, +1, -1}, float2{1, 0}},
            {float3{-1, +1, +1}, float2{0, 0}},
            {float3{-1, -1, +1}, float2{0, 1}},
            {float3{-1, -1, -1}, float2{1, 1}},

            {float3{-1, -1, +1}, float2{1, 1}},
            {float3{+1, -1, +1}, float2{0, 1}},
            {float3{+1, +1, +1}, float2{0, 0}},
            {float3{-1, +1, +1}, float2{1, 0}},
        };

    BufferDesc VertBuffDesc;
    VertBuffDesc.Name      = "Cube vertex buffer";
    VertBuffDesc.Usage     = USAGE_IMMUTABLE;
    VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    VertBuffDesc.Size      = sizeof(CubeVerts);
    BufferData VBData;
    VBData.pData    = CubeVerts;
    VBData.DataSize = sizeof(CubeVerts);
    m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_CubeVertexBuffer);
}

void Tutorial03_Texturing::CreateIndexBuffer()
{
    // clang-format off
    constexpr Uint32 Indices[] =
    {
        2,0,1,    2,3,0,
        4,6,5,    4,7,6,
        8,10,9,   8,11,10,
        12,14,13, 12,15,14,
        16,18,17, 16,19,18,
        20,21,22, 20,22,23
    };
    // clang-format on

    BufferDesc IndBuffDesc;
    IndBuffDesc.Name      = "Cube index buffer";
    IndBuffDesc.Usage     = USAGE_IMMUTABLE;
    IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
    IndBuffDesc.Size      = sizeof(Indices);
    BufferData IBData;
    IBData.pData    = Indices;
    IBData.DataSize = sizeof(Indices);
    m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_CubeIndexBuffer);
}

void Tutorial03_Texturing::LoadTexture()
{
    TextureLoadInfo loadInfo;
    loadInfo.IsSRGB = true;
    RefCntAutoPtr<ITexture> Tex;
    CreateTextureFromFile("DGLogo.png", loadInfo, m_pDevice, &Tex);
    // Get shader resource view from the texture
    m_TextureSRV = Tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    // Set texture SRV in the SRB
    m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);
}


void Tutorial03_Texturing::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    CreatePipelineState();
    CreateVertexBuffer();
    CreateIndexBuffer();
    LoadTexture();
}

// Render a frame
void Tutorial03_Texturing::Render()
{
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
    // Clear the back buffer
    float4 ClearColor = {0.350f, 0.350f, 0.350f, 1.0f};
    if (m_ConvertPSOutputToGamma)
    {
        // If manual gamma correction is required, we need to clear the render target with sRGB color
        ClearColor = LinearToSRGB(ClearColor);
    }
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Bind vertex and index buffers
    const Uint64 offset   = 0;
    IBuffer*     pBuffs[] = {m_CubeVertexBuffer};
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set the pipeline state
    m_pImmediateContext->SetPipelineState(m_pPSO);

    // Función auxiliar para dibujar un cubo
    auto DrawCube = [&](const float4x4& WorldViewProjMatrix) {
        // Map the buffer and write current world-view-projection matrix
        MapHelper<float4x4> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        *CBConstants = WorldViewProjMatrix;

        // Commit shader resources
        m_pImmediateContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Draw the cube
        DrawIndexedAttribs DrawAttrs;
        DrawAttrs.IndexType  = VT_UINT32;
        DrawAttrs.NumIndices = 36;
        DrawAttrs.Flags      = DRAW_FLAG_VERIFY_ALL;
        m_pImmediateContext->DrawIndexed(DrawAttrs);
    };

    // Dibujar los doce cubos
    DrawCube(m_WorldViewProjMatrix1);  // Cubo central
    DrawCube(m_WorldViewProjMatrix2);  // Cubo derecho
    DrawCube(m_WorldViewProjMatrix3);  // Cubo izquierdo
    DrawCube(m_WorldViewProjMatrix4);  // Cubo debajo del derecho
    DrawCube(m_WorldViewProjMatrix5);  // Cubo debajo del izquierdo
    DrawCube(m_WorldViewProjMatrix6);  // Cubo a la derecha del cubo 4 (órbita + rotación propia)
    DrawCube(m_WorldViewProjMatrix7);  // Cubo a la izquierda del cubo 4 (órbita + rotación propia)
    DrawCube(m_WorldViewProjMatrix8);  // Cubo arriba del cubo central
    DrawCube(m_WorldViewProjMatrix9);  // Cubo alargado (línea entre cubo central y cubo 8)
    DrawCube(m_WorldViewProjMatrix10); // Cubo alargado (línea entre cubo 2 y cubo 3)
    DrawCube(m_WorldViewProjMatrix11); // Cubo alargado (línea entre cubo 2 y cubo 4)
    DrawCube(m_WorldViewProjMatrix12); // Cubo alargado (línea entre cubo 3 y cubo 5)
}

void Tutorial03_Texturing::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    // Apply rotation to the central cube (Cube1)
    float4x4 Cube1ModelTransform = float4x4::RotationY(static_cast<float>(CurrTime) * 1.0f) * float4x4::RotationX(-PI_F * 0.1f);

    float4x4 Cube8ModelTransform = float4x4::RotationY(static_cast<float>(CurrTime) * -0.50f) * float4x4::RotationX(-PI_F * 0.1f);
    // Aplicar rotación en Y al cubo 4 y 8 sobre su propio eje
    float    rotationSpeed = 1.0f; // Velocidad de rotación
    float4x4 Cube4Rotation = float4x4::RotationY(static_cast<float>(CurrTime) * 1.0f);
    float4x4 Cube8Rotation = float4x4::RotationY(static_cast<float>(CurrTime) * 1.0f);


    // Cubos 2, 3, 4 y 5 usan la misma rotación que el cubo central
    float4x4 Cube2ModelTransform = Cube1ModelTransform; // Cubo derecho
    float4x4 Cube3ModelTransform = Cube1ModelTransform; // Cubo izquierdo     
    float4x4 Cube4ModelTransform = Cube1ModelTransform;// Cubo debajo del derecho
    float4x4 Cube5ModelTransform = Cube1ModelTransform; // Cubo debajo del izquierdo

    // Posicionar los cubos principales
    Cube2ModelTransform = float4x4::Translation(3.0f, 0.0f, 0.0f) * Cube2ModelTransform;   // Cubo derecho
    Cube3ModelTransform = float4x4::Translation(-3.0f, 0.0f, 0.0f) * Cube3ModelTransform;  // Cubo izquierdo
    Cube4ModelTransform = float4x4::Translation(3.0f, -3.0f, 0.0f) * Cube4ModelTransform;  // Cubo debajo del derecho
    Cube5ModelTransform = float4x4::Translation(-3.0f, -4.0f, 0.0f) * Cube5ModelTransform; // Cubo debajo del izquierdo

    // Cubo 8: Arriba del cubo central
                                             // Misma rotación que el cubo central
    Cube8ModelTransform = float4x4::Translation(0.0f, 5.0f, 0.0f) * Cube8ModelTransform; // Posicionar arriba del cubo central

    // Cubo alargado (línea entre el cubo central y el cubo 8)
    float4x4 CubeLineModelTransform = Cube1ModelTransform;                                              // Misma rotación que el cubo central
    CubeLineModelTransform          = float4x4::Scale(0.1f, 2.0f, 0.1f) * CubeLineModelTransform;       // Escalar para hacerlo delgado y alto
    CubeLineModelTransform          = float4x4::Translation(0.0f, 1.0f, 0.0f) * CubeLineModelTransform; // Posicionar entre el cubo central y el cubo 8

    // Cubo alargado (línea entre el cubo 2 y el cubo 3)
    float4x4 CubeLine23ModelTransform = Cube1ModelTransform;                                                // Misma rotación que el cubo central
    CubeLine23ModelTransform          = float4x4::Scale(4.0f, 0.1f, 0.1f) * CubeLine23ModelTransform;       // Escalar para hacerlo largo y delgado en el eje X
    CubeLine23ModelTransform          = float4x4::Translation(0.0f, 0.0f, 0.0f) * CubeLine23ModelTransform; // Posicionar entre el cubo 2 y el cubo 3

    // Cubo alargado (línea entre el cubo 2 y el cubo 4)
    float4x4 CubeLine24ModelTransform = Cube1ModelTransform;                                                  // Misma rotación que el cubo central
    CubeLine24ModelTransform          = float4x4::Scale(0.1f, 2.0f, 0.1f) * CubeLine24ModelTransform;         // Escalar para hacerlo delgado y alto
    CubeLine24ModelTransform          = float4x4::Translation(30.0f, -1.0f, 0.0f) * CubeLine24ModelTransform; // Posicionar entre el cubo 2 y el cubo 4

    // Cubo alargado (línea entre el cubo 3 y el cubo 5)
    float4x4 CubeLine35ModelTransform = Cube1ModelTransform;                                                   // Misma rotación que el cubo central
    CubeLine35ModelTransform          = float4x4::Scale(0.1f, 2.0f, 0.1f) * CubeLine35ModelTransform;          // Escalar para hacerlo delgado y alto
    CubeLine35ModelTransform          = float4x4::Translation(-30.0f, -1.0f, 0.0f) * CubeLine35ModelTransform; // Posicionar entre el cubo 3 y el cubo 5

    // Calcular la posición de los cubos 6 y 7 en relación con el cubo 4
    float orbitRadius = 1.0f;  // Radio de la órbita alrededor del cubo 4
    float orbitSpeed  = 0.3f; // Velocidad de la órbita

    // Rotación local sobre el propio eje Y de los cubos 6 y 7
    float    localRotationSpeed = 0.5f;                                                                   // Velocidad de rotación local
    float4x4 Cube6LocalRotation = float4x4::RotationY(static_cast<float>(CurrTime) * localRotationSpeed); // Rotación local del cubo 6
    float4x4 Cube7LocalRotation = float4x4::RotationY(static_cast<float>(CurrTime) * localRotationSpeed); // Rotación local del cubo 7

    // Cubo 6: Orbita a la derecha del cubo 4
    float4x4 Cube6OrbitTransform = float4x4::RotationY(static_cast<float>(CurrTime) * orbitSpeed) * float4x4::Translation(orbitRadius, 0.0f, 0.0f);
    float4x4 Cube6ModelTransform = Cube5ModelTransform * Cube6OrbitTransform * Cube6LocalRotation * Cube4ModelTransform; // Aplicar rotación local, órbita y rotación del cubo 1

    // Cubo 7: Orbita a la izquierda del cubo 4
    float4x4 Cube7OrbitTransform = float4x4::RotationY(static_cast<float>(CurrTime) * orbitSpeed + PI_F) * float4x4::Translation(orbitRadius, 0.0f, 0.0f);
    float4x4 Cube7ModelTransform =  Cube5ModelTransform * Cube7OrbitTransform * Cube7LocalRotation * Cube4ModelTransform; // Aplicar rotación local, órbita y rotación del cubo 1

    // Camera is at (0, 0, -5) looking along the Z axis
    float4x4 View = float4x4::Translation(0.f, 1.0f, 30.0f);

    // Get pretransform matrix that rotates the scene according the surface orientation
    auto SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    // Get projection matrix adjusted to the current screen orientation
    auto Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

    // Compute world-view-projection matrix for all cubes
    m_WorldViewProjMatrix1  = Cube1ModelTransform * View * SrfPreTransform * Proj;      // Cubo central
    m_WorldViewProjMatrix2  = Cube2ModelTransform * View * SrfPreTransform * Proj;      // Cubo derecho
    m_WorldViewProjMatrix3  = Cube3ModelTransform * View * SrfPreTransform * Proj;      // Cubo izquierdo
    m_WorldViewProjMatrix4  = Cube4ModelTransform * View * SrfPreTransform * Proj;      // Cubo debajo del derecho
    m_WorldViewProjMatrix5  = Cube5ModelTransform * View * SrfPreTransform * Proj;      // Cubo debajo del izquierdo
    m_WorldViewProjMatrix6  = Cube6ModelTransform * View * SrfPreTransform * Proj;      // Cubo a la derecha del cubo 4 (órbita + rotación propia)
    m_WorldViewProjMatrix7  = Cube7ModelTransform * View * SrfPreTransform * Proj;      // Cubo a la izquierda del cubo 4 (órbita + rotación propia)
    m_WorldViewProjMatrix8  = Cube8ModelTransform * View * SrfPreTransform * Proj;      // Cubo arriba del cubo central
    m_WorldViewProjMatrix9  = CubeLineModelTransform * View * SrfPreTransform * Proj;   // Cubo alargado (línea entre cubo central y cubo 8)
    m_WorldViewProjMatrix10 = CubeLine23ModelTransform * View * SrfPreTransform * Proj; // Cubo alargado (línea entre cubo 2 y cubo 3)
    m_WorldViewProjMatrix11 = CubeLine24ModelTransform * View * SrfPreTransform * Proj; // Cubo alargado (línea entre cubo 2 y cubo 4)
    m_WorldViewProjMatrix12 = CubeLine35ModelTransform * View * SrfPreTransform * Proj; // Cubo alargado (línea entre cubo 3 y cubo 5)
}

} // namespace Diligent
