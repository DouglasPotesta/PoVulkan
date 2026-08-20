

struct MatrixParameters
{
	float4x4 model;
	float4x4 view;
	float4x4 proj;
}


struct Vertex
{
	float3 position;
	float3 color;
	float2 uv;
}

struct VOut
{
	float4 position : SV_POSITION;
	float3 fragColor;
	float2 fragTexCoord;
}

ConstantBuffer<MatrixParameters> ubo;


[shader("vertex")]
VOut main(Vertex input)
{
	VOut output;
	output.position = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.position, 1.0))));
	output.fragColor = input.color;
	output.fragTexCoord = input.uv;
	return output;
}


uniform Sampler2D texSampler;

[shader("fragment")]
float4 main(VOut input) : SV_TARGET
{
	float4 outColor = float4(input.fragColor * texSampler.Sample(input.fragTexCoord).rgb, 1.0);
	return outColor;
}