#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

void main(){
    vec2 st=gl_FragCoord.xy/u_resolution*2.-1.;// 转换为[-1,1]范围
    
    // 鼠标位置影响
    vec2 mouse=(u_mouse/u_resolution)*2.-1.;
    
    // 创建多个同心圆
    float pattern=0.;
    for(int i=0;i<5;i++){
        float i_f=float(i);
        float radius=.2+i_f*.1;
        
        // 随时间移动中心点
        vec2 center=vec2(
            sin(u_time*.5+i_f)*.5,
            cos(u_time*.3+i_f)*.5
        );
        
        // 鼠标可以拖拽图案
        center+=mouse*.3;
        
        float dist=length(st-center);
        pattern+=smoothstep(radius+.01,radius,dist);
    }
    
    // 添加扫描线效果
    float scanline=sin(st.y*100.+u_time*5.)*.1+.9;
    
    // 最终颜色
    vec3 color=vec3(
        pattern*scanline,
        sin(u_time+st.x*5.)*.5+.5,
        cos(u_time+st.y*5.)*.5+.5
    );
    
    gl_FragColor=vec4(color,1.);
}