
#ifdef GL_ES
// 4. float类型在shaders中非常重要，所以精度非常重要。更低的精度会有更快的渲染速度，但是会以质量为代价。
precision mediump float;
#endif

uniform float u_time;

#define cloor_red vec4(1.,0.,0.,1.);

//5. 函数
vec4 blue(){
	return vec4(0.,0.,1.,1.);
}

void main() {
	//1. 最终的像素颜色取决于预设的全局变量gl_FragColor
	//2. 这些变量是规范化的，意思是它们的值是从0到1的
	gl_FragColor = vec4(1.0,0.0,1.0,1.0);

	// 3. 可以使用宏
	gl_FragColor = cloor_red

	gl_FragColor = blue();
}
