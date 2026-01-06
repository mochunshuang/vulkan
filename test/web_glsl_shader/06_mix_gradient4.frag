#ifdef GL_ES
precision mediump float;
#endif

#define PI 3.14159265359

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

// ==============================================
// 全局参数和颜色定义
// ==============================================
float timeScale=.2;// 时间缩放因子

// William Turner 日落颜色
vec3 turnerSunset1=vec3(.902,.329,.102);// 深橙红色
vec3 turnerSunset2=vec3(.976,.573,.157);// 金黄橙色
vec3 turnerSunset3=vec3(.588,.259,.424);// 紫红色
vec3 turnerSunset4=vec3(.220,.102,.349);// 深紫色

// 日出日落动画颜色
vec3 sunriseColor=vec3(1.,.4,.1);// 日出橙红色
vec3 noonColor=vec3(1.,.95,.8);// 正午淡黄色
vec3 sunsetColor=vec3(.9,.3,.1);// 日落橙红色
vec3 nightColor=vec3(.05,.05,.2);// 夜晚深蓝色

// 彩虹七色 (ROYGBIV)
vec3 rainbowRed=vec3(1.,0.,0.);
vec3 rainbowOrange=vec3(1.,.5,0.);
vec3 rainbowYellow=vec3(1.,1.,0.);
vec3 rainbowGreen=vec3(0.,1.,0.);
vec3 rainbowBlue=vec3(0.,0.,1.);
vec3 rainbowIndigo=vec3(.29,0.,.51);
vec3 rainbowViolet=vec3(.58,0.,.83);

// 旗帜颜色
vec3 flagRed=vec3(.8,.1,.1);
vec3 flagOrange=vec3(.9,.5,.1);
vec3 flagYellow=vec3(.9,.9,.1);
vec3 flagGreen=vec3(.1,.7,.1);
vec3 flagBlue=vec3(.1,.3,.8);
vec3 flagPurple=vec3(.6,.1,.7);

// ==============================================
// 辅助函数
// ==============================================

// 绘制函数曲线
float plot(vec2 st,float pct){
    return smoothstep(pct-.01,pct,st.y)-
    smoothstep(pct,pct+.01,st.y);
}

// 噪声函数，用于创建自然纹理
float noise(vec2 p){
    return sin(p.x*10.)*sin(p.y*10.)*.5+.5;
}

// 创建云状纹理
float cloud(vec2 p,float time){
    float c=0.;
    p*=2.;
    p+=time*.05;
    c+=sin(p.x*2.+sin(p.y*3.))*.2;
    c+=sin(p.y*1.5+sin(p.x*2.))*.1;
    return c*.5+.5;
}

// 平滑的脉冲函数
float pulse(float x,float center,float width){
    return smoothstep(center-width,center,x)-
    smoothstep(center,center+width,x);
}

// ==============================================
// 挑战1: William Turner 日落渐变
// ==============================================

vec3 challenge1_turnerSunset(vec2 st){
    vec3 color;
    
    // 使用多层渐变模拟Turner的绘画风格
    float t=st.y;// 使用垂直方向作为渐变
    
    // 第一层：橙红到金黄
    vec3 layer1=mix(turnerSunset1,turnerSunset2,smoothstep(0.,.4,t));
    
    // 第二层：金黄到紫红
    vec3 layer2=mix(turnerSunset2,turnerSunset3,smoothstep(.4,.7,t));
    
    // 第三层：紫红到深紫
    vec3 layer3=mix(turnerSunset3,turnerSunset4,smoothstep(.7,1.,t));
    
    // 合并所有层次
    if(t<.4){
        color=layer1;
    }else if(t<.7){
        color=layer2;
    }else{
        color=layer3;
    }
    
    // 添加云层纹理
    float cloudPattern=cloud(st*3.,u_time*.5)*.3+.7;
    color*=cloudPattern;
    
    // 添加阳光辉光效果
    float sunGlow=smoothstep(.7,.3,distance(st,vec2(.5,.7)));
    color+=vec3(1.,.7,.3)*sunGlow*.3;
    
    // 添加水平线
    if(abs(st.y-.3)<.002){
        color=mix(color,vec3(.2,.1,.05),.8);
    }
    
    return color;
}

