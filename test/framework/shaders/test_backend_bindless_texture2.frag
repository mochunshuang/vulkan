#version 450
// diff: 添加非均匀索引扩展
#extension GL_EXT_nonuniform_qualifier:require

layout(location=0)in vec3 fragColor;
layout(location=1)in vec2 fragTexCoord;
layout(location=2)in flat uint fragTextureIndex;
layout(location=3)in flat uint point_cloud;

layout(location=0)out vec4 outColor;

// diff: [修改] 使用纹理数组，不限制大小
layout(set=0,binding=0)uniform sampler2D textures[];

// 简单的伪随机函数
float random(vec2 st){
    return fract(sin(dot(st,vec2(12.9898,78.233)))*43758.5453123);
}

void main(){
    uint textureIdx=fragTextureIndex;
    vec4 texColor=texture(textures[nonuniformEXT(textureIdx)],fragTexCoord);
    if(point_cloud<.1){
        // 使用纹理坐标和纹理索引创建随机阈值. 每个像素（片段）生成一个独立的随机值
        float randomValue=random(vec2(fragTexCoord)+float(textureIdx));
        if(randomValue<.5){
            // 只使用纹理颜色
            outColor=texColor;
        }else if(randomValue<.7){
            // 只使用顶点颜色
            outColor=vec4(fragColor,texColor.a);
        }else{
            // 混合纹理和顶点颜色
            outColor=vec4(fragColor*texColor.rgb,texColor.a);
        }
    }else{
        outColor=texColor;
    }
    
}