#version 450

// 输入：填充色和线框色
layout(location=0)in vec3 fragFillColor;
layout(location=1)in vec3 fragWireColor;
layout(location=2)in float fragAlpha;

// 输出
layout(location=0)out vec4 outColor;

// 推送常量：控制渲染模式
layout(push_constant)uniform PushConstants{
    int renderMode;// 0=填充, 1=线框, 2=两者
    float lineWidth;// 线宽
    vec4 wireColorOverride;// 可选的线框颜色覆盖 (包含alpha)
}push;

void main(){
    if(push.renderMode==0){
        // 仅填充模式：使用填充色
        outColor=vec4(fragFillColor,fragAlpha);
    }
    else if(push.renderMode==1){
        // 仅线框模式：使用线框色（优先使用覆盖色）
        if(push.wireColorOverride.a>0.){
            outColor=push.wireColorOverride;
        }else{
            outColor=vec4(fragWireColor,fragAlpha);
        }
    }
    else if(push.renderMode==2){
        // 两者模式：填充+线框
        // 这里实现简单的边框效果
        outColor=vec4(fragFillColor*.7+fragWireColor*.3,fragAlpha);
    }
    else{
        // 默认：填充
        outColor=vec4(fragFillColor,fragAlpha);
    }
}