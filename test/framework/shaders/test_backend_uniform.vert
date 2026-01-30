#version 450

// Uniform Buffer定义
layout(set=0,binding=0)uniform UniformBufferObject{
    mat4 model;
    mat4 view;
    mat4 proj;
}ubo;

layout(location=0)out vec3 fragColor;

vec2 positions[3]=vec2[](
    vec2(0.,-.5),
    vec2(.5,.5),
    vec2(-.5,.5)
);

vec3 colors[3]=vec3[](
    vec3(1.,0.,0.),
    vec3(0.,1.,0.),
    vec3(0.,0.,1.)
);

void main(){
    fragColor=colors[gl_VertexIndex];
    
    // 使用MVP矩阵变换顶点位置
    mat4 mvp=ubo.proj*ubo.view*ubo.model;
    gl_Position=mvp*vec4(positions[gl_VertexIndex],0.,1.);
}