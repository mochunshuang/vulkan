#ifdef GL_ES
precision mediump float;
#endif

#define PI 3.14159265359

uniform vec2 u_resolution;
uniform float u_time;

float plot(vec2 st,float pct){
    return smoothstep(pct-.02,pct,st.y)-
    smoothstep(pct,pct+.02,st.y);
}

void main(){
    vec2 st=gl_FragCoord.xy/u_resolution;
    
    /*
    step()插值函数需要输入两个参数。
    第一个是极限或阈值，第二个是我们想要检测或通过的值。
    对任何小于阈值的值，返回0.，大于阈值，则返回1.0
    */
    // Step will return 0.0 unless the value is over 0.5,
    // in that case it will return 1.0
    float start=.5;
    float y=step(start,st.x);
    
    // 背景颜色：要么黑色(0,0,0)要么白色(1,1,1)
    vec3 color=vec3(y);
    
    float pct=plot(st,y);
    
    //NOTE: 匹配 plot 是绿色，否则是背景色
    color=(1.-pct)*color+pct*vec3(0.,1.,0.);
    
    gl_FragColor=vec4(color,1.);
}