// ==============================================
// 挑战2: 日出日落动画
// ==============================================
vec3 challenge2_sunriseSunset(vec2 st){
    // 创建一天的时间循环 (24秒模拟一天)
    float dayTime=mod(u_time*timeScale,24.);
    
    // 计算太阳位置
    float sunY;
    vec3 skyColor;
    
    if(dayTime<6.){// 日出前 (0-6秒)
        float t=dayTime/6.;
        sunY=.2+.3*t;// 太阳从地平线升起
        skyColor=mix(nightColor,sunriseColor,t);
    }else if(dayTime<12.){// 上午 (6-12秒)
        float t=(dayTime-6.)/6.;
        sunY=.5+.3*t;// 太阳升高
        skyColor=mix(sunriseColor,noonColor,t);
    }else if(dayTime<18.){// 下午 (12-18秒)
        float t=(dayTime-12.)/6.;
        sunY=.8-.3*t;// 太阳降低
        skyColor=mix(noonColor,sunsetColor,t);
    }else{// 夜晚 (18-24秒)
        float t=(dayTime-18.)/6.;
        sunY=.5-.3*t;// 太阳落下
        skyColor=mix(sunsetColor,nightColor,t);
    }
    
    // 太阳的水平位置随正弦变化，模拟真实移动
    float sunX=.5+.3*sin(dayTime*.1);
    
    // 创建渐变天空
    vec3 color=mix(skyColor,nightColor,st.y);
    
    // 添加太阳
    float sun=smoothstep(.05,.04,distance(st,vec2(sunX,sunY)));
    float sunGlow=smoothstep(.15,0.,distance(st,vec2(sunX,sunY)));
    
    vec3 sunColor;
    if(dayTime<6.||dayTime>18.){
        sunColor=sunriseColor;// 日出日落时的红色太阳
    }else{
        sunColor=vec3(1.,.95,.8);// 正午的白色太阳
    }
    
    color=mix(color,sunColor*1.5,sun);
    color+=sunColor*sunGlow*.3;
    
    // 添加云朵
    float cloudValue=cloud(st*4.,u_time*.2)*.5;
    vec3 cloudColor=mix(vec3(1.),skyColor,.5);
    color=mix(color,cloudColor,cloudValue*smoothstep(.3,.7,st.y));
    
    // 添加地面
    if(st.y<.3){
        float groundPattern=noise(st*20.)*.1+.9;
        vec3 groundColor=mix(vec3(.1,.08,.05),vec3(.2,.15,.1),st.x);
        color=mix(color,groundColor,smoothstep(.3,.29,st.y));
        color*=groundPattern;
    }
    
    // 显示时间文本背景
    if(st.x>.65&&st.y>.9){
        color=mix(color,vec3(0.,0.,0.),.5);
    }
    
    return color;
}

// ==============================================
// 挑战3: 彩虹
// ==============================================
vec3 challenge3_rainbow(vec2 st){
    vec3 color=vec3(0.);
    
    // 彩虹中心位置
    vec2 rainbowCenter=vec2(.5,.3);
    
    // 计算角度和距离
    vec2 toCenter=st-rainbowCenter;
    float angle=atan(toCenter.y,toCenter.x);
    float distance=length(toCenter);
    
    // 创建彩虹 (多个同心圆环)
    float rainbow=0.;
    
    // 彩虹七层，每层对应一种颜色
    float redBand=smoothstep(.25,.251,distance)-
    smoothstep(.265,.266,distance);
    float orangeBand=smoothstep(.23,.231,distance)-
    smoothstep(.245,.246,distance);
    float yellowBand=smoothstep(.21,.211,distance)-
    smoothstep(.225,.226,distance);
    float greenBand=smoothstep(.19,.191,distance)-
    smoothstep(.205,.206,distance);
    float blueBand=smoothstep(.17,.171,distance)-
    smoothstep(.185,.186,distance);
    float indigoBand=smoothstep(.15,.151,distance)-
    smoothstep(.165,.166,distance);
    float violetBand=smoothstep(.13,.131,distance)-
    smoothstep(.145,.146,distance);
    
    // 应用彩虹颜色
    color+=rainbowRed*redBand;
    color+=rainbowOrange*orangeBand;
    color+=rainbowYellow*yellowBand;
    color+=rainbowGreen*greenBand;
    color+=rainbowBlue*blueBand;
    color+=rainbowIndigo*indigoBand;
    color+=rainbowViolet*violetBand;
    
    // 添加彩虹背景 (天空和云)
    vec3 skyColor=mix(vec3(.5,.7,1.),vec3(.8,.9,1.),st.y);
    float clouds=cloud(st*3.,u_time*.1)*.3;
    skyColor=mix(skyColor,vec3(1.),clouds*smoothstep(.4,.8,st.y));
    
    // 混合彩虹和天空
    color=mix(skyColor,color,max(max(redBand,orangeBand),
    max(yellowBand,max(greenBand,max(blueBand,max(indigoBand,violetBand))))));
    
    // 添加彩虹的发光效果
    float glow=0.;
    for(float i=0.;i<10.;i+=1.){
        float band=.13+i*.015;
        glow+=smoothstep(band,band-.001,distance)*
        smoothstep(band+.05,band,distance);
    }
    color+=vec3(1.,1.,1.)*glow*.1;
    
    // 添加地面
    if(st.y<.15){
        vec3 groundColor=mix(vec3(.2,.3,.1),vec3(.3,.4,.2),noise(st*10.));
        color=mix(color,groundColor,smoothstep(.15,.14,st.y));
    }
    
    // 让彩虹随时间轻微波动
    float wave=sin(angle*7.+u_time*2.)*.005;
    float rainbowMask=smoothstep(.13+wave,.14+wave,distance)-
    smoothstep(.265+wave,.266+wave,distance);
    color=mix(skyColor,color,rainbowMask);
    
    return color;
}

