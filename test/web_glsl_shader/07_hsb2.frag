#ifdef GL_ES
precision mediump float;
#endif

#define TWO_PI 6.28318530718

uniform vec2 u_resolution;
uniform float u_time;

//  Function from Iñigo Quiles
//  https://www.shadertoy.com/view/MsS3Wc
vec3 hsb2rgb(in vec3 c){
    vec3 rgb=clamp(abs(mod(c.x*6.+vec3(0.,4.,2.),6.)-3.)-1.,0.,1.);
    rgb=rgb*rgb*(3.-2.*rgb);
    return c.z*mix(vec3(1.),rgb,c.y);
}

void main(){
    vec2 st=gl_FragCoord.xy/u_resolution;
    vec3 color=vec3(0.);
    
    // 使用极坐标
    vec2 toCenter=vec2(.5)-st;
    float angle=atan(toCenter.y,toCenter.x);
    float radius=length(toCenter)*2.;
    
    // 标准拾色器色轮：
    // 1. 角度决定色相 (0-360度)
    // 2. 半径决定饱和度 (中心为0，边缘为1)
    // 3. 亮度固定为1（或可以调整）
    
    // 将角度从[-π, π]映射到[0, 1]
    float hue=angle/TWO_PI+.5;
    
    // 确保红色在顶部（标准拾色器通常红色在顶部/12点方向）
    // 默认情况下红色在0度（右侧），我们可以旋转-90度使红色在顶部
    hue=fract(hue-.25);
    
    // 标准拾色器：中心是白色（饱和度0），边缘是纯色（饱和度1）
    float saturation=radius;
    
    // 创建圆形遮罩 - 只显示半径<=1的部分
    float mask=1.-smoothstep(.99,1.01,radius);
    
    // 白色中心效果 - 中心区域保持白色
    float whiteCenter=1.-smoothstep(0.,.1,radius);
    
    // 组合颜色
    if(radius<=1.){
        // 色轮部分
        color=hsb2rgb(vec3(hue,saturation,1.));
        
        // 混合白色中心
        color=mix(color,vec3(1.),whiteCenter);
        
        // 添加一个微妙的边框
        float border=smoothstep(.95,1.,radius);
        color=mix(color,vec3(.2),border*.3);
    }else{
        // 背景色
        color=vec3(.1);
    }
    
    gl_FragColor=vec4(color,1.);
}