#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform vec2 u_mouse;// 像素坐标，如(512.0, 384.0)
uniform float u_time;

void main(){
    // 规范化当前像素坐标
    vec2 st=gl_FragCoord.xy/u_resolution;
    
    // 规范化鼠标坐标（这样我们可以看到鼠标在画布上的位置）
    vec2 mouse_st=u_mouse/u_resolution;
    
    // 用鼠标位置偏移颜色
    vec2 offset=mouse_st;
    vec2 color_pos=st+offset*.5;// 限制偏移范围
    
    // 确保值在[0,1]范围内
    color_pos=fract(color_pos);
    
    gl_FragColor=vec4(color_pos.x,color_pos.y,.5,1.);
}