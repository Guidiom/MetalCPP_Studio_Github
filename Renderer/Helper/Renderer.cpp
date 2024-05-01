 ///
/// Renderer.cpp
/// MetalCCP
///
/// Created by Guido Schneider on 23.07.22.
///

#if __has_feature(objc_arc)
#error This file must be compiled with -fno-objc-arc
#endif

const bool _useMultisampleAntialiasing = true;

#define ROWS    2
#define COLUMNS 2
#define DEPTH   2

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

#undef NS_PRIVATE_IMPLEMENTATION
#undef MTL_PRIVATE_IMPLEMENTATION
#undef CA_PRIVATE_IMPLEMENTATION
#undef MTK_PRIVATE_IMPLEMENTATION

#include <simd/simd.h>
#include "AAPLUtilities.h"
#include "AAPLShaderTypes.h"
#include "Renderer.h"


Renderer::Renderer( MTK::View &pView )
: _pDevice( pView.device())
, _colorTargetPixelFormat (pView.colorPixelFormat())
, _depthStencilTargetPixelFormat(pView.depthStencilPixelFormat())
, _sampleCount(pView.sampleCount())
, _pShaderLibrary ( _pDevice->newDefaultLibrary() )
, _aspect (1.f)
, _angle (0.f)
, _frame (0)
, _mouseButtonMask(0)
, _cursorPosition{0, 0}
, _cameraPosition{0,0,0}
, _animationIndex(0)
, _instanceArray { size_t( ROWS ), size_t( COLUMNS ), size_t( DEPTH ) }
, _kNumInstances(numberOfInstances())
, _indexCount (0)
, _instSize(0.5f)
, _objScale(1.f)
, _textureScale(1.f)
, _stepPerFrame(MOVE_SPEED)
, _computedTextureValue(1.0f)
,  num_families(family)
,  num_particles(NS::UInteger(PARTICLE_N))
, _senseAngleValue(SENSE_ANGLE)
, _turnSpeedValue(TURN_SPEED)
, _senseOffsetValue(SENSE_OFFSET)
, _evaporationValue(EVAPORATION)
, _trailWeightValue(TRAIL_WEIGHT)

{
    _pCommandQueue = _pDevice->newCommandQueue();
    buildMainPipeline();
    buildComputePipeline();
    buildSkyPipeline();
    buildDepthStencilStates();
    buildTextures();
    buildBuffers();
    buildParticleBuffer();
    _semaphore = dispatch_semaphore_create( kMaxFramesInFlight );
}

Renderer::~Renderer()
{
    cleanup();
}

void Renderer::buildMainPipeline()
{
    NS::Error* pError = nullptr;

    MTL::PixelFormat depthStencilPixelFormat = depthStencilTargetPixelFormat();
    MTL::PixelFormat colorPixelFormat = colorTargetPixelFormat();

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

    MTL::Function* pVertexFn = _pShaderLibrary->newFunction( AAPLSTR( "vertexMain" ) );
    MTL::Function* pFragFn = _pShaderLibrary->newFunction( AAPLSTR( "fragmentMain" ) );
    AAPL_ASSERT( pVertexFn, "Failed to load vertex");
    AAPL_ASSERT( pFragFn, "Failed to load fragment");
    
    MTL::RenderPipelineDescriptor* pDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pDesc->setLabel( AAPLSTR( "Objects" ) );
    pDesc->setVertexDescriptor(_pVertexDescriptor);
    pDesc->setVertexFunction( pVertexFn );
    pDesc->setFragmentFunction( pFragFn );
    pDesc->colorAttachments()->object(0)->setPixelFormat( colorPixelFormat );
    pDesc->colorAttachments()->object(0)->setBlendingEnabled(true);
    pDesc->colorAttachments()->object(0)->setRgbBlendOperation(MTL::BlendOperationAdd);
    pDesc->colorAttachments()->object(0)->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
    pDesc->colorAttachments()->object(0)->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    pDesc->setDepthAttachmentPixelFormat( depthStencilPixelFormat );
    pDesc->setAlphaToOneEnabled(true);
    pDesc->setStencilAttachmentPixelFormat( depthStencilPixelFormat );
    if(_useMultisampleAntialiasing == true){
        pDesc->setSampleCount(sampleCount());};
    _pRPState = _pDevice->newRenderPipelineState( pDesc, &pError );
    AAPL_ASSERT_NULL_ERROR( pError, "Failed to create render pipeline state ");
    _pVertexDescriptor->release();
    pVertexFn->release();
    pFragFn->release();
    pDesc->release();

///Shadow pass render pipeline setup

    MTL::Function* pShadowVertexFn = _pShaderLibrary->newFunction( AAPLSTR( "shadow_vertexMain" ) );

    MTL::Function* pShadowFragFn = _pShaderLibrary->newFunction( AAPLSTR( "shadow_fragmentMain" ) );

    AAPL_ASSERT( pShadowVertexFn, "Failed to load vertex");
    AAPL_ASSERT( pShadowFragFn, "Failed to load fragment");

    MTL::RenderPipelineDescriptor* pshadowDesc = MTL::RenderPipelineDescriptor::alloc()->init();

    pshadowDesc->setLabel( AAPLSTR( "Shadows" ) );
    pshadowDesc->setVertexDescriptor(nullptr);
    pshadowDesc->setVertexFunction( pShadowVertexFn );
    pshadowDesc->setFragmentFunction( nullptr );

    pshadowDesc->setDepthAttachmentPixelFormat( depthStencilPixelFormat );

    _pRPShadowState = _pDevice->newRenderPipelineState( pshadowDesc, &pError );

    AAPL_ASSERT_NULL_ERROR( pError, "Failed to create render pipeline state ");

    pshadowDesc->release();
    pShadowVertexFn->release();

}

