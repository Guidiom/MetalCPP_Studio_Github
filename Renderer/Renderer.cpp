///
/// Renderer.cpp
/// MetalCCP
///
/// Created by Guido Schneider on 23.07.22.
///

#if __has_feature(objc_arc)
#error This file must be compiled with -fno-objc-arc
#endif

#define METALCPP_SYMBOL_VISIBILITY_HIDDEN

#define ROWS    1
#define COLUMNS 1
#define DEPTH   1

#define TRANSFORMATION_SPEED 0.0007
#define ROTATION_SPEED 0.004

#import <stdlib.h>
#import <cassert>

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#import <Foundation/Foundation.hpp>
#import <Metal/Metal.hpp>
#import <MetalKit/MetalKit.hpp>

#include <simd/simd.h>

#undef NS_PRIVATE_IMPLEMENTATION
#undef MTL_PRIVATE_IMPLEMENTATION
#undef CA_PRIVATE_IMPLEMENTATION
#undef MTK_PRIVATE_IMPLEMENTATION


#include "AAPLUtilities.h"
#include "AAPLMathUtilities.h"
#import  "AAPLShaderTypes.h"
#include "Renderer.h"

Renderer::Renderer(MTK::View &pView )
: _pDevice( pView.device())
, _pShaderLibrary ( _pDevice->newDefaultLibrary())
, _colorTargetPixelFormat (pView.colorPixelFormat())
, _depthStencilTargetPixelFormat(pView.depthStencilPixelFormat())
, _albedo_specular_GBufferFormat(MTL::PixelFormatRGBA8Unorm_sRGB)
, _normal_shadow_GBufferFormat(MTL::PixelFormatRGBA8Snorm)
, _depth_GBufferFormat(MTL::PixelFormatR32Float)
, _sampleCount(pView.sampleCount())
, _aspect (1.f)
, _angle (0.f)
, _frame (0)
, _frameNumber(0)
, _mouseButtonMask(0)
, _cursorPosition{0, 0}
, _animationIndex(0)
, _instanceArray { size_t( ROWS ), size_t( COLUMNS ), size_t( DEPTH ) }
, _kNumInstances(numberOfInstances())
, _indexCount (0)
, _instSize(1.f)
, _objScale(1.f)
, _textureScale(1.f)
, _stepPerFrame(MOVE_SPEED)
, _baseColorMixValue(0.5f)
, _metallTextureValue(1.f)
, _roughnessTextureValue(0.f)
, _trailWeightValue(TRAIL_WEIGHT)
,  num_families(family)
,  updatePass(0)
,  num_particles(NS::UInteger(PARTICLE_N))
, _senseAngleValue(SENSE_ANGLE)
, _turnSpeedValue(TURN_SPEED)
, _senseOffsetValue(SENSE_OFFSET)
, _evaporationValue(EVAPORATION)
{
    _pCommandQueue = _pDevice->newCommandQueue();
    buildShadowPipeline();
    buildGBufferPipeline();
    buildGroundPipeline();
    buildComputePipeline();
    buildDirectionalLightPipeline();
    buildPointLightMaskPipeline();
    buildPointLightPipeline();
    buildSkyPipeline();
    buildPointsPipeline();
    buildDepthStencilStates();
    buildTextures();
    buildRenderPasses();
    buildBuffers();
    buildParticleBuffer();
    buildLightsBuffer();
    _semaphore = dispatch_semaphore_create( kMaxFramesInFlight );
}

Renderer::~Renderer()
{
    cleanup();
}

void Renderer::buildShadowPipeline()
{
/// Shadow pass render pipeline setup
    MTL::PixelFormat shadowMapPixelFormat = MTL::PixelFormatDepth16Unorm;

    NS::Error *pError = nullptr;

    MTL::Function* pShadowVertexFn = _pShaderLibrary->newFunction( AAPLSTR( "shadow_vertex" ) );
    AAPL_ASSERT( pShadowVertexFn, "Failed to load shadow vertex");
    
    MTL::RenderPipelineDescriptor* pRenderPipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    
    pRenderPipelineDescriptor->setLabel( AAPLSTR( "Shadow Pipeline Object Descriptor " ));
    
    pRenderPipelineDescriptor->setVertexDescriptor(nullptr);
    pRenderPipelineDescriptor->setVertexFunction( pShadowVertexFn );
    pRenderPipelineDescriptor->setFragmentFunction( nullptr );
    pRenderPipelineDescriptor->setDepthAttachmentPixelFormat( shadowMapPixelFormat );
    pRenderPipelineDescriptor->colorAttachments()->object(RenderTargetLighting)->setPixelFormat(MTL::PixelFormatInvalid);
    pRenderPipelineDescriptor->setRasterizationEnabled(true);
    pRenderPipelineDescriptor->setRasterSampleCount(NS::UInteger(1));
    
    _pShadowPipelineState = _pDevice->newRenderPipelineState( pRenderPipelineDescriptor, &pError );
    AAPL_ASSERT_NULL_ERROR( pError, "Failed to create  shadow map render pipeline state for Objects:");
    pRenderPipelineDescriptor->release();
    pShadowVertexFn->release();
   }

void Renderer::buildGBufferPipeline()
{
    NS::Error* pError = nullptr;

    // MTL::PixelFormat colorPixelFormat = colorTargetPixelFormat();
    MTL::PixelFormat depthStencilPixelFormat = depthStencilTargetPixelFormat();

    /// objects with lights starts here
    _pVertexDescriptor = MTL::VertexDescriptor::alloc()->init();
    _pVertexDescriptor->attributes()->object(VertexAttributePosition)->setFormat( MTL::VertexFormatFloat3);
    _pVertexDescriptor->attributes()->object(VertexAttributePosition)->setOffset( 0 );
    _pVertexDescriptor->attributes()->object(VertexAttributePosition)->setBufferIndex( BufferIndexVertexData );
    
    _pVertexDescriptor->attributes()->object(VertexAttributeTexcoord)->setFormat( MTL::VertexFormatFloat2 );
    _pVertexDescriptor->attributes()->object(VertexAttributeTexcoord)->setOffset( 12 );
    _pVertexDescriptor->attributes()->object(VertexAttributeTexcoord)->setBufferIndex( BufferIndexVertexData );
 
    _pVertexDescriptor->attributes()->object(VertexAttributeNormal)->setFormat( MTL::VertexFormatFloat3 );
    _pVertexDescriptor->attributes()->object(VertexAttributeNormal)->setOffset( 20 );
    _pVertexDescriptor->attributes()->object(VertexAttributeNormal)->setBufferIndex( BufferIndexVertexData );
    _pVertexDescriptor->layouts()->object(BufferIndexVertexData)->setStride( 32 );
    
    MTL::Function* pGBufferVertexFunction = _pShaderLibrary->newFunction( AAPLSTR( "vertexMain" ) );
    MTL::Function* pGBufferFragmentFunction = _pShaderLibrary->newFunction( AAPLSTR( "fragmentMain" ) );
    
    AAPL_ASSERT( pGBufferVertexFunction, "Failed to load gbuffer_vertex shader" );
    AAPL_ASSERT( pGBufferFragmentFunction, "Failed to load gbuffer_fragment shader" );

    MTL::RenderPipelineDescriptor* pRenderPipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    pRenderPipelineDescriptor->setLabel( AAPLSTR( "GBuffer Pipeline Descriptor" ) );
    pRenderPipelineDescriptor->setVertexDescriptor(_pVertexDescriptor);
    pRenderPipelineDescriptor->setVertexFunction( pGBufferVertexFunction );
    pRenderPipelineDescriptor->setFragmentFunction( pGBufferFragmentFunction );
    pRenderPipelineDescriptor->colorAttachments()->object(RenderTargetLighting)->setPixelFormat( MTL::PixelFormatInvalid );
    pRenderPipelineDescriptor->colorAttachments()->object(RenderTargetAlbedo)->setPixelFormat( _albedo_specular_GBufferFormat );
    pRenderPipelineDescriptor->colorAttachments()->object(RenderTargetNormal)->setPixelFormat( _normal_shadow_GBufferFormat );
    pRenderPipelineDescriptor->colorAttachments()->object(RenderTargetDepth)->setPixelFormat( _depth_GBufferFormat );
    pRenderPipelineDescriptor->setDepthAttachmentPixelFormat( depthStencilPixelFormat );
    pRenderPipelineDescriptor->setStencilAttachmentPixelFormat( depthStencilPixelFormat );

    _pGBufferPipelineState = _pDevice->newRenderPipelineState( pRenderPipelineDescriptor, &pError );
    AAPL_ASSERT_NULL_ERROR( pError, "Failed to create GBuffer render pipeline state" );

    _pVertexDescriptor->release();
    pGBufferVertexFunction->release();
    pGBufferFragmentFunction->release();
    pRenderPipelineDescriptor->release();
}

void Renderer::buildGroundPipeline()
{
    //MTL::PixelFormat colorPixelFormat = colorTargetPixelFormat();
    MTL::PixelFormat depthStencilPixelFormat = depthStencilTargetPixelFormat();
    
    NS::Error* pError = nullptr;
    
    _pVertexDescriptor = MTL::VertexDescriptor::alloc()->init();
    _pVertexDescriptor->attributes()->object(VertexAttributePosition)->setFormat( MTL::VertexFormatFloat3);
    _pVertexDescriptor->attributes()->object(VertexAttributePosition)->setBufferIndex( BufferIndexGroundVertexData );
    _pVertexDescriptor->attributes()->object(VertexAttributePosition)->setOffset( 0 );
    
    _pVertexDescriptor->attributes()->object(VertexAttributeNormal)->setFormat( MTL::VertexFormatFloat3 );
    _pVertexDescriptor->attributes()->object(VertexAttributeNormal)->setBufferIndex( BufferIndexGroundVertexData );
    _pVertexDescriptor->attributes()->object(VertexAttributeNormal)->setOffset( 12 );

    _pVertexDescriptor->layouts()->object(BufferIndexGroundVertexData)->setStride( 24 );
    
    MTL::Function* pGroundVertexFunction = _pShaderLibrary->newFunction( AAPLSTR( "ground_vertex" ) );
    AAPL_ASSERT( pGroundVertexFunction, "Failed to load ground_vertex shader" );
    
    MTL::Function* pGroundFragmentFunction = _pShaderLibrary->newFunction( AAPLSTR( "ground_fragment" ) );
    AAPL_ASSERT( pGroundFragmentFunction, "Failed to load ground_fragment shader" );
    
    MTL::RenderPipelineDescriptor* pRenderPipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    pRenderPipelineDescriptor->setLabel( AAPLSTR( "GBuffer Ground Pipeline" ) );
    pRenderPipelineDescriptor->setVertexDescriptor( nullptr );
    pRenderPipelineDescriptor->setVertexFunction( pGroundVertexFunction );
    pRenderPipelineDescriptor->setFragmentFunction( pGroundFragmentFunction );
    pRenderPipelineDescriptor->colorAttachments()->object(RenderTargetLighting)->setPixelFormat( MTL::PixelFormatInvalid);
    pRenderPipelineDescriptor->colorAttachments()->object(RenderTargetAlbedo)->setPixelFormat( _albedo_specular_GBufferFormat );
    pRenderPipelineDescriptor->colorAttachments()->object(RenderTargetNormal)->setPixelFormat( _normal_shadow_GBufferFormat );
    pRenderPipelineDescriptor->colorAttachments()->object(RenderTargetDepth)->setPixelFormat( _depth_GBufferFormat );
    pRenderPipelineDescriptor->setDepthAttachmentPixelFormat( depthStencilPixelFormat );
    pRenderPipelineDescriptor->setStencilAttachmentPixelFormat( depthStencilPixelFormat );

    _pGroundPipelineState = _pDevice->newRenderPipelineState(pRenderPipelineDescriptor, &pError);
    AAPL_ASSERT_NULL_ERROR( pError, "Failed to create ground render pipeline state:" );
    
    _pVertexDescriptor->release();
    pRenderPipelineDescriptor->release();
    pGroundVertexFunction->release();
    pGroundFragmentFunction->release();
}
void Renderer::buildComputePipeline()
{
    NS::Error* pError = nullptr;
    
    MTL::Function* pInitComputeFn = _pShaderLibrary->newFunction( AAPLSTR( "init_function" ) );
    MTL::Function* pComputeFn = _pShaderLibrary->newFunction( AAPLSTR( "compute_function" ) );
    MTL::Function* pTrailFn = _pShaderLibrary->newFunction( AAPLSTR( "trail_function" ));
    MTL::Function* pUpdateFamilyFn = _pShaderLibrary->newFunction( AAPLSTR( "update_family_function" ));
    //MTL::Function* pInteractionsFn = _pShaderLibrary->newFunction( AAPLSTR( "interactions_function" ));
    
    AAPL_ASSERT( pInitComputeFn, "init_function failed to load!");
    AAPL_ASSERT( pComputeFn, "compute_function failed to load!");
    AAPL_ASSERT( pTrailFn, "trail_function failed to load!");
    AAPL_ASSERT( pUpdateFamilyFn, "update_family_function failed to load!");
   // AAPL_ASSERT( pInteractionsFn, "interactions_function failed to load!");
    
    _pInitComputePSO = _pDevice->newComputePipelineState( pInitComputeFn, &pError );
    AAPL_ASSERT_NULL_ERROR(pError, "Failed to create init pipeline state ");
    _pComputePSO = _pDevice->newComputePipelineState( pComputeFn, &pError );
    AAPL_ASSERT_NULL_ERROR(pError, "Failed to create compute pipeline state ");
    _pTrailComputePSO = _pDevice->newComputePipelineState(pTrailFn ,&pError);
    AAPL_ASSERT_NULL_ERROR(pError , "Failed to create trail pipeline state ");
    _pUpdateFamilyComputePSO =_pDevice->newComputePipelineState(pUpdateFamilyFn ,&pError);
    AAPL_ASSERT_NULL_ERROR(pError , "Failed to create UpdateFamily pipeline state ");
    //_pInteractionsComputePSO = _pDevice->newComputePipelineState(pInteractionsFn ,&pError);
    AAPL_ASSERT_NULL_ERROR(pError , "Failed to create interactions pipeline state ");
    
    //pInteractionsFn->release();
    pUpdateFamilyFn->release();
    pTrailFn->release();
    pComputeFn->release();
    pInitComputeFn->release();
}

