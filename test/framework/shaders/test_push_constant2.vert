#version 450

layout(push_constant,std430 ) uniform PushConstants {
    vec2 positions[4];
    vec3 colors[4];
} push;

layout(location = 0) out vec3 fragColor;

void main() {
    // gl_VertexIndex 是顶点索引缓冲区的当前索引值
    uint vertexIndex = gl_VertexIndex;
    
    // 从推送常量数组获取顶点位置和颜色
    vec2 position = push.positions[vertexIndex];
    fragColor = push.colors[vertexIndex];
    
    // 转换为齐次坐标
    gl_Position = vec4(position, 0.0, 1.0);
}