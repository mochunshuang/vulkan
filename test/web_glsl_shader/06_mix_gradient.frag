#ifdef GL_ES
precision mediump float;
#endif

#define PI 3.14159265359

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

vec3 colorA=vec3(.149,.141,.912);
vec3 colorB=vec3(1.,.833,.224);

void main(){
    vec2 st=gl_FragCoord.xy/u_resolution.xy;
    vec3 color=vec3(0.);
    
    //NOTE: 0-1. 一开始是0就没有蓝色。
    vec3 pct=vec3(st.x);
    color=mix(colorA,colorB,pct);
    
    gl_FragColor=vec4(color,1.);
}
