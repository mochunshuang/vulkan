#ifdef GL_ES
precision mediump float;
#endif

#define PI 3.14159265359

uniform vec2 u_resolution;
uniform float u_time;

void main(){
    vec2 st=gl_FragCoord.xy/u_resolution;
    
    // 1. 创建网格背景
    vec3 color=vec3(.1);
    
    // 网格线
    float grid_density=20.;// 网格密度：20x20
    float grid_line_width=.04;// 线宽（相对比例）
    if(mod(st.x*grid_density,1.)<grid_line_width
    ||
    mod(st.y*grid_density,1.)<grid_line_width){
        color=vec3(.3);
    }
    
    // 2. 绘制多个正弦函数
    // 显示3个正弦波
    for(float i=0.;i<3.;i+=1.){
        // 每个波的参数不同
        float frequency=4.+i*2.;// 频率递增
        float amplitude=.4/(i+1.);// 振幅递减
        float phase=u_time*.5+i*.5;// 相位偏移
        
        // 计算正弦波
        float x=st.x*2.*PI*frequency;
        float y=sin(x+phase)*amplitude;
        
        // 映射到显示范围
        float display_y=y+.5;
        
        // 为不同波使用不同颜色
        vec3 wave_color;
        if(i==0.)wave_color=vec3(1.,0.,0.);// 红色
        else if(i==1.)wave_color=vec3(0.,1.,0.);// 绿色
        else wave_color=vec3(0.,0.,1.);// 蓝色
        
        // 绘制波
        float line_width=.006;
        if(abs(st.y-display_y)<line_width){
            color=mix(color,wave_color,.8);
        }
    }
    
    gl_FragColor=vec4(color,1.);
}