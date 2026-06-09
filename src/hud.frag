#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D hudTexture;
uniform bool useTexture;
uniform vec3 tintColor;
uniform float alpha;

void main()
{
    vec4 baseColor = useTexture ? texture(hudTexture, TexCoord) : vec4(1.0);
    FragColor = vec4(baseColor.rgb * tintColor, baseColor.a * alpha);
}
