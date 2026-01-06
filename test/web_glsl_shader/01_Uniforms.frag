#ifdef GL_ES
precision mediump float;
#endif

//1. 每个线程负责给完整图像的一部分配置颜色。
//   尽管每个线程和其他线程之间不能有数据交换，但我们能从CPU给每个线程输入数据
//2. 因为显卡的架构，所有线程的输入值必须统一（uniform），而且必须设为只读。
//   也就是说，每条线程接收相同的数据，并且是不可改变的数据
/*
这些输入值叫做uniform（统一值），
它们的数据类型通常为：float,vec2,vec3,vec4,mat2,mat3,mat4,sampler2D and samplerCube。
uniform值需要数值类型前后一致。且在shader的开头，在设定精度之后，就对其进行定义。
按业界传统应在uniform值的名字前加u_
*/
uniform float u_time;

void main(){
    //---------------------------1. 理解基础代码 -------------------------
    // gl_FragColor是输出颜色，vec4(r,g,b,a)
    // abs(sin(u_time))会在0-1之间周期变化
    // gl_FragColor=vec4(abs(sin(u_time)),0.,0.,1.);
    //--------------------------------------------------------------------
    
    //---------------------------2. 修改速率 -------------------------
    // 把速度乘以0.2 变慢
    // 把速度乘以10，变化变快10倍
    // float slow_speed=.2*u_time;
    // float slow_speed=10.*u_time;
    // float slow_speed=100.*u_time;
    // gl_FragColor=vec4(abs(sin(slow_speed)),0.,0.,1.);
    //--------------------------------------------------------------------
    
    //---------------------------2. 修改速率 -------------------------
    // 分别控制RGB三通道
    // 红色：正常速度
    // float r=abs(sin(u_time));
    // // 绿色：2倍速
    // float g=abs(sin(u_time*2.));
    // // 蓝色：3倍速
    // float b=abs(sin(u_time*3.));
    // gl_FragColor=vec4(r,g,b,1.);
    
    //3. 添加相位偏移
    // 每个颜色不仅速度不同，起始时间也不同
    // float r=abs(sin(u_time));// 红：正常
    // float g=abs(sin(u_time*1.5+1.));// 绿：1.5倍速，偏移1.0
    // float b=abs(sin(u_time*2.+2.));// 蓝：2倍速，偏移2.0
    // gl_FragColor=vec4(r,g,b,1.);
    
    // 4. 使用时间做循环动画
    // 使用fmod实现循环，每2秒循环一次
    float loop_time=mod(u_time,2.);
    // 红：从0到1渐变
    float r=loop_time/2.;
    // gl_FragColor=vec4(r,0,0,1.);
    
    // 绿：从1到0渐变
    float g=1.-loop_time/2.;
    // gl_FragColor=vec4(0,g,0,1.);
    
    // 蓝：呼吸效果
    float b=abs(sin(loop_time*3.14159));
    // gl_FragColor=vec4(0,0,b,1.);
    
    // gl_FragColor=vec4(0,g,b,1.);
    
    gl_FragColor=vec4(r,g,b,1.);
    
}