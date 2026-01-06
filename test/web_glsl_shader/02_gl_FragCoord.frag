#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;//分辨率
uniform vec2 u_mouse;//鼠标位置
uniform float u_time;
/*
就像GLSL有个默认输出值vec4 gl_FragColor一样，
它也有一个默认输入值（vec4 gl_FragCoord）。
gl_FragCoord存储了活动线程正在处理的像素或屏幕碎片的坐标。
有了它我们就知道了屏幕上的哪一个线程正在运转。


*/
void main(){
    //1. 用gl_FragCoord.xy除以u_resolution，对坐标进行了规范化
    vec2 st=gl_FragCoord.xy/u_resolution;
    /*
    // 直接显示：红色=X坐标，绿色=Y坐标
    // 所以：
    // (0,0) = 黑色（红=0，绿=0）
    // (1,0) = 红色（红=1，绿=0）
    // (0,1) = 绿色（红=0，绿=1）
    // (1,1) = 黄色（红=1，绿=1）
    // (0.5,0.5) = 中等黄褐色（红=0.5，绿=0.5）
    */
    gl_FragColor=vec4(st.x,st.y,0.,1.);
}
