#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

#define change_y 0
#define change_color 1
#define chnage_plot 0

float plot(vec2 st){
    #if chnage_plot
    //NOTE: 3. 翻转颜色
    return step(.01,abs(st.y-st.x));// 使用step而不是smoothstep
    #else
    return smoothstep(.02,0.,abs(st.y-st.x));
    #endif
}

void main(){
    
    vec2 st=gl_FragCoord.xy/u_resolution;
    
    #if change_y
    float y=sin(st.x*3.14);// NOTE: 1. 正弦曲线
    #else
    float y=st.x;
    #endif
    
    vec3 color=vec3(y);
    
    float value=1.;
    // value=.4; //NOTE: 更难以亮
    // value=1.4;//NOTE: >1 更快
    
    float pct=plot(st);
    #if change_color
    //NOTE: 2. 改变颜色：
    color=(value-pct)*color+pct*vec3(1.,0.,0.);// 红色线
    #else
    color=(value-pct)*color+pct*vec3(0.,1.,0.);
    #endif
    
    gl_FragColor=vec4(color,1.);
}