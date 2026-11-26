float4 PSMain([[vk::location(0)]] float3 color : COLOR0) : SV_Target0
{
    return float4(color, 1.0f);
}