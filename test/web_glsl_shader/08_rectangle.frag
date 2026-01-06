// Author @patriciogv - 2015
// http://patriciogonzalezvivo.com

#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

void main(){
    vec2 st=gl_FragCoord.xy/u_resolution.xy;
    
    // bottom-left
    vec2 bl=step(vec2(.05),st);
    float pct=bl.x*bl.y;
    
    // top-right
    vec2 tr=step(vec2(.05),1.-st);
    pct*=tr.x*tr.y;
    
    vec3 color=vec3(pct);
    
    gl_FragColor=vec4(color,1.);
}
