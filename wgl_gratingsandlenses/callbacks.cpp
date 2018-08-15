// this file contains the glue code common to windows and os x.  We'll have a separate 
// "main.cpp" file for each platform, but hopefully this can stay unchanged.

#include <string>
#include <sstream>
#include <ios>
#include <iostream>
#include <iomanip>
//using std::string;
//using std::istringstream;
//using std::ios;
using namespace std;
//#include "skipline.cpp"

// openGL
#define GLEW_STATIC 1
#ifdef WIN32
#include "glew.h"
#include <gl/gl.h>
#include <gl/glu.h>
#else
#include "/opt/local/include/GL/glew.h"
#include <GLUT/glut.h>
#include <OpenGL/OpenGL.h>
#endif

#ifdef WIN32
#define P_SSCANF sscanf_s
#else
#define P_SSCANF sscanf
#endif

#include "GPGPU.h"
#include "UDPServer.h"
//#include "IterationTimer.h"

// constants
#define PORT "61557"
#define MAX_UDP_BUFFER_LENGTH 655360
#define DEFAULTS_FILENAME "hologram_server_settings.rc"

//gloabls
GPGPU  *gpgpu;
UDPServer *udpServer;
//IterationTimer *iterationTimer;
char buffer[MAX_UDP_BUFFER_LENGTH];
bool networkReply=false;
float initialWaitTime=0.0f; //we wait for a certain amount of time at the start of each iteration to reduce latency
//(the buffer swap occurs at a certain time, so by leaving things until the last millisecond we can avoid lag)


#ifndef WIN32
void timer_function(int v); //forward declaration so we can call-back this function from idle()
#endif

void postRedisplay(){	
#ifdef WIN32
		InvalidateRect(windowHandle,NULL,FALSE); //windowHandle is defined in the windows main cpp file, honest...
#else
		glutPostRedisplay();
		glutTimerFunc(2,timer_function,42);
#endif
}

void moveAndSizeWindow(int x, int y, int w, int h){
#ifdef WIN32
	MoveWindow(windowHandle, x,y,w,h,true);
	gpgpu->reshape(w, h);
#else
    glutReshapeWindow(w, h);
    glutPositionWindow(x, y);
#endif
}


#ifdef WIN32
BOOL CALLBACK goFullScreenOnGivenMonitor(
  HMONITOR hMonitor,  // handle to display monitor
  HDC hdcMonitor,     // handle to monitor DC
  LPRECT rectangle,   // monitor intersection rectangle
  LPARAM dwData       // data
  ){
	  static int currentMonitor=1;
	  if(dwData==999){ //we use this to reset our little countery thing :)
		  currentMonitor=1;
		  return false;
	  }else{
		if(currentMonitor==dwData){
			  moveAndSizeWindow(rectangle->left,rectangle->top,rectangle->right-rectangle->left,rectangle->bottom-rectangle->top);
			  currentMonitor=0;
			  return false;
		  }else{
			  currentMonitor++;
			  return true;
		  }
	  }
}
#endif

void fullScreen(int monitor){
#ifdef WIN32
	//go fullscreen on the given monitor
	goFullScreenOnGivenMonitor(NULL,NULL,NULL,999);
	EnumDisplayMonitors(NULL,NULL,goFullScreenOnGivenMonitor,monitor);
#else
    glutFullScreen();
#endif
}

void setSwapBuffersAtRefreshRate(int lock){
	if(lock!=0) lock=1;
#ifdef WIN32
	// Get swap interval function pointer if it exists
	PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	if(wglSwapIntervalEXT != NULL){ 
		wglSwapIntervalEXT(lock);
	//if(lock==1)	MessageBox(windowHandle,_T("Successfully asked Windows to sync buffer swaps from vertical retrace"),_T("info"),MB_OK);
	//else 		MessageBox(windowHandle,_T("Successfully asked Windows to unsync buffer swaps from vertical retrace"),_T("info"),MB_OK);
	}
#else
    CGLContextObj cgl_context = CGLGetCurrentContext();
    CGLSetParameter(cgl_context, kCGLCPSwapInterval, &lock);
#endif
}

#ifdef WIN32
void swapBuffersAndWait(HDC hDC){
	
	static PFNWGLGETSYNCVALUESOMLPROC wglGetSyncValuesOML;
	static PFNWGLWAITFORSBCOMLPROC wglWaitForSbcOML;

	if(wglGetSyncValuesOML == NULL || wglWaitForSbcOML == NULL){
		//get the functions
		wglGetSyncValuesOML = (PFNWGLGETSYNCVALUESOMLPROC) wglGetProcAddress("wglGetSyncValuesOML");
		wglWaitForSbcOML = (PFNWGLWAITFORSBCOMLPROC) wglGetProcAddress("wglWaitForSbcOML");
	}

//	INT64 ust, msc, sbc;
	SwapBuffers(hDC);

//	wglGetSyncValuesOML(hDC, &ust, &msc, &sbc);
//	wglWaitForSbcOML(hDC, sbc+1, &ust, &msc, &sbc);
}
#endif