void Renderer::buildDirectionalLightPipeline()
{
    NS::Error *pError = nullptr;

    MTL::PixelFormat colorPixelFormat = colorTargetPixelFormat();
    MTL::PixelFormat depthStencilPixelFormat = depthStencilTargetPixelFormat();

    MTL::Function* pDirectionalVertexFunction = _pShaderLibrary->newFunction( AAPLSTR( "lighting_vertex" ) );
    AAPL_ASSERT( pDirectionalVertexFunction, "Failed to load lighting_vertex shader" );
    MTL::Function* pDirectionalFragmentFunction = _pShaderLibrary->newFunction( AAPLSTR( "lighting_fragment" ) );
    AAPL_ASSERT( pDirectionalFragmentFunction, "Failed to load lighting_fragment shader" );

    MTL::RenderPipelineDescriptor* pRenderPipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    pRenderPipelineDescriptor->setLabel( AAPLSTR( "Deferred Directional Lighting" ) );
    pRenderPipelineDescriptor->setVertexDescriptor( nullptr );
    pRenderPipelineDescriptor->setVertexFunction( pDirectionalVertexFunction );
    pRenderPipelineDescriptor->setFragmentFunction( pDirectionalFragmentFunction );
    pRenderPipelineDescriptor->colorAttachments()->object(RenderTargetLighting)->setPixelFormat( colorPixelFormat );
   
    pRenderPipelineDescriptor->setDepthAttachmentPixelFormat( depthStencilPixelFormat );
    pRenderPipelineDescriptor->setStencilAttachmentPixelFormat( depthStencilPixelFormat );
    _pDirectLightPipelineState = _pDevice->newRenderPipelineState(pRenderPipelineDescriptor, &pError);
    AAPL_ASSERT_NULL_ERROR( pError, "Failed to create directional light render pipeline state:" );

    pRenderPipelineDescriptor->release();
    pDirectionalVertexFunction->release();
    pDirectionalFragmentFunction->release();
}


void Renderer::buildPointLightMaskPipeline()
{
    NS::Error* pError = nullptr;
    
    MTL::PixelFormat colorPixelFormat = colorTargetPixelFormat();
    MTL::PixelFormat depthStencilPixelFormat = depthStencilTargetPixelFormat();
    
    MTL::Function* pLightMaskVertex = _pShaderLibrary->newFunction( AAPLSTR( "light_mask_vertex" ) );
    AAPL_ASSERT( pLightMaskVertex, "Failed to load light_mask_vertex shader" );

    MTL::RenderPipelineDescriptor* pRenderPipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    pRenderPipelineDescriptor->setLabel( AAPLSTR( "Point Light Mask" ) );
    pRenderPipelineDescriptor->setVertexDescriptor( nullptr );
    pRenderPipelineDescriptor->setVertexFunction( pLightMaskVertex );
    pRenderPipelineDescriptor->setFragmentFunction( nullptr );
    pRenderPipelineDescriptor->colorAttachments()->object(RenderTargetLighting)->setPixelFormat( colorPixelFormat );
    pRenderPipelineDescriptor->setDepthAttachmentPixelFormat( depthStencilPixelFormat );
    pRenderPipelineDescriptor->setStencilAttachmentPixelFormat( depthStencilPixelFormat );

    _pLightMaskPipelineState = _pDevice->newRenderPipelineState( pRenderPipelineDescriptor, &pError );
    AAPL_ASSERT_NULL_ERROR( pError, "Failed to create directional light mask pipeline state:" );
    
    pRenderPipelineDescriptor->release();
    pLightMaskVertex->release();
    
}

void Renderer::buildPointLightPipeline(){
    
    NS::Error* pError = nullptr;
    
    MTL::RenderPipelineDescriptor* pRenderPipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    pRenderPipelineDescriptor->setLabel( AAPLSTR( "Light" ) );
    pRenderPipelineDescriptor->setVertexDescriptor( nullptr );
    
    pRenderPipelineDescriptor->colorAttachments()->object(RenderTargetLighting)->setPixelFormat( colorTargetPixelFormat() );
    
    // Enable additive blending
    pRenderPipelineDescriptor->colorAttachments()->object(RenderTargetLighting)->setBlendingEnabled( true );
    pRenderPipelineDescriptor->colorAttachments()->object(RenderTargetLighting)->setRgbBlendOperation( MTL::BlendOperationAdd );
    pRenderPipelineDescriptor->colorAttachments()->object(RenderTargetLighting)->setAlphaBlendOperation( MTL::BlendOperationAdd );
    pRenderPipelineDescriptor->colorAttachments()->object(RenderTargetLighting)->setDestinationRGBBlendFactor( MTL::BlendFactorOne );
    pRenderPipelineDescriptor->colorAttachments()->object(RenderTargetLighting)->setDestinationAlphaBlendFactor( MTL::BlendFactorOne );
    pRenderPipelineDescriptor->colorAttachments()->object(RenderTargetLighting)->setSourceRGBBlendFactor( MTL::BlendFactorOne );
    pRenderPipelineDescriptor->colorAttachments()->object(RenderTargetLighting)->setSourceAlphaBlendFactor( MTL::BlendFactorOne );
    
    pRenderPipelineDescriptor->setDepthAttachmentPixelFormat( depthStencilTargetPixelFormat() );
    pRenderPipelineDescriptor->setStencilAttachmentPixelFormat( depthStencilTargetPixelFormat() );
    
    MTL::Function* pLightVertexFunction = _pShaderLibrary->newFunction( AAPLSTR( "deferred_point_lighting_vertex" ) );
    MTL::Function* pLightFragmentFunction = _pShaderLibrary->newFunction( AAPLSTR( "deferred_point_lighting_fragment_traditional" ) );
    
    AAPL_ASSERT( pLightVertexFunction, "Failed to load deferred_point_lighting_vertex" );
    AAPL_ASSERT( pLightFragmentFunction, "Failed to load deferred_point_lighting_fragment_traditional" );
    
    pRenderPipelineDescriptor->setVertexFunction( pLightVertexFunction );
    pRenderPipelineDescriptor->setFragmentFunction( pLightFragmentFunction );
    
    _pLightPipelineState = _pDevice->newRenderPipelineState( pRenderPipelineDescriptor, &pError );
    
    AAPL_ASSERT_NULL_ERROR( pError, "Failed to create lighting render pipeline state" );
    
    pLightVertexFunction->release();
    pLightFragmentFunction->release();
    pRenderPipelineDescriptor->release();
}

void Renderer::buildPointsPipeline()
{
    NS::Error * pError = nullptr;
    
    MTL::PixelFormat colorPixelFormat = colorTargetPixelFormat();
    MTL::PixelFormat depthStencilPixelFormat = depthStencilTargetPixelFormat();
    
    _pVertexDescriptor = MTL::VertexDescriptor::alloc()->init();
    _pVertexDescriptor->attributes()->object(VertexAttributePosition)->setFormat( MTL::VertexFormatFloat3);
    _pVertexDescriptor->attributes()->object(VertexAttributePosition)->setBufferIndex( BufferIndexPointVertexData );
    _pVertexDescriptor->attributes()->object(VertexAttributePosition)->setOffset( 0 );
    _pVertexDescriptor->layouts()->object(BufferIndexPointVertexData)->setStride( 12 );
    
    MTL::Function* pointVertexFunction = _pShaderLibrary->newFunction( AAPLSTR( "point_vertex" ) );
    MTL::Function* pointFragmentFunction = _pShaderLibrary->newFunction( AAPLSTR( "point_fragment" ) );
    
    AAPL_ASSERT( pointVertexFunction, "Failed to load point_vertex shader" );
    AAPL_ASSERT( pointFragmentFunction, "Failed to load point_fragment shader" );
    
    MTL::RenderPipelineDescriptor* pRenderPipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    
    pRenderPipelineDescriptor->setLabel( AAPLSTR( "Point Drawing" ) );
    pRenderPipelineDescriptor->setVertexDescriptor( _pVertexDescriptor );
    pRenderPipelineDescriptor->setVertexFunction( pointVertexFunction );
    pRenderPipelineDescriptor->setFragmentFunction( pointFragmentFunction );
    pRenderPipelineDescriptor->colorAttachments()->object(0)->setPixelFormat( colorPixelFormat );
    
    pRenderPipelineDescriptor->setDepthAttachmentPixelFormat( depthStencilPixelFormat );
    pRenderPipelineDescriptor->setStencilAttachmentPixelFormat( depthStencilPixelFormat );
    pRenderPipelineDescriptor->colorAttachments()->object(0)->setBlendingEnabled( true );
    pRenderPipelineDescriptor->colorAttachments()->object(0)->setRgbBlendOperation( MTL::BlendOperationAdd );
    pRenderPipelineDescriptor->colorAttachments()->object(0)->setAlphaBlendOperation( MTL::BlendOperationAdd );
    pRenderPipelineDescriptor->colorAttachments()->object(0)->setSourceRGBBlendFactor( MTL::BlendFactorSourceAlpha );
    pRenderPipelineDescriptor->colorAttachments()->object(0)->setSourceAlphaBlendFactor ( MTL::BlendFactorSourceAlpha );
    pRenderPipelineDescriptor->colorAttachments()->object(0)->setDestinationRGBBlendFactor( MTL::BlendFactorOne );
    pRenderPipelineDescriptor->colorAttachments()->object(0)->setDestinationAlphaBlendFactor( MTL::BlendFactorOne );
    
    _pPointPipelineState = _pDevice->newRenderPipelineState( pRenderPipelineDescriptor, &pError );
    
    AAPL_ASSERT_NULL_ERROR( pError, "Failed to create point render pipeline state:" );
    
    _pVertexDescriptor->release();
    pRenderPipelineDescriptor->release();
    pointVertexFunction->release();
    pointFragmentFunction->release();
}

