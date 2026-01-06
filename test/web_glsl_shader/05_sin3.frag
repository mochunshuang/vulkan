#ifdef GL_ES
precision mediump float;
#endif

#define PI 3.14159265359

uniform vec2 u_resolution;
uniform vec2 u_mouse;

void main(){
    vec2 st=gl_FragCoord.xy/u_resolution;
    vec2 mouse=u_mouse/u_resolution;
    
    // 1. 网格背景
    vec3 color=vec3(.1);
    
    // 网格线
    float grid_density=20.;// 网格密度：20x20
    float grid_line_width=.04;// 线宽（相对比例）
    if(mod(st.x*grid_density,1.)<grid_line_width
    ||
    mod(st.y*grid_density,1.)<grid_line_width){
        color=vec3(.3);
    }
    
    // 坐标轴
    if(abs(st.x-.5)<.002)color=vec3(.5);
    if(abs(st.y-.5)<.002)color=vec3(.5);
    
    // 2. 绘制正弦波（鼠标控制参数）
    float frequency=5.+mouse.x*10.;// 鼠标x控制频率
    float amplitude=.4+mouse.y*.3;// 鼠标y控制振幅
    float phase=0.;// 相位
    
    float x=st.x*2.*PI*frequency;
    float y=sin(x+phase)*amplitude;
    float display_y=y+.5;
    
    // 3. 绘制正弦曲线
    float line_width=.01;
    if(abs(st.y-display_y)<line_width){
        color=vec3(1.,.5,0.);// 橙色曲线
    }
    
    // 4. 显示鼠标位置影响
    if(abs(st.x-mouse.x)<.01){
        color=mix(color,vec3(1.,0.,1.),.3);// 紫色竖线
    }
    
    gl_FragColor=vec4(color,1.);
}