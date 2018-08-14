/*
 *  GPGPU.cpp
 *  cmd2
 *
 *  Created by Richard Bowman on 12/01/2009.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include "GPGPU.h"

#include <string>
#include <sstream>
#include <ios>
using std::string;
using std::istringstream;
using std::ios;


#ifdef WIN32
#define P_SSCANF sscanf_s
#else
#define P_SSCANF sscanf
#endif

// This class encapsulates all of the GPGPU functionality of the example.
GPGPU::GPGPU(bool benchmark) 
{
	_benchmark=benchmark;
	_lastTime=time(0);
	_iterations=0;
	_framerate=0.0;
	_screenAspect=1.0;
	_viewportWidth=512; //this really needs to be set (using reshape()) when we know the window size.
	_viewportHeight=512;//512 matches the initial constant in wgl_gratingsandlenses.c.
	
    #include "source_code.cpp" //this just contains GLSLSource as a string constant (and it gets replaced usually by a string passed in)
	if(initialiseFragmentShader(&GLSLSource) != 0) exit(1);
	setUniformTexture(0, (GLubyte *) splashgraphics, 512, 256, 4);
}

int GPGPU::initialiseFragmentShader(const char * * shaderSource){
	//create, compile, link the fragment shader.  Also, parses the uniform variables.
	
	//lets create the fragment shader and compile it.
	_programObject = glCreateProgram();

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, shaderSource, NULL);
	glCompileShader(fragmentShader);
	glAttachShader(_programObject, fragmentShader);
	
	// Link the shader into a complete GLSL program.
	glLinkProgram(_programObject);
	GLint ret;
	glGetShaderiv(_programObject, GL_LINK_STATUS,&ret); //check it linked ok
	if (!ret)
	{
		fprintf(stderr, "Filter shader could not be linked :(.\n");
		return 1;
	}
	// Activate our shader
	glUseProgram(_programObject);
	
	parseUniforms(*shaderSource);

	if(fragmentShader) glDeleteShader(fragmentShader);
	return 0;
}

int GPGPU::recompileShader(const char ** shaderSource){
	//recompile the fragment shader (and link, parse uniforms, etc.)
	if(_programObject) glDeleteProgram(_programObject);
	return initialiseFragmentShader(shaderSource);
}

void GPGPU::setUniform(int uniformID,GLfloat * value, size_t length){
	//this function sets the value of the specified uniform.  The uniform ID is NOT an OpenGL 
	//handle, it is the index of the uniform in my array of uniforms that gets parsed from the 
	//shader source.  This ought to be the uniforms in the order they are declared in the 
	//shader.
	if(uniformID>_n_uniforms) return;
	switch(_uniformTypes[uniformID]){
		case UNIFORM_TYPE_1fv:
			if(length>_uniformLengths[uniformID]) length=_uniformLengths[uniformID];
			glUniform1fv(_uniformLocations[uniformID],length,value);
			break;
		case UNIFORM_TYPE_2fv:
			if(length/2>_uniformLengths[uniformID]) length=2*_uniformLengths[uniformID];
			glUniform2fv(_uniformLocations[uniformID],length/2,value);
			break;
		case UNIFORM_TYPE_3fv:
			if(length/3>_uniformLengths[uniformID]) length=3*_uniformLengths[uniformID];
			glUniform3fv(_uniformLocations[uniformID],length/3,value);
			break;
		case UNIFORM_TYPE_4fv:
			if(length/4>_uniformLengths[uniformID]) length=4*_uniformLengths[uniformID];
			glUniform4fv(_uniformLocations[uniformID],length/4,value);
			break;
		case UNIFORM_TYPE_1i:
			if(length>_uniformLengths[uniformID]) length=_uniformLengths[uniformID];
			glUniform1i(_uniformLocations[uniformID],(int) value[0]);
			break;
		case UNIFORM_TYPE_1iv:
			if(length>_uniformLengths[uniformID]) length=_uniformLengths[uniformID];
			GLint * ivalue = (GLint *) malloc(sizeof(GLint)*length);
			for(unsigned int i=0; i<length; i++) ivalue[i]=(int)value[i]; //convert to integer
			free(ivalue);
			break;
	}
}

void GPGPU::setUniformTexture(int uniformID, GLfloat * value, size_t width, size_t height, size_t components){
	//set a texture corresponding to a uniform
	if(uniformID > _n_uniforms) return;
	if(_uniformTypes[uniformID] != UNIFORM_TYPE_sampler2D) return;
	
	glActiveTexture(GL_TEXTURE0 + _textureUnits[uniformID]);                       //pick the right slot
	glBindTexture(GL_TEXTURE_2D, _textureNames[uniformID]);                        //bind our texture (i.e. select that one)
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, value);  //load the data into the texture

	glUniform1i(_uniformLocations[uniformID], _textureUnits[uniformID]);           //ok, so this really only needs to be done once, but...
}
void GPGPU::setUniformTexture(int uniformID, GLubyte * value, size_t width, size_t height, size_t components){
	//set a texture corresponding to a uniform
	if(uniformID > _n_uniforms) return;
	if(_uniformTypes[uniformID] != UNIFORM_TYPE_sampler2D) return;
	
	glActiveTexture(GL_TEXTURE0 + _textureUnits[uniformID]);                       //pick the right slot
	glBindTexture(GL_TEXTURE_2D, _textureNames[uniformID]);                        //bind our texture (i.e. select that one)
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, value);  //load the data into the texture

	glUniform1i(_uniformLocations[uniformID], _textureUnits[uniformID]);           //ok, so this really only needs to be done once, but...
}


void GPGPU::setUniform(const char * uniformName, float * value, size_t length){
	//disused- this function sets a uniform keyed on its name rather than its index.
	//it has not been heavily tested, and if there are variable names that are prefixes 
	//(e.g. one called spots and one called spotsa) then there is no guarantee it will
	//work!
	const char * pos = strstr(_uniformNames, uniformName); //find the name
	if(pos == NULL) return;
	pos += strlen(uniformName)+1; //skip the name and the space
	int uniformID=-1;
	sscanf(pos,"%03d",&uniformID);
	if(uniformID<0) return;
	setUniform(uniformID, value, length);
}


void GPGPU::parseUniforms(const char * shaderSource){
	// Get handles on the "uniforms" (inputs) of the shaderprogram
	int numberOfSamplers = 0;

	const char * pos = shaderSource;       //find the start and end of the source code
	const char * endpos = pos + strlen(pos);
	
	_n_uniforms=0;  //scan through the text and find where 
	_uniformNames[0]='\0'; //forget about the uniforms!
	const char * lineend = pos;
	while(lineend !=NULL && (pos=strstr(lineend,"uniform")) != NULL && lineend<endpos && _n_uniforms<MAX_N_UNIFORMS){ 
			//go through the uniform declarations in the GLSL source and take note of them
			//uniform declarations should be one per line, which is why we look for the next "uniform" after the line end
		lineend = strpbrk(pos,"\n\r\f;");      //find the end of this line
		pos += 7;                             //skip past "uniform"
		pos += strspn(pos," \t");             //skip whitespace after the "uniform"
		UniformType t = UNIFORM_TYPE_INVALID;
		if(strstr(pos,"float") == pos) t=UNIFORM_TYPE_1fv;
		if(strstr(pos,"vec2") == pos) t=UNIFORM_TYPE_2fv;
		if(strstr(pos,"vec3") == pos) t=UNIFORM_TYPE_3fv;
		if(strstr(pos,"vec4") == pos) t=UNIFORM_TYPE_4fv;
		if(strstr(pos,"int") == pos) t=UNIFORM_TYPE_1iv;
		if(strstr(pos,"sampler2D") == pos) t=UNIFORM_TYPE_sampler2D;

		if(t!=UNIFORM_TYPE_INVALID){
			//the next four lines skip the whitespace after the type, placing pos at the start of the variable name.
			pos = strpbrk(pos," \t");				//skip whitespace after the type declaration
			if(pos==NULL || pos>=lineend) continue;//skip to next uniform declaration on error
			pos += strspn(pos," \t");
			if(pos==NULL || pos>=lineend) continue;//skip to next uniform declaration on error

			int nlen = (int)strpbrk(pos," \t;[=") - (int)pos; //find the length of the name
			if(strpbrk(pos," \t;[")==NULL || strpbrk(pos," \t;[") > lineend) continue;

			char * name = _uniformNames + strlen(_uniformNames);
			assert(strlen(_uniformNames)+nlen+10 < MAX_N_UNIFORMS*32); //avoid buffer overflow by crashing (!)
			memcpy((void *)name,pos,nlen);
			name[nlen]='\0';//terminated

			size_t length=1;                    //if we find a [, check the variable's length
			if(pos[nlen]=='[') P_SSCANF(pos+nlen,"[%d]",&length);

			if(t==UNIFORM_TYPE_1iv && length==1) t=UNIFORM_TYPE_1i;

			_uniformTypes[_n_uniforms]=t;
			_uniformLengths[_n_uniforms]=length;
			_uniformLocations[_n_uniforms]=glGetUniformLocation(_programObject,name);

			name[nlen]=' ';
			sprintf(name+nlen," %03d %03d\n",length,_n_uniforms);
			name[nlen+9]='\0';

			if(t==UNIFORM_TYPE_sampler2D && numberOfSamplers < GL_MAX_TEXTURE_UNITS){
				//if the uniform expects a texture as argument, we better set one up!!
				//NB there is a limit on the number of textures we can have- and the above if statement respects it.
				_textureUnits[_n_uniforms] = numberOfSamplers;  //this is the texture unit ("slot") we will use
				glActiveTexture(GL_TEXTURE0 + numberOfSamplers);//switch to using said slot
				glGenTextures(1, &_textureNames[_n_uniforms]);  //generate a name for our texture and store it in the array
				glBindTexture(GL_TEXTURE_2D, _textureNames[_n_uniforms]);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);//don't interpolate (TODO: work this out from source)
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);      //do I even care about texture wrapping? I suspect not...
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
				numberOfSamplers++;
			}
			_n_uniforms++;
		}
	}
}

//this renders a quadrilateral using our shader.  It doesn't mess with the size of the viewport, so one pixel on screen=one pixel shaded.
void GPGPU::update()
{   

	//uniforms should be passed separately now

	//now we actually render the thing
	glClear(GL_COLOR_BUFFER_BIT); //make sure there's no junk (I had wierd stuff appearing before)

	glBegin(GL_QUADS);
	{            
		glTexCoord2f(0, 0); glVertex2f(-1, -1);
		glTexCoord2f(1, 0); glVertex2f( 1, -1);
		glTexCoord2f(1, 1); glVertex2f( 1,  1);
		glTexCoord2f(0, 1); glVertex2f(-1,  1);
	}

	glEnd();
	
	// disable the filter
	//glUseProgram(0);
	
	//timing
	if((time(0)-_lastTime)>5.0){
		_framerate=float(_iterations)/(time(0)-_lastTime);
		if(_benchmark) printf("Frame rate: %ffps\n",_framerate);
		_iterations=0;
		_lastTime=time(0);
	}else{
		_iterations++;
	}
}

void GPGPU::reshape(int w, int h){
	//change viewport parameters so that we're drawing into something 
	//which has the aspect ratio (on the screen) defined in _screenAspect
	float fw=(float)w, fh=(float)h;
	_viewportWidth=w;
	_viewportHeight=h;
	
	GLfloat sw,sh;  //these would both be 1 if viewport aspect ratio matched _screenAspect
	//we define these to scale the viewport so that [-1,1] in X and Y has the correct aspect ratio
	
	if(fw>fh*_screenAspect){
		sw=fw/fh/_screenAspect;
		sh=1;
	}else{
		sw=1;
		sh=fh/fw*_screenAspect;
	}
	
	glViewport(0, 0, w, h); //we make the viewport the same number of pixels as the window, so one pixel->one pixel
    
    // we want identity matrices in projection and modelview, to (effectively) disable those transformations
    glMatrixMode(GL_PROJECTION);    
    glLoadIdentity();               
    gluOrtho2D(-sw, sw, -sh, sh); //we use an orthographic projection, no point in perspective for a 2d surface!
    glMatrixMode(GL_MODELVIEW);     
    glLoadIdentity();    
}