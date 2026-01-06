#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform float u_time;

/*
K=vec4(0.,-1./3.,2./3.,-1.)是预计算的常数
使用step()和mix()避免条件分支（GPU友好）
1.e-10是防止除以零的小量

*/
vec3 rgb2hsb(in vec3 c){
    vec4 K=vec4(0.,-1./3.,2./3.,-1.);
    vec4 p=mix(vec4(c.bg,K.wz),
    vec4(c.gb,K.xy),
    step(c.b,c.g));
    vec4 q=mix(vec4(p.xyw,c.r),
    vec4(c.r,p.yzx),
    step(p.x,c.r));
    float d=q.x-min(q.w,q.y);
    float e=1.e-10;
    return vec3(abs(q.z+(q.w-q.y)/(6.*d+e)),d/(q.x+e),q.x);
}

//  Function from Iñigo Quiles
//  https://www.shadertoy.com/view/MsS3Wc
vec3 hsb2rgb(in vec3 c){
    /*
    将色相(0-1)映射到6个60度区间
    魔法数字0.,4.,2.是为了错开R/G/B通道的相位
    6.和3.是HSV色轮的常数（360°/6段）
    */
    vec3 rgb=clamp(abs(mod(c.x*6.+vec3(0.,4.,2.),6.)-3.)-1.,0.,1.);
    //三次平滑曲线,标准的平滑插值公式，使颜色过渡更自然。
    rgb=rgb*rgb*(3.-2.*rgb);
    return c.z*mix(vec3(1.),rgb,c.y);
}

/*
我们不能脱离色彩空间来谈论颜色。正如你所知，除了rgb值，有其他不同的方法去描述定义颜色。

HSB代表色相，饱和度和亮度（或称为值）。这更符合直觉也更有利于组织颜色。稍微花些时间阅读下面的rgb2hsv()和hsv2rgb()函数。
将x坐标（位置）映射到Hue值并将y坐标映射到明度，我们就得到了五彩的可见光光谱。这样的色彩空间分布实现起来非常方便，比起RGB，用HSB来拾取颜色更直观。
*/
void main(){
    vec2 st=gl_FragCoord.xy/u_resolution;
    vec3 color=vec3(0.);
    
    //NOTE:不要修改转换函数内的常数-这是标准数学转换
    //NOTE:在调用函数时调整输入参数-这是正确的自定义方式
    // We map x (0.0 - 1.0) to the hue (0.0 - 1.0)
    // And the y (0.0 - 1.0) to the brightness
    color=hsb2rgb(vec3(st.x,1.,st.y));
    
    gl_FragColor=vec4(color,1.);
    
    //六个60°扇区（红→黄→绿→青→蓝→紫→红）
}
