/*
 *  source_code.cpp
 *  this contains the GLSL source code for the fragment shader that we use to render the hologram
 *
 */

// This shader performs the gratings and lenses algorithm
static const char *GLSLSource = {
 "const vec4 white = vec4(1,1,1,1);"
 "const float pi = 3.1415;"
 "uniform int n;"
 "uniform float aspect=1.0;"//physical width/height of hologram (usually this is 1 for a square hologram)
 "uniform vec2 centre=vec2(0.5,0.5);"//centre of the hologram as a fraction of its size (usually 0.5,0.5)
 "uniform vec2 size=vec2(0.003,0.003);"//size of the hologram in metres
 "uniform float f=0.0016;"//focal length
 "uniform float k=10000000.0;"//wavevector
 "uniform float totalA;"    //WARNING both of the below must match up with MAX_N_SPOTS and BLAZING_TABLE_LENGTH (and line at bottom)
 "uniform vec4 spots[200];" //spot parameters- each spot corresponds to 2 vec4, first one is x,y,z,l, second one is amplitude, -,-,- 
 "uniform float blazing[32];" //blazing function
 "uniform vec4 zernikeCoefficients[3];" //zernike coefficients, matching the modes defined below.  NB should be ZERNIKE_COEFFICIENTS_LENGTH/4 long
 "uniform float nasmoothness=0.0;" //sets the fuzz factor for the edge of spots with limited NA.
 "void main(void)"
 "{"
 "   float totalr=0.0;"
 "   float totali=0.0;"
 "   float phase;"                    //phase of current pixel, due to the spot (in loop) or of the resultant hologram (after loop)
 "   float amplitude;"                //ditto for amplitude
 "   vec2 xy=(gl_TexCoord[0].xy-centre)*size;"//current xy position in the hologram, range from (-1 to 1)*size (see above for aspect) (if centre=0.5,0.5)
 "   float x=gl_TexCoord[0].x-centre.x, y=gl_TexCoord[0].y-centre.y;"
 "   float r2=x*x+y*y;"            //distance from centre of hologram, squared
 "   float phi=atan2(x,y);"		      //angle of the line joining our point to the centre of the pattern
 "   float length;"                   //length of a line
 "   vec4 pos=vec4(xy/f,1.0-dot(xy,xy)/2.0/f/f,phi/k);"
#ifdef SPHERICALLENS
 "   pos[2]=f/sqrt(dot(xy,xy)+f*f);"  //this is the nonparaxial approximation
#endif
 "   vec4 na;"
 "	 float sx;"
#if defined(ZERNIKE_PERSPOT) || defined(ZERNIKE_GLOBAL)
 "   x*=2.0;"                        //the Zernike polynomials are only orthogonal on a unit disc and this is easier than munging coefficients!
 "   y*=2.0;"
 "   r2*=4.0;"
 "   vec4 zerna=vec4(2.0*x*y,2.0*r2-1.0,x*x-y*y,3.0*x*x*y-y*y*y);" //zernike modes (2 -2, 2 0, 2 2, 3 -3)
 "   vec4 zernb=vec4((3.0*r2-2.0)*y,(3.0*r2-2.0)*x,x*x*x-3.0*x*y*y,4.0*x*y*(x*x-y*y));" //3 -1 through 4 -4
 "   vec4 zernc=vec4((4.0*r2-3.0)*zerna[0],6.0*r2*r2-6*r2+1,(4.0*r2-3.0)*zerna[2],x*x*x*x-6.0*x*x*y*y+y*y*y*y);" //4 -2 through 4 4
 "   x/=2.0;"                        //the Zernike polynomials are only orthogonal on a unit disc and this is easier than munging coefficients!
 "   y/=2.0;"
 "   r2/=4.0;"
#endif
 "   int j=0;"
 "   for(int i=0; i<n; i++){"
 "		j=4*i;"
 "      amplitude=spots[j+1][0];"        //amplitude of current spot
 "      phase=k*dot(spots[j],pos)+spots[j+1][1];" 
#ifdef ZERNIKE_PERSPOT
 "		phase += dot(spots[j+3],zerna)+dot(spots[j+4],zernb)+dot(spots[j+5],zernc);"
#endif
#ifdef NA_MANIPULATION
 "      na = spots[j+2];" //restrict the spot to a region of the back aperture which is na[2] in radius, centred on na.xy
// "      if(dot(na.xy-xy,na.xy-xy)>na[2]*na[2]) amplitude=0.0;"
 "      amplitude *= 1.0-smoothstep(1.0,1.0001+nasmoothness,dot(na.xy-xy/size,na.xy-xy/size)/na[2]/na[2]);" //soften the edge
#endif
#ifdef LINETRAP3D //creates an xyz line trap, needs amplitude shaping.
 "      length=sqrt(dot(spots[j+3].xyz,spots[j+3].xyz));"
 "      if(length>0.0){"
 "		  sx=k*dot(vec4(pos.xyz,1.0*length),spots[j+3]);"
 "		  if(sx!=0.0) amplitude*=sin(sx)/sx;"
 "      }"
#endif
 "      totalr += amplitude*sin(phase);"	  //the way atan is defined, we need to use sin for r and cos for i.
 "      totali += amplitude*cos(phase);"
 "   }"
 "   amplitude = sqrt(totalr*totalr+totali*totali);"
 "   phase=atan2(totalr,totali);"
 "   if(amplitude==0.0) phase=0.0;"
#ifdef AMPLITUDE_SHAPING
 "   if(totalA>0.0){" //do amplitude-shaping (dumps light into zero order when not needed)
 "     phase *= clamp(amplitude/totalA,0.0,1.0);"
 "   }"
#endif
#ifdef ZERNIKE_GLOBAL
 "   phase += dot(zernikeCoefficients[0],zerna) + dot(zernikeCoefficients[1],zernb) + dot(zernikeCoefficients[2],zernc);"
 "   phase = mod(phase + pi, 2*pi) -pi;"
#endif
#ifdef OUTPUT_COLOUR
 // the 3 lines below do colour output
 "   vec4 twhite = vec4(1,1,1,0);"
 "   vec4 rainbow = vec4(4.0/3.0*pi,2.0/3.0*pi,0,0);"
 "   gl_FragColor = clamp(2.0-(abs(mod(twhite*(phase+3.0*pi)-rainbow,2.0*pi)-pi))*(3.0/pi),0.0,1.0);" // phase rainbow output
#else
#ifdef OUTPUT_BOULDER
 // the 3 lines below do 16 bit output to the red (LSB) and green (MSB) channels, for Boulder XY series 16 bit SLMs
 "   float phint = floor((phase/2.0/pi +0.5)*255.9);" //this needs to be BLAZING_TABLE_LENGTH-(something small)
 "   float alpha = fract((phase/2.0/pi +0.5)*255.9);"
 "   gl_FragColor = vec4(alpha,phint/255.0,0.0,1.0);" //this uses the blazing table with linear interpolation
#else
#ifdef OUTPUT_UNBLAZED  // this one does grayscale output, black=-pi white=pi
 "   gl_FragColor = white*(phase/2.0/pi +0.5);"
#else
 // the 3 lines below do blazed black and white output to all 3 colour channels (most SLMs use this)
 "   int phint = int(floor((phase/2.0/pi +0.5)*30.9999999));" //this needs to be BLAZING_TABLE_LENGTH-(something small)-1
 "   float alpha = fract((phase/2.0/pi +0.5)*30.9999999);"
 "   gl_FragColor = white * blazing[phint]*(1.0-alpha)+blazing[phint+1]*alpha;" //this uses the blazing table with linear interpolation
#endif
#endif
#endif
 // "	 gl_FragColor = float(n)/4.0*white;"
 // "	 gl_FragColor = mod(y/f,1.0)*white;"
 "}"
};