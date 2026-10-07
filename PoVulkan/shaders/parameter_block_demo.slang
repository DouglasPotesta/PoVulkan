


struct Material
{
	float4 tint;
	Texture2D<float4> layers[4];
	SamplerState sampler;
};

struct Scene
{
	float4 ambient;
	ParameterBlock<Material> material;
};

ParameterBlock<Scene> scene;

[shader("compute")]
[numthreads(1,1,1)]
void computeMain(
	uint3 tid: SV_DispatchThreadID,
	uniform RWStructuredBuffer<float4> output,
	uniform uint count)
{
    if (tid.x < count)
        output[tid.x] = scene.ambient + scene.material.tint * scene.material.layers[0].SampleLevel(scene.material.sampler, float2(0.5), 0);
}