char * skipLineBreak(char* pos){
	pos=strpbrk(pos,"\n\r\f\t");
	if(pos!=NULL)return pos+strspn(pos,"\n\r\f\t");
	else return NULL;
}
char * skipSpace(char* pos){
	pos=strpbrk(pos," ,");
	if(pos!=NULL)return pos+strspn(pos," ,");
	else return NULL;
}

bool find_tag(char * str, const char * name, char ** tag, char ** tagend, char ** closingtag){
	//this function is as close to a proper XML parser as I can be bothered writing.
	//I know there's libraries for this, but I care mostly about speed here...
	char * start = (char *) malloc(strlen(name)+2);
	start[0] = '<';
	memcpy(start+1, name, strlen(name));
	start[strlen(name)+1] = 0;

	char * tagi = strstr(str, start); //the opening tag
	free(start);
	if(tagi == NULL) return false;
	if(tag != NULL) *tag = tagi;

	char * tagendi = strstr(tagi, ">"); //the end of the opening tag
	if(tagendi == NULL) return false;
	if(tagend != NULL) *tagend = tagendi;
	
	char * end = (char *) malloc(strlen(name)+4);
	end[0] = '<';
	end[1] = '/';
	memcpy(end+2, name, strlen(name));
	end[strlen(name)+2] = '>';
	end[strlen(name)+3] = 0;

	char * closingtagi = strstr(tagendi, end); //the closing tag
	free(end);
	if(closingtagi == NULL) return false;
	if(closingtag != NULL) *closingtag = closingtagi;

	return true;
}

char * find_parameter_value(char * tag, const char * name){
	//find the start of the value for a tag parameter (e.g. <tag parameter="value"...>)
	const char * tagend = strstr(tag, ">");
	tag = strstr(tag, name);
	if(tag == NULL || tagend == NULL || tag > tagend) return NULL;
	tag += strlen(name);
	tag += strspn(tag, " =\"'");
	return tag;
}

bool get_tag_parameter_int(char * tag, const char * name, const int * value){
	//scan inside the parameter's value, and extract an integer.  Nearly all my parameters are integers.
		tag = find_parameter_value(tag, name);
		if(tag == NULL) return false;

		return (1 == P_SSCANF(tag,"%d", value)); //We have already skipped over any quotes that may be present
}

void clean_binary_bits_from_buffer(char * dirtybuffer, char * cleanbuffer, int messagelength){
	//the packet can contain binary blobs of data for packed textures (maybe at some point packed uniforms too, though their size makes that less of an issue)
	//we copy out the text (so strstr and the like can be used without worrying about binary junk), but keep pointers to the
	//original binary data.  If what we have is lots of binary data and only a little text around it, this should be efficient.
	char * pos = dirtybuffer;
	char * text_start = pos;
	char * write_pos = cleanbuffer;
	while(pos < dirtybuffer + messagelength && (pos = strstr(pos,"<binary"))!=NULL){  //loop through all the binary tags
		char * tag_start = pos;
		int size = -1;
		if(!get_tag_parameter_int(pos, "size", &size)) continue; //find size or skip

		pos = strstr(pos, ">"); //move pos to the final non-binary character
		if(pos == NULL) continue;
		pos += 1;

		int len = tag_start - text_start;
		memcpy(write_pos, text_start, len); //copy the text up to and including the tag into the clean buffer
		write_pos += len;
		sprintf(write_pos,"<binary offset=\"%08d\" size=\"%08d\" />", pos - dirtybuffer, size); //add the offset and size of binary data in the dirty buffer
		write_pos += 44;

		pos += size + 8;  //skip over the binary blob and closing tag
		pos += strspn(pos, " >");
		text_start = pos;
	}
	if(text_start > dirtybuffer + messagelength - 1) return; //prevent access error if packets are split in binary bits.
	//now we copy whatever [text] remains.
	memcpy(write_pos, text_start, dirtybuffer + messagelength - text_start +1); //copy from the start of the last string section
		//up to the null character at the end of the string (the +1 copies the null).
}

