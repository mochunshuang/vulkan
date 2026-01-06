#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform float u_time;

// ==============================================
// 情感颜色定义
// ==============================================

// 情感1: 平静 (Serenity) - 天空蓝和淡紫的渐变
// 代表平静、安宁、冥想的状态
vec3 calmColorStart=vec3(.482,.698,.859);// 浅天蓝色
vec3 calmColorEnd=vec3(.686,.580,.847);// 淡紫色

// 情感2: 活力 (Vitality) - 日落到火焰的渐变
// 代表能量、热情、活力的状态
vec3 vibrantColorStart=vec3(1.,.647,0.);// 橙黄色
vec3 vibrantColorEnd=vec3(.863,.078,.235);// 深红色

// ==============================================
// 自定义缓动函数 (Easing Functions)
// ==============================================

// 1. 平滑缓入缓出 (Smooth In-Out)
// 类似正弦曲线，开始和结束都很平滑
float easeInOutSmooth(float t){
    // 使用sin函数创建平滑的S形曲线
    // sin在π弧度内从-1到1，我们调整到0到1
    return(sin((t-.5)*3.14159)+1.)*.5;
}

// 2. 弹跳效果 (Bounce Effect)
// 模拟物理弹跳，增加趣味性
float easeOutBounce(float t){
    if(t<1./2.75){
        return 7.5625*t*t;
    }else if(t<2./2.75){
        t-=1.5/2.75;
        return 7.5625*t*t+.75;
    }else if(t<2.5/2.75){
        t-=2.25/2.75;
        return 7.5625*t*t+.9375;
    }else{
        t-=2.625/2.75;
        return 7.5625*t*t+.984375;
    }
}

// 3. 弹性效果 (Elastic Effect)
// 像弹簧一样振动，然后稳定
float easeOutElastic(float t){
    if(t==0.||t==1.)return t;
    
    float p=.3;// 振动周期
    float s=p/4.;
    t=t-1.;
    
    return-pow(2.,10.*t)*sin((t-s)*(2.*3.14159)/p);
}

// 4. 心跳效果 (Heartbeat)
// 模拟心跳的节奏 - 快速两次跳动后暂停
float heartbeatPulse(float t){
    // 将时间映射到心跳周期
    float cycleTime=mod(t*2.,1.);
    
    if(cycleTime<.3){
        // 第一次心跳
        return sin(cycleTime*10.471975)*.5+.5;
    }else if(cycleTime<.6){
        // 第二次心跳
        return sin((cycleTime-.3)*10.471975)*.5+.5;
    }else{
        // 暂停期
        return 0.;
    }
}

// 5. 呼吸效果 (Breathing)
// 模拟缓慢的呼吸节奏
float breathing(float t){
    // 使用sin函数创建缓慢的波浪
    return sin(t*1.5708)*.5+.5;
}

// ==============================================
// 主函数
// ==============================================

void main(){
    // 获取归一化像素坐标 (0.0 到 1.0)
    vec2 st=gl_FragCoord.xy/u_resolution;
    
    // 初始化颜色为黑色
    vec3 color=vec3(0.);
    
    // 控制动画速度
    float speed=.5;// 较慢的动画速度，更好观察
    
    // ==============================================
    // 选择缓动函数 (取消注释其中一个来测试)
    // ==============================================
    
    // float pct=easeInOutSmooth(abs(sin(u_time*speed)));// 平滑过渡
    // float pct=easeOutBounce(abs(sin(u_time*speed)));// 弹跳效果
    // float pct=easeOutElastic(abs(sin(u_time*speed)));// 弹性效果
    // float pct=heartbeatPulse(u_time*speed);// 心跳效果
    float pct=breathing(u_time*speed);// 呼吸效果
    
    // ==============================================
    // 选择情感颜色组合 (取消注释其中一个)
    // ==============================================
    
    // 组合1: 平静情感过渡 (浅天蓝 ↔ 淡紫)
    // vec3 colorA=calmColorStart;
    // vec3 colorB=calmColorEnd;
    
    // 组合2: 活力情感过渡 (橙黄 ↔ 深红)
    // vec3 colorA=vibrantColorStart;
    // vec3 colorB=vibrantColorEnd;
    
    // 组合3: 平静到活力的过渡 (跨越情感的过渡)
    vec3 colorA=mix(calmColorStart,vibrantColorStart,abs(sin(u_time*.2)));
    vec3 colorB=mix(calmColorEnd,vibrantColorEnd,abs(sin(u_time*.2)));
    
    // ==============================================
    // 应用颜色混合
    // ==============================================
    
    // 使用mix函数混合两种颜色
    // pct值控制混合比例：0.0=纯colorA, 1.0=纯colorB
    color=mix(colorA,colorB,pct);
    
    // ==============================================
    // 可视化效果增强
    // ==============================================
    
    // 1. 添加渐变背景
    vec3 bgColor=mix(vec3(.05),vec3(.15),st.y);
    color=mix(bgColor,color,.9);
    
    // 2. 在屏幕中央添加光晕效果
    float centerDist=distance(st,vec2(.5));
    float glow=1.-smoothstep(0.,.5,centerDist);
    color+=vec3(.1,.1,.15)*glow*.5;
    
    // 3. 显示当前pct值 (在右上角)
    if(st.x>.7&&st.y>.9){
        // 创建一个小进度条显示当前pct
        float barWidth=.25;
        float barHeight=.05;
        float barX=.7;
        float barY=.92;
        
        // 进度条背景
        if(st.x>barX&&st.x<barX+barWidth&&
        st.y>barY&&st.y<barY+barHeight){
            
            float barPos=(st.x-barX)/barWidth;
            
            if(barPos<pct){
                // 进度条前景 (使用当前混合颜色)
                color=mix(color,color,.7);
            }else{
                // 进度条背景
                color=mix(color,vec3(.3),.5);
            }
            
            // 进度条边框
            if(abs(st.x-barX)<.002||abs(st.x-(barX+barWidth))<.002||
            abs(st.y-barY)<.002||abs(st.y-(barY+barHeight))<.002){
                color=vec3(.8);
            }
        }
    }
    
    // 4. 在左上角显示情感标签
    if(st.x<.3&&st.y>.9){
        // 平静区域显示"Calm"
        if(st.x<.15){
            color=mix(color,calmColorStart,.3);
        }
        // 活力区域显示"Vibrant"
        else{
            color=mix(color,vibrantColorEnd,.3);
        }
    }
    
    // 输出最终颜色
    gl_FragColor=vec4(color,1.);
}