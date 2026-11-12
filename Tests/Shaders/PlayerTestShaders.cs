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
