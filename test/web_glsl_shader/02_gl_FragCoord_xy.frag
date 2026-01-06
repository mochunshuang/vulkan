#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;

void main(){
    // 规范化坐标
    vec2 st=gl_FragCoord.xy/u_resolution;
    
    // 创建颜色网格来可视化坐标
    vec3 color=vec3(0.);
    
    // 1. 左下角(0,0)区域 - 显示为红色
    if(st.x<.1&&st.y<.1){
        color=vec3(1.,0.,0.);// 红色
    }
    // 2. 右下角(1,0)区域 - 显示为绿色
    else if(st.x>.9&&st.y<.1){
        color=vec3(0.,1.,0.);// 绿色
    }
    // 3. 左上角(0,1)区域 - 显示为蓝色
    else if(st.x<.1&&st.y>.9){
        color=vec3(0.,0.,1.);// 蓝色
    }
    // 4. 右上角(1,1)区域 - 显示为黄色
    else if(st.x>.9&&st.y>.9){
        color=vec3(1.,1.,0.);// 黄色
    }
    // 5. 中心点(0.5,0.5)区域 - 显示为白色
    else if(distance(st,vec2(.5,.5))<.05){
        color=vec3(1.,1.,1.);// 白色
    }
    // 6. 其他区域显示坐标值
    else{
        color=vec3(st.x,st.y,0.);
    }
    
    gl_FragColor=vec4(color,1.);
}