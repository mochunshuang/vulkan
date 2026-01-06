#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;

/*
vec3 color=vec3(y);// 等价于 vec3(y, y, y)
vec3 gray=vec3(.5);// 等价于 vec3(0.5, 0.5, 0.5) -> 中灰色
vec3 white=vec3(1.);// 等价于 vec3(1.0, 1.0, 1.0) -> 白色
vec3 black=vec3(0.);// 等价于 vec3(0.0, 0.0, 0.0) -> 黑色
*/
void main(){
    vec2 st=gl_FragCoord.xy/u_resolution;
    
    // 测试不同的构造函数
    float gray_value=st.x;
    
    // 方法1：标量扩展
    vec3 color1=vec3(gray_value);
    
    // 方法2：显式指定所有分量
    vec3 color2=vec3(gray_value,gray_value,gray_value);
    
    // 验证两者是否相等
    vec3 color;
    if(st.y<.5){
        color=color1;// 上半部分使用简写
    }else{
        color=color2;// 下半部分使用完整形式
    }
    
    // 添加分界线
    if(abs(st.y-.5)<.005){
        color=vec3(1.,0.,0.);// 红色分界线
    }
    
    gl_FragColor=vec4(color,1.);
}