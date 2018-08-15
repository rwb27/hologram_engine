#ifndef __GPGPU
#define __GPGPU

/*
 *  GPGPU.h
 *  cmd2
 *
 *  Created by Richard Bowman on 12/01/2009.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 *  This GPU processing class owes a great deal to the following tutorial,
 *  which can be found on www.gpgpu.org
 *
 *  GPGPU Lesson 0: "helloGPGPU_GLSL" (a GLSL version of "helloGPGPU")
 *  written by  Mark Harris (harrism@gpgpu.org) - original helloGPGPU
 *  and Mike Weiblen (mike.weiblen@3dlabs.com) - GLSL version
 *
 *  I won't presume to claim that this code is a version of the above- that
 *  would be stretching the point a bit.  Whether or not it's a "derivative work"
 *  for  licensing purposes I am not really sure, but I'd like to acknowledge 
 *  that it was very useful and as I'm making it open source anyway, there 
 *  shouldn't be a problem
 *
 *  Richard Bowman - University of Glasgow - 2009
 */

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <stdlib.h>
#define GLEW_STATIC 1
#include <GL/glew.h>
#ifdef WIN32
//#include <gl/gl.h>//replaced by GLEW
//#include <gl/glu.h>
#else
#include <GLUT/glut.h>
#endif

//the following options are all parameters to the built-in shader source.  They are
//not really relevant any more.
// #define ZERNIKE_PERSPOT
#define ZERNIKE_GLOBAL
#define ZERNIKE_COEFFICIENTS_LENGTH 12
#define SPOT_ELEMENTS 16 //the number of parameters per spot
#define MAX_N_SPOTS 50  //nb this must be smaller than the spots[] array size in source_code.c
#define BLAZING_TABLE_LENGTH 32
// #define SPHERICALLENS
#define NA_MANIPULATION
#define LINETRAP3D
#define AMPLITUDE_SHAPING
#define OUTPUT_UNBLAZEDs
//just for reference, the spot format is (x y z l I phi [nax nay nar] [linex liney] [z0 z1 ... z11])

typedef enum{
UNIFORM_TYPE_1fv,
UNIFORM_TYPE_2fv,
UNIFORM_TYPE_3fv,
UNIFORM_TYPE_4fv,
UNIFORM_TYPE_1i,
UNIFORM_TYPE_1iv,
UNIFORM_TYPE_sampler2D,
UNIFORM_TYPE_INVALID,
UNIFORM_TYPE_COUNT
}UniformType;
#define MAX_N_UNIFORMS 50

class GPGPU{
public: // methods
	GPGPU(bool benchmark);
	~GPGPU(){}

	int recompileShader(const char ** shaderSource);

	void update();
	void reshape(int w, int h);
	
	float getScreenAspect() { return _screenAspect; }
	float getFrameRate() { return _framerate; }
	int getViewportWidth() {return _viewportWidth; }
	int getViewportHeight() {return _viewportHeight; }

	const char * getUniforms() {return (const char *) _uniformNames;}
	
	void setUniform(int uniformID, float * value, size_t length);
	void setUniform(const char * uniformName, float * value, size_t length);
	void setUniformTexture(int uniformID, GLfloat * value, size_t width, size_t height, size_t components);
	void setUniformTexture(int uniformID, GLubyte * value, size_t width, size_t height, size_t components);

	void setScreenAspect(float width_over_height){if(width_over_height>0.0) _screenAspect=width_over_height;}// this->reshape(_viewportWidth,_viewportHeight); this->reshape(64,64);}

	size_t getHologramChannelAsU8(unsigned char * buffer, size_t length, GLenum channel = GL_GREEN);
protected: // data
	int initialiseShader();			 //set up the shader programs (and re-set-up when required)
	int initialiseFragmentShader(const char ** shaderSource);
	void parseUniforms(const char * shaderSource);
	
	bool          _benchmark;        //if it's benchmark mode, output FPS
	
	float		  _screenAspect;
	int			  _viewportWidth, _viewportHeight; //size of the viewport
	
	unsigned int  _iterations;       // used for calculating FPS
	time_t        _lastTime;         // used for calculating FPS
	float         _framerate;        // stores current FPS
	
	GLuint        _programObject;    // this holds the shader program
	
	int           _n_uniforms;            // the number of uniforms we have
	GLint         _uniformLocations[MAX_N_UNIFORMS]; // handles on the uniforms in the shader
	UniformType   _uniformTypes[MAX_N_UNIFORMS];     // a note of which method to use to transfer the uniforms (float, int, vector, array, etc.)
	size_t        _uniformLengths[MAX_N_UNIFORMS];   // the number of elements in each uniform (for vectors, this is still 1 unless it's an array)
	char          _uniformNames[MAX_N_UNIFORMS*32];  // this contains the names of the uniforms separated by \n, mostly for debugging.
	GLuint		  _textureNames[MAX_N_UNIFORMS];     // for sampler2D uniforms, we need to keep track of a texture "name" (corresponding to the actual texture data
	GLint         _textureUnits[MAX_N_UNIFORMS];     // we also need a "texture unit" which is the handle on that data that's actually passed to the uniform

};

#endif