#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

void main(){
    vec2 st=gl_FragCoord.xy/u_resolution;
    vec2 mouse=u_mouse/u_resolution;
    
    // 基础颜色：显示坐标的渐变
    vec3 color=vec3(st.x,st.y,0.);
    
    // 创建网格
    float grid=step(.99,fract(st.x*10.))+
    step(.99,fract(st.y*10.));
    color=mix(color,color*1.5,grid*.3);
    
    // 标记坐标轴
    if(st.x<.01)color=vec3(1.,.2,.2);// 左边缘 - 红(X=0)
    if(st.y<.01)color=vec3(.2,1.,.2);// 下边缘 - 绿(Y=0)
    if(st.x>.99)color=vec3(.8,.2,.2);// 右边缘 - 暗红(X=1)
    if(st.y>.99)color=vec3(.2,.8,.2);// 上边缘 - 暗绿(Y=1)
    
    // 标记关键点 - 不使用数组，分别处理
    // (0,0) - 红色
    if(distance(st,vec2(0.,0.))<.02)color=vec3(1.,0.,0.);
    // (1,0) - 绿色
    else if(distance(st,vec2(1.,0.))<.02)color=vec3(0.,1.,0.);
    // (0,1) - 蓝色
    else if(distance(st,vec2(0.,1.))<.02)color=vec3(0.,0.,1.);
    // (1,1) - 黄色
    else if(distance(st,vec2(1.,1.))<.02)color=vec3(1.,1.,0.);
    // (0.5,0.5) - 白色
    else if(distance(st,vec2(.5,.5))<.02)color=vec3(1.,1.,1.);
    
    // 显示鼠标位置 - 粉色十字
    if(abs(st.x-mouse.x)<.005)color=mix(color,vec3(1.,0.,1.),.7);
    if(abs(st.y-mouse.y)<.005)color=mix(color,vec3(1.,0.,1.),.7);
    
    // 鼠标位置圆点
    if(distance(st,mouse)<.02){
        color=vec3(1.,0.,1.);// 粉色
    }
    
    gl_FragColor=vec4(color,1.);
}