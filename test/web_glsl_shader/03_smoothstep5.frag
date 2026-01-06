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
    给定一个范围的上下限和一个数值，这个函数会在已有的范围内给出插值。前两个参数规定转换的开始和结束点，第三个是给出一个值用来插值
    */
    // Smooth interpolation between start and end
    float start=.1;
    float end=.5;
    float y=smoothstep(start,end,st.x);
    
    //NOTE: 组合左右 就形成正态分布图了
    y=smoothstep(.2,.5,st.x)-smoothstep(.5,.8,st.x);
    
    vec3 color=vec3(y);
    
    float pct=plot(st,y);
    color=(1.-pct)*color+pct*vec3(0.,1.,0.);
    
    gl_FragColor=vec4(color,1.);
}
