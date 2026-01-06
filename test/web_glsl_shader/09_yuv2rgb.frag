// Author @patriciogv - 2015
// http://patriciogonzalezvivo.com
#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;
uniform float u_time;

// YUV to RGB matrix
mat3 yuv2rgb=mat3(1.,0.,1.13983,
    1.,-.39465,-.58060,
1.,2.03211,0.);

// RGB to YUV matrix
mat3 rgb2yuv=mat3(.2126,.7152,.0722,
    -.09991,-.33609,.43600,
.615,-.5586,-.05639);

void main(){
    vec2 st=gl_FragCoord.xy/u_resolution;
    vec3 color=vec3(0.);
    
    // UV values goes from -1 to 1
    // So we need to remap st (0.0 to 1.0)
    st-=.5;// becomes -0.5 to 0.5
    st*=2.;// becomes -1.0 to 1.0
    
    // we pass st as the y & z values of
    // a three dimensional vector to be
    // properly multiply by a 3x3 matrix
    color=yuv2rgb*vec3(.5,st.x,st.y);
    
    // color =rgb2yuv*vec3(.5,st.x,st.y);
    
    gl_FragColor=vec4(color,1.);
}
