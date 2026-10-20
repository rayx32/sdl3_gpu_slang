// Slang shader for 2D SpriteBatch in SDL3 GPU

struct VertexInput
{
    float2 position : POSITION;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

struct SpriteUniforms
{
    float4x4 projectionMatrix;
};

// SDL3 GPU SPIR-V layout requirement:
// Vertex shader uniform buffers must be placed in DescriptorSet 1.
[[vk::binding(0, 1)]]
ConstantBuffer<SpriteUniforms> u_uniforms : register(b0, space1);

[shader("vertex")]
VertexOutput vs_main(VertexInput input)
{
    VertexOutput output;
    output.position = mul(u_uniforms.projectionMatrix, float4(input.position, 0.0, 1.0));
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

// SDL3 GPU SPIR-V layout requirement:
// Fragment shader sampled textures (combined image/sampler) must be placed in DescriptorSet 2.
[[vk::binding(0, 2)]]
Sampler2D spriteTexture : register(t0, space2);

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target
{
    float4 texColor = spriteTexture.Sample(input.uv);
    return texColor * input.color;
}

