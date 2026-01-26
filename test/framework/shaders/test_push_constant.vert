#version 450

layout(push_constant)uniform PushConstants{
    vec4 positions[3];
    vec4 colors[3];
}push;

layout(location=0)out vec3 fragColor;

void main(){
    gl_Position=push.positions[gl_VertexIndex];
    fragColor=push.colors[gl_VertexIndex].rgb;
}