///  Renderer.h
///  MetalCCP
///
///  Created by Guido Schneider on 23.07.22.
#pragma once
#ifndef Renderer_h
#define Renderer_h

#include <simd/simd.h>

#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>

#include "AAPLMesh.h"
#include "AAPLMathUtilities.h"
#include "AAPLCamera3DTypes.h"

using simd::float4;
using simd::float3;
using simd::float2;

static constexpr uint  PARTICLE_N = 100000;
static constexpr float SENSE_ANGLE = 0.3f;
static constexpr float SENSE_OFFSET = 50.f;
static constexpr float TURN_SPEED = 50;
static constexpr float MOVE_SPEED = 100.f;
static constexpr float EVAPORATION = 0.1f;
static constexpr float TRAIL_WEIGHT = 2.f;
static constexpr int   SENSOR_SIZE = 1;
static constexpr int   family = 1;
static constexpr int16_t kMaxFramesInFlight = 3;
static constexpr int32_t kTextureWidth = 2048;
static constexpr int32_t kTextureHeight = 2048;
static constexpr int32_t kFrameRate  = 60;
static constexpr uint32_t NumPointVertices = 7;
static constexpr uint32_t NumLights = 15;
static const struct CameraData cdata = CameraData();

class Renderer
{
public:
    explicit Renderer( MTK::View& pView );
    virtual ~Renderer();
    
    void buildShadowPipeline();
    void buildGBufferPipeline();
    void buildDirectionalLightPipeline();
    void buildPointLightMaskPipeline();
    void buildPointLightPipeline();
    void buildPointsPipeline();
    void buildGroundPipeline();
    void buildSkyPipeline();
    void buildComputePipeline();
    void generateComputedTexture( MTL::CommandBuffer* pCommandBuffer, MTL::Buffer* pUniformsBuffer );
    
    void buildDepthStencilStates();
    void buildTextures();
    void buildRenderPasses();
    
    void buildBuffers();
    void buildParticleBuffer();
    void buildLightsBuffer();
    
    void updateLights(const simd::float4x4 & modelViewMatrix);
    
    void drawShadow(MTL::CommandBuffer * pCommandBuffer, MTL::Buffer * pFrameDataBuffer, MTL::Buffer * pInstanceDataBuffer);
    void drawPointLights(MTL::RenderCommandEncoder * pEncoder, MTL::Buffer * pFrameDataBuffer);
    void drawPointLightsCommon(MTL::RenderCommandEncoder * pEncoder, MTL::Buffer * pFrameDataBuffer);
    void drawPointLightMask(MTL::RenderCommandEncoder * pEncoder, MTL::Buffer * pFrameDataBuffer);
    void drawPoints(MTL::RenderCommandEncoder* pRenderEncoder,  MTL::Buffer * pFrameDataBuffer);
    void drawInView( MTK::View * pView, MTL::Drawable* pCurrentDrawable, MTL::Texture* pDepthStencilTexture );
    void drawableSizeWillChange( const MTL::Size & size);
    auto currentDrawableTexture( MTL::Drawable* pCurrentDrawable ) -> MTL::Texture *;
    void step_animation();
    
    void updateDebugOutput();
    auto device() -> MTL::Device *;
    auto colorTargetPixelFormat() -> MTL::PixelFormat ;
    auto depthStencilTargetPixelFormat() -> MTL::PixelFormat;
    auto shadowMap() -> MTL::Texture* const;
    void setColorTargetPixelFormat( const MTL::PixelFormat & format);
    void setDepthStencilTargetPixelFormat( const MTL::PixelFormat & format);
    auto sampleCount() -> NS::UInteger & ;
    void setSampleCount( const NS::UInteger & count);
    
    void setInstances(const size_t& rows , const size_t& depth, const size_t& columns);
    const std::array <size_t, 3> instances();
    const size_t& numberOfInstances();
    void setInstancesSize(const float& size);
    void setGroupScale (const float& scale);
    const float& getGroupScale() { return _objScale;}
    const float& instancesSize() { return _instSize; }
    void setCursorPosition( const simd::float2 &newPostion );
    void setMouseButtonMask( const NS::UInteger& newButtonMask );
    void setTextureScale ( const float& scale );
    const float& textureScale() {return  _textureScale; }
    
    void setCameraData ( const struct CameraData & newCameraData);
    inline auto cameraData() -> const struct CameraData &;
    
