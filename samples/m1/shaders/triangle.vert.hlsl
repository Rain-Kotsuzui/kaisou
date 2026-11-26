struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 color : COLOR0;
};

VertexOutput VSMain(uint vertexIndex : SV_VertexID)
{
    // M1 诊断三角形：只支持从索引 0 开始绘制三个顶点。
    const float2 positions[3] = {
        float2(0.0f, -0.6f),
        float2(0.6f, 0.6f),
        float2(-0.6f, 0.6f)
    };

    const float3 colors[3] = {
        float3(1.0f, 0.0f, 0.0f),
        float3(0.0f, 1.0f, 0.0f),
        float3(0.0f, 0.0f, 1.0f)
    };

    VertexOutput output;
    output.position = float4(positions[vertexIndex], 0.5f, 1.0f);
    output.color = colors[vertexIndex];
    return output;
}