// Author: Inigo Quiles
// Title: Expo

#ifdef GL_ES
precision mediump float;
#endif

#define PI 3.14159265359

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

float plot(vec2 st,float pct){
    return smoothstep(pct-.02,pct,st.y)-
    smoothstep(pct,pct+.02,st.y);
}

void main(){
    vec2 st=gl_FragCoord.xy/u_resolution;
    
    /*
    把x看作输入，y看作输出：
    
    平方根函数：放大小的输入值
    比如x=.01→y=.1（放大了10倍！）
    所以左边区域明显更亮
    
    线性函数：保持原样
    
    平方函数：缩小小的输入值
    比如x=.1→y=.01（缩小了10倍！）
    所以左边区域更暗
    */
    // float y=pow(st.x,.5);
    // float y=pow(st.x,1.);
    float y=pow(st.x,2.);
    
    vec3 color=vec3(y);
    
    float pct=plot(st,y);
    color=(1.-pct)*color+pct*vec3(0.,1.,0.);
    
    gl_FragColor=vec4(color,1.);
}
