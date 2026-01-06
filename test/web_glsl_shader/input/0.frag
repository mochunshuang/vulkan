#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform float u_time;

void main(){
    vec2 uv=gl_FragCoord.xy/u_resolution;
    
    // 计算到两个圆心的距离
    float dist1=length(uv-vec2(.3,.5));// 左圆心 (0.3, 0.5)
    float dist2=length(uv-vec2(.7,.5));// 右圆心 (0.7, 0.5)
    
    // 关键操作：取两个距离的最小值
    float combinedDist=min(dist1,dist2);
    
    float radius=.15;// 每个圆的半径
    float edgeWidth=.01;// 边缘柔化宽度
    
    // 创建合并的形状
    float shape=1.-smoothstep(radius-edgeWidth,
        radius+edgeWidth,
    combinedDist);
    
    gl_FragColor=vec4(vec3(shape),1.);
}