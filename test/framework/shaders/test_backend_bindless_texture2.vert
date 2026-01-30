#version 450
#extension GL_EXT_buffer_reference:require
#extension GL_EXT_scalar_block_layout:require
#extension GL_EXT_shader_explicit_arithmetic_types_int64:require

struct VertexData{
    vec2 pos;
    vec3 color;
    vec2 texCoord;
};

layout(buffer_reference,scalar)readonly buffer VertexBuffer{
    VertexData vertices[];
};

// diff: [修改] 推送常量现在包含纹理索引
layout(push_constant)uniform PushConstants{
    uint64_t vertexBufferAddress;
    uint textureIndex;// diff: 添加纹理索引，传递给片段着色器
    uint point_cloud;
}pc;

layout(location=0)out vec3 fragColor;
layout(location=1)out vec2 fragTexCoord;
layout(location=2)out flat uint fragTextureIndex;// diff: 添加纹理索引输出
layout(location=3)out flat uint point_cloud;

void main(){
    VertexBuffer vertexBuffer=VertexBuffer(pc.vertexBufferAddress);
    uint vertexIndex=gl_VertexIndex;
    
    vec2 position=vertexBuffer.vertices[vertexIndex].pos;
    vec3 color=vertexBuffer.vertices[vertexIndex].color;
    vec2 texCoord=vertexBuffer.vertices[vertexIndex].texCoord;
    
    gl_Position=vec4(position,0.,1.);
    fragColor=color;
    fragTexCoord=texCoord;
    fragTextureIndex=pc.textureIndex;// diff: 传递纹理索引
    point_cloud=pc.point_cloud;
}