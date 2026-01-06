#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;

void main(){
    vec2 st=gl_FragCoord.xy/u_resolution;
    
    // 参数控制
    float grid_density=20.;// 网格密度：20x20
    float line_width=.02;// 线宽（相对比例）
    
    vec3 color=vec3(.1);//背景色
    
    // 创建网格
    if(mod(st.x*grid_density,1.)<line_width||
    mod(st.y*grid_density,1.)<line_width){
        color=vec3(.8);// 更亮的线条
    }
    
    gl_FragColor=vec4(color,1.);
}