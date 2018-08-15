#ifndef __GPGPU
#define __GPGPU

/*
 *  GPGPU.h
 *  cmd2
 *
 *  Created by Richard Bowman on 12/01/2009.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */
#include <afxforcelibs.h>

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <stdlib.h>
#define GLEW_STATIC 1
#include <GL/glew.h>
#ifdef WIN32
#include <gl/gl.h>
#include <gl/glu.h>
#else
#include <GLUT/glut.h>
#endif

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

class GPGPU{
public: // methods
	GPGPU(bool benchmark);
	~GPGPU(){}

	int initialiseShader();
	
	void update();
	void reshape(int w, int h);
	
	float getPhysicalAspect() { return _physicalAspect; }
	float getScreenAspect() { return _screenAspect; }
	float getCentreX() { return _xCentre; }
	float getCentreY() { return _yCentre; }
	float getFrameRate() { return _framerate; }
	int getViewportWidth() {return _viewportWidth; }
	int getViewportHeight() {return _viewportHeight; }
	
	float * getSpots() { return _spots; } //these functions are fast but dangerous- we could get corrupted spots.
	float * getBlazing() { return _blazingTable; }
	float * getZernike() { return _zernikeCoefficients; }
	void setNSpots(int nspots){if(_nspots>=0) _nspots=nspots;}
	void setTotalA(float totalA){_totalA=totalA;}
	void setNASmoothness(float naSmoothness){if(naSmoothness>=0.0) _naSmoothness=naSmoothness;}
	void setCentre(float x, float y){_xCentre=x; _yCentre=y;}
	void setPhysicalAspect(float width_over_height){return; if(width_over_height>0.0) _physicalAspect=width_over_height;}
	void setScreenAspect(float width_over_height){return; if(width_over_height>0.0) _screenAspect=width_over_height;}// this->reshape(_viewportWidth,_viewportHeight); this->reshape(64,64);}
	void setGeometry(float hologramWidth, float hologramHeight, float focalLength, float k){ _hologramSize[0]=hologramWidth; _hologramSize[1]=hologramHeight; _focalLength=focalLength; _wavevector=k;}
	void dumptofile();
	unsigned char* getHologramAsU8(int width, int height);
	void getHologramAsU8(unsigned char * hologram, int width, int height);

protected: // data
	bool          _benchmark;        //if it's benchmark mode, output FPS
	
	GLfloat       _spots[MAX_N_SPOTS*SPOT_ELEMENTS];   // the array of k vectors, lens strengths, l values and amplitudes (SPOT_ELEMENTS items per spot, spots are concatenated)
	int           _nspots;           // the number of currently active spots
	
	GLfloat       _blazingTable[BLAZING_TABLE_LENGTH]; // this stores the blazing function
	GLfloat       _zernikeCoefficients[ZERNIKE_COEFFICIENTS_LENGTH]; // this stores the zernike correction
	GLfloat		  _xCentre, _yCentre;
	GLfloat       _physicalAspect;
	float		  _screenAspect;
	GLfloat       _totalA;
	GLfloat       _naSmoothness;
	GLfloat       _hologramSize[2];
	GLfloat       _focalLength;
	GLfloat       _wavevector;

	int			  _viewportWidth, _viewportHeight; //size of the viewport
	
	unsigned int  _iterations;       // used for calculating FPS
	time_t        _lastTime;         // used for calculating FPS
	float         _framerate;        // stores current FPS
	
	GLuint        _programObject;    // this holds the shader program
	GLuint        _fragmentShader;   //this holds the shader after it's been compiled and linked
	
	GLint         _spotsUniform;     // parameters to the fragment program
	GLint         _blazingUniform;
	GLint         _zernikeUniform;
	GLint         _nUniform;
	GLint		  _aspectUniform;
	GLint		  _centreUniform;
    GLint         _totalAUniform;
	GLint         _naSmoothnessUniform;
	GLint         _hologramSizeUniform;
	GLint         _focalLengthUniform;
	GLint         _kUniform;

};

#endif