void Renderer::buildSkyPipeline(){
    
    NS::Error* pError = nullptr;

    MTL::PixelFormat colorPixelFormat = colorTargetPixelFormat();
    MTL::PixelFormat depthStencilPixelFormat = depthStencilTargetPixelFormat();

    _pSkyVertexDescriptor = MTL::VertexDescriptor::alloc()->init();
    _pSkyVertexDescriptor->attributes()->object(VertexAttributePosition)->setFormat( MTL::VertexFormatFloat3);
    _pSkyVertexDescriptor->attributes()->object(VertexAttributePosition)->setOffset( 0 );
    _pSkyVertexDescriptor->attributes()->object(VertexAttributePosition)->setBufferIndex( BufferIndexVertexData );
    
    _pSkyVertexDescriptor->attributes()->object(VertexAttributeNormal)->setFormat( MTL::VertexFormatFloat3 );
    _pSkyVertexDescriptor->attributes()->object(VertexAttributeNormal)->setOffset( 12 );
    _pSkyVertexDescriptor->attributes()->object(VertexAttributeNormal)->setBufferIndex( BufferIndexVertexData );
    _pSkyVertexDescriptor->layouts()->object(BufferIndexVertexData)->setStride( 24 );
    
    MTL::Function* pSkyboxVertexFunction = _pShaderLibrary->newFunction( AAPLSTR( "skybox_vertex" ) );
    MTL::Function* pSkyboxFragmentFunction = _pShaderLibrary->newFunction( AAPLSTR( "skybox_fragment" ) );
    
    AAPL_ASSERT( pSkyboxVertexFunction, "Failed to load skybox_vertex shader" );
    AAPL_ASSERT( pSkyboxFragmentFunction, "Failed to load skybox_fragment shader" );
    
    MTL::RenderPipelineDescriptor* pSkyRenderPipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    pSkyRenderPipelineDescriptor->setLabel( AAPLSTR( "Sky Pipeline Descriptor" ) );
    pSkyRenderPipelineDescriptor->setVertexDescriptor( _pSkyVertexDescriptor );
    pSkyRenderPipelineDescriptor->setVertexFunction( pSkyboxVertexFunction );
    pSkyRenderPipelineDescriptor->setFragmentFunction( pSkyboxFragmentFunction );
    pSkyRenderPipelineDescriptor->colorAttachments()->object(RenderTargetLighting)->setPixelFormat( colorPixelFormat );
    pSkyRenderPipelineDescriptor->setDepthAttachmentPixelFormat( depthStencilPixelFormat );
    pSkyRenderPipelineDescriptor->setStencilAttachmentPixelFormat( depthStencilPixelFormat );
    
    _pSkyboxPipelineState = _pDevice->newRenderPipelineState( pSkyRenderPipelineDescriptor, &pError );
    AAPL_ASSERT_NULL_ERROR( pError, "Failed to create skybox render pipeline state:" );
    
    pSkyRenderPipelineDescriptor->release();
    pSkyboxVertexFunction->release();
    pSkyboxFragmentFunction->release();
}


void Renderer::buildDepthStencilStates()
{
    /// skybox depth stencil state setup
    MTL::DepthStencilDescriptor* pDepthStencilDesc = MTL::DepthStencilDescriptor::alloc()->init();
    pDepthStencilDesc->setLabel( AAPLSTR( "dont write depth" ) );
    pDepthStencilDesc->setDepthCompareFunction( MTL::CompareFunction::CompareFunctionLess );
    pDepthStencilDesc->setDepthWriteEnabled( false );
    _pDontWriteDepthStencilState = _pDevice->newDepthStencilState( pDepthStencilDesc );
    pDepthStencilDesc->release();

    ///Shadow pass depth stencil state setup
    pDepthStencilDesc = MTL::DepthStencilDescriptor::alloc()->init();
    pDepthStencilDesc->setLabel( AAPLSTR( "Shadow Depth Stencil" ) );
    pDepthStencilDesc->setDepthCompareFunction( MTL::CompareFunctionLessEqual );
    pDepthStencilDesc->setDepthWriteEnabled( true );
    _pShadowDepthStencilState = _pDevice->newDepthStencilState( pDepthStencilDesc );
    pDepthStencilDesc->release();
    
    ///GBuffer depth state setup
    MTL::StencilDescriptor* pStencilStateDesc = MTL::StencilDescriptor::alloc()->init();
    pStencilStateDesc->setStencilCompareFunction( MTL::CompareFunctionAlways );
    pStencilStateDesc->setStencilFailureOperation( MTL::StencilOperationKeep );
    pStencilStateDesc->setDepthFailureOperation( MTL::StencilOperationKeep );
    pStencilStateDesc->setDepthStencilPassOperation( MTL::StencilOperationReplace );
    pStencilStateDesc->setReadMask( 0x0 );
    pStencilStateDesc->setWriteMask( 0xFF );

    pDepthStencilDesc = MTL::DepthStencilDescriptor::alloc()->init();
    pDepthStencilDesc->setLabel( AAPLSTR( "Buffer Depth Stencil" ) );
    pDepthStencilDesc->setDepthCompareFunction( MTL::CompareFunctionLess );
    pDepthStencilDesc->setDepthWriteEnabled( true );
    pDepthStencilDesc->setFrontFaceStencil( pStencilStateDesc );
    pDepthStencilDesc->setBackFaceStencil(  pStencilStateDesc );
    _pGBufferDepthStencilState = _pDevice->newDepthStencilState( pDepthStencilDesc );
    pDepthStencilDesc->release();
    pStencilStateDesc->release();
    
///Directional lighting mask depth stencil state setup
    ///
    pStencilStateDesc = MTL::StencilDescriptor::alloc()->init();
    pStencilStateDesc->setStencilCompareFunction( MTL::CompareFunctionEqual );
    pStencilStateDesc->setStencilFailureOperation( MTL::StencilOperationKeep );
    pStencilStateDesc->setDepthFailureOperation( MTL::StencilOperationKeep );
    pStencilStateDesc->setDepthStencilPassOperation( MTL::StencilOperationKeep );
    pStencilStateDesc->setReadMask( 0xFF );
    pStencilStateDesc->setWriteMask( 0x0 );

    pDepthStencilDesc = MTL::DepthStencilDescriptor::alloc()->init();
    pDepthStencilDesc->setLabel( AAPLSTR( "Deferred Directional Lighting Depth Stencil"));
    pDepthStencilDesc->setDepthWriteEnabled( false );
    pDepthStencilDesc->setDepthCompareFunction( MTL::CompareFunctionAlways );
    pDepthStencilDesc->setFrontFaceStencil( pStencilStateDesc );
    pDepthStencilDesc->setBackFaceStencil( pStencilStateDesc );
    
    _pDirectionLightDepthStencilState = _pDevice->newDepthStencilState( pDepthStencilDesc );

    pDepthStencilDesc->release();
    pStencilStateDesc->release();
    
    
    pStencilStateDesc = MTL::StencilDescriptor::alloc()->init();
    pStencilStateDesc->setStencilCompareFunction( MTL::CompareFunctionLess );
    pStencilStateDesc->setStencilFailureOperation( MTL::StencilOperationKeep );
    pStencilStateDesc->setDepthFailureOperation( MTL::StencilOperationKeep );
    pStencilStateDesc->setDepthStencilPassOperation( MTL::StencilOperationKeep );
    pStencilStateDesc->setReadMask( 0xFF );
    pStencilStateDesc->setWriteMask( 0x0 );

    pDepthStencilDesc = MTL::DepthStencilDescriptor::alloc()->init();
    pDepthStencilDesc->setDepthWriteEnabled( false );
    pDepthStencilDesc->setLabel( AAPLSTR( "Point Light" ) );
    pDepthStencilDesc->setDepthCompareFunction( MTL::CompareFunctionLessEqual );
    pDepthStencilDesc->setFrontFaceStencil( pStencilStateDesc );
    pDepthStencilDesc->setBackFaceStencil( pStencilStateDesc );
    _pPointLightDepthStencilState = _pDevice->newDepthStencilState( pDepthStencilDesc );
    
    pDepthStencilDesc->release();
    pStencilStateDesc->release();
    
    pStencilStateDesc = MTL::StencilDescriptor::alloc()->init();
    pStencilStateDesc->setStencilCompareFunction( MTL::CompareFunctionAlways );
    pStencilStateDesc->setStencilFailureOperation( MTL::StencilOperationKeep );
    pStencilStateDesc->setDepthFailureOperation( MTL::StencilOperationIncrementClamp );
    pStencilStateDesc->setDepthStencilPassOperation( MTL::StencilOperationKeep );
    pStencilStateDesc->setReadMask( 0x0 );
    pStencilStateDesc->setWriteMask( 0xFF );
    
    pDepthStencilDesc = MTL::DepthStencilDescriptor::alloc()->init();
    pDepthStencilDesc->setLabel( AAPLSTR( "Point Light Mask" ) );
    pDepthStencilDesc->setDepthWriteEnabled( false );
    pDepthStencilDesc->setDepthCompareFunction( MTL::CompareFunctionLessEqual );
    pDepthStencilDesc->setFrontFaceStencil( pStencilStateDesc );
    pDepthStencilDesc->setBackFaceStencil( pStencilStateDesc );
    _pLightMaskDepthStencilState = _pDevice->newDepthStencilState( pDepthStencilDesc );
    
    pDepthStencilDesc->release();
    pStencilStateDesc->release();
}

