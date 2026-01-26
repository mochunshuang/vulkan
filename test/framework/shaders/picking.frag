#version 450
layout(location=0)out vec4 outColor;

// 对象ID通过push constant传递
layout(push_constant)uniform PushConstants{
    uint objectId;
}pushConstants;

void main(){
    // 将对象ID编码为颜色
    // 使用24位存储对象ID (R:8, G:8, B:8)
    uint id=pushConstants.objectId;
    float r=float((id>>16)&0xFF)/255.;
    float g=float((id>>8)&0xFF)/255.;
    float b=float(id&0xFF)/255.;
    outColor=vec4(r,g,b,1.);
}