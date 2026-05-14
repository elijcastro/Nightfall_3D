#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform DirLight dirLight;
uniform vec3 viewPos;
uniform sampler2D diffuseMap;
uniform float texScale;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(-dirLight.direction);
    vec2 scaledUV = TexCoord * texScale;
    vec3 texColor = texture(diffuseMap, TexCoord).rgb;

    vec3 ambient = dirLight.ambient * texColor;

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = dirLight.diffuse * diff * texColor;

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);
    vec3 specular = dirLight.specular * spec;

    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}
