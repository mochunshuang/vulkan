#version 450
#extension GL_ARB_separate_shader_objects:enable

layout(location=0)in vec3 fragColor;
layout(location=1)in vec3 normal;

layout(location=0)out vec4 outColor;

// 俯视45度的光照（从上方偏右前方来）
vec3 calculateLighting(vec3 baseColor,vec3 normalVec){
    // 光源方向：从右前方上方来（俯视45度时，光源应该在右上角）
    vec3 lightDir=normalize(vec3(1.,2.,1.5));
    
    // 环境光
    vec3 ambient=vec3(.25,.25,.25);
    
    // 漫反射
    float diff=max(dot(normalize(normalVec),lightDir),0.);
    vec3 diffuse=diff*vec3(.75,.75,.75);
    
    // 简单的镜面高光
    vec3 viewDir=normalize(vec3(0.,1.,2.));// 从斜上方看
    vec3 reflectDir=reflect(-lightDir,normalize(normalVec));
    float spec=pow(max(dot(viewDir,reflectDir),0.),32.);
    vec3 specular=spec*vec3(.3,.3,.3);
    
    // 组合光照
    return(ambient+diffuse+specular)*baseColor;
}

void main(){
    vec3 litColor=calculateLighting(fragColor,normal);
    outColor=vec4(litColor,1.);
}