void Renderer::buildTextures()
{
    //NS::Error * pError = nullptr;
    MTL::PixelFormat colorPixelFormat = colorTargetPixelFormat();
    MTL::PixelFormat shadowMapPixelFormat = MTL::PixelFormatDepth16Unorm;

    MTL::TextureDescriptor* pTextureDesc = MTL::TextureDescriptor::alloc()->init();
    pTextureDesc->setWidth( kTextureWidth );
    pTextureDesc->setHeight( kTextureHeight );
    pTextureDesc->setSampleCount(NS::UInteger(sampleCount()/sampleCount()));
    pTextureDesc->setPixelFormat( colorPixelFormat );
    pTextureDesc->setTextureType( MTL::TextureType2D );
    pTextureDesc->setStorageMode( MTL::StorageModePrivate );
    pTextureDesc->setUsage( MTL::ResourceUsageSample | MTL::ResourceUsageRead | MTL::ResourceUsageWrite);
    pTextureDesc->allowGPUOptimizedContents();
    _pTexture = _pDevice->newTexture( pTextureDesc );
    _pTexture->setLabel(AAPLSTR( "Computed Texture" ));
    pTextureDesc->release();

    _pMaterialTexture[0] = newTextureFromCatalog(_pDevice, "BaseColorMap", MTL::StorageModePrivate, MTL::TextureUsageShaderRead);
    _pMaterialTexture[0]->allowGPUOptimizedContents();
    _pMaterialTexture[0]->newTextureView(MTL::PixelFormatRGBA16Float);


    _pMaterialTexture[1] = newTextureFromCatalog(_pDevice, "NormalMap", MTL::StorageModePrivate, MTL::TextureUsageShaderRead);
    _pMaterialTexture[1]->allowGPUOptimizedContents();
    _pMaterialTexture[1]->newTextureView(MTL::PixelFormatRGBA16Float);


    _pMaterialTexture[2] = newTextureFromCatalog(_pDevice, "MetallicMap", MTL::StorageModePrivate, MTL::TextureUsageShaderRead);
    _pMaterialTexture[2]->allowGPUOptimizedContents();
    _pMaterialTexture[2]->newTextureView(MTL::PixelFormatRGBA16Float);


    _pMaterialTexture[3] = newTextureFromCatalog(_pDevice, "RoughnessMap", MTL::StorageModePrivate, MTL::TextureUsageShaderRead);
    _pMaterialTexture[3]->allowGPUOptimizedContents();
    _pMaterialTexture[3]->newTextureView(MTL::PixelFormatRGBA16Float);


    _pMaterialTexture[4] = newTextureFromCatalog(_pDevice, "AOMap", MTL::StorageModePrivate, MTL::TextureUsageShaderRead);
    _pMaterialTexture[4]->allowGPUOptimizedContents();
    _pMaterialTexture[4]->newTextureView(MTL::PixelFormatRGBA16Float);


    _pIrradianceMap = newTextureFromCatalog( _pDevice, "IrradianceMap", MTL::StorageModePrivate, MTL::TextureUsageShaderRead );
    _pIrradianceMap->allowGPUOptimizedContents();
    _pIrradianceMap->newTextureView(MTL::PixelFormatRGBA16Float);


    _pPreFilterMap = newTextureFromCatalog(_pDevice, "PreFilterMap", MTL::StorageModePrivate, MTL::TextureUsageShaderRead );
    _pPreFilterMap->allowGPUOptimizedContents();
    _pPreFilterMap->newTextureView(MTL::PixelFormatRGBA16Float);


    _pBDRFMap = newTextureFromCatalog(_pDevice, "BDRFMap", MTL::StorageModePrivate, MTL::TextureUsageShaderRead );
    _pBDRFMap->allowGPUOptimizedContents();
    _pBDRFMap->newTextureView(MTL::PixelFormatRG16Float);
    
    _pPointMap = newTextureFromCatalog(_pDevice, "PointMap", MTL::StorageModePrivate,MTL::TextureUsageShaderRead);
    _pPointMap->allowGPUOptimizedContents();
    
    _skyMesh = makeSphereMesh(_pDevice, *_pSkyVertexDescriptor, 60, 60, 150.f );
    
    _pSkyMap = newTextureFromCatalog( _pDevice , "IrradianceMap", MTL::StorageModePrivate, MTL::TextureUsageShaderRead);
    _pSkyMap->allowGPUOptimizedContents();
    _pSkyMap->newTextureView(MTL::PixelFormatRGBA16Float);
    
/// Shadow map setup
    MTL::TextureDescriptor* pShadowTextureDesc = MTL::TextureDescriptor::alloc()->init();
    pShadowTextureDesc->setPixelFormat( shadowMapPixelFormat );
    pShadowTextureDesc->setWidth( 2048 );
    pShadowTextureDesc->setHeight( 2048 );
    pShadowTextureDesc->setMipmapLevelCount( 1 );
    pShadowTextureDesc->setResourceOptions( MTL::ResourceStorageModePrivate );
    pShadowTextureDesc->setUsage( MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead | MTL::ResourceUsageRead | MTL::ResourceUsageWrite);
    _pShadowMap = _pDevice->newTexture( pShadowTextureDesc );
    _pShadowMap->setLabel( AAPLSTR( "shadow Map" ) );
    _pShadowMap->allowGPUOptimizedContents();
    pShadowTextureDesc->release();

    MTL::TextureDescriptor* _pGBufferTextureDesc = MTL::TextureDescriptor::alloc()->init();
    _pGBufferTextureDesc->allowGPUOptimizedContents();
    _pGBufferTextureDesc->setMipmapLevelCount( 1 );
    _pGBufferTextureDesc->setTextureType( MTL::TextureType2D );
    _pGBufferTextureDesc->setStorageMode( MTL::StorageModePrivate);
    _pGBufferTextureDesc->setUsage( MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead );
    
    _pGBufferTextureDesc->setPixelFormat( _albedo_specular_GBufferFormat );
    _albedo_specular_GBuffer = _pDevice->newTexture( _pGBufferTextureDesc );
    _albedo_specular_GBuffer->setLabel( AAPLSTR( "BDRF + specular GBuffer" ) );

    _pGBufferTextureDesc->setPixelFormat( _normal_shadow_GBufferFormat );
    _normal_shadow_GBuffer = _pDevice->newTexture( _pGBufferTextureDesc );
    _normal_shadow_GBuffer->setLabel( AAPLSTR( "Normal + Shadow GBuffer" ) );

    _pGBufferTextureDesc->setPixelFormat( _depth_GBufferFormat );
    _depth_GBuffer = _pDevice->newTexture( _pGBufferTextureDesc );
    _depth_GBuffer->setLabel( AAPLSTR( "Depth GBuffer" ) );

    _pGBufferTextureDesc->release();
}

void Renderer::buildRenderPasses()
{
    ///Shadow render pass descriptor setup

    _pShadowRenderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
    _pShadowRenderPassDescriptor->setRenderTargetWidth(NS::UInteger(_pShadowMap->width()));
    _pShadowRenderPassDescriptor->setRenderTargetHeight(NS::UInteger(_pShadowMap->height()));
    _pShadowRenderPassDescriptor->depthAttachment()->setClearDepth( 1.0 );
    _pShadowRenderPassDescriptor->colorAttachments()->object(0)->clearColor();
    _pShadowRenderPassDescriptor->depthAttachment()->setTexture( _pShadowMap );
    _pShadowRenderPassDescriptor->depthAttachment()->setLoadAction( MTL::LoadActionClear );
    _pShadowRenderPassDescriptor->depthAttachment()->setStoreAction( MTL::StoreActionStore );
    
    /// Create a render pass descriptor to create an encoder for rendering to the GBuffers.
    /// The encoder stores rendered data of each attachment when encoding ends.

    _pGBufferRenderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
    _pGBufferRenderPassDescriptor->colorAttachments()->object(RenderTargetLighting)->setLoadAction( MTL::LoadActionDontCare);
    _pGBufferRenderPassDescriptor->colorAttachments()->object(RenderTargetLighting)->setStoreAction( MTL::StoreActionDontCare);
    _pGBufferRenderPassDescriptor->colorAttachments()->object(RenderTargetAlbedo)->setLoadAction( MTL::LoadActionClear);
    _pGBufferRenderPassDescriptor->colorAttachments()->object(RenderTargetAlbedo)->setStoreAction( MTL::StoreActionStore );
    _pGBufferRenderPassDescriptor->colorAttachments()->object(RenderTargetNormal)->setLoadAction( MTL::LoadActionClear );
    _pGBufferRenderPassDescriptor->colorAttachments()->object(RenderTargetNormal)->setStoreAction( MTL::StoreActionStore );
    _pGBufferRenderPassDescriptor->colorAttachments()->object(RenderTargetDepth)->setLoadAction( MTL::LoadActionClear );
    _pGBufferRenderPassDescriptor->colorAttachments()->object(RenderTargetDepth)->setStoreAction( MTL::StoreActionStore );

    _pGBufferRenderPassDescriptor->depthAttachment()->setClearDepth( 1.0 );
    _pGBufferRenderPassDescriptor->depthAttachment()->setLoadAction( MTL::LoadActionClear );
    _pGBufferRenderPassDescriptor->depthAttachment()->setStoreAction( MTL::StoreActionStore );

    _pGBufferRenderPassDescriptor->stencilAttachment()->setClearStencil( 0 );
    _pGBufferRenderPassDescriptor->stencilAttachment()->setLoadAction( MTL::LoadActionClear );
    _pGBufferRenderPassDescriptor->stencilAttachment()->setStoreAction( MTL::StoreActionStore );
    
    _pGBufferRenderPassDescriptor->colorAttachments()->object(RenderTargetAlbedo)->setTexture( _albedo_specular_GBuffer );
    _pGBufferRenderPassDescriptor->colorAttachments()->object(RenderTargetNormal)->setTexture( _normal_shadow_GBuffer );
    _pGBufferRenderPassDescriptor->colorAttachments()->object(RenderTargetDepth)->setTexture( _depth_GBuffer );

    // Create a render pass descriptor for the lighting and composition pass
    // Whatever rendered in the final pass needs to be stored so it can be displayed
    
    _pFinalRenderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
    _pFinalRenderPassDescriptor->colorAttachments()->object(RenderTargetLighting)->setLoadAction(MTL::LoadActionClear);
    _pFinalRenderPassDescriptor->colorAttachments()->object(RenderTargetLighting)->setStoreAction( MTL::StoreActionStore);
    _pFinalRenderPassDescriptor->depthAttachment()->setLoadAction( MTL::LoadActionLoad);
    _pFinalRenderPassDescriptor->stencilAttachment()->setLoadAction( MTL::LoadActionLoad );
}

