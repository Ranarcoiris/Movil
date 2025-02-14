#pragma once

#include "SampleBase.hpp"
#include "BasicMath.hpp"

namespace Diligent
{

class Tutorial03_Texturing final : public SampleBase
{
public:
    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;

    virtual const Char* GetSampleName() const override final { return "Tutorial03: Texturing"; }

private:
    void CreatePipelineState();
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void LoadTexture();

    RefCntAutoPtr<IPipelineState>         m_pPSO;
    RefCntAutoPtr<IBuffer>                m_CubeVertexBuffer;
    RefCntAutoPtr<IBuffer>                m_CubeIndexBuffer;
    RefCntAutoPtr<IBuffer>                m_VSConstants;
    RefCntAutoPtr<ITextureView>           m_TextureSRV;
    RefCntAutoPtr<IShaderResourceBinding> m_SRB;
    float4x4                              m_WorldViewProjMatrix;
    float4x4                              m_WorldViewProjMatrix1;
    float4x4                              m_WorldViewProjMatrix2;
    float4x4                              m_WorldViewProjMatrix3;
    float4x4                              m_WorldViewProjMatrix4;
    float4x4                              m_WorldViewProjMatrix5;
    float4x4                              m_WorldViewProjMatrix6;
    float4x4                              m_WorldViewProjMatrix7;
    float4x4                              m_WorldViewProjMatrix8;
    float4x4                              m_WorldViewProjMatrix9;
    float4x4                              m_WorldViewProjMatrix10;
    float4x4                              m_WorldViewProjMatrix11;
    float4x4                              m_WorldViewProjMatrix12;
    float4x4                              m_WorldViewProjMatrix13;
    float4x4                              m_WorldViewProjMatrix14;

};

} // namespace Diligent