int updateSpotsFromBuffer(char* recvbuffer, int messagelength){
	//our buffer contains a message, which starts with <data> and ends with </data>.  Inside are a number of elements
	//which look like <thing> stuff </thing>, so we search for each in turn and parse them.  NB this is NOT an XML parser
	//we rely on a lot of newlines.

	//to cope with binary bits of data, I'm using a pre-cleaner
	char buffer[MAX_UDP_BUFFER_LENGTH];
	clean_binary_bits_from_buffer(recvbuffer, buffer, messagelength);

	char * pos, *dataStart, * endpos;
	find_tag(buffer, "data", &dataStart, NULL, &endpos); //locate the <data> </data> tags that enclose all of the data

	pos = dataStart;
	float uniformValue[4096];
	char * valEnd, * tagEnd;
	while(find_tag(pos, "uniform", &pos, &tagEnd, &valEnd)){ //the uniform tags are most common- they set the value of a uniform variable
		int uniformID = -1;
		if(!get_tag_parameter_int(pos, "id", &uniformID)) continue; //insert code to look up uniforms by name here...
		pos = tagEnd + 1;                  //skip to the end of the tag
        
        int n=0;
        float val;
        pos += strspn(pos, " \t\n\r<");               //skip any initial whitespace
        while(pos && pos < valEnd && P_SSCANF(pos, " %f", &val) && n<4096){ //check that: 
			                                                                //we successfully skipped over whitespace, 
			                                                                //we have not reached the closing </uniform>, 
			                                                                //we have got another number, 
			                                                                //and we aren't going to exceed the size of the buffer
            uniformValue[n]=val;                      //store it in the array + increment n
            pos = strpbrk(pos, " \t\n\r<")+1;
            n++;
        }
        
		if(n>0) gpgpu->setUniform(uniformID,uniformValue,n);
	}

	pos=dataStart;
	while(find_tag(pos, "texture", &pos, &tagEnd, &valEnd)){ //texture tags are analogous to uniforms, but with texture data
		int uniformID, width, height;
		bool ok = true;
		ok &= get_tag_parameter_int(pos, "id", &uniformID);
		ok &= get_tag_parameter_int(pos,"width",&width);
		ok &= get_tag_parameter_int(pos,"height",&height);
		if(!ok) continue;

		bool packedData = false, integerData = false;
		char * fmt = find_parameter_value(pos, "format");
		if(fmt != NULL && strstr(fmt, "packed") == fmt){				//are we dealing with packed or human-readable data?
			packedData = true;
			fmt += 6;
			if(strstr(fmt, "u8") == fmt) integerData = true;
		}

		int components = 4;                     //we assume that all textures are RGBA for now

		pos = tagEnd + 1;                  //skip to the end of the tag
        
		//now we should be in the data section.

		if(packedData){
			int offset = -1, size = -1;
			P_SSCANF(pos, " <binary offset=\"%08d\" size=\"%08d\" />", &offset, &size); //this is generated in clean_binary_bits_from_buffer, hence no worries re: space/quotes
			if(offset<0 || size<0) continue;

			if(integerData){
				int n = size; //number of bytes in array
				if(n=width*height*components) gpgpu->setUniformTexture(uniformID, (unsigned char *) (recvbuffer + offset), width,height,4); //point to the binary data in place
			}else{
				int n = size/sizeof(0.0f); //number of floats in array
				if(n=width*height*components) gpgpu->setUniformTexture(uniformID, (float *) (recvbuffer + offset), width,height,4); //point to the binary data in place
			}
		}else{
			int n=0;
			float val;
			float * textureData = (float *) malloc(width * height * components * sizeof(float));
			pos += strspn(pos, " \t\n\r<");               //skip any initial whitespace
			while(pos && pos < valEnd && P_SSCANF(pos, " %f", &val) && n<width*height*components){ //check that: 
																				//we successfully skipped over whitespace, 
																				//we have not reached the closing </uniform>, 
																				//we have got another number, 
																				//and we aren't going to exceed the size of the buffer
				textureData[n]=val;                      //store it in the array + increment n
				pos = strpbrk(pos, " \t\n\r<")+1;
				n++;
			}
        
			if(n=width*height*components) gpgpu->setUniformTexture(uniformID,textureData,width,height,4);
			free(textureData);
		}
	}
	if(true){ //config-related stuff in here- we could disable this based on the presence of a <config> section...?
		if(find_tag(dataStart, "aspect", NULL, &tagEnd, NULL)){
			float screen;
			if(P_SSCANF(tagEnd+1, " %f", &screen) == 1) if(screen > 0.0) gpgpu->setScreenAspect(screen);
		}
		if(find_tag(dataStart, "window_rect", NULL, &tagEnd, NULL)){
			int x,y,w,h,m; //get the parameters
			if(P_SSCANF(tagEnd + 1," %d, %d, %d, %d",&x,&y,&w,&h)==4
				|| P_SSCANF(tagEnd + 1," %d %d %d %d",&x,&y,&w,&h)==4){ //if we've got four numbers with spaces
				moveAndSizeWindow(x,y,w,h);
			}else if(P_SSCANF(tagEnd + 1,"all monitor %d",&m)==1){ //if we've got a request for fullscreen
				fullScreen(m);
			}
		}
		if(find_tag(dataStart, "network_reply", NULL, &tagEnd, NULL)){
			int y;
			if(P_SSCANF(tagEnd+1, " %d", &y) == 1) networkReply = y==1; //if we've got a number, set whether or not we reply to packets
		}
		if(find_tag(dataStart, "swap_buffers_at_refresh_rate", NULL, &tagEnd, NULL)){
			int y;
			if(P_SSCANF(tagEnd+1, " %d", &y) == 1) setSwapBuffersAtRefreshRate(y); //whether or not updates are locked to screen refreshes
		}
		
		if(find_tag(dataStart, "shader_source", &pos, &tagEnd, &valEnd)){
			//the <shader_source> tag causes a recompilation of the fragment shader, using the contents of the tag as the new shader program.
			char * shaderSourceStart = skipLineBreak(tagEnd);
			char * shaderSourceEnd = valEnd;

			size_t shaderLength = shaderSourceEnd - shaderSourceStart;
			char * shaderSource = (char *) malloc(shaderLength+1); //yes, I could probably just whack a null character into the buffer, but this is nicer...
			memcpy(shaderSource, shaderSourceStart, shaderLength);
			shaderSource[shaderLength]='\0';
			gpgpu->recompileShader((const char **) &shaderSource);
			free(shaderSource);
		}
	}
    gpgpu->update();
	return 0;
}