void Renderer::buildBuffers()
{
    NS::Error * pError = nullptr;
    
    using simd::float3;
    using simd::float2;
    
    /// Quad for redering Gbuffer on Screen
    
    static const SimpleVertex quadVertices[] =
    {
        { { -1.0f,  -1.0f, } },
        { { -1.0f,   1.0f, } },
        { {  1.0f,  -1.0f, } },
        
        { {  1.0f,  -1.0f, } },
        { { -1.0f,   1.0f, } },
        { {  1.0f,   1.0f, } },
    };
    
    size_t quadDataSize = sizeof(quadVertices);
    _pQuadVertexBuffer = _pDevice->newBuffer(  quadVertices, quadDataSize, MTL::ResourceStorageModeShared);
    AAPL_ASSERT( _pQuadVertexBuffer->length() != quadDataSize * sizeof(SimpleVertex), pError);
    _pQuadVertexBuffer->setLabel( AAPLSTR( "Quad Vertice Buffer" ));
    
    _indexQuadCount =  sizeof(quadVertices)/ sizeof(quadVertices[0]);
    
    /// Quad for redering a Plane on Ground

    static const GroundVertex groundVertices[] =
    {
        vector_float4 { -250.0f, -250.0f,  0.0f,  1.0f },      // 1
        vector_float3 {   -1.0f,   -1.0f,  0.0f } ,
        vector_float4 { -250.0f,  250.0f,  0.0f,  1.0f },      // 2
        vector_float3 {   -1.0f,    1.0f,  0.0f },
        vector_float4 {  250.0f, -250.0f,  0.0f,  1.0f },      // 3
        vector_float3 {    1.0f,   -1.0f,  0.0f } ,
        vector_float4 {  250.0f, -250.0f,  0.0f,  1.0f },      // 4
        vector_float3 {    1.0f,   -1.0f,  0.0f } ,
        vector_float4 { -250.0f,  250.0f,  0.0f,  1.0f },      // 5
        vector_float3 {   -1.0f,    1.0f,  0.0f } ,
        vector_float4 {  250.0f,  250.0f,  0.0f,  1.0f },      // 6
        vector_float3 {    1.0f,    1.0f,  0.0f }
    };
    
    size_t groundDataSize = sizeof(groundVertices);
    _pGroundVertexBuffer = _pDevice->newBuffer(  groundVertices, groundDataSize, MTL::ResourceStorageModeShared);
     AAPL_ASSERT( _pGroundVertexBuffer->length() != groundDataSize * sizeof(GroundVertex), pError);
    _pGroundVertexBuffer->setLabel( AAPLSTR( "Ground Vertice Buffer" ));
    _indexGroundCount =  sizeof(groundVertices)/ sizeof(groundVertices[0]);
    

    static SimpleVertex pointVertices[NumPointVertices];
    const float angle = 2*M_PI/(float)NumPointVertices;
    for(int vtx = 0; vtx < NumPointVertices; vtx++)
    {
        int point = (vtx%2) ? (vtx+1)/2 : -vtx/2;
        simd_float2 position = {sin(point*angle), cos(point*angle)};
        pointVertices[vtx].position = position;
    }
    
    size_t pointDataSize = sizeof(pointVertices);
    _pPointVertexBuffer = _pDevice->newBuffer(pointVertices, pointDataSize, MTL::ResourceStorageModeShared);
    AAPL_ASSERT( _pPointVertexBuffer->length() != pointDataSize * sizeof(SimpleVertex), pError);
    _pPointVertexBuffer->setLabel( AAPLSTR( "Point Vertices" ) );

    // Create an icosahedron mesh for fairy light volumes
    
    // Create vertex descriptor with layout for icoshedron
    MTL::VertexDescriptor * icosahedronVertexDescriptor = MTL::VertexDescriptor::alloc()->init();
    icosahedronVertexDescriptor->attributes()->object(VertexAttributePosition)->setFormat( MTL::VertexFormatFloat4);
    icosahedronVertexDescriptor->attributes()->object(VertexAttributePosition)->setBufferIndex( BufferIndexVertexData );
    icosahedronVertexDescriptor->attributes()->object(VertexAttributePosition)->setOffset( 0 );
    icosahedronVertexDescriptor->layouts()->object(BufferIndexVertexData)->setStride(sizeof(simd::float4));
    
    float icoshedronRadius = 1.0 / (sqrtf(3.0) / 12.0 * (3.0 + sqrtf(5.0)));
    _icosahedronMesh = makeIcosahedronMesh( _pDevice, *icosahedronVertexDescriptor, icoshedronRadius);
   
    icosahedronVertexDescriptor->release();
    /// properties erstellen

    std::vector <float3> _positions;
    std::vector <float2> _uv;
    std::vector <float3> _normals;
    std::vector <int16_t>_indices;

    const int16_t X_SEGMENTS = 69;
    const int16_t Y_SEGMENTS = 69;

    for (int16_t x = 0; x <= X_SEGMENTS; ++x)
    {
        for (int16_t y = 0; y <= Y_SEGMENTS; ++y)
        {
            float xSegment = (float)x / (float)X_SEGMENTS;
            float ySegment = (float)y / (float)Y_SEGMENTS;
            float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
            float yPos = std::cos(ySegment * PI);
            float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

            _positions.push_back( float3 { xPos, yPos, zPos});
            _uv.push_back( float2 {xSegment, ySegment});
            _normals.push_back( float3 {xPos, yPos, zPos});
        }
    }

    VertexData verts[_positions.size()];

    for (int16_t j = 0; j < _positions.size(); ++j)
    {
        verts[j] = { vector_float3 { _positions[j].x, _positions[j].y, _positions[j].z },
                     vector_float2 { _uv[j].x, _uv[j].y },
                     vector_float3 { _normals[j].x, _normals[j].y, _normals[j].z }};
    }

    bool oddRow = false;
    for ( int16_t y = 0; y < Y_SEGMENTS; ++y)
    {
        if (!oddRow)
        {
            for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
            {
                _indices.push_back(y * (X_SEGMENTS + 1) + x);
                _indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
            }
        }
        else
        {
            for (int x = X_SEGMENTS; x >= 0; --x)
            {
                _indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                _indices.push_back(y * (X_SEGMENTS + 1) + x);
            }
        }
        oddRow = !oddRow;
    }

    int16_t indices[_indices.size()];

    for(int16_t i = 0 ; i < _indices.size() ; ++i)
    {
        indices[i] = _indices[i];
    }

    _indexCount =  _indices.size();

    const size_t vertexDataSize = sizeof( verts );
    const size_t indexDataSize  = sizeof( indices);

    MTL::Buffer* pVertexBuffer = _pDevice->newBuffer( vertexDataSize, MTL::ResourceStorageModeShared );
    MTL::Buffer* pIndexBuffer  = _pDevice->newBuffer( indexDataSize, MTL::ResourceStorageModeShared );

    _pVertexDataBuffer = pVertexBuffer;
    _pVertexDataBuffer->setLabel( AAPLSTR( "Vertex Buffer" ));
    _pIndexBuffer = pIndexBuffer;
    _pIndexBuffer->setLabel(AAPLSTR( "Index Buffer" ));
    
    memcpy( _pVertexDataBuffer->contents(), verts, vertexDataSize );
    memcpy( _pIndexBuffer->contents(), indices, indexDataSize );

    const size_t instanceDataSize = kMaxFramesInFlight * numberOfInstances() * sizeof( InstanceData );
    for ( size_t i = 0; i < kMaxFramesInFlight; ++i )
    {
        _pInstanceDataBuffer[ i ] = _pDevice->newBuffer( instanceDataSize, MTL::ResourceStorageModeShared);
        _pInstanceDataBuffer[ i ]->setLabel(AAPLSTR("InstanceBuffer"));
    }
    
    const size_t frameDataSize = kMaxFramesInFlight * sizeof( FrameData );
    for ( size_t i = 0; i < kMaxFramesInFlight; ++i )
    {
        _pFrameDataBuffer[i] = _pDevice->newBuffer( frameDataSize, MTL::ResourceStorageModeShared );
        _pFrameDataBuffer[i]->setLabel(AAPLSTR("FrameBuffer"));
    }
}

void Renderer::buildParticleBuffer(){
    NS::Error * pError = nullptr;

    _pTimeBuffer = _pDevice->newBuffer( sizeof(float), MTL::ResourceStorageModeShared );
    _pTimeBuffer->setLabel(AAPLSTR("Timebuffer"));
    
    const size_t unifornDataSize = kMaxFramesInFlight * sizeof( Uniforms );
    for ( size_t i = 0; i < kMaxFramesInFlight; ++i )
    {
        _pUniformsBuffer[i] = _pDevice->newBuffer( unifornDataSize, MTL::ResourceStorageModeShared );
        _pUniformsBuffer[i]->setLabel(AAPLSTR("UniformBuffer"));
    }

    Particle particles[num_particles];

    for( NS::UInteger i = 0; i < num_particles; i++) {
        particles[i]={
            uint{
                1
            },
            simd::float2{
                roundf(random_float( 0.f, 1.f) * kTextureWidth),
                roundf(random_float( 0.f, 1.f) * kTextureHeight)
            },
            float{
                float(random_float( 0.f, 1.f ) * 2.0 * PI)
            },
            simd::int4 {
                int(0),
                int(1),
                int(1),
                int(1)
            }
        };
    }

    const size_t particleDataSize = sizeof(particles);
    
    _pParticleBuffer = _pDevice->newBuffer(particleDataSize, MTL::ResourceStorageModeShared );
    _pParticleBuffer->setLabel(AAPLSTR("ParticelBuffer"));
    AAPL_ASSERT( _pParticleBuffer->length() != sizeof(Particle), pError);
    memcpy(_pParticleBuffer->contents(), particles, particleDataSize);
    initCompute = false;
}

void Renderer::buildLightsBuffer() {
    
    using simd::float4;
    using simd::float3;
    
    _pLightsDataBuffer = _pDevice->newBuffer(sizeof(PointLightData) * NumLights, MTL::ResourceStorageModeShared);
    _pLightsDataBuffer->setLabel(AAPLSTR("LightDataBuffer"));
    
    PointLightData *light_data = reinterpret_cast<PointLightData*>(_pLightsDataBuffer->contents());

    _original_light_positions = new float4[ NumLights ];

    const size_t lightPositionSize = kMaxFramesInFlight * sizeof( _original_light_positions );
    for ( size_t i = 0; i < kMaxFramesInFlight; ++i ){
        _pLightPositionsBuffer[i] = _pDevice->newBuffer(lightPositionSize, MTL::ResourceStorageModeShared);
        _pLightPositionsBuffer[i]->setLabel(AAPLSTR("LightPositionBuffer"));
    }
    
    float4 *light_position = _original_light_positions;
    
    for(uint32_t lightId = 0; lightId < NumLights; lightId++){
        
        float distance = random_float(1.2,1.2);
        float height = random_float(0,0);
        float angle = lightId *36.f;
        float speed = 0.027;
        speed *= (random()%2)*2-1;
        
        speed *= .5;
        *light_position = {(vector_float4){distance*sinf(angle), height, distance*cosf(angle), 1.f}};
        light_data->light_radius = 36 / 10.0;
        light_data->light_speed  = speed;
        light_data->pointLightColor = (float3){ 0.9f,  0.8f,  0.4f};
        light_data++;
        light_position++;
    }
    
    
}

void Renderer::updateLights(const simd::float4x4 & modelViewMatrix) {
    using simd::float4;
    
    PointLightData *light_Data = reinterpret_cast<PointLightData*>(_pLightsDataBuffer->contents());
    
    float4 *currentBuffer =
    reinterpret_cast<float4*>(_pLightPositionsBuffer[_frame]->contents());

    float4 *originalLightPositions =  (float4 *)_original_light_positions;
    
    for(uint32_t i = 0; i < NumLights; i++)
    {
        float4 currentPosition;
        
        float rotationRadians = light_Data[i].light_speed * _frameNumber;
        
        simd::float4x4 rotation = matrix4x4_rotation(rotationRadians, 0, 1, 0);
        
        currentPosition = rotation * originalLightPositions[i];
        
        currentPosition = modelViewMatrix * currentPosition;
        
        currentBuffer[i] = currentPosition;
    }
    
}
void Renderer::step_animation()
{
    t_transformation = fmod(1.0 + t_transformation + dir * TRANSFORMATION_SPEED, 1.0);
    t_rotation = fmod(1.0 + t_rotation + dir * ROTATION_SPEED, 1.0);
}

void Renderer::generateComputedTexture( MTL::CommandBuffer* pCommandBuffer, MTL::Buffer* pUniformsBuffer )
{
    AAPL_ASSERT( pCommandBuffer, "CommandBuffer for Kernel Computing not valid");

    using simd::float2;
    using simd::float4;
    
    CFAbsoluteTime currentTime = CFAbsoluteTime();
    
    float* delta= reinterpret_cast<float*>(_pTimeBuffer->contents());
  
    if(_previousTime != currentTime) {
        *delta = fmin(float(currentTime - _previousTime), 1.f / 30.f);
    } else {
        *delta = (1.f / 60.f);
    }
    
    if(!initCompute){
        MTL::ComputeCommandEncoder * pInitComputeEncoder = pCommandBuffer->computeCommandEncoder();
        pInitComputeEncoder->setLabel(AAPLSTR("Initialize"));
        pInitComputeEncoder->setComputePipelineState(_pInitComputePSO);
        pInitComputeEncoder->setBuffer( _pParticleBuffer, 0, BufferIndexParticleData );
        pInitComputeEncoder->setBuffer(  pUniformsBuffer, 0, BufferIndexUniformData);
        pInitComputeEncoder->setTexture( _pTexture, TextureIndexReadMap);
        MTL::Size threadsPerGrid = MTL::Size().Make( NS::Integer( num_particles), 1, 1 );
        MTL::Size threadsPerThreadgroup = MTL::Size().Make( 1, 1, 1 );
        pInitComputeEncoder->dispatchThreads(threadsPerGrid, threadsPerThreadgroup);
        pInitComputeEncoder->endEncoding();
        initCompute = true;
    }
    
    MTL::ComputeCommandEncoder * pComputeEncoder = pCommandBuffer->computeCommandEncoder();
    pComputeEncoder->setLabel(AAPLSTR("Computing"));
    pComputeEncoder->setComputePipelineState(_pComputePSO);
    pComputeEncoder->setTexture( _pTexture, TextureIndexReadMap);
    pComputeEncoder->setTexture( _pTexture, TextureIndexWriteMap);
    pComputeEncoder->setBuffer( _pParticleBuffer, 0, BufferIndexParticleData );
    pComputeEncoder->setBuffer(  pUniformsBuffer, 0, BufferIndexUniformData);
    pComputeEncoder->setBuffer( _pTimeBuffer, 0 , BufferIndexTimeData);
    MTL::Size threadsPerThreadgroup = MTL::Size().Make( 1, 1, 1 );
    MTL::Size threadsPerGrid = MTL::Size().Make( NS::Integer( num_particles)  , 1, 1 );
    pComputeEncoder->dispatchThreads(threadsPerGrid, threadsPerThreadgroup);
    pComputeEncoder->endEncoding();

    MTL::ComputeCommandEncoder * pTrailComputeEncoder = pCommandBuffer->computeCommandEncoder();
    pTrailComputeEncoder->setLabel(AAPLSTR("Trail&Deposit"));
    pTrailComputeEncoder->setComputePipelineState(_pTrailComputePSO);
    pTrailComputeEncoder->setTexture( _pTexture, TextureIndexReadMap);
    pTrailComputeEncoder->setTexture( _pTexture, TextureIndexWriteMap);
    pTrailComputeEncoder->setBuffer( _pParticleBuffer, 0, BufferIndexParticleData );
    pTrailComputeEncoder->setBuffer(  pUniformsBuffer, 0, BufferIndexUniformData );
    threadsPerThreadgroup = MTL::Size().Make( 16 , 16 , 1);
    NS::UInteger width = NS::UInteger( _pTexture->width() );
    NS::UInteger height = NS::UInteger( _pTexture->height() );
    threadsPerGrid = MTL::Size().Make( width , height , 1);
    pTrailComputeEncoder->dispatchThreads(threadsPerGrid, threadsPerThreadgroup);
    pTrailComputeEncoder->endEncoding();
    
    if( updatePass != get_num_families()){
        MTL::ComputeCommandEncoder * pUpdateComputeEncoder = pCommandBuffer->computeCommandEncoder();
        pUpdateComputeEncoder->pushDebugGroup(AAPLSTR("UpdateFamily"));
        pUpdateComputeEncoder->setComputePipelineState(_pUpdateFamilyComputePSO);
        pUpdateComputeEncoder->setBuffer( _pParticleBuffer, 0, BufferIndexParticleData );
        pUpdateComputeEncoder->setBuffer(  pUniformsBuffer, 0, BufferIndexUniformData);
        MTL::Size threadsPerThreadgroup = MTL::Size().Make( 1, 1, 1 );
        MTL::Size threadsPerGrid = MTL::Size().Make(  num_particles  , 1, 1 );
        pUpdateComputeEncoder->dispatchThreads(threadsPerGrid, threadsPerThreadgroup);
        pUpdateComputeEncoder->endEncoding();
        updatePass = get_num_families();
    } else {
        return;
    }

    _previousTime = currentTime;

}

