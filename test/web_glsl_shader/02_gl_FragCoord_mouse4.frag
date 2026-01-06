#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

void main(){
    vec2 st=gl_FragCoord.xy/u_resolution;
    
    // 使用时间创建动画
    float time_factor=u_time*.5;// 控制速度
    
    // 鼠标影响：使用鼠标位置作为偏移
    vec2 mouse_st=u_mouse/u_resolution;
    
    // 创建随时间脉动的颜色
    float pulse=sin(u_time)*.5+.5;// 0到1的脉动值
    
    // 创建随时间旋转的图案
    vec2 rotated_st=vec2(
        st.x*cos(time_factor)-st.y*sin(time_factor),
        st.x*sin(time_factor)+st.y*cos(time_factor)
    );
    
    // 鼠标位置影响图案缩放
    float mouse_scale=mouse_st.x*2.+1.;
    vec2 scaled_st=rotated_st*mouse_scale;
    
    // 创建有趣的图案
    float pattern=sin(scaled_st.x*10.+u_time)*
    cos(scaled_st.y*10.+u_time);
    
    // 鼠标y位置控制混合
    float mix_factor=mouse_st.y;
    
    // 组合颜色
    vec3 color1=vec3(pattern*.5+.5,pulse,rotated_st.x);
    vec3 color2=vec3(rotated_st.y,pattern,pulse*.5+.5);
    vec3 final_color=mix(color1,color2,mix_factor);
    
    gl_FragColor=vec4(final_color,1.);
}