void Renderer::buildComputePipeline()
{
    NS::Error* pError = nullptr;

    MTL::Function* pInitComputeFn = _pShaderLibrary->newFunction( AAPLSTR( "init_function" ) );
    MTL::Function* pComputeFn = _pShaderLibrary->newFunction( AAPLSTR( "compute_function" ) );
    MTL::Function* pTrailFn = _pShaderLibrary->newFunction( AAPLSTR( "trail_function" ));
    MTL::Function* pUpdateFamilyFn = _pShaderLibrary->newFunction( AAPLSTR( "update_family_function" ));
    MTL::Function* pInteractionsFn = _pShaderLibrary->newFunction( AAPLSTR( "interactions_function" ));

    AAPL_ASSERT( pInitComputeFn, "init_function failed to load!");
    AAPL_ASSERT( pComputeFn, "compute_function failed to load!");
    AAPL_ASSERT( pTrailFn, "trail_function failed to load!");
    AAPL_ASSERT( pUpdateFamilyFn, "update_family_function failed to load!");
    AAPL_ASSERT( pInteractionsFn, "interactions_function failed to load!");

    _pInitComputePSO = _pDevice->newComputePipelineState( pInitComputeFn, &pError );
    AAPL_ASSERT_NULL_ERROR(pError, "Failed to create init pipeline state ");
    _pComputePSO = _pDevice->newComputePipelineState( pComputeFn, &pError );
    AAPL_ASSERT_NULL_ERROR(pError, "Failed to create compute pipeline state ");
    _pTrailComputePSO = _pDevice->newComputePipelineState(pTrailFn ,&pError);
    AAPL_ASSERT_NULL_ERROR(pError , "Failed to create trail pipeline state ");
    _pUpdateFamilyComputePSO =_pDevice->newComputePipelineState(pUpdateFamilyFn ,&pError);
    AAPL_ASSERT_NULL_ERROR(pError , "Failed to create UpdateFamily pipeline state ");
    _pInteractionsComputePSO = _pDevice->newComputePipelineState(pInteractionsFn ,&pError);
    AAPL_ASSERT_NULL_ERROR(pError , "Failed to create interactions pipeline state ");

    pInteractionsFn->release();
    pUpdateFamilyFn->release();
    pTrailFn->release();
    pComputeFn->release();
    pInitComputeFn->release();
}

void Renderer::buildSkyPipeline(){
    
    NS::Error* pError = nullptr;
    
    MTL::PixelFormat depthStencilPixelFormat = depthStencilTargetPixelFormat();
    MTL::PixelFormat colorPixelFormat = colorTargetPixelFormat();
    
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
    
    MTL::RenderPipelineDescriptor* pSkyDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pSkyDesc->setLabel( AAPLSTR( "Sky" ) );
    pSkyDesc->setVertexDescriptor( _pSkyVertexDescriptor );
    pSkyDesc->setVertexFunction( pSkyboxVertexFunction );
    pSkyDesc->setFragmentFunction( pSkyboxFragmentFunction );
    pSkyDesc->colorAttachments()->object(0)->setPixelFormat( colorPixelFormat );
    pSkyDesc->setDepthAttachmentPixelFormat( depthStencilPixelFormat );
    pSkyDesc->setStencilAttachmentPixelFormat( depthStencilPixelFormat );
    if(_useMultisampleAntialiasing){ pSkyDesc->setSampleCount(sampleCount());}
    _pRPSkyboxState = _pDevice->newRenderPipelineState( pSkyDesc, &pError );
    AAPL_ASSERT_NULL_ERROR( pError, "Failed to create skybox render pipeline state:" );
    pSkyboxVertexFunction->release();
    pSkyboxFragmentFunction->release();
    pSkyDesc->release();
    
    /// Load Mesh for Sky-Environment
    _skyMesh = makeSphereMesh(_pDevice, *_pSkyVertexDescriptor, 80.0f, 80.0f, 150.f );
    _pSkyVertexDescriptor->release();
}

void Renderer::buildDepthStencilStates()
{
    MTL::DepthStencilDescriptor* pDsDesc = MTL::DepthStencilDescriptor::alloc()->init();
    pDsDesc->setLabel( AAPLSTR( "write" ) );
    pDsDesc->setDepthCompareFunction( MTL::CompareFunction::CompareFunctionLess );
    pDsDesc->setDepthWriteEnabled( true );
    _pDepthStencilState = _pDevice->newDepthStencilState( pDsDesc );
    pDsDesc->release();
    
    MTL::DepthStencilDescriptor* pDsDesc2 = MTL::DepthStencilDescriptor::alloc()->init();
    pDsDesc2->setLabel( AAPLSTR( "dont write" ) );
    pDsDesc2->setDepthCompareFunction( MTL::CompareFunction::CompareFunctionLess );
    pDsDesc2->setDepthWriteEnabled( false );
    _pDontWriteDepthStencilState = _pDevice->newDepthStencilState( pDsDesc2 );
    pDsDesc2->release();

    MTL::DepthStencilDescriptor* pDsDesc3 = MTL::DepthStencilDescriptor::alloc()->init();
    pDsDesc3->setLabel( AAPLSTR( "Shadow Gen" ) );
    pDsDesc3->setDepthCompareFunction( MTL::CompareFunctionLessEqual );
    pDsDesc3->setDepthWriteEnabled( true );
    _pShadowDepthStencilState = _pDevice->newDepthStencilState( pDsDesc3 );
    pDsDesc3->release();
}

