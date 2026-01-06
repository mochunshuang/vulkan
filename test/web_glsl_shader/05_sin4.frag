#ifdef GL_ES
precision mediump float;
#endif

#define PI 3.14159265359

uniform vec2 u_resolution;
uniform float u_time;

// 选择要显示的练习 (1-8)
#define EXERCISE 4

// 基础网格绘制函数
vec3 drawGrid(vec2 st){
    vec3 color=vec3(.1);// 深灰色背景
    
    // 网格线
    if(mod(st.x*20.,1.)<.05||mod(st.y*20.,1.)<.05){
        color=vec3(.3);// 浅灰色网格线
    }
    
    // 坐标轴
    if(abs(st.x-.5)<.003)color=vec3(.5);// Y轴
    if(abs(st.y-.5)<.003)color=vec3(.5);// X轴
    
    return color;
}

// 绘制曲线函数
vec3 drawCurve(vec2 st,float display_y,vec3 curve_color){
    vec3 color=drawGrid(st);
    
    // 绘制曲线
    float line_width=.01;
    if(abs(st.y-display_y)<line_width){
        color=curve_color;
    }
    
    return color;
}

void main(){
    vec2 st=gl_FragCoord.xy/u_resolution;
    vec3 color=vec3(0.);
    
    // 基础x值（映射到[0, 2π]）
    float base_x=st.x*2.*PI;
    
    // 根据选择的练习计算y值
    float y=0.;
    float display_y=0.;
    vec3 curve_color=vec3(0.,1.,0.);// 默认绿色
    
    #if EXERCISE==1
    // 练习1：sin(x + time) - 曲线随x轴动起来
    y=sin(base_x+u_time);
    curve_color=vec3(0.,1.,0.);// 绿色
    display_y=y*.5+.5;
    
    #elif EXERCISE==2
    // 练习2：sin(PI * x) - 每两个整数循环一次
    float x_scaled=st.x*4.;// 放大x范围以便看到多个周期
    y=sin(PI*x_scaled);
    curve_color=vec3(1.,.5,0.);// 橙色
    display_y=y*.5+.5;
    
    #elif EXERCISE==3
    // 练习3：sin(time * x) - 频率随时间增加
    y=sin(u_time*base_x);
    curve_color=vec3(1.,0.,1.);// 紫色
    display_y=y*.5+.5;
    
    #elif EXERCISE==4
    // 练习4：sin(x) + 1.0 - 曲线向上移动，值域[0,2]
    y=sin(base_x)+1.;
    curve_color=vec3(0.,.5,1.);// 蓝色
    display_y=y*.5;// 注意：y在[0,2]，映射到[0,1]
    
    #elif EXERCISE==5
    // 练习5：sin(x) * 2.0 - 振幅放大两倍
    y=sin(base_x)*2.;
    curve_color=vec3(1.,0.,0.);// 红色
    display_y=y*.25+.5;// 注意：y在[-2,2]，需要缩小映射
    
    #elif EXERCISE==6
    // 练习6：abs(sin(x)) - 弹力球轨迹
    y=abs(sin(base_x));
    curve_color=vec3(1.,1.,0.);// 黄色
    display_y=y*.5+.5;
    
    #elif EXERCISE==7
    // 练习7：fract(sin(x)) - 取小数部分
    y=fract(sin(base_x));
    curve_color=vec3(.5,0.,.5);// 深紫色
display_y=y;// y已经在[0,1)范围内
    
    #elif EXERCISE==8
    // 练习8：ceil(sin(x))和floor(sin(x))的混合 - 电子波
    float ceil_wave=ceil(sin(base_x));
    float floor_wave=floor(sin(base_x));
    y=mix(floor_wave,ceil_wave,.5);// 取中间值，显示更明显
    curve_color=vec3(0.,1.,1.);// 青色
    display_y=y*.5+.5;
    
    #else
    // 默认：基本sin(x)
    y=sin(base_x);
    display_y=y*.5+.5;
    curve_color=vec3(0.,1.,0.);// 绿色
    #endif
    
    color=drawCurve(st,display_y,curve_color);
    
    // 显示当前练习编号
    float text_pos=mod(float(EXERCISE)*.1,1.);
    if(st.x>.9&&st.y>.9){
        color=mix(color,vec3(text_pos,text_pos,0.),.5);
    }
    
    gl_FragColor=vec4(color,1.);
}