// this function updates the spots from the network.
int updateSpotsFromNetwork(int timeout){
	int messagelength=udpServer->receive(buffer, MAX_UDP_BUFFER_LENGTH, timeout, "<data", "</data>"); //try to recieve a packet

	if(messagelength>0){ //if we've got something (which starts with <data and ends with </data>),
		return updateSpotsFromBuffer(buffer, messagelength);
	}


	else return -1;
}

void clearNetworkBacklogAndUpdate(){
	int n;
	bool firstTime=true, somebodylovesyou=false;
	do{
		n=updateSpotsFromNetwork(firstTime?100000:100); 
						 //the reason we loop until it times out is that we want to clear any backlog of packets that
						 //might have accumulated since last time we listened.  For this reason, we need to use a 
						 //short timeout in the network reciever, because we're guaranteed to time out at least once.
						 //the trick is to use a long(ish) timeout the first time, when there may not be any queued packets,
						 //and then a really short (<1ms) timeout thereafter, as we're only interested in queued packets, 
						 //not new ones.  This should keep latency to a minimum.
		if(n>=0) somebodylovesyou=true; //flag the fact that we've recieved something at least once (because n=0 when we
						 //leave the loop)
		firstTime=false; //subsequent iterations shouldn't wait as long.
	}while(n>=0);
	if(somebodylovesyou) postRedisplay(); //update the screen if we've recieved spot coordinates
						 //using wGL (windows native openGL), buffer swaps are synced with screen refresh, a Good Thing.
						 //However, it means this function cannot execute more often than the screen refreshes- 
						 //usually circa 60Hz.
}

// make sure the area we draw in (+-1 in X and Y) always has the aspect ratio defined in the GPGPU object.
void reshape(int w, int h)
{
	if(gpgpu==NULL) return;
	else gpgpu->reshape(w,h);    
}

// Called at startup
void graphics_initialize(){
    // Initialize GLEW to make sure the relevant extensions are available (important under windows)
	glewInit();
	
    // Ensure we have the necessary OpenGL Shading Language extensions.
    if (glewGetExtension("GL_ARB_fragment_shader")      != GL_TRUE ||
        glewGetExtension("GL_ARB_vertex_shader")        != GL_TRUE ||
        glewGetExtension("GL_ARB_shader_objects")       != GL_TRUE ||
        glewGetExtension("GL_ARB_shading_language_100") != GL_TRUE)
    {
        fprintf(stderr, "Driver does not support OpenGL Shading Language\n");
        exit(-42);
    }
	/*if (glewGetExtension("OML_sync_control") !=GL_TRUE){
		fprintf(stderr, "Driver does not support frame sync\n");
		exit(-13);
	}*/ //need to check wgl extensions string instead
}

/* useful for pop-up debugging...

				
#if defined(DEBUG) & defined(WIN32)
				LPCSTR shaderString = gpgpu->getUniforms(); //the extensions as a const char*
				CA2W WshaderString(shaderString);
				MessageBox(windowHandle,WshaderString,_T("Shader Source"),MB_OK);
#endif
				*/