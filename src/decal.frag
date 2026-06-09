#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D decalTex;

void main()
{
    vec4 tex = texture(decalTex, TexCoord);

    // Descartar transparencia
    if (tex.a < 0.2)
        discard;

    FragColor = tex;
}