void Renderer::buildTextures()
{
    MTL::PixelFormat colorPixelFormat = colorTargetPixelFormat();
    MTL::TextureDescriptor* pTextureDesc = MTL::TextureDescriptor::alloc()->init();
    pTextureDesc->setWidth( kTextureWidth );
    pTextureDesc->setHeight( kTextureHeight );
    pTextureDesc->setSampleCount(NS::UInteger(sampleCount()/sampleCount()));
    pTextureDesc->setPixelFormat( colorPixelFormat);
    pTextureDesc->setTextureType( MTL::TextureType2D );
    pTextureDesc->setStorageMode( MTL::StorageModePrivate );
    pTextureDesc->setUsage( MTL::ResourceUsageSample | MTL::ResourceUsageRead | MTL::ResourceUsageWrite);
    MTL::Texture *pTexture = _pDevice->newTexture( pTextureDesc );
    _pTexture = pTexture;
    pTextureDesc->release();

    MTL::Texture  *pBaseTexture = newTextureFromCatalog(_pDevice, "BaseColorMap", MTL::StorageModePrivate, MTL::TextureUsageShaderRead);
    _pMaterialTexture[0] = pBaseTexture;
    _pMaterialTexture[0]->allowGPUOptimizedContents();
    _pMaterialTexture[0]->newTextureView(MTL::PixelFormatRGBA16Float);
//    _pMaterialTexture[0]->release();

    MTL::Texture  *pNormalTexture = newTextureFromCatalog(_pDevice, "NormalMap", MTL::StorageModePrivate, MTL::TextureUsageShaderRead);
    _pMaterialTexture[1] = pNormalTexture;
    _pMaterialTexture[1]->allowGPUOptimizedContents();
    _pMaterialTexture[1]->newTextureView(MTL::PixelFormatRGBA16Float);
//    _pMaterialTexture[1]->release();

    MTL::Texture  *pMetallicTexture = newTextureFromCatalog(_pDevice, "MetallicMap", MTL::StorageModePrivate, MTL::TextureUsageShaderRead);
    _pMaterialTexture[2] = pMetallicTexture;
    _pMaterialTexture[2]->allowGPUOptimizedContents();
    _pMaterialTexture[2]->newTextureView(MTL::PixelFormatRGBA16Float);
//    _pMaterialTexture[2]->release();

    MTL::Texture  *pRoughnessTexture = newTextureFromCatalog(_pDevice, "RoughnessMap", MTL::StorageModePrivate, MTL::TextureUsageShaderRead);
    _pMaterialTexture[3] = pRoughnessTexture;
    _pMaterialTexture[3]->allowGPUOptimizedContents();
    _pMaterialTexture[3]->newTextureView(MTL::PixelFormatRGBA16Float);
//    pRoughnessTexture->autorelease();

    MTL::Texture  *pAOTexture = newTextureFromCatalog(_pDevice, "AOMap", MTL::StorageModePrivate, MTL::TextureUsageShaderRead);
    _pMaterialTexture[4] = pAOTexture;
    _pMaterialTexture[4]->allowGPUOptimizedContents();
    _pMaterialTexture[4]->newTextureView(MTL::PixelFormatRGBA16Float);
//    _pMaterialTexture[4]->release();

    MTL::Texture *pIrradianceMap = newTextureFromCatalog( _pDevice, "IrradianceMap", MTL::StorageModePrivate, MTL::TextureUsageShaderRead );
    _pIrradianceMap = pIrradianceMap;
    _pIrradianceMap->allowGPUOptimizedContents();
    _pIrradianceMap->newTextureView(MTL::PixelFormatRGBA16Float);
//    _pIrradianceMap->release();

    MTL::Texture *pPreFilterMap= newTextureFromCatalog(_pDevice, "PreFilterMap", MTL::StorageModePrivate, MTL::TextureUsageShaderRead );
    _pPreFilterMap = pPreFilterMap;
    _pPreFilterMap->allowGPUOptimizedContents();
    _pPreFilterMap->newTextureView(MTL::PixelFormatRGBA16Float);
//    _pPreFilterMap->release();

    MTL::Texture *pBDRFMap= newTextureFromCatalog(_pDevice, "BDRFMap", MTL::StorageModePrivate, MTL::TextureUsageShaderRead );
    _pBDRFMap = pBDRFMap;
    _pBDRFMap->allowGPUOptimizedContents();
    _pBDRFMap->newTextureView(MTL::PixelFormatRG16Float);
//    _pBDRFMap->release();

    MTL::Texture *pSkyMap = newTextureFromCatalog( _pDevice , "IrradianceMap", MTL::StorageModePrivate, MTL::TextureUsageShaderRead);
    _pSkyMap = pSkyMap;
    _pSkyMap->allowGPUOptimizedContents();
    _pSkyMap->newTextureView(MTL::PixelFormatRGBA16Float);
//    _pSkyMap->release();

    MTL::TextureDescriptor* pShadowDesc = MTL::TextureDescriptor::alloc()->init();
    //pShadowDesc->setWidth( kTextureWidth );
    //pShadowDesc->setHeight( kTextureHeight );
    pShadowDesc->setSampleCount(NS::UInteger(sampleCount()/sampleCount()));
    pShadowDesc->setPixelFormat( colorPixelFormat );
    pShadowDesc->setTextureType( MTL::TextureType2D );
    pShadowDesc->setStorageMode( MTL::StorageModePrivate );
    pShadowDesc->setUsage( MTL::ResourceUsageSample | MTL::ResourceUsageRead | MTL::ResourceUsageWrite);
    MTL::Texture *pShadowTexture = _pDevice->newTexture( pShadowDesc );
    _pShadowTexture = pShadowTexture;
   // pShadowTexture->release();

    _pShaderLibrary->release();
}