void Renderer::drawInView( MTK::View * pView, MTL::Drawable* pCurrentDrawable, MTL::Texture* pDepthStencilTexture )
{
    /// RENDER START NON DRAWABLE
    ///
    dispatch_semaphore_wait( this->_semaphore, DISPATCH_TIME_FOREVER );
    
    MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();
    
    step_animation();
    
    _frame = (_frame + 1) % kMaxFramesInFlight;
    _frameRate = ( _frameRate + 1) % kFrameRate;
    
    _frameNumber=( _frameNumber + 1);
    
    _angle += 0.01;
    
    using simd::float3;
    using simd::float4;
    using simd::float4x4;
    using simd::float3x3;
    
    const size_t kInstanceRows    = instances()[0];
    const size_t kInstanceColumns = instances()[1];
    const size_t kInstanceDepth   = instances()[2];
    
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();
    
    MTL::Buffer* pInstanceDataBuffer = _pInstanceDataBuffer[_frame];
    InstanceData* pInstanceData = reinterpret_cast< InstanceData *>( pInstanceDataBuffer->contents());
    
    float xRotate = 360.f * ease_circular_in_out(t_transformation);
    // float yRotate = 360.f * ease_circular_in_out(xRotate);
    
    const float scl = instancesSize();
    const float obscl = getGroupScale();
    const float instanceRowCount = pow( numberOfInstances(), 1.f / 3 );
    float3 objectPosition = { 0.f,  4.f + float(instanceRowCount / 2) , -1.f -float(instanceRowCount * 1.5f) };
    
    float4x4 rt = makeTranslate((float3){ objectPosition.x , objectPosition.y , objectPosition.z});
    float4x4 rr1 = makeYRotate( -_angle);
    float4x4 rr0 = makeXRotate(  _angle);
    float4x4 rtInv = makeTranslate( (float3){ -objectPosition.x, -objectPosition.y , -objectPosition.z });
    _lightModelMatrix = rt  * rr1 * rr0;
    _modelMatrix = rt  * rr1 * rr0  * rtInv ;
    
    size_t ix = 0;
    size_t iy = 0;
    size_t iz = 0;
    
    for ( size_t i = 0; i < numberOfInstances(); ++i )
    {
        
        if ( ix == kInstanceRows )
        {
            ix = 0;
            iy += 1;
        }
        if ( iy == kInstanceRows )
        {
            iy = 0;
            iz += 1;
        }
        
        float x = ((float)ix - (float)kInstanceRows   /2.5f) * (2.5f * obscl) + obscl;
        float y = ((float)iy - (float)kInstanceColumns/2.5f) * (2.5f * obscl) + obscl;
        float z = ((float)iz - (float)kInstanceDepth  /2.5f) * (2.5f * obscl) + obscl;
        
        float3 instanceObjectPosition = add( objectPosition , (float3){ x, y, z });
        float4x4 translate = makeTranslate(instanceObjectPosition);
        float4x4 zrot = makeZRotate(2 * _angle * sinf((float)ix) );
        ///float4x4 yrot = makeYRotate( ((_angle *  xRotate) / xRotate ) * cosf((float)iy) );
        float4x4 yrot = makeYRotate( ((0.01 * _angle +  xRotate) / xRotate) * cosf((float)iy) );
        float4x4 scale = makeScale( (float3){ scl , scl , scl } );
        float4x4 _modelTrans = _modelMatrix * translate * yrot * zrot * scale;
        
        pInstanceData[ i ].instanceTransform = _modelTrans;
        pInstanceData[ i ].instanceNormalTransform = matrix3x3_upper_left(pInstanceData[ i ].instanceTransform);
        
                
        float iDivNumInstances = i / (float) numberOfInstances();
        float r = sinf(iDivNumInstances);
        float g = cosf(iDivNumInstances);
        float b = sinf( PI * 2.0f * iDivNumInstances );
        pInstanceData[ i ].instanceColor = (float4){ r, g, b, 1.0 };
        ix += 1;
    }
    
    /// Update camera, view matrix, state:
    
    MTL::Buffer* pFrameDataBuffer = _pFrameDataBuffer[ _frame ];
    FrameData* pFrameData = reinterpret_cast< FrameData *>( pFrameDataBuffer->contents());
    
    /// Set screen dimensions
    pFrameData->framebuffer_width= (uint)pView->currentDrawable()->texture()->width();
    pFrameData->framebuffer_height= (uint)pView->currentDrawable()->texture()->height();
    
    pFrameData->textureScale = textureScale();
    pFrameData->colorMixBias = baseColorMixValue();
    pFrameData->metallnessBias = metallTextureValue();
    pFrameData->roughnessBias = roughnessTextureValue();
    
    
    pFrameData->scaleMatrix = matrix4x4_scale(vector_float3(1.f));
    
    pFrameData->viewMatrix = cameraData().viewMatrix;
    
    pFrameData->cameraPos = cameraData().cameraPosition;
    
    pFrameData->cameraDir = cameraData().cameraDirection;
    
    pFrameData->worldTransform = cameraData().viewMatrix;
    
    pFrameData->worldNormalTransform = matrix3x3_upper_left(pFrameData->worldTransform);
    
    pFrameData->perspectiveTransform = _projectionMatrix;
    
    pFrameData->projection_matrix_inverse = matrix_invert(pFrameData->perspectiveTransform);
    
    // matrix_float4x4 planeModelMatrix = cameraData().skyModel;
    
    matrix_float4x4 planeModelMatrix = matrix4x4_scale_translation((vector_float3){1,1,1}, (vector_float3){0,0,0});
    
    pFrameData->planeModelViewMatrix = matrix_multiply(planeModelMatrix, pFrameData->viewMatrix);
    
    pFrameData->normalPlaneModelViewMatrix = matrix3x3_upper_left(pFrameData->planeModelViewMatrix);
    
    
    pFrameData->skyModelMatrix = cameraData().skyModel;
    
    pFrameData->sky_modelview_matrix = matrix_multiply( pFrameData->skyModelMatrix , pFrameData->viewMatrix);
    
    ///sun updat
    pFrameData->sun_color = (float4){ 0.8,  0.8,  0.8, 1.f };
    pFrameData->sunPosition = cameraData().sunLightPosition;
    
    pFrameData->sun_specular_intensity = 1;
    pFrameData->shininess_factor = 1.;
    pFrameData->point_size = 0.2;
    
    pFrameData->sun_eye_direction = cameraData().sunEyeDirection;
    
    pFrameData->shadow_view_matrix = shadowCameraData().viewMatrix;
    
    pFrameData->shadow_projections_matrix = shadowCameraData().projectionMatrix;
    
    float4x4 shadowScale = matrix4x4_scale(0.5f,-0.5f, 1.0);
    float4x4 shadowTranslate = matrix4x4_translation(0.5, 0.5, 0);
    float4x4 shadowTransform = matrix_multiply(shadowTranslate, shadowScale);
    pFrameData->shadow_xform_matrix = shadowTransform;
    
    
    MTL::Buffer *pUniformsBuffer = _pUniformsBuffer [_frame];
    Uniforms * uniforms = reinterpret_cast<Uniforms*>(pUniformsBuffer->contents());
    uniforms->particleCount = num_particles;
    uniforms->sensorAngle = senseAngleValue() * M_PI ;
    uniforms->sensorOffset = senseOffsetValue();
    uniforms->sensorSize = uint(SENSOR_SIZE);
    uniforms->evaporation = evaporationValue();
    uniforms->turnSpeed = turnSpeedValue();
    uniforms->trailWeight = trailWeightValue();
    uniforms->Dimensions = simd::uint2{ kTextureWidth, kTextureHeight };
    uniforms->moveSpeed = stepPerFrameValue();
    uniforms->family = get_num_families();
    
    updateLights( matrix_multiply( pFrameData->viewMatrix, _lightModelMatrix));
    
    //updateDebugOutput();
    
    pCmd->setLabel(AAPLSTR("Compute & Shadow & GBuffer Commands"));
    
    generateComputedTexture( pCmd , pUniformsBuffer);
    
    /// BEGINN RENDERPASS
    
    drawShadow( pCmd, pFrameDataBuffer, pInstanceDataBuffer);
    
    _pGBufferRenderPassDescriptor->depthAttachment()->setTexture( pDepthStencilTexture );
    _pGBufferRenderPassDescriptor->stencilAttachment()->setTexture( pDepthStencilTexture );

    MTL::RenderCommandEncoder* pNonEnc =  pCmd->renderCommandEncoder( _pGBufferRenderPassDescriptor );
    pNonEnc->setLabel( AAPLSTR( "Draw Objects to GBuffer "));
    pNonEnc->setRenderPipelineState( _pGBufferPipelineState );
    pNonEnc->setDepthStencilState( _pGBufferDepthStencilState);
    pNonEnc->setVertexBuffer( _pVertexDataBuffer,       /* offset */  0, BufferIndexVertexData );
    pNonEnc->setVertexBuffer(  pInstanceDataBuffer,     /* offset */  0, BufferIndexInstanceData );
    pNonEnc->setVertexBuffer(  pFrameDataBuffer,        /* offset */  0, BufferIndexFrameData );
    pNonEnc->setFragmentBuffer(  pFrameDataBuffer,      /* offset */  0, BufferIndexFrameData );
    pNonEnc->setFragmentTexture( _pMaterialTexture[0], TextureIndexBaseColor );
    pNonEnc->setFragmentTexture( _pMaterialTexture[1], TextureIndexNormal);
    pNonEnc->setFragmentTexture( _pMaterialTexture[2], TextureIndexMetallic);
    pNonEnc->setFragmentTexture( _pMaterialTexture[3], TextureIndexRoughness);
    pNonEnc->setFragmentTexture( _pMaterialTexture[4], TextureIndexAmbientOcclusion);
    pNonEnc->setFragmentTexture( _pIrradianceMap, TextureIndexIrradianceMap );
    pNonEnc->setFragmentTexture( _pPreFilterMap, TextureIndexPreFilterMap );
    pNonEnc->setFragmentTexture( _pBDRFMap, TextureIndexBDRF );
    pNonEnc->setFragmentTexture( _pTexture, TextureIndexWriteMap );
    pNonEnc->setFragmentTexture( _pShadowMap, TextureIndexShadowMap );
    pNonEnc->setCullMode( MTL::CullModeBack );
    pNonEnc->setStencilReferenceValue( 128 );
    pNonEnc->setFrontFacingWinding( MTL::Winding::WindingCounterClockwise );
    pNonEnc->drawIndexedPrimitives( primitiveType(),
                                _indexCount, MTL::IndexType::IndexTypeUInt16,
                                _pIndexBuffer,
                                0,
                                numberOfInstances());
    
    pNonEnc->pushDebugGroup( AAPLSTR( "Draw Ground Plane to GBuffer" ) );
    pNonEnc->setRenderPipelineState(_pGroundPipelineState);
    pNonEnc->setVertexBuffer( _pGroundVertexBuffer, 0,   BufferIndexGroundVertexData );
    pNonEnc->setStencilReferenceValue( 128 );
    pNonEnc->setFrontFacingWinding( MTL::Winding::WindingCounterClockwise );
    pNonEnc->setCullMode( MTL::CullModeBack );
    pNonEnc->drawPrimitives( MTL::PrimitiveTypeTriangle,
                         (NS::UInteger)0,
                         (NS::UInteger)_indexGroundCount);
    pNonEnc->popDebugGroup();
    pNonEnc->endEncoding();
    pCmd->commit();
    
    /// BEGIN DRAWABLE RENDERING
    pCmd = _pCommandQueue->commandBuffer();
    pCmd->setLabel( AAPLSTR("Final Render Pass"));
    
    pCmd->addCompletedHandler([this]( MTL::CommandBuffer* ){
        dispatch_semaphore_signal( _semaphore );
    });
    
    MTL::Texture * pDrawableTexture = currentDrawableTexture(pCurrentDrawable);
    
    if( pDrawableTexture )
    {
        _pFinalRenderPassDescriptor->colorAttachments()->object(RenderTargetLighting)->setTexture( pDrawableTexture);
        _pFinalRenderPassDescriptor->depthAttachment()->setTexture( pDepthStencilTexture );
        _pFinalRenderPassDescriptor->stencilAttachment()->setTexture( pDepthStencilTexture);
        
        /// Render the lighting and composition pass
        MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder(_pFinalRenderPassDescriptor );
        pEnc->setLabel( AAPLSTR( "Draw Directional Light" ) );
        pEnc->setRenderPipelineState(_pDirectLightPipelineState);
        pEnc->setDepthStencilState(_pDirectionLightDepthStencilState);
        pEnc->setStencilReferenceValue( 128 );
        pEnc->setFrontFacingWinding( MTL::Winding::WindingClockwise );
        pEnc->setCullMode( MTL::CullModeBack );
        pEnc->setVertexBuffer(  _pQuadVertexBuffer, 0,   BufferIndexQuadVertexData );
        pEnc->setVertexBuffer(   pFrameDataBuffer,  0,   BufferIndexFrameData );
        pEnc->setFragmentBuffer( pFrameDataBuffer,  0,   BufferIndexFrameData );
        pEnc->setFragmentTexture( _albedo_specular_GBuffer, RenderTargetAlbedo );
        pEnc->setFragmentTexture(  _normal_shadow_GBuffer, RenderTargetNormal );
        pEnc->setFragmentTexture(          _depth_GBuffer, RenderTargetDepth);
        pEnc->drawPrimitives( MTL::PrimitiveTypeTriangle,
                             (NS::UInteger)0,
                             (NS::UInteger)_indexQuadCount);
        
        /// Point Light Mask
        drawPointLightMask( pEnc, pFrameDataBuffer);
        
        /// Point Light Rendering
        drawPointLights( pEnc, pFrameDataBuffer);
        
        /// SkyBox Renderring
        pEnc->pushDebugGroup( AAPLSTR( "Draw Sky" ) );
        pEnc->setRenderPipelineState( _pSkyboxPipelineState );
        pEnc->setDepthStencilState( _pDontWriteDepthStencilState );
        pEnc->setCullMode( MTL::CullModeFront );
        pEnc->setFragmentTexture( _pSkyMap, TextureIndexSkyMap );
        pEnc->setFrontFacingWinding( MTL::Winding::WindingCounterClockwise );
        
        for (auto& meshBuffer : _skyMesh.vertexBuffers()){
            pEnc->setVertexBuffer(meshBuffer.buffer(),
                                  meshBuffer.offset(),
                                  meshBuffer.argumentIndex());}
        
        for (auto& submesh : _skyMesh.submeshes()) {
            pEnc->drawIndexedPrimitives(submesh.primitiveType(),
                                        submesh.indexCount(),
                                        submesh.indexType(),
                                        submesh.indexBuffer().buffer(),
                                        submesh.indexBuffer().offset());
        }
        pEnc->popDebugGroup();
        
        drawPoints(pEnc , pFrameDataBuffer);
        
        pEnc->endEncoding();
    }
    
    if(pCurrentDrawable)
    {
        pCurrentDrawable->retain();
        pCmd->addScheduledHandler( [pCurrentDrawable, this]( MTL::CommandBuffer* ){
            pCurrentDrawable->present();
            pCurrentDrawable->release();});
        
    }
    
    pCmd->commit();
    pPool->release();
}

