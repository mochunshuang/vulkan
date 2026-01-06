#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform float u_time;
uniform vec2 u_mouse;

// 来自 https://iquilezles.org/articles/distfunctions
float roundedBoxSDF(vec2 CenterPosition,vec2 Size,float Radius){
    return length(max(abs(CenterPosition)-Size+Radius,0.))-Radius;
}

void main(){
    // 矩形的大小（使用相对坐标）
    float w=.3;
    float h=.3;
    vec2 size=vec2(w,h)*u_resolution;
    
    // 矩形的位置（屏幕中心）
    // vec2 location=u_resolution*.5;
    // 使用鼠标位置
    vec2 location=u_mouse;
    
    // 边缘柔和度
    float edgeSoftness=1.;
    
    // 圆角半径（随时间变化）
    float radius=(sin(u_time)+1.)*30.;
    
    // 计算到边缘的距离
    float distance=roundedBoxSDF(gl_FragCoord.xy-location-(size/2.),size/2.,radius);
    
    // 平滑处理（免费抗锯齿）
    float smoothedAlpha=1.-smoothstep(0.,edgeSoftness*2.,distance);
    
    // 创建矩形颜色
    vec4 quadColor=mix(vec4(1.,1.,1.,1.),vec4(0.,.2,1.,smoothedAlpha),smoothedAlpha);
    
    // 应用阴影效果
    float shadowSoftness=30.;
    vec2 shadowOffset=vec2(0.,10.);
    float shadowDistance=roundedBoxSDF(gl_FragCoord.xy-location+shadowOffset-(size/2.),size/2.,radius);
    float shadowAlpha=1.-smoothstep(-shadowSoftness,shadowSoftness,shadowDistance);
    vec4 shadowColor=vec4(.9804,.0392,.0392,1.);
    
    // 混合阴影和矩形
    vec4 finalColor=mix(quadColor,shadowColor,shadowAlpha-smoothedAlpha);
    
    gl_FragColor=finalColor;
}