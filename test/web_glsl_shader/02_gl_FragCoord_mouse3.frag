#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform vec2 u_mouse;

void main(){
    vec2 st=gl_FragCoord.xy/u_resolution;
    vec2 mouse=u_mouse/u_resolution;
    
    // 基础背景
    vec3 color=vec3(.1);
    
    // 网格线
    for(float i=0.;i<=1.;i+=.1){
        if(abs(st.x-i)<.002)color=vec3(.3);
        if(abs(st.y-i)<.002)color=vec3(.3);
    }
    
    // 显示鼠标位置的圆
    float dist=distance(st,mouse);
    if(dist<.03){
        if(dist<.005){
            color=vec3(1.,0.,1.);// 中心点
        }else{
            color=vec3(.8,0.,.8);// 圆环
        }
    }
    
    // 显示鼠标坐标的文本位置（简单方法）
    float textX=mouse.x;
    float textY=mouse.y;
    
    // 在鼠标位置显示坐标值（近似显示）
    if(abs(st.x-textX)<.01&&abs(st.y-textY)<.01){
        color=vec3(1.,1.,0.);
    }
    
    // 标记坐标原点(0,0)
    if(st.x<.02&&st.y<.02){
        color=vec3(1.,0.,0.);
    }
    
    gl_FragColor=vec4(color,1.);
}