    void setShadowCameraData ( const struct CameraData & newShadowCameraData);
    inline auto shadowCameraData() -> const struct CameraData &;
    
    const float& metallTextureValue() { return _metallTextureValue;}
    void setMetallTextureValue( const float & value);
    const float& roughnessTextureValue() { return _roughnessTextureValue;}
    void setRoughnessTextureValue( const float & value);
    const float& baseColorMixValue() { return _baseColorMixValue;}
    void setBaseColorMixValue( const float & value);
    auto primitiveType()-> MTL::PrimitiveType;
    void setPrimitiveType(const MTL::PrimitiveType &primitiveType);
    void setStepPerFrameValue( const float & value);
    const float& stepPerFrameValue(){ return _stepPerFrame;}
    auto get_num_families() {return num_families;}
    auto get_num_sources() {return num_sources;}
    void setNumFamilies( const int & value);
    void setSenseAngleValue( const float & value);
    const float& senseAngleValue() { return _senseAngleValue;}
    void setTurnSpeedValue( const float & value);
    const float& turnSpeedValue() { return _turnSpeedValue;}
    void setSenseOffsetValue( const float & value);
    const float& senseOffsetValue() { return _senseOffsetValue;}
    void setEvaporationValue( const float & value);
    const float& evaporationValue( ) { return _evaporationValue;}
    void setTrailWeightValue( const float & value);
    const float& trailWeightValue() { return _trailWeightValue;}
    virtual void cleanup();
    
private:
    
    MTL::Device* _pDevice;
    MTL::PrimitiveType _primitiveType;
    MTL::CommandQueue* _pCommandQueue;
    MTL::Library* _pShaderLibrary;
    
    MTL::PixelFormat _colorTargetPixelFormat;
    MTL::PixelFormat _depthStencilTargetPixelFormat;
    
    /// GBuffer pixel formats
    MTL::PixelFormat _albedo_specular_GBufferFormat;
    MTL::PixelFormat _normal_shadow_GBufferFormat;
    MTL::PixelFormat _depth_GBufferFormat;
    
    /// Render pipeline states
    MTL::RenderPipelineState* _pGBufferPipelineState;
    MTL::RenderPipelineState* _pSkyboxPipelineState;
    MTL::RenderPipelineState* _pShadowPipelineState;
    MTL::RenderPipelineState* _pGroundShadowPipelineState;
    MTL::RenderPipelineState* _pDirectLightPipelineState;
    MTL::RenderPipelineState* _pGroundPipelineState;
    MTL::RenderPipelineState* _pLightPipelineState;
    MTL::RenderPipelineState* _pLightMaskPipelineState;
    MTL::RenderPipelineState* _pPointPipelineState;
    
    /// Compute pipeline states
    MTL::ComputePipelineState* _pInitComputePSO;
    MTL::ComputePipelineState* _pComputePSO;
    MTL::ComputePipelineState* _pTrailComputePSO;
    MTL::ComputePipelineState* _pInteractionsComputePSO;
    MTL::ComputePipelineState* _pUpdateFamilyComputePSO;
    
    /// Vertex descriptor
    MTL::VertexDescriptor* _pSkyVertexDescriptor;
    MTL::VertexDescriptor* _pVertexDescriptor;
    
    /// Depth stencil states
    MTL::DepthStencilState* _pGBufferDepthStencilState;
    MTL::DepthStencilState* _pDontWriteDepthStencilState;
    MTL::DepthStencilState* _pShadowDepthStencilState;
    MTL::DepthStencilState* _pDirectionLightDepthStencilState;
    MTL::DepthStencilState* _pLightMaskDepthStencilState;
    MTL::DepthStencilState* _pPointLightDepthStencilState;
    
    /// Render pass descriptors
    MTL::RenderPassDescriptor* _pShadowRenderPassDescriptor;
    MTL::RenderPassDescriptor* _pGBufferRenderPassDescriptor;
    MTL::RenderPassDescriptor* _pFinalRenderPassDescriptor;
    
    /// Mesh Objects
    Mesh _skyMesh;
    Mesh _icosahedronMesh;
    
    MTL::Texture* _pTexture;
    MTL::Texture* _pMaterialTexture[5];
    MTL::Texture* _pSkyMap;
    MTL::Texture* _pIrradianceMap;
    MTL::Texture* _pPreFilterMap;
    MTL::Texture* _pBDRFMap;
    MTL::Texture* _pShadowMap;
    MTL::Texture*  pDrawableTexture;
    MTL::Texture* _pPointMap;
    
