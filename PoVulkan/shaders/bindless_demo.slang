

// wtf is this for?
export T getDescriptorFromHandle<T>(DescriptorHandle<T> handle) where T : IOpaqueDescriptor
{
    return defaultGetDescriptorFromHandle(handle, BindlessDescriptorOptions.None);
}

struct Material
{
    float4 tint;
    DescriptorHandle<Texture2D<float4>> layers[4];
    DescriptorHandle<SamplerState> sampler;
};

struct Scene
{
    float4 ambient;
};

ParameterBlock<Scene> scene;

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 tid: SV_DispatchThreadID,
    uniform StructuredBuffer<Material> materials,
    uniform RWStructuredBuffer<float4> output,
    uniform uint count)
{
    if (tid.x < count)
    {
        Material m = materials[tid.x];
        // non uniform is needed for each descriptor handle because this shader can be used with 
        // different objects that have different DescriptorHandles.
        // since descriptor handles effectively index into an array the gpu needs to know that so that
        // it is aware of the change from execution to execution
        output[tid.x] = scene.ambient + m.tint * nonuniform(
            m.layers[0])->SampleLevel(nonuniform(m.sampler), float2(0.5), 0);
    }

}