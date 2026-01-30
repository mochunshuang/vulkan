#version 450

layout(location=0)in vec3 fragColor;
layout(location=1)in vec2 fragTexCoord;

layout(location=0)out vec4 outColor;

// 纹理采样器，绑定在绑定点0
layout(binding=0)uniform sampler2D texSampler;

void main(){
    // 采样纹理并与顶点颜色混合
    vec4 texColor=texture(texSampler,fragTexCoord);
    outColor=vec4(fragColor*texColor.rgb,1.);
}