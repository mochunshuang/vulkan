// Author @patriciogv ( patriciogonzalezvivo.com ) - 2015

#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform float u_time;
uniform vec2 u_mouse;

float box(in vec2 _st,in vec2 _size){
    _size=vec2(.5)-_size*.5;
    vec2 uv=smoothstep(_size,_size+vec2(.001),_st);
    uv*=smoothstep(_size,_size+vec2(.001),vec2(1.)-_st);
    return uv.x*uv.y;
}

float circle(in vec2 _st,float _radius){
    vec2 dist=_st-vec2(.5);
    return 1.-smoothstep(_radius-(_radius*.01),
    _radius+(_radius*.01),
    dot(dist,dist)*4.);
}

void main(){
    vec2 st=gl_FragCoord.xy/u_resolution.xy;
    vec3 color=vec3(0.);
    
    // 鼠标位置（0-1范围）
    vec2 mousePos=u_mouse/u_resolution;
    
    // 矩形应该放在画布中央然后移动
    vec2 translate=vec2(cos(u_time),sin(u_time))*.35;
    
    // 矩形位置（先居中，然后加上移动）
    vec2 rectPos=st-vec2(.5)-translate;
    
    // 检查鼠标是否在矩形内（同样的变换）
    vec2 mouseRectPos=mousePos-vec2(.5)-translate;
    
    // 矩形形状（使用绝对值让矩形保持中心对称）
    float boxShape=box(abs(rectPos)+vec2(.5),vec2(.3,.5));
    
    // 检查鼠标是否在矩形内（同样的计算方式）
    float mouseInBox=box(abs(mouseRectPos)+vec2(.5),vec2(.3,.5));
    bool isClicked=mouseInBox>0.;
    
    // 基础背景颜色
    vec3 bgColor=vec3(st.x,st.y,0.);
    
    // 点击效果：脉冲动画
    float pulse=0.;
    if(isClicked){
        // 计算鼠标到当前像素的距离
        float dist=distance(st,mousePos);
        // 创建点击波纹效果
        pulse=sin(u_time*10.)*.5+.5;
        pulse*=smoothstep(.2,0.,dist);
        
        // 点击时改变背景
        bgColor=mix(
            vec3(1.,.7,.8),// 粉色
            vec3(.7,.8,1.),// 蓝色
            dist*2.
        );
        
        // 添加脉冲效果到背景
        bgColor+=vec3(pulse*.3);
    }
    
    // 矩形颜色
    vec3 boxColor=vec3(boxShape);
    if(isClicked){
        // 点击时矩形变成金色并有脉动效果
        boxColor=vec3(boxShape)*vec3(1.,.84,0.);
        boxColor*=(1.+pulse*.5);
    }
    
    // 合并所有元素
    color=bgColor+boxColor;
    
    // 点击时在鼠标位置添加一个光环
    if(isClicked){
        // 创建一个围绕鼠标的光环
        float clickGlow=0.;
        float distToMouse=distance(st,mousePos);
        clickGlow=smoothstep(.15,.1,distToMouse)*(1.-smoothstep(.1,.05,distToMouse));
        color+=vec3(1.,1.,.5)*clickGlow*pulse;
    }
    
    gl_FragColor=vec4(color,1.);
}