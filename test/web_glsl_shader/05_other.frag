#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;

// 选择要测试的函数（取消注释其中一个）
// #define MOD_FUNCTION// y = mod(x, 0.5)
// #define FRACT_FUNCTION// y = fract(x)
// #define CEIL_FUNCTION// y = ceil(x)
// #define FLOOR_FUNCTION// y = floor(x)
// #define SIGN_FUNCTION// y = sign(x)
// #define ABS_FUNCTION// y = abs(x)
// #define CLAMP_FUNCTION// y = clamp(x, 0.0, 1.0)
// #define MIN_FUNCTION    // y = min(0.0, x)
// #define MAX_FUNCTION// y = max(0.0, x)

void main(){
    vec2 st=gl_FragCoord.xy/u_resolution;
    
    // 将坐标转换到 [-1, 1] 范围，使中心在屏幕中央
    vec2 centered_st=st*2.-1.;
    
    // 参数控制
    float grid_density=20.;// 网格密度：20x20
    float line_width=.02;// 线宽（相对比例）
    float graph_width=.02;// 函数曲线宽度
    
    vec3 color=vec3(.1);// 背景色
    
    // // 创建网格（使用原始坐标，以便网格填充整个屏幕）
    // if(mod(st.x*grid_density,1.)<line_width||
    // mod(st.y*grid_density,1.)<line_width){
        //     color=vec3(.8);// 更亮的线条
    // }
    
    // 绘制坐标轴
    if(abs(centered_st.x)<.005){// Y轴
        color=mix(color,vec3(0.,1.,0.),.8);
    }
    if(abs(centered_st.y)<.005){// X轴
        color=mix(color,vec3(0.,1.,0.),.8);
    }
    
    // 绘制刻度线（每0.25单位一个刻度）
    for(float i=-1.;i<=1.;i+=.25){
        if(abs(centered_st.x-i)<.003){
            color=mix(color,vec3(0.,1.,0.),.5);
        }
        if(abs(centered_st.y-i)<.003){
            color=mix(color,vec3(0.,1.,0.),.5);
        }
    }
    
    // 定义输入值x（使用水平坐标）
    float x=centered_st.x*2.;// 将x范围扩展到[-2, 2]以便观察更多效果
    float y;
    
    // 根据选择的宏应用不同的函数
    #ifdef MOD_FUNCTION
    y=mod(x,.5);
    #endif
    
    #ifdef FRACT_FUNCTION
    y=fract(x);
    #endif
    
    #ifdef CEIL_FUNCTION
    y=ceil(x);
    #endif
    
    #ifdef FLOOR_FUNCTION
    y=floor(x);
    #endif
    
    #ifdef SIGN_FUNCTION
    y=sign(x);
    #endif
    
    #ifdef ABS_FUNCTION
    y=abs(x);
    #endif
    
    #ifdef CLAMP_FUNCTION
    y=clamp(x,0.,1.);
    #endif
    
    #ifdef MIN_FUNCTION
    y=min(0.,x);
    #endif
    
    #ifdef MAX_FUNCTION
    y=max(0.,x);
    #endif
    
    // 绘制函数曲线
    float graph_y=y*.5;// 缩放函数输出值以适应屏幕
    float dist=abs(centered_st.y-graph_y);
    
    if(dist<graph_width){
        // 根据函数值给曲线着色
        float t=(y+1.)*.5;// 将y映射到[0,1]用于颜色插值
        vec3 curve_color=mix(vec3(1.,0.,0.),vec3(0.,0.,1.),t);
        color=mix(color,curve_color,.8);
    }
    
    // 在曲线上标记当前x对应的点
    float marker_size=.015;
    vec2 point_pos=vec2(centered_st.x,graph_y);
    float point_dist=distance(centered_st,point_pos);
    
    if(point_dist<marker_size){
        color=vec3(1.,1.,0.);// 黄色标记点
    }
    
    gl_FragColor=vec4(color,1.);
}