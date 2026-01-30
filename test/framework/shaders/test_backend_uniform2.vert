#version 450

// 确保使用正确的实例名称
layout(set=0,binding=0)uniform ModelUniform{
    mat4 model;
}u_model;

layout(set=0,binding=1)uniform ViewUniform{
    mat4 view;
}u_view;

layout(set=0,binding=2)uniform ProjUniform{
    mat4 proj;
}u_proj;

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
    
    // 使用正确的实例名称访问
    mat4 mvp=u_proj.proj*u_view.view*u_model.model;
    gl_Position=mvp*vec4(positions[gl_VertexIndex],0.,1.);
}