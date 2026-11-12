using HelixEngine.Builtin;
using HelixEngine.Shaders;
using static HelixEngine.Shaders.ShaderMath;

namespace HelixPlayer.TestShaders;

/// <summary>Lambert lighting from the main light of the camera block, with the ambient light.</summary>
[Shader("Tests/PlayerLit")]
public sealed class PlayerLit : ShaderDefinition
{
    [Property("Color"), MainColor, Default(1.0f, 1.0f, 1.0f, 1.0f)] public float4 color;

    public struct MaterialData
    {
        public float4 color;
    }

    public struct Varyings
    {
        [Position] public float4 position;
        [Location(0)] public float3 normal;
    }

    [Pass("Forward", LightMode = "ForwardBase")]
    public static class Forward
    {
        [Uniform(0)] public static MaterialData material;

        [VertexShader]
        public static Varyings Vertex([Location(VertexInputs.Position)] float3 position, [Location(VertexInputs.Normal)] float3 normal)
        {
            Varyings o;
            o.position = ShaderVariables.ObjectToClip(position);
            o.normal = ShaderVariables.ObjectToWorldNormal(normal);
            return o;
        }

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input)
        {
            float3 n = Normalize(input.normal);
            float light = Saturate(Dot(n, ShaderVariables.camera.helix_MainLightDirection.xyz));
            float3 lit = material.color.xyz * (ShaderVariables.camera.helix_MainLightColor.xyz * light + ShaderVariables.camera.helix_AmbientColor.xyz);
            return new float4(lit, 1.0f);
        }
    }
}

/// <summary>A texture times a color, lit by the main light of the camera block with the ambient light.</summary>
[Shader("Tests/PlayerLitTextured")]
public sealed class PlayerLitTextured : ShaderDefinition
{
    [Property("Base Map"), MainTexture, DefaultTexture("white")] public Texture2D<float4> baseMap;
    [Property("Color"), MainColor, Default(1.0f, 1.0f, 1.0f, 1.0f)] public float4 color;

    public struct MaterialData
    {
        public float4 color;
        public float4 baseMap_ST;
    }

    public struct Varyings
    {
        [Position] public float4 position;
        [Location(0)] public float3 normal;
        [Location(1)] public float2 uv;
    }

    [Pass("Forward", LightMode = "ForwardBase")]
    public static class Forward
    {
        [Uniform(0)] public static MaterialData material;
        [Texture(0)] public static Texture2D<float4> baseMap;
        [Sampler(0)] public static SamplerState baseSampler;

        [VertexShader]
        public static Varyings Vertex([Location(VertexInputs.Position)] float3 position, [Location(VertexInputs.Normal)] float3 normal,
                                      [Location(VertexInputs.TexCoord0)] float2 uv)
        {
            Varyings o;
            o.position = ShaderVariables.ObjectToClip(position);
            o.normal = ShaderVariables.ObjectToWorldNormal(normal);
            o.uv = uv * material.baseMap_ST.xy + material.baseMap_ST.zw;
            return o;
        }

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input)
        {
            float3 n = Normalize(input.normal);
            float light = Saturate(Dot(n, ShaderVariables.camera.helix_MainLightDirection.xyz));
            float4 albedo = baseMap.Sample(baseSampler, input.uv) * material.color;
            float3 lit = albedo.xyz * (ShaderVariables.camera.helix_MainLightColor.xyz * light + ShaderVariables.camera.helix_AmbientColor.xyz);
            return new float4(lit, 1.0f);
        }
    }
}

/// <summary>The mesh's vertex colors, lit by the main light with the ambient light.</summary>
[Shader("Tests/PlayerVertexColor")]
public sealed class PlayerVertexColor : ShaderDefinition
{
    public struct Varyings
    {
        [Position] public float4 position;
        [Location(0)] public float3 normal;
        [Location(1)] public float4 color;
    }

    [Pass("Forward", LightMode = "ForwardBase")]
    public static class Forward
    {
        [VertexShader]
        public static Varyings Vertex([Location(VertexInputs.Position)] float3 position, [Location(VertexInputs.Normal)] float3 normal,
                                      [Location(VertexInputs.Color)] float4 color)
        {
            Varyings o;
            o.position = ShaderVariables.ObjectToClip(position);
            o.normal = ShaderVariables.ObjectToWorldNormal(normal);
            o.color = color;
            return o;
        }

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input)
        {
            float3 n = Normalize(input.normal);
            float light = Saturate(Dot(n, ShaderVariables.camera.helix_MainLightDirection.xyz));
            float3 lit = input.color.xyz * (ShaderVariables.camera.helix_MainLightColor.xyz * light + ShaderVariables.camera.helix_AmbientColor.xyz);
            return new float4(lit, 1.0f);
        }
    }
}