// ==============================================
// 挑战4: 五彩旗子 (使用step函数)
// ==============================================
vec3 challenge4_colorfulFlag(vec2 st){
    vec3 color;
    
    // 使用step函数创建清晰的色块边界
    // 水平条纹
    if(step(0.,st.y)-step(.166,st.y)>0.){
        color=flagRed;// 红色条纹
    }else if(step(.166,st.y)-step(.332,st.y)>0.){
        color=flagOrange;// 橙色条纹
    }else if(step(.332,st.y)-step(.498,st.y)>0.){
        color=flagYellow;// 黄色条纹
    }else if(step(.498,st.y)-step(.664,st.y)>0.){
        color=flagGreen;// 绿色条纹
    }else if(step(.664,st.y)-step(.83,st.y)>0.){
        color=flagBlue;// 蓝色条纹
    }else{
        color=flagPurple;// 紫色条纹
    }
    
    // 添加垂直条纹
    float verticalStep=step(.5,st.x);
    color=mix(color,color*.7,verticalStep*.3);
    
    // 添加对角线分割
    float diagonal=step(st.x,st.y);
    color=mix(color,color*1.2,diagonal*.2);
    
    // 添加旗子边框
    float border=step(.02,st.x)*step(.02,st.y)*
    step(st.x,.98)*step(st.y,.98);
    color=mix(vec3(.2),color,border);
    
    // 添加旗杆
    if(st.x<.03&&st.y>.05){
        color=mix(color,vec3(.4,.3,.2),.8);
    }
    
    // 添加旗子飘动效果
    float wave=sin(st.y*20.+u_time*3.)*.01;
    float flagMask=step(.03+wave,st.x)*step(.05,st.y);
    color=mix(vec3(.9),color,flagMask);
    
    // 添加星形装饰
    for(int i=0;i<5;i++){
        float starX=.2+.15*float(i);
        float starY=.5;
        float starSize=.02;
        
        // 创建五角星
        float angle=atan(st.y-starY,st.x-starX);
        float distance=length(st-vec2(starX,starY));
        float star=smoothstep(starSize,starSize-.005,distance)*
        (.8+.2*sin(angle*5.));
        
        color=mix(color,vec3(1.,1.,1.),star);
    }
    
    return color;
}

// ==============================================
// 主函数 - 控制所有挑战的切换
// ==============================================
void main(){
    // 获取归一化坐标
    vec2 st=gl_FragCoord.xy/u_resolution.xy;
    
    // 保持宽高比
    st.x*=u_resolution.x/u_resolution.y;
    
    // 默认返回黑色
    vec3 color=vec3(0.);
    
    // 通过时间自动循环切换不同挑战
    // 每10秒切换一个效果
    float challengeSelector=mod(u_time*.1,4.);
    
    // 也可以通过鼠标点击位置来选择
    // 将屏幕分为4个区域，点击不同区域查看不同效果
    if(u_mouse.x>0.&&u_mouse.y>0.){
        vec2 mouseNorm=u_mouse.xy/u_resolution.xy;
        if(mouseNorm.x<.25){
            challengeSelector=0.;// 挑战1
        }else if(mouseNorm.x<.5){
            challengeSelector=1.;// 挑战2
        }else if(mouseNorm.x<.75){
            challengeSelector=2.;// 挑战3
        }else{
            challengeSelector=3.;// 挑战4
        }
    }
    
    // 根据选择器显示不同的挑战
    if(challengeSelector<1.){
        color=challenge1_turnerSunset(st);
    }else if(challengeSelector<2.){
        color=challenge2_sunriseSunset(st);
    }else if(challengeSelector<3.){
        color=challenge3_rainbow(st);
    }else{
        color=challenge4_colorfulFlag(st);
    }
    
    // 添加UI显示当前挑战
    vec3 uiColor;
    if(challengeSelector<1.){
        uiColor=turnerSunset2;
    }else if(challengeSelector<2.){
        uiColor=sunriseColor;
    }else if(challengeSelector<3.){
        uiColor=rainbowBlue;
    }else{
        uiColor=flagPurple;
    }
    
    // 在底部显示挑战名称
    if(st.y<.05){
        float segment=floor(st.x*4.)/4.;
        float segmentWidth=1./4.;
        float isActive=step(segment,challengeSelector/4.)-
        step(segment+segmentWidth,challengeSelector/4.);
        
        vec3 segmentColor=uiColor*.5;
        if(isActive>0.){
            segmentColor=uiColor;
        }
        
        color=mix(color,segmentColor,.7);
        
        // 添加文字标签
        float labelPos=st.x*4.;
        if(labelPos<1.){
            color=mix(color,turnerSunset1,step(.05,abs(labelPos-.5)));
        }else if(labelPos<2.){
            color=mix(color,sunriseColor,step(.05,abs(labelPos-1.5)));
        }else if(labelPos<3.){
            color=mix(color,rainbowGreen,step(.05,abs(labelPos-2.5)));
        }else{
            color=mix(color,flagBlue,step(.05,abs(labelPos-3.5)));
        }
    }
    
    // 添加边框
    if(st.x<.01||st.x>.99||st.y<.01||st.y>.99){
        color=mix(color,vec3(.2),.8);
    }
    
    // 输出最终颜色
    gl_FragColor=vec4(color,1.);
}