void Renderer::drawShadow(MTL::CommandBuffer * pCommandBuffer,  MTL::Buffer * pFrameDataBuffer, MTL::Buffer * pInstanceDataBuffer)
{
    MTL::RenderCommandEncoder* pEncoder = pCommandBuffer->renderCommandEncoder(_pShadowRenderPassDescriptor);
    pEncoder->setLabel( AAPLSTR( "Shadow Map Drawing" ) );
    pEncoder->setRenderPipelineState( _pShadowPipelineState);
    pEncoder->setDepthStencilState( _pShadowDepthStencilState );
    pEncoder->setVertexBuffer( _pVertexDataBuffer,      0, BufferIndexVertexData);
    pEncoder->setVertexBuffer(  pInstanceDataBuffer,    0, BufferIndexInstanceData );
    pEncoder->setVertexBuffer(  pFrameDataBuffer,       0, BufferIndexFrameData );
    pEncoder->setFrontFacingWinding( MTL::Winding::WindingCounterClockwise );
    pEncoder->setCullMode( MTL::CullModeBack );
    pEncoder->setDepthBias( 0.015, 7, 0.02 );
    pEncoder->drawIndexedPrimitives( primitiveType(),
                                    _indexCount, MTL::IndexType::IndexTypeUInt16,
                                    _pIndexBuffer,
                                     0,
                                     numberOfInstances());
    pEncoder->endEncoding();
}


void Renderer::drawPointLightMask(MTL::RenderCommandEncoder * pEncoder, MTL::Buffer * pFrameDataBuffer)
{
    pEncoder->pushDebugGroup( AAPLSTR( "Draw Light Mask" ) );
    
    pEncoder->setRenderPipelineState( _pLightMaskPipelineState );
    pEncoder->setDepthStencilState( _pLightMaskDepthStencilState );

    pEncoder->setStencilReferenceValue( 128 );
    pEncoder->setCullMode( MTL::CullModeFront );
    pEncoder->setFrontFacingWinding( MTL::Winding::WindingClockwise );
    
    //pEncoder->setVertexBuffer( pFrameDataBuffer, 0, BufferIndexFrameData );
    //pEncoder->setFragmentBuffer( pFrameDataBuffer, 0, BufferIndexFrameData );
    pEncoder->setVertexBuffer( _pLightsDataBuffer, 0, BufferIndexLightData );
    pEncoder->setVertexBuffer( _pLightPositionsBuffer[_frame], 0, BufferIndexLightsPosition );
    
    for( auto& vertexBuffers : _icosahedronMesh.vertexBuffers())
    {
        pEncoder->setVertexBuffer( vertexBuffers.buffer(),
                                   vertexBuffers.offset(),
                                   vertexBuffers.argumentIndex());
    }
    
    for( auto& icosahedronSubmesh : _icosahedronMesh.submeshes())
    {
        pEncoder->drawIndexedPrimitives(icosahedronSubmesh.primitiveType(),
                                        icosahedronSubmesh.indexCount(),
                                        icosahedronSubmesh.indexType(),
                                        icosahedronSubmesh.indexBuffer().buffer(),
                                        icosahedronSubmesh.indexBuffer().offset(),
                                        NumLights);
    }
    
    pEncoder->popDebugGroup();
}

void Renderer::drawPointLights(MTL::RenderCommandEncoder * pEncoder, MTL::Buffer * pFrameDataBuffer)
{
    pEncoder->pushDebugGroup( AAPLSTR( "Draw Point Lights" ) );
    pEncoder->setRenderPipelineState( _pLightPipelineState );
    //pEncoder->setFragmentTexture( _albedo_specular_GBuffer, RenderTargetAlbedo );
    //pEncoder->setFragmentTexture( _normal_shadow_GBuffer, RenderTargetNormal );
    //pEncoder->setFragmentTexture( _depth_GBuffer, RenderTargetDepth );

    // Call common base class method after setting state in the renderEncoder specific to the
    // traditional deferred renderer
    
    drawPointLightsCommon( pEncoder , pFrameDataBuffer );

    pEncoder->popDebugGroup();
}

void Renderer::drawPointLightsCommon(MTL::RenderCommandEncoder * pEncoder, MTL::Buffer * pFrameDataBuffer)
{
    pEncoder->setDepthStencilState( _pPointLightDepthStencilState );
    pEncoder->setStencilReferenceValue( 128 );
    pEncoder->setCullMode( MTL::CullModeBack );
    pEncoder->setFrontFacingWinding( MTL::Winding::WindingClockwise);
    ///pEncoder->setVertexBuffer( _pPointBuffer, 0, BufferIndexMeshPositions );
    //pEncoder->setVertexBuffer( pFrameDataBuffer, 0, BufferIndexFrameData );
    //pEncoder->setVertexBuffer( _pLightsDataBuffer, 0, BufferIndexLightData );
    //pEncoder->setVertexBuffer( _pLightPositionsBuffer[_frame], 0, BufferIndexLightsPosition );
    
    //pEncoder->setFragmentBuffer( pFrameDataBuffer, 0,   BufferIndexFrameData );
    pEncoder->setFragmentBuffer( _pLightsDataBuffer, 0, BufferIndexLightData );
    pEncoder->setFragmentBuffer( _pLightPositionsBuffer[_frame], 0, BufferIndexLightsPosition );
    
    /*
    for( auto& vertexBuffers : _icosahedronMesh.vertexBuffers())
    {
        pEncoder->setVertexBuffer( vertexBuffers.buffer(),
                                   vertexBuffers.offset(),
                                   vertexBuffers.argumentIndex());
    };
    */
    for( auto& icosahedronSubmesh : _icosahedronMesh.submeshes()){
        pEncoder->drawIndexedPrimitives(icosahedronSubmesh.primitiveType(),
                                        icosahedronSubmesh.indexCount(),
                                        icosahedronSubmesh.indexType(),
                                        icosahedronSubmesh.indexBuffer().buffer(),
                                        icosahedronSubmesh.indexBuffer().offset(),
                                        NumLights);
    };
}

void Renderer::drawPoints(MTL::RenderCommandEncoder* pEncoder, MTL::Buffer * pFrameDataBuffer)
{
    pEncoder->pushDebugGroup( AAPLSTR( "Draw Points" ) );
    pEncoder->setRenderPipelineState( _pPointPipelineState );
   // pEncoder->setDepthStencilState( _pDontWriteDepthStencilState );
    pEncoder->setCullMode( MTL::CullModeBack );
    pEncoder->setVertexBuffer( _pPointVertexBuffer, 0, BufferIndexPointVertexData);
   // pEncoder->setVertexBuffer( _pLightsDataBuffer, 0, BufferIndexLightData );
   // pEncoder->setVertexBuffer( _pLightPositionsBuffer[_frame], 0, BufferIndexLightsPosition );
   // pEncoder->setVertexBuffer( pFrameDataBuffer, 0, BufferIndexFrameData );
    pEncoder->setFragmentTexture( _pPointMap, TextureIndexAlpha );
    pEncoder->setFrontFacingWinding( MTL::Winding::WindingClockwise);
    pEncoder->drawPrimitives( MTL::PrimitiveTypeTriangleStrip,
                             (NS::UInteger)0,
                             (NS::UInteger)NumPointVertices,
                             (NS::UInteger)NumLights);
    pEncoder->popDebugGroup();
}

