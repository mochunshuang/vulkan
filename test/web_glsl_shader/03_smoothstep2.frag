#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

float plot(vec2 st){
    // NOTE: 更宽的线
    return smoothstep(.05,0.,abs(st.y-st.x));
}

void main(){
    
    vec2 st=gl_FragCoord.xy/u_resolution;
    
    float y=st.x;
    
    vec3 color=vec3(y);
    
    float pct=plot(st);
    
    color=(1.-pct)*color+pct*vec3(0.,1.,0.);
    
    gl_FragColor=vec4(color,1.);
}