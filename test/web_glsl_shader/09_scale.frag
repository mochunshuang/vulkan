// Author @patriciogv ( patriciogonzalezvivo.com ) - 2015

#ifdef GL_ES
precision mediump float;
#endif

#define PI 3.14159265359

uniform vec2 u_resolution;
uniform float u_time;

mat2 scale(vec2 _scale){
    return mat2(_scale.x,0.,0.,_scale.y);
}

float box(in vec2 _st,in vec2 _size){
    _size=vec2(.5)-_size*.5;
    vec2 uv=smoothstep(_size,_size+vec2(.001),_st);
    uv*=smoothstep(_size,_size+vec2(.001),vec2(1.)-_st);
    return uv.x*uv.y;
}

void main(){
    vec2 st=gl_FragCoord.xy/u_resolution.xy;
    vec3 color=vec3(0.);
    
    st-=vec2(.5);
    st=scale(vec2(sin(u_time)+1.))*st;
    st+=vec2(.5);
    
    // Show the coordinates of the space on the background
    color=vec3(st.x,st.y,0.);
    
    // Add the shape on the foreground
    color+=vec3(box(st,vec2(.3,.5)));
    
    gl_FragColor=vec4(color,1.);
}