void Renderer::drawableSizeWillChange( const MTL::Size & size){

    float aspect = (float)size.width / float(size.height  != 0 ? size.height : 1);
    if (_aspect != aspect)  {
        _aspect = aspect;
    }
    
    float near = cameraData().near;
    float far = cameraData().far;
    
    _viewSize = CGSizeMake(size.width, size.height);
    _projectionMatrix = makePerspective(radians_from_degrees(45.f), _aspect, near, far );
    
    MTL::TextureDescriptor* _pGBufferTextureDesc = MTL::TextureDescriptor::alloc()->init();
    _pGBufferTextureDesc->setWidth( size.width );
    _pGBufferTextureDesc->setHeight( size.height);
    _pGBufferTextureDesc->setMipmapLevelCount( 1 );
    _pGBufferTextureDesc->setTextureType( MTL::TextureType2D );

    _pGBufferTextureDesc->setUsage( MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead );
    
    _pGBufferTextureDesc->setStorageMode( MTL::StorageModePrivate );

    _pGBufferTextureDesc->setPixelFormat( _albedo_specular_GBufferFormat );
    _albedo_specular_GBuffer = _pDevice->newTexture( _pGBufferTextureDesc );
    _albedo_specular_GBuffer->setLabel( AAPLSTR( "BDRF + specular GBuffer" ) );

    _pGBufferTextureDesc->setPixelFormat( _normal_shadow_GBufferFormat );
    _normal_shadow_GBuffer = _pDevice->newTexture( _pGBufferTextureDesc );
    _normal_shadow_GBuffer->setLabel( AAPLSTR( "Normal + Shadow GBuffer" ) );

    _pGBufferTextureDesc->setPixelFormat( _depth_GBufferFormat );
    _depth_GBuffer = _pDevice->newTexture( _pGBufferTextureDesc );
    _depth_GBuffer->setLabel( AAPLSTR( "Depth GBuffer" ) );
    
    _pGBufferTextureDesc->release();
   
    _pGBufferRenderPassDescriptor->colorAttachments()->object(RenderTargetAlbedo)->setTexture( _albedo_specular_GBuffer );
    _pGBufferRenderPassDescriptor->colorAttachments()->object(RenderTargetNormal)->setTexture( _normal_shadow_GBuffer );
    _pGBufferRenderPassDescriptor->colorAttachments()->object(RenderTargetDepth)->setTexture( _depth_GBuffer );
}

void Renderer::setInstances(const size_t& rows , const size_t& columns , const size_t& depth)
{
    if(rows != _instanceArray[0]){
        if(2 < rows || rows < 10){
            _instanceArray[0] = rows;
        } else {
            _instanceArray[0] = ROWS;;
        }
    }
    
    if(columns != _instanceArray[1]){
        if(2 < columns || columns < 10){
            _instanceArray[1] = columns;
        } else {
            _instanceArray[1] = COLUMNS;
        }
    }
    
    if(depth != _instanceArray[2]){
        if(2 < depth || depth < 10){
            _instanceArray[2] = depth;
        } else {
            _instanceArray[2] = DEPTH;
        }
    }
    _kNumInstances = (_instanceArray[0] * _instanceArray[1] * _instanceArray[2]);
}

const std::array <size_t, 3> Renderer::instances(){
    
    return _instanceArray;
}

const size_t& Renderer::numberOfInstances() {
    
    const size_t& multipliedInstances = (_instanceArray[0] * _instanceArray[1] * _instanceArray[2]);
    if( multipliedInstances != _kNumInstances ){
        _kNumInstances = multipliedInstances;
    } else {
        return _kNumInstances;
    }
    return _kNumInstances;
}

void Renderer::setCursorPosition( const simd_float2& newPostion )
{
    if( newPostion.x !=_cursorPosition.x)
    {
        _cursorPosition.x = newPostion.x;
    }
    if( newPostion.y != _cursorPosition.y)
    {
        _cursorPosition.y = newPostion.y;
    }
}

void Renderer::setMouseButtonMask( const NS::UInteger& newButtonMask )
{
    if(newButtonMask != _mouseButtonMask)
    {
        _mouseButtonMask = newButtonMask;
    }
}

void Renderer::setInstancesSize(const float& size){
    if( _instSize != size)
    {
        if( 0.01f < size || size > 1.0f ) {
            _instSize = size;
        }
    }
}

void Renderer::setGroupScale(const float &scale) {
    if( _objScale != scale)
    {
        if( 0.01f < scale || scale > 1.0f ) {
            _objScale = scale;
        }
    }
}

void Renderer::setTextureScale ( const float& scale ){
    if( _textureScale != scale ){
        if( 0.01f < scale || scale > 2.0f ) {
            _textureScale = scale;
        }
    }
}

void Renderer::setColorTargetPixelFormat( const MTL::PixelFormat & format){
    if(_colorTargetPixelFormat != format){
        _colorTargetPixelFormat = format;
    }
}

void Renderer::setDepthStencilTargetPixelFormat( const MTL::PixelFormat & format){
    if( _depthStencilTargetPixelFormat != format){
        _depthStencilTargetPixelFormat = format;
    }
}

void Renderer::setBaseColorMixValue(const float &value) {
    if( _baseColorMixValue != value){
        _baseColorMixValue = value;
    }
}

void Renderer::setMetallTextureValue(const float &value) {
    if( _metallTextureValue != value){
        _metallTextureValue = value;
    }
}

void Renderer::setRoughnessTextureValue(const float &value) {
    if( _roughnessTextureValue != value){
        _roughnessTextureValue = value;
    }
}

void Renderer::setPrimitiveType(const MTL::PrimitiveType &primitiveType){
    if(_primitiveType != primitiveType){
        _primitiveType = primitiveType;
    }
}

void Renderer::setStepPerFrameValue( const float & value){
    if(_stepPerFrame != value){
        _stepPerFrame = value;
    }
}

void Renderer::setNumFamilies(const int &value) {
    if ( value != num_families){
        num_families = value;
    }
}

void Renderer::setSenseAngleValue(const float &value ) {
    if(_senseAngleValue != value){
        _senseAngleValue = value;
    }
}

void Renderer::setTurnSpeedValue(const float &value) {
    if(_turnSpeedValue != value){
        _turnSpeedValue = value;
    }
}

void Renderer::setSenseOffsetValue(const float &value) {
    if(_senseOffsetValue != value){
        _senseOffsetValue = value;
    }
}

void Renderer::setEvaporationValue(const float &value) {
    if(_evaporationValue != value){
        _evaporationValue = value;
    }
}

void Renderer::setTrailWeightValue(const float &value) {
    if(_trailWeightValue != value){
        _trailWeightValue = value;
    }
}

void Renderer::setCameraData ( const struct CameraData & newCameraData){
    _cameraData = newCameraData;
}

void Renderer::setShadowCameraData ( const struct CameraData & newShadowCameraData){
    _shadowCameraData = newShadowCameraData;
}

void Renderer::cleanup()
{
    _pCommandQueue->release();
    _pVertexDataBuffer->release();
    _pIndexBuffer->release();
    _pShaderLibrary->release();
    _pSkyVertexDescriptor->release();
    _pSkyboxPipelineState->release();
    _pDontWriteDepthStencilState->release();
    _pComputePSO->release();
    _pTrailComputePSO->release();
    _pGBufferPipelineState->release();
    _pTexture->release();
    _pIrradianceMap->release();
    _pPreFilterMap->release();
    _pBDRFMap->release();
    _pSkyMap->release();
    _pShadowMap->release();
    _pPointMap->release();
    _albedo_specular_GBuffer->release();
    _normal_shadow_GBuffer->release();
    _depth_GBuffer->release();
    
    _pDevice->release();

    for(auto& del : _pMaterialTexture){
        del->release();
    }

    for(auto& del : _skyMesh.submeshes())
    {
        delete &del;
    }
    if(_skyMesh.submeshes().empty()){
        for (auto& del : _skyMesh.vertexBuffers())
        {
            delete &del;
        }
    }
    
    for(auto& del : _icosahedronMesh.submeshes())
    {
        delete &del;
    }
    
    if(_icosahedronMesh.submeshes().empty()){
        for (auto& del : _icosahedronMesh.vertexBuffers())
        {
            delete &del;
        }
    }
    
    for(auto& del : _pInstanceDataBuffer){
        del->release();
    }
    
    for(auto& del : _pFrameDataBuffer){
        del->release();
    }
    
    for(auto& del : _pLightPositionsBuffer){
        del->release();
    }
    
    for(auto& del : _pUniformsBuffer){
        del->release();
    }
}

void Renderer::updateDebugOutput() {
    
    
    std::cout <<  "   | ShadowViewMatrix float4x4" << std::endl;
    std::cout <<  "---+--------------------------------" << std::endl;
    for ( uint i = 0; i < 4; ++i ){
        std::cout <<  "m:" << i << "| " << shadowCameraData().viewMatrix.columns[i].x  << " " << shadowCameraData().viewMatrix.columns[i].y  << " " << shadowCameraData().viewMatrix.columns[i].z << " " << shadowCameraData().viewMatrix.columns[i].w << std::endl;
    }
    std::cout <<  "---+--------------------------------" << std::endl;
    
    std::cout <<  "   | viewMatrix float4x4" << std::endl;
    std::cout <<  "---+--------------------------------" << std::endl;
    for ( uint i = 0; i < 4; ++i ){
        std::cout <<  "n:" << i << "| " << cameraData().viewMatrix.columns[i].x  << " " <<cameraData().viewMatrix.columns[i].y  << " " << cameraData().viewMatrix.columns[i].z << " " << cameraData().viewMatrix.columns[i].w << std::endl;
    }
    std::cout <<  "---+--------------------------------" << std::endl << std::ends;
    
    std::cout <<  "   | CameraPosition float3" << std::endl;
    std::cout <<  "---+--------------------------------" << std::endl;
    std::cout <<  "c:" << "0" << "| " << cameraData().viewMatrix.columns[3].x  << " " << cameraData().viewMatrix.columns[3].y  << " " << cameraData().viewMatrix.columns[3].z << " " << std::endl;
    std::cout <<  "---+--------------------------------" << std::endl << std::ends;
    
    std::cout <<  "   | CameraDirection float4" << std::endl;
    std::cout <<  "---+--------------------------------" << std::endl;
    std::cout <<  "d:" << "0" << "| " << cameraData().cameraDirection.x  << " " << cameraData().cameraDirection.y  << " " << cameraData().cameraDirection.z  << " " << std::endl;
    std::cout <<  "---+--------------------------------" << std::endl << std::ends;
    
    std::cout <<  "   | SunEyeDirection float4" << std::endl;
    std::cout <<  "---+--------------------------------" << std::endl;
    std::cout <<  "e:" << "0" << "| " << cameraData().sunEyeDirection.x  << " " << cameraData().sunEyeDirection.y  << " " << cameraData().sunEyeDirection.z << " " << cameraData().sunEyeDirection.w << std::endl;
    std::cout <<  "---+--------------------------------" << std::endl << std::ends;
    
    std::cout <<  "   | SunLightPosition float4" << std::endl;
    std::cout <<  "---+--------------------------------" << std::endl;
    std::cout <<  "e:" << "0" << "| " << cameraData().sunLightPosition.x  << " " << cameraData().sunLightPosition.y  << " " << cameraData().sunLightPosition.z << " " << cameraData().sunLightPosition.w << std::endl;
    std::cout <<  "---+--------------------------------" << std::endl << std::ends;
    
    
    std::cout <<  "   | shadowProjectionMatrix float4x4" << std::endl;
    std::cout <<  "---+--------------------------------" << std::endl;
    for ( uint i = 0; i < 4; ++i ){
    std::cout <<  "s:" << "i" << "| " << shadowCameraData().projectionMatrix.columns[i].x  << " " << shadowCameraData().projectionMatrix.columns[i].y  << " " << shadowCameraData().projectionMatrix.columns[i].z << " " << shadowCameraData().projectionMatrix.columns[i].w << std::endl;
    }
    std::cout <<  "---+--------------------------------" << std::endl << std::ends;
    
}