void Renderer::step_animation()
{
    t_transformation = fmod(1.0 + t_transformation + dir * TRANSFORMATION_SPEED, 1.0);
    t_rotation = fmod(1.0 + t_rotation + dir * ROTATION_SPEED, 1.0);
}

void Renderer::buildBuffers()
{
    using simd::float3;
    using simd::float2;
    
    /// properties erstellen

    std::vector <float3> _positions;
    std::vector <float2> _uv;
    std::vector <float3> _normals;
    std::vector <int16_t>_indices;
    
    const int16_t X_SEGMENTS = 98;
    const int16_t Y_SEGMENTS = 98;

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
    
    MTL::Buffer* pVertexBuffer = _pDevice->newBuffer( vertexDataSize, MTL::ResourceStorageModeManaged );
    MTL::Buffer* pIndexBuffer  = _pDevice->newBuffer( indexDataSize, MTL::ResourceStorageModeManaged );
    
    _pVertexDataBuffer = pVertexBuffer;
    _pIndexBuffer = pIndexBuffer;
    
    memcpy( _pVertexDataBuffer->contents(), verts, vertexDataSize );
    memcpy( _pIndexBuffer->contents(), indices, indexDataSize );
    
    _pVertexDataBuffer->didModifyRange( NS::Range::Make( 0, _pVertexDataBuffer->length() ) );
    _pIndexBuffer->didModifyRange( NS::Range::Make( 0, _pIndexBuffer->length() ) );
    
    const size_t instanceDataSize = kMaxFramesInFlight * numberOfInstances() * sizeof( InstanceData );
    for ( size_t i = 0; i < kMaxFramesInFlight; ++i )
    {
        _pInstanceDataBuffer[ i ] = _pDevice->newBuffer( instanceDataSize, MTL::ResourceStorageModeManaged);
    }
    
    const size_t frameDataSize = kMaxFramesInFlight * sizeof( FrameData );
    for ( size_t i = 0; i < kMaxFramesInFlight; ++i )
    {
        _pFrameDataBuffer[ i ] = _pDevice->newBuffer( frameDataSize, MTL::ResourceStorageModeManaged );
    }
}

void Renderer::buildParticleBuffer()
{
    NS::Error * pError = nullptr;

    _pTextureAnimationBuffer = _pDevice->newBuffer( sizeof(uint), MTL::ResourceStorageModeManaged );
    _pTimeBuffer = _pDevice->newBuffer( sizeof(float), MTL::ResourceStorageModeManaged );

    const size_t unifornDataSize = kMaxFramesInFlight * sizeof( Uniforms );
    for ( size_t i = 0; i < kMaxFramesInFlight; ++i )
    {
        _pUniformsBuffer[i] = _pDevice->newBuffer( unifornDataSize, MTL::ResourceStorageModeManaged );
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
                float(random_float( 0.f, 1.f ) * 2 * PI)
            },
            simd::int4 {
                int(0),
                int(1),
                int(1),
                int(1)
            }
        };
    }

    const size_t particleDataSize = sizeof( particles );

    MTL::Buffer* pParticleBuffer = _pDevice->newBuffer( particleDataSize, MTL::ResourceStorageModeManaged );
    AAPL_ASSERT( pParticleBuffer->length() != particleDataSize * sizeof(Particle), pError);
    _pParticleBuffer = pParticleBuffer;

    memcpy(_pParticleBuffer->contents(), particles, particleDataSize );

    _pParticleBuffer->didModifyRange( NS::Range::Make( 0, _pParticleBuffer->length() ) );

    initCompute = false;
}

void Renderer::generateComputedTexture( MTL::CommandBuffer* pCommandBuffer, size_t _frame )
{
    using simd::float2;
    using simd::float4;

    AAPL_ASSERT( pCommandBuffer, "CommandBuffer for Kernel Computing not valid");

    uint* ptr = reinterpret_cast<uint*>(_pTextureAnimationBuffer->contents());
    *ptr = (_animationIndex++) % 3;
    _pTextureAnimationBuffer->didModifyRange(NS::Range::Make(0, sizeof(uint)));


    float* delta= reinterpret_cast<float*>(_pTimeBuffer->contents());
    double currentTime = CFAbsoluteTime();
    if(previousTime != previousTime) {
        *delta = fmin(float(currentTime - previousTime), 1.f / 30.f);
    } else {
        *delta = (1.f / 60.f);
    }
    previousTime = currentTime;
    _pTimeBuffer->didModifyRange(NS::Range::Make(0, sizeof(float)));

    MTL::Buffer *pUniformsBuffer = _pUniformsBuffer [ _frame ];
    Uniforms * uniforms = reinterpret_cast<Uniforms*>(pUniformsBuffer->contents());
    uniforms->particleCount = num_particles;
    uniforms->sensorAngle = senseAngleValue() * PI ;
    uniforms->sensorOffset = senseOffsetValue();
    uniforms->sensorSize = uint(SENSOR_SIZE);
    uniforms->evaporation = evaporationValue();
    uniforms->turnSpeed = turnSpeedValue();
    uniforms->trailWeight = trailWeightValue();
    uniforms->Dimensions = simd::uint2{ kTextureWidth, kTextureHeight };
    uniforms->moveSpeed = stepPerFrameValue();
    uniforms->family = get_num_families();
    pUniformsBuffer->didModifyRange( NS::Range::Make( 0, pUniformsBuffer->length() ) );

    if(!initCompute)
    {
        MTL::ComputeCommandEncoder* pInitComputeEncoder = pCommandBuffer->computeCommandEncoder();
        pInitComputeEncoder->setComputePipelineState(_pInitComputePSO);
        pInitComputeEncoder->setLabel(AAPLSTR("Initialize"));
        pInitComputeEncoder->setBuffer(  _pParticleBuffer , 0, BufferIndexParticleData );
        pInitComputeEncoder->setBuffer(  pUniformsBuffer, 0, BufferIndexUniformData);
        pInitComputeEncoder->setTexture( _pTexture, TextureIndexReadMap);
        pInitComputeEncoder->setTexture( _pTexture, TextureIndexWriteMap);
        MTL::Size threadsPerGrid = MTL::Size( NS::Integer( num_particles), 1, 1 );
        MTL::Size threadsPerThreadgroup = MTL::Size( 1, 1, 1 );
        pInitComputeEncoder->dispatchThreads(threadsPerGrid, threadsPerThreadgroup);
        pInitComputeEncoder->endEncoding();
        initCompute = true;
    }

    MTL::ComputeCommandEncoder* pComputeEncoder = pCommandBuffer->computeCommandEncoder();
    pComputeEncoder->setComputePipelineState(_pComputePSO);
    pComputeEncoder->setLabel(AAPLSTR("Computing"));
    pComputeEncoder->setTexture( _pTexture, TextureIndexReadMap);
    pComputeEncoder->setTexture( _pTexture, TextureIndexWriteMap);
    pComputeEncoder->setBuffer( _pParticleBuffer, 0, BufferIndexParticleData );
    pComputeEncoder->setBuffer(  pUniformsBuffer, 0, BufferIndexUniformData);
    pComputeEncoder->setBuffer( _pTextureAnimationBuffer, 0, BufferIndexAnimationData);
    pComputeEncoder->setBuffer( _pTimeBuffer, 0 , BufferIndexTimeData);
    MTL::Size threadsPerThreadgroup = MTL::Size( 1, 1, 1 );
    MTL::Size threadsPerGrid = MTL::Size( NS::Integer( num_particles)  , 1, 1 );
    pComputeEncoder->dispatchThreads(threadsPerGrid, threadsPerThreadgroup);
    pComputeEncoder->endEncoding();

    MTL::ComputeCommandEncoder* pTrailComputeEncoder = pCommandBuffer->computeCommandEncoder();
    pTrailComputeEncoder->setLabel(AAPLSTR("Trail&Deposit"));
    pTrailComputeEncoder->setComputePipelineState(_pTrailComputePSO);
    pTrailComputeEncoder->setTexture( _pTexture, TextureIndexReadMap);
    pTrailComputeEncoder->setTexture( _pTexture, TextureIndexWriteMap);
    pTrailComputeEncoder->setBuffer( _pParticleBuffer, 0, BufferIndexParticleData );
    pTrailComputeEncoder->setBuffer(  pUniformsBuffer, 0, BufferIndexUniformData );
    pTrailComputeEncoder->setBuffer( _pTextureAnimationBuffer, 0, BufferIndexAnimationData);
    NS::UInteger width = NS::UInteger(_pComputePSO->threadExecutionWidth());
    NS::UInteger height = NS::UInteger(_pComputePSO->maxTotalThreadsPerThreadgroup()) / width;
    threadsPerThreadgroup = MTL::Size( 16 , 16 , 1);

    width = NS::UInteger( kTextureWidth );
    height = NS::UInteger( kTextureHeight );
    threadsPerGrid = MTL::Size( width , height, 1);

    pTrailComputeEncoder->dispatchThreads(threadsPerGrid, threadsPerThreadgroup);
    pTrailComputeEncoder->endEncoding();

    if( uniforms->family != num_families){
        MTL::ComputeCommandEncoder * pUpdateFamilyEncoder = pCommandBuffer->computeCommandEncoder();
        uniforms->family = num_families;
        pUpdateFamilyEncoder->setComputePipelineState(_pUpdateFamilyComputePSO);
        pUpdateFamilyEncoder->setLabel(AAPLSTR("UpdateFamily"));
        pUpdateFamilyEncoder->setBuffer( _pParticleBuffer, 0, BufferIndexParticleData );
        pUpdateFamilyEncoder->setBuffer(  pUniformsBuffer, 0, BufferIndexUniformData);
        MTL::Size threadsPerThreadgroup = MTL::Size( 1, 1, 1 );
        MTL::Size threadsPerGrid = MTL::Size(  num_particles  , 1, 1 );
        pUpdateFamilyEncoder->dispatchThreads(threadsPerGrid, threadsPerThreadgroup);
        pUpdateFamilyEncoder->endEncoding();
        } else {
    return;
    }

    MTL::ComputeCommandEncoder* pInteractionsComputeEncoder = pCommandBuffer->computeCommandEncoder();
    pInteractionsComputeEncoder->setComputePipelineState(_pInteractionsComputePSO);
    pInteractionsComputeEncoder->setLabel(AAPLSTR("Interactions"));
    pInteractionsComputeEncoder->setTexture( _pTexture, TextureIndexReadMap);
    pInteractionsComputeEncoder->setTexture( _pTexture, TextureIndexWriteMap);
    pInteractionsComputeEncoder->setBuffer( _pParticleBuffer, 0, BufferIndexParticleData);
    pInteractionsComputeEncoder->setBuffer(  pUniformsBuffer, 0, BufferIndexUniformData);
    pInteractionsComputeEncoder->setBytes( (void*)_pInterActionBuffer, 4 , 4);
    threadsPerThreadgroup = MTL::Size( 1, 1, 1 );
    threadsPerGrid = MTL::Size( num_particles  , 1, 1 );
    pInteractionsComputeEncoder->dispatchThreads(threadsPerGrid, threadsPerThreadgroup);
    pInteractionsComputeEncoder->endEncoding();
}

void Renderer::draw( MTK::View * pView )
{
    using simd::float3;
    using simd::float4;
    using simd::float4x4;

    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();
    
    step_animation();

    const size_t kInstanceRows    = instances()[0];
    const size_t kInstanceColumns = instances()[1];
    const size_t kInstanceDepth   = instances()[2];

    _frame = (_frame + 1) % kMaxFramesInFlight;
    _frameRate = ( _frameRate + 1) % kFrameRate;

    MTL::Buffer* pInstanceDataBuffer = _pInstanceDataBuffer[ _frame ];
    InstanceData* pInstanceData = reinterpret_cast< InstanceData *>( pInstanceDataBuffer->contents() );
    
    MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();
    dispatch_semaphore_wait( _semaphore, DISPATCH_TIME_FOREVER );
    Renderer* pRenderer = this;
    pCmd->addCompletedHandler( ^void( MTL::CommandBuffer* pCmd ){ dispatch_semaphore_signal( pRenderer->_semaphore );});

    _angle += 0.002f;
//    qNormalizeAngle(_angle);
//    float xRotate = 360.f * ease_circular_in_out(t_rotation);
//    qNormalizeAngle(xRotate);
//    float yRotate = 360.f * ease_quad_in_out(_angle);
//    qNormalizeAngle(yRotate);

    const float scl = instancesSize();
    const float distance = sqrt(numberOfInstances());

    float3 objectPosition { 0.f, 0.f, 2.f - distance };
    float4x4 rt = makeTranslate( objectPosition );
    float4x4 rr1 = makeYRotate( -_angle );
    float4x4 rr0 = makeXRotate( _angle / 0.5);
    float4x4 rtInv = makeTranslate( (float3){ -objectPosition.x, -objectPosition.y, -objectPosition.z } );
    float4x4 rscale = makeScale ( (float3) { 1.f , 1.f,  1.f} );
    float4x4 modelMatrix = rt * rr1 * rr0 * rscale * rtInv;
    
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

        //float4x4 zrot = makeZRotate( (_angle /  xRotate  ) * sinf((float)ix) );
        //float4x4 yrot = makeYRotate( (_angle /  xRotate  ) * cosf((float)iy) );

        float4x4 zrot = makeZRotate( _angle  * sinf((float)ix) );
        float4x4 yrot = makeYRotate( _angle  * cosf((float)iy) );

        float x = ((float)ix - (float)kInstanceRows   /2.f) * (1.5f) ;
        float y = ((float)iy - (float)kInstanceColumns/2.f) * (1.5f) ;
        float z = ((float)iz - (float)kInstanceDepth  /2.f) * (1.5f) ;

        float4x4 translate = makeTranslate( add( objectPosition , (float3){ x, y, z }) );
        float4x4 scale = makeScale( (float3){ scl, scl , scl } );

        pInstanceData[ i ].instanceTransform = modelMatrix * translate * yrot * zrot * scale;
        pInstanceData[ i ].instanceNormalTransform = discardTranslation( pInstanceData[ i ].instanceTransform );
        
        float iDivNumInstances = i / (float) numberOfInstances();
        float r = sinf(iDivNumInstances);
        float g = cosf(iDivNumInstances);
        float b = sinf( PI * 2.0f * iDivNumInstances );
        pInstanceData[ i ].instanceColor = (float4){ r, g, b, 1.0 };
        ix += 1;

    }
    pInstanceDataBuffer->didModifyRange( NS::Range::Make( 0, pInstanceDataBuffer->length()));
    
    // Update camera, view matrix, state:
    MTL::Buffer* pFrameDataBuffer = _pFrameDataBuffer[ _frame ];
    FrameData* pFrameData = reinterpret_cast< FrameData *>( pFrameDataBuffer->contents());
    viewMatrix  = getCameraViewMatrix();
    std::cout << getCameraPosition().x << " " << getCameraPosition().y << " " << getCameraPosition().z << std::endl;

    static const vector_float3 lightColor[2]    = {(vector_float3) { 300.f, 300.f, 300.f },
                                                   (vector_float3) { 300.f, 300.f, 300.f }};
    static const vector_float3 lightPosition[2] = {(vector_float3) {  10.f,  10.f,  10.f },
                                                   (vector_float3) { -10.f,  10.f,  10.f }};
    vector_float4 lightModelPosition[2];
    vector_float4 lightWorldDirection[2];

    for (unsigned int i = 0 ; i < 2 ; ++i)
    {
        /// Update light direction in view space
        vector_float3 newLeightPos = lightPosition[i] + (simd_float3){ sin(_frame * 5.f) * 5.f, 0.f, 0.f};
        pFrameData->lightPosition[i] = newLeightPos;
        pFrameData->lightColor[i] = lightColor[i];
    }

    simd_float4 liT;
    for (unsigned int i = 0 ; i < 2 ; ++i)
    {
            liT.xyzw = (float4){
            pFrameData->lightPosition[i].x,
            pFrameData->lightPosition[i].y,
            pFrameData->lightPosition[i].z,
            float{0}
            };
        const vector_float4 &lightModelPosition[i] = vector_float4{liT.x, liT.y, liT.z, 0.f};
    }

    for (unsigned int i = 0 ; i < 2 ; ++i)
    {
        lightModelPosition[i] = matrix_multiply( pFrameData->worldTransform,lightModelPosition[i]);

        lightWorldDirection[i] = - lightModelPosition[i];
    }

    //vector_float4 hasi = simd_mul(    )( viewMatrix , lightWorldDirection);
    pFrameData->light_eye_direction = vector_float4 ( simd_mul( viewMatrix , lightWorldDirection));
    pFrameData->cameraPos = getCameraPosition();
    pFrameData->textureScale = textureScale();
    pFrameData->colorMixBias = computedTextureValue();
    pFrameData->metallnessBias = metallTextureValue();
    pFrameData->roughnessBias = roughnessTextureValue();
    pFrameData->worldTransform = viewMatrix;
    pFrameData->worldNormalTransform = discardTranslation(pFrameData->worldTransform);
    pFrameData->perspectiveTransform = _projectionMatrix;

    vector_float4 directionalLightUpVector = {0.0, 1.0, 1.0, 1.0};
    directionalLightUpVector = matrix_multiply(_projectionMatrix, directionalLightUpVector);
    directionalLightUpVector.xyz = simd::normalize(directionalLightUpVector.xyz);

    matrix_float4x4 * shadowViewMatrix = matrix_look_at_left_hand(lightWorldDirection[i].xyz / 10,
                                                                  (float3){0,0,0},
                                                                  directionalLightUpVector.xyz);

    matrix_float4x4 shadowModelViewMatrix = matrix_multiply((matrix_float4x4) shadowViewMatrix ,(matrix_float4x4) templeModelMatrix);
    pFrameData->shadow_mvp_matrix = matrix_multiply(_projectionMatrix , shadowModelViewMatrix);



    // When calculating texture coordinates to sample from shadow map, flip the y/t coordinate and
    // convert from the [-1, 1] range of clip coordinates to [0, 1] range of
    // used for texture sampling
    float4x4 shadowScale = matrix4x4_scale(0.1f, -0.1f, 1.0);
    float4x4 shadowTranslate = matrix4x4_translation(0.5, 0.5, 0);
    float4x4 shadowTransform = shadowTranslate * shadowScale;

    pFrameData->shadow_mvp_xform_matrix = shadowTransform * pFrameData->shadow_mvp_matrix;


    pFrameDataBuffer->didModifyRange( NS::Range::Make( 0, sizeof( FrameData) ) );

    /// Update texture:
    generateComputedTexture( pCmd , _frame);

    /// Begin render pass:
    MTL::RenderPassDescriptor* pRpd = pView->currentRenderPassDescriptor();
    MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder( pRpd );
    pEnc->setRenderPipelineState( _pRPState );
    pEnc->setVertexBuffer( _pVertexDataBuffer,       /* offset */  0, BufferIndexVertexData );
    pEnc->setVertexBuffer(  pInstanceDataBuffer,     /* offset */  0, BufferIndexInstanceData );
    pEnc->setVertexBuffer(  pFrameDataBuffer,        /* offset */  0, BufferIndexFrameData );
    pEnc->setFragmentBuffer(  pFrameDataBuffer,      /* offset */  0, BufferIndexFrameData );
    pEnc->setFragmentTexture( _pMaterialTexture[0], TextureIndexBaseColor );
    pEnc->setFragmentTexture( _pMaterialTexture[1], TextureIndexNormal);
    pEnc->setFragmentTexture( _pMaterialTexture[2], TextureIndexMetallic);
    pEnc->setFragmentTexture( _pMaterialTexture[3], TextureIndexRoughness);
    pEnc->setFragmentTexture( _pMaterialTexture[4], TextureIndexAmbientOcclusion);
    pEnc->setFragmentTexture( _pIrradianceMap, TextureIndexIrradianceMap );
    pEnc->setFragmentTexture( _pPreFilterMap, TextureIndexPreFilterMap );
    pEnc->setFragmentTexture( _pBDRFMap, TextureIndexBDRF );
    pEnc->setFragmentTexture( _pTexture, TextureIndexWriteMap );
    pEnc->setDepthStencilState( _pDepthStencilState );
    pEnc->setCullMode( MTL::CullModeBack);
    pEnc->setFrontFacingWinding( MTL::Winding::WindingCounterClockwise );
    pEnc->drawIndexedPrimitives( primitiveType(),
                                _indexCount, MTL::IndexType::IndexTypeUInt16,
                                _pIndexBuffer,
                                0,
                                numberOfInstances() );

/*
    MTL::RenderPassDescriptor * _pShadowRPD = pView->currentRenderPassDescriptor();
    _pShadowRPD->depthAttachment()->setTexture( _pShadowTexture );
    _pShadowRPD->depthAttachment()->setLoadAction( MTL::LoadActionClear );
    _pShadowRPD->depthAttachment()->setStoreAction( MTL::StoreActionStore );
    _pShadowRPD->depthAttachment()->setClearDepth( 1.0 );
*/

     //pEnc = pCmd->renderCommandEncoder( _pShadowRPD );

    drawShadow( pEnc, pCmd,  _frame);

    pEnc->pushDebugGroup( AAPLSTR( "Draw Sky" ) );
    pEnc->setRenderPipelineState( _pRPSkyboxState );
    pEnc->setDepthStencilState( _pDontWriteDepthStencilState );
    pEnc->setCullMode( MTL::CullModeBack );
    pEnc->setFragmentTexture( _pSkyMap, TextureIndexSkyMap );
    pEnc->setFrontFacingWinding( MTL::Winding::WindingCounterClockwise );
    for (auto& meshBuffer : _skyMesh.vertexBuffers())
    {
        pEnc->setVertexBuffer(meshBuffer.buffer(),
                              meshBuffer.offset(),
                              meshBuffer.argumentIndex() );
    }
    
    for (auto& submesh : _skyMesh.submeshes())
    {
        pEnc->drawIndexedPrimitives(submesh.primitiveType(),
                                    submesh.indexCount(),
                                    submesh.indexType(),
                                    submesh.indexBuffer().buffer(),
                                    submesh.indexBuffer().offset() );
    }
    pEnc->popDebugGroup();
    pEnc->endEncoding();
    pCmd->presentDrawable(pView->currentDrawable());
    pCmd->commit();
    pCmd->waitUntilCompleted();
    pPool->release();
}

void Renderer::drawShadow( MTL::RenderCommandEncoder* pEnc, MTL::CommandBuffer* pCmd, size_t shadowframe){

    pEnc->setLabel( AAPLSTR( "Shadows" ) );
    pEnc->setRenderPipelineState( _pRPShadowState );
    pEnc->setDepthStencilState( _pShadowDepthStencilState );
    pEnc->setCullMode( MTL::CullModeBack);
    pEnc->setDepthBias(0.015, 7, 0.02 );
    pEnc->setVertexBuffer(  _pFrameDataBuffer[shadowframe],          /* offset */  0, BufferIndexFrameData );
    pEnc->setFragmentTexture( _pShadowTexture,          TextureIndexShadowMap );
    pEnc->drawIndexedPrimitives( primitiveType(),
                                _indexCount, MTL::IndexType::IndexTypeUInt16,
                                _pIndexBuffer,
                                0,
                                numberOfInstances());

   // matrix_float4x4 shadowProjectionMatrix = matrix_ortho_left_hand(-53, 53, -33, 53, -53, 53);

    pEnc->endEncoding();
}

void Renderer::drawableSizeWillChange( const MTL::Size& size){

    float aspect = (float)size.width / float(size.height  != 0 ? size.height : 1);
    if (_aspect != aspect)  {
        _aspect = aspect;
    }
    _projectionMatrix = makeIdentity(); ///  65.0f * (M_PI / 180.0f)
    _projectionMatrix = matrix_perspective_right_hand( 65.0f * (M_PI / 180.0f), _aspect, 0.1f, 1000.0f );
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

void Renderer::setCameraViewMatrix ( const matrix_float4x4 & cameraViewMatrix ){
    _cameraViewMatrix = cameraViewMatrix;
}

void Renderer::setCameraPosition ( const vector_float3 & cameraPosition){
        _cameraPosition = cameraPosition;

}

void Renderer::setComputedTextureValue(const float &value) {
    if( _computedTextureValue != value){
        _computedTextureValue = value;
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

void Renderer::setCameraDirection( const simd_float3 & newDir) {
    if ( _cameraDirection.x != newDir.x || _cameraDirection.y != newDir.y || _cameraDirection.z != newDir.z){
        _cameraDirection = newDir;
    }
}

void Renderer::cleanup()
{
    _pDepthStencilState->release();
    _pDontWriteDepthStencilState->release();
    _pCommandQueue->release();
    _pVertexDataBuffer->release();
    _pIndexBuffer->release();
    _pShaderLibrary->release();
    _pVertexDescriptor->release();
    _pSkyVertexDescriptor->release();
    _pRPSkyboxState->release();
    _pComputePSO->release();
    _pTrailComputePSO->release();
    _pRPState->release();
    _pTextureAnimationBuffer->release();
    _pTexture->release();
    _pIrradianceMap->release();;
    _pPreFilterMap->release();;
    _pBDRFMap->release();;
    _pSkyMap->release();;
    _pDevice->release();
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
    for(auto& del : _pInstanceDataBuffer){
        del->release();
    }
    for(auto& del : _pFrameDataBuffer){
        del->release();
    }
    for(auto& del : _pMaterialTexture){
        del->release();
    }
}

















