///
///  AAPLKernels.metal
///  MetalCCP
///
///  Created by Guido Schneider on 31.07.22.
///
#include <metal_stdlib>
using namespace metal;
#include <metal_common>
#include "AAPLShaderTypes.h"

uint hash(uint seed);

float randomf(uint seed);

float sense(Particle p, float ang, Uniforms uniforms,texture2d<float, access::read> readTexture );

void clearTexture( texture2d<float, access::write> writeTexture, uint2 gid );

float3 unitcircle_random(thread uint *seed);

inline uint hash(uint seed) {
    seed ^= 2447636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    return seed;
}

inline float randomf(uint seed) {
    auto argSeed = hash(seed);
    auto arg = (float) argSeed / UINT_MAX * 2 * PI;
    return float(cos(arg));
}

inline float sense(Particle p, float ang, Uniforms uniforms, texture2d<float, access::read> readTexture)
{
    auto dim = int2(uniforms.Dimensions);
    auto sensor_angle = p.dir + ang;
    auto sensor_dir = float2(cos(sensor_angle),sin(sensor_angle));
    auto newpos =  p.position + sensor_dir * uniforms.sensorOffset;

    float sum = 0.f;
    auto bound = uniforms.sensorSize - 1;

    for (uint dy = -bound; dy <= bound; dy++) {
        for (uint dx = -bound; dx <= bound; dx++) {
            int x = round(newpos.x + dx);
            int y = round(newpos.y + dy);

            if (x >= 0 && y >= 0 && x < dim.x && y < dim.y) {
                sum += dot(readTexture.read(uint2(x, y)), float4(p.families) * 2.0 -1);
            }
        }
    }
    return sum;
}


inline void clearTexture( texture2d<float, access::write> writeTexture [[texture(TextureIndexWriteMap)]], uint2 index )
{
    writeTexture.write(float4(0, 0, 0, 1), uint2(index));
}


inline float3 unitcircle_random(thread uint *seed) {
    auto argSeed = hash(*seed);
    auto absSeed = hash(argSeed);
    *seed = absSeed;

    auto arg = (float) argSeed / UINT_MAX * 2 * PI;
    auto absSqrt = (float) absSeed / UINT_MAX;
    auto absR = absSqrt * absSqrt;
    return float3(absR * cos(arg), absR * sin(arg), arg + PI);
}

kernel void init_function(device Particle * particles                   [[buffer(BufferIndexParticleData)]],
                          device const Uniforms &uniforms               [[buffer(BufferIndexUniformData)]],
                          texture2d<float, access::write> writeTexture   [[texture(TextureIndexWriteMap)]],
                          uint index                                    [[thread_position_in_grid]])
{
    Particle p = particles[index];
    if (uniforms.family == 1) {
        p.families = int4(0, 1, 1, 1);
    } else if (uniforms.family == 2) {
        p.families = int4(0, index % 2, 1 - index % 2, 1);
    } else if (uniforms.family == 3) {
        p.families = int4(index % 3 == 2, index % 3 == 1, index % 3 == 0, 1);
    }
    writeTexture.write(float4(p.families) * uniforms.trailWeight -1, uint2(p.position));
    particles[index] = p;
}

kernel void compute_function(texture2d<float, access::read> readTexture     [[texture(TextureIndexReadMap)]],
                             texture2d<float, access::write> writeTexture   [[texture(TextureIndexWriteMap)]],
                             device const Uniforms &uniforms                [[buffer(BufferIndexUniformData)]],
                             device Particle* particles                     [[buffer(BufferIndexParticleData)]],
                             constant float &time_delta                     [[buffer(BufferIndexTimeData)]],
                             uint index                                     [[thread_position_in_grid]])
{
    Particle p = particles[index];
    auto dim = uint2(uniforms.Dimensions);
    auto rnd = hash(p.position.y * dim.x + p.position.x + hash(index));
    auto dir_vec = float2(cos(p.dir),sin(p.dir));
    auto newPos = p.position + uniforms.moveSpeed * time_delta * dir_vec;

    if (newPos.x < 0 || newPos.y < 0 || newPos.x >= dim.x || newPos.y >= dim.y) {
        newPos = clamp(newPos, float2(0.f, 0.f), float2(dim.x - 0.01 , dim.y - 0.01));
        p.dir = ((float) rnd / UINT_MAX ) * 3.f * PI;
    }
    p.position = newPos;

    auto f_sample = sense(p,   0 ,uniforms, readTexture );
    auto l_sample = sense(p, -uniforms.sensorAngle, uniforms , readTexture );
    auto r_sample = sense(p,  uniforms.sensorAngle, uniforms, readTexture );

    rnd = hash(rnd);

    auto rnd_steer_strength = (float) rnd / UINT_MAX;

    if( f_sample >= l_sample && f_sample >= r_sample){

    }
    else if (f_sample < l_sample && f_sample < r_sample){
        p.dir += (rnd_steer_strength > 0.5 -1 ) * 2.f * uniforms.turnSpeed * time_delta;
    }
    else if (r_sample > l_sample) {
        p.dir -= rnd_steer_strength * uniforms.turnSpeed * time_delta;
    }
    else if (l_sample > r_sample) {
        p.dir += rnd_steer_strength * uniforms.turnSpeed * time_delta;
    }

    particles[index] = p;

    writeTexture.write(float4(p.families) * uniforms.trailWeight -1, uint2(newPos));
}

