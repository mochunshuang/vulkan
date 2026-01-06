#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;

void main(){
    vec2 st=gl_FragCoord.xy/u_resolution;
    
    // 创建更清晰的可视化
    vec3 color;
    
    // 显示X坐标（红色通道）和Y坐标（绿色通道）
    color.r=st.x;// X从0到1，红色从黑到红
    color.g=st.y;// Y从0到1，绿色从黑到绿
    
    // 添加坐标轴
    // X轴（底部，y=0附近）显示为白色
    if(abs(st.y)<.005){
        color=vec3(1.);
    }
    // Y轴（左侧，x=0附近）显示为黄色
    else if(abs(st.x)<.005){
        color=vec3(.902,.9647,.0314);
    }else if(abs(st.y)>.995){//上侧
        color=vec3(.9294,.251,.3412);
    }else if(abs(st.x)>.995){//右侧
        color=vec3(.0392,.7569,.9569);
    }
    
    // 标记关键点
    // 左下角(0,0)
    if(distance(st,vec2(0.,0.))<.01){
        color=vec3(1.,0.,0.);// 红色标记
    }
    // 右下角(1,0)
    else if(distance(st,vec2(1.,0.))<.01){
        color=vec3(0.,1.,0.);// 绿色标记
    }
    // 左上角(0,1)
    else if(distance(st,vec2(0.,1.))<.01){
        color=vec3(0.,0.,1.);// 蓝色标记
    }
    // 右上角(1,1)
    else if(distance(st,vec2(1.,1.))<.01){
        color=vec3(.8157,0.,1.);// 紫标记
    }
    // 中心点(0.5,0.5)
    else if(distance(st,vec2(.5,.5))<.01){
        color=vec3(1.);// 白色标记
    }
    
    gl_FragColor=vec4(color,1.);
}