    /// GBuffer textures
    MTL::Texture* _albedo_specular_GBuffer;
    MTL::Texture* _normal_shadow_GBuffer;
    MTL::Texture* _depth_GBuffer;
    
    MTL::Buffer* _pVertexDataBuffer;
    MTL::Buffer* _pInstanceDataBuffer[kMaxFramesInFlight];
    MTL::Buffer* _pFrameDataBuffer[kMaxFramesInFlight];
    MTL::Buffer* _pIndexBuffer;
    MTL::Buffer* _pParticleBuffer;
    MTL::Buffer* _pUniformsBuffer[kMaxFramesInFlight];
    MTL::Buffer* _pLightsDataBuffer;
    MTL::Buffer* _pLightPositionsBuffer[kMaxFramesInFlight];
    MTL::Buffer* _pTimeBuffer;
    MTL::Buffer* _pInterActionBuffer;
    MTL::Buffer* _pQuadVertexBuffer;
    MTL::Buffer* _pGroundVertexBuffer;
    MTL::Buffer* _pShadowBuffer;
    MTL::Buffer* _pPointVertexBuffer;
    
    float4* _original_light_positions;
    float   _aspect;
    float   _angle;
    size_t  _frame;
    size_t  _frameRate;
    size_t _frameNumber;
    float   _instSize;
    float   _objScale;
    float    yRotate;
    float   _textureScale;
    float   _stepPerFrame;
    int     num_families;
    int     num_sources;
    uint num_particles;
    int updatePass;
    NS::UInteger _sampleCount;
    CFAbsoluteTime _previousTime;
    float _metallTextureValue;
    float _roughnessTextureValue;
    float _baseColorMixValue;
    float _senseAngleValue;
    float _turnSpeedValue;
    float _senseOffsetValue;
    float _evaporationValue;
    float _trailWeightValue;
    float t_transformation {0.0f};
    float t_rotation {0.0f};
    int   dir = 1;
    bool  initCompute = false;
    
    NS::UInteger _mouseButtonMask;
    simd_float2 _cursorPosition;
    CGSize _pSize;
    dispatch_semaphore_t _semaphore;
    uint _animationIndex;
    
    CGSize _viewSize;
    size_t _kNumInstances;
    size_t _indexCount;
    size_t _indexQuadCount;
    size_t _indexGroundCount;
    
    std::array <size_t, 3> _instanceArray;
    simd::float4x4 _projectionMatrix;
    simd::float4x4 _shadowProjectionMatrix;
    simd::float4x4 _shadowViewMatrix;
    simd::float4x4 _modelMatrix;
    simd::float4x4 _lightModelMatrix;
    simd::float4x4 _cameraViewMatrix;
    simd::float4x4 _skyModel;
    
    struct CameraData _cameraData;
    struct CameraData _shadowCameraData;
};

inline auto Renderer::device() -> MTL::Device *
{
    return _pDevice;
}

inline auto Renderer::currentDrawableTexture( MTL::Drawable* pCurrentDrawable ) -> MTL::Texture *
{
    if(pCurrentDrawable)
    {
        auto pMtlDrawable = static_cast< CA::MetalDrawable* >(pCurrentDrawable);
        return pMtlDrawable->texture();
    }else{
        return nullptr;
    }
}

inline  auto Renderer::colorTargetPixelFormat() ->  MTL::PixelFormat
{
    return Renderer::_colorTargetPixelFormat;
}

inline auto Renderer::depthStencilTargetPixelFormat() ->  MTL::PixelFormat
{
    return Renderer::_depthStencilTargetPixelFormat;
}

inline auto Renderer::sampleCount() -> NS::UInteger &
{
    return Renderer::_sampleCount;
}

inline auto Renderer::primitiveType() -> MTL::PrimitiveType
{
    return Renderer::_primitiveType;
}

inline auto Renderer::shadowMap() ->MTL::Texture* const
{
    return Renderer::_pShadowMap;
}

inline auto Renderer::cameraData() -> const struct CameraData &
{
    return Renderer::_cameraData;
}

inline auto Renderer::shadowCameraData() -> const struct CameraData &
{
    return Renderer::_shadowCameraData;
}
#endif /* Renderer_h */
