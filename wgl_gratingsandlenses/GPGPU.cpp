/*
 *  GPGPU.cpp
 *  cmd2
 *
 *  Created by Richard Bowman on 12/01/2009.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include "GPGPU.h"

// This class encapsulates all of the GPGPU functionality of the example.
GPGPU::GPGPU(bool benchmark) 
{
	_benchmark=benchmark;
	_lastTime=time(0);
	_iterations=0;
	_framerate=0.0;
	_screenAspect=1.0;
	_physicalAspect=1.0;
	_xCentre=0.5;
	_yCentre=0.5;
	_viewportWidth=512; //this really needs to be set (using reshape()) when we know the window size.
	_viewportHeight=512;//512 matches the initial constant in wgl_gratingsandlenses.c.
	_totalA=0.00001;
	_naSmoothness=0.0;
	_hologramSize[1]=0.003;
	_hologramSize[2]=0.003;
	_focalLength=0.0016;
	_wavevector=10000000.0;

	// I'm going to initialise the spots array here
	for(int i=0; i<MAX_N_SPOTS*SPOT_ELEMENTS; i++) _spots[i]=0.0;
	_nspots=0;
	
	// And the blazing table (let it be uniform for now)
	for(int i=0; i<BLAZING_TABLE_LENGTH; i++) _blazingTable[i]=float(i)/float(BLAZING_TABLE_LENGTH-1); 
	for(int i=0; i<ZERNIKE_COEFFICIENTS_LENGTH; i++) _zernikeCoefficients[i]=0.0;

	if(initialiseShader() != 0) exit(1);
	
}

int GPGPU::initialiseShader(){
	
	//lets create the fragment shader and compile it.
	_programObject = glCreateProgram();
	
    #include "source_code.cpp" //this just contains GLSLSource as a string constant
	_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(_fragmentShader, 1, &GLSLSource, NULL);
	glCompileShader(_fragmentShader);
	glAttachShader(_programObject, _fragmentShader);
	
	// Link the shader into a complete GLSL program.
	glLinkProgram(_programObject);
	GLint ret;
	glGetShaderiv(_programObject, GL_LINK_STATUS,&ret); //check it linked ok
	if (!ret)
	{
		fprintf(stderr, "Filter shader could not be linked :(.\n");
		return 1;
	}
	
	// Get handles on the "uniforms" (inputs) of the program
	_spotsUniform = glGetUniformLocation(_programObject, "spots");
	_nUniform = glGetUniformLocation(_programObject, "n");
	_blazingUniform = glGetUniformLocation(_programObject, "blazing");
	_zernikeUniform = glGetUniformLocation(_programObject, "zernikeCoefficients");
	_aspectUniform = glGetUniformLocation(_programObject, "aspect");
	_centreUniform = glGetUniformLocation(_programObject, "centre");
	_totalAUniform = glGetUniformLocation(_programObject, "totalA");
	_naSmoothnessUniform = glGetUniformLocation(_programObject, "nasmoothness");
	_kUniform = glGetUniformLocation(_programObject, "k"); //this is disabled in GLSL source for now
	_focalLengthUniform = glGetUniformLocation(_programObject, "f"); //this is disabled in GLSL source for now
	_hologramSizeUniform = glGetUniformLocation(_programObject, "size"); //this is disabled in GLSL source for now
	return 0;
}

//this renders a quadrilateral using our shader.  It doesn't mess with the size of the viewport, so one pixel on screen=one pixel shaded.
void GPGPU::update()
{   
	// Activate our shader
	glUseProgram(_programObject);
	
	// pass the spots array and n to the program
	glUniform4fv(_spotsUniform, _nspots*SPOT_ELEMENTS/4, _spots);
	glUniform1i(_nUniform, _nspots);
	glUniform1fv(_blazingUniform, BLAZING_TABLE_LENGTH, _blazingTable);
	glUniform4fv(_zernikeUniform, ZERNIKE_COEFFICIENTS_LENGTH/4, _zernikeCoefficients);
	glUniform1f(_aspectUniform,_physicalAspect);
	glUniform2f(_centreUniform,_xCentre,_yCentre);
	glUniform1f(_totalAUniform, float(_totalA)); //disabled in the GLSL source unless you enable amplitude shaping
	glUniform1f(_naSmoothnessUniform, float(_naSmoothness));
	glUniform2f(_hologramSizeUniform,_hologramSize[0],_hologramSize[1]);
	glUniform1f(_focalLengthUniform,_focalLength);
	glUniform1f(_kUniform,10000000);//_wavevector);

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
	glUseProgram(0);
	
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

unsigned char * GPGPU::getHologramAsU8(int width, int height){
	GLubyte *hologram = NULL;	//pointer to hologram
	GLint viewport[4];			//place to put the viewport
	glGetIntegerv(GL_VIEWPORT,viewport);
	unsigned long numPixels = viewport[2]*viewport[3];

	if(width != viewport[2] || height != viewport[3]) return NULL;

	//RAM it up...
	hologram = (GLubyte*)malloc(numPixels);//we assume 1 byte per pixel...
	if(hologram == NULL) return NULL;

	//set up transfer settings
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);

	//make sure there are no floaters (force a render of the hologram)
	glFlush();
    
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, viewport[2], viewport[3], GL_GREEN, GL_UNSIGNED_BYTE, hologram);


    return (unsigned char *) hologram; //GLubyte should be the same as unsigned char, i.e. unsigned byte.

}

void GPGPU::getHologramAsU8(unsigned char * hologram, int width, int height){
	GLint viewport[4];			//place to put the viewport
	glGetIntegerv(GL_VIEWPORT,viewport);
	unsigned long numPixels = viewport[2]*viewport[3];

	if(width != viewport[2] || height != viewport[3]) return;

	//RAM it up...
	//hologram = (GLubyte*)malloc(numPixels);//we assume 1 byte per pixel... in this version, ram is allocated externally.
	if(hologram == NULL) return;

	//set up transfer settings
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);

	//make sure there are no floaters (force a render of the hologram)
	glFlush();
    
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, viewport[2], viewport[3], GL_GREEN, GL_UNSIGNED_BYTE, hologram);


    //return (unsigned char *) hologram; //GLubyte should be the same as unsigned char, i.e. unsigned byte.

}

void GPGPU::dumptofile(){

	////////////////////////////////////////////////////////////////////
	// Capture the current viewport and save it as a targa file.
	// Be sure and call SwapBuffers for double buffered contexts or
	// glFinish for single buffered contexts before calling this function.
	// Returns 0 if an error occurs, or 1 on success.
    FILE *pFile;                // File pointer
    unsigned long lImageSize;   // Size in bytes of image
    GLubyte	*pBits = NULL;      // Pointer to bits
    GLint iViewport[4];         // Viewport in pixels
    GLenum lastBuffer;          // Storage for the current read buffer setting
    
	// Get the viewport dimensions
	glGetIntegerv(GL_VIEWPORT, iViewport);
	
    // How big is the image going to be (targas are tightly packed)
	lImageSize = iViewport[2] * 1 * iViewport[3];	
	
    // Allocate block. If this doesn't work, go home
    pBits = (GLubyte *)malloc(lImageSize);
    if(pBits == NULL)
        return;
	
    // Read bits from color buffer
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);

	glFlush();
    
    // Get the current read buffer setting and save it. Switch to
    // the front buffer and do the read operation. Finally, restore
    // the read buffer state
    glGetIntegerv(GL_READ_BUFFER, (GLint *)&lastBuffer);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, iViewport[2], iViewport[3], GL_GREEN, GL_UNSIGNED_BYTE, pBits);
    glReadBuffer(lastBuffer);
    
    // Attempt to open the file
	if(fopen_s(&pFile,"test.txt","w+") !=0){
        free(pBits);    // Free buffer and return error
        return;
		}
	
    // Write the header
    //fwrite(&tgaHeader, sizeof(TGAHEADER), 1, pFile);
    
    // Write the image data
    //fwrite(pBits, sizeof(GLubyte), lImageSize, pFile);
	for(int i=0; i<iViewport[3]; i++){
		for(int j=0; j<iViewport[2]; j++){
			fprintf(pFile,"%03d ",pBits[i*iViewport[2]+j]);
		}
		fprintf(pFile,"\n");
	}
	

    // Free temporary buffer and close the file
    free(pBits);    
    fclose(pFile);
    
    // Success!
    return;

}