kernel void trail_function(texture2d<float, access::read> readTexture   [[texture(TextureIndexReadMap)]],
                           texture2d<float, access::write> writeTexture [[texture(TextureIndexWriteMap)]],
                           device const Particle * particles            [[buffer(BufferIndexParticleData)]],
                           device const Uniforms &uniforms              [[buffer(BufferIndexUniformData)]],
                           uint2 gid                                    [[thread_position_in_threadgroup]],
                           uint2 grid                                   [[threadgroup_position_in_grid]],
                           uint2 threads                                [[threads_per_threadgroup]])
{
    float4 out = 0.f;
    uint2 index = grid * threads + gid;
    const int2 idim = int2 (uniforms.Dimensions.x, uniforms.Dimensions.y);
    for (int dy = -1; dy <= 1 && (int(index.y) > 0 && int(index.y) < idim.y - 1); dy++) {
        for (int dx = -1; dx <= 1 && (int(index.x) > 0 && int(index.x) < idim.x - 1); dx++) {
            int x = index.x + dx;
            int y = index.y + dy;
            if (x >= 0 && y >= 0 && x < idim.x && y < idim.y) {
                out += readTexture.read(uint2(x, y));
            }
        }
    }
        auto current =  out / 9;
        current *= max(0.01, 1.0 - uniforms.evaporation);
        current.w = 1.f;

        auto source_radius = (float) min(threads.x,threads.y) * 0.01f;
        float4 sources = float4( gid.x + threads.x , gid.y + threads.y, -1.f , 0);
        auto dist = distance( float2(sources.xy), float2(grid * threads)) / source_radius;
        if (dist <= 1) {
            if (sources.z < 0) {
                current = min(max(dist - 0.2, 0.0f), current);
            } else if (uniforms.family == 1) {
                current.yz = max(1 - float(dist), float2(current.yz));
            } else {
                current.y = max(1 - float(dist), float(current.y));
            }
        }
        writeTexture.write(float4(current), uint2(index));
}

kernel void update_family_function(device Particle *particles [[buffer(BufferIndexParticleData)]],
                                   device const Uniforms &uniforms [[buffer(BufferIndexUniformData)]],
                                   uint index [[thread_position_in_grid]])
{
    if (uniforms.family == 1) {
        particles[index].families = int4(0, 1, 1, 1);
    } else if (uniforms.family == 2) {
        particles[index].families = int4(0, index % 2, 1 - index % 2, 1);
    } else if (uniforms.family == 3) {
        particles[index].families = int4(index % 3 == 2, index % 3 == 1, index % 3 == 0, 1);
    }
}
/*
kernel void interactions_function(device Particle *particles        [[buffer(BufferIndexParticleData)]],
                                  device const Uniforms &uniforms   [[buffer(BufferIndexUniformData)]],
                                  uint index                        [[thread_position_in_grid]])
{
    
    device Particle &p = particles[index];
    auto rnd = hash(hash(index) ^ uint(p.dir));
    rnd = hash(rnd);
    float2 pos = p.position.xy;
    float2 dirVec = float2(cos(p.dir),sin(p.dir));
    float dir = atan2(dirVec.y, dirVec.x);
    if ((float) rnd / UINT_MAX <= 0.0001) {
        p.position = pos + unitcircle_random(&rnd).xy * 20 - 10;
        p.dir = dir;
    }
    particles[index] = p;
}

kernel void blur_function(texture2d<half, access::read_write> texture [[texture(0)]],
                          constant Uniforms &uniforms [[buffer(0)]],
                          uint2 index [[thread_position_in_grid]])
{
    
    uint x0 = (index.x - 1 + int(uniforms.Dimensions.x)) % int(uniforms.Dimensions.y);
    uint x2 = (index.x + 1) % int(uniforms.Dimensions.x);
    uint y0 = (index.y - 1 + int(uniforms.Dimensions.y)) % int(uniforms.Dimensions.y);
    uint y2 = (index.y + 1) % int(uniforms.Dimensions.y);
    half4 out = texture.read(index)* 1.0/4.0
    + 1.0/8.0 * texture.read(uint2(index.x, y0))
    + 1.0/8.0 * texture.read(uint2(index.x, y2))
    + 1.0/8.0 * texture.read(uint2(x0, index.y))
    + 1.0/8.0 * texture.read(uint2(x2, index.y))
    + 1.0/16.0 * texture.read(uint2(x0, y0))
    + 1.0/16.0 * texture.read(uint2(x2, y0))
    + 1.0/16.0 * texture.read(uint2(x0, y2))
    + 1.0/16.0 * texture.read(uint2(x2, y2));
    half4 color =  half4(out.r * 0.9999f,out.g * 0.9999f,out.b * 0.9999f, out.a)  ;
    texture.write(color, uint2(index));
}
*/
