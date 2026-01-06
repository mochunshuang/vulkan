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
    
    // Smooth interpolation between 0.1 and 0.9
    // float y=smoothstep(.1,.9,st.x);
    
    /*
    smoothstep(a,b,x)函数的作用是：
    当x≤a时，返回0.。
    当x≥b时，返回1.。
    当a<x<b时，返回一个在0.到1.之间平滑过渡的插值（使用Hermite插值）。
    
    所以y的值分三种情况：
    如果st.x≤.1
    y=0.
    如果st.x≥.5
    y=1.
    如果.1<st.x<.5
    y会从0.到1.平滑增加
    */
    float y=smoothstep(.1,.5,st.x);
    
    vec3 color=vec3(y);
    
    float pct=plot(st,y);
    color=(1.-pct)*color+pct*vec3(0.,1.,0.);
    
    gl_FragColor=vec4(color,1.);
}
