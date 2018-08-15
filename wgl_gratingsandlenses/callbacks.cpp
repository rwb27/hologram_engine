// this file contains the glue code common to windows and os x.  We'll have a separate 
// "main.cpp" file for each platform, but hopefully this can stay unchanged.

#include <string>
#include <sstream>
#include <ios>
using std::string;
using std::istringstream;
using std::ios;
#include "skipline.cpp"

#ifdef WIN32
#include "multimon.h"
#endif

// openGL
#include <gl/gl.h>
#include <gl/glu.h>

#ifndef WIN32
#include <GLUT/glut.h>
#endif

#ifdef WIN32
#define P_SSCANF sscanf_s
#else
#define P_SSCANF sscanf
#endif

#include "GPGPU.h"
#include "UDPServer.h"
#include "IterationTimer.h"
#include "BoulderSLM.h"

// constants
#define PORT "61557"
#define MAX_UDP_BUFFER_LENGTH 32768
#define DEFAULTS_FILENAME "hologram_server_settings.rc"

//gloabls
unsigned char * hologramBuffer;
GPGPU  *gpgpu;
UDPServer *udpServer;
IterationTimer *iterationTimer;
BoulderSLM *boulderSLM;
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

int updateSpotsFromBuffer(char* buffer, int messagelength){
	//our buffer contains a message, which starts with <data> and ends with </data>.  Inside are a number of elements
	//which look like <thing> stuff </thing>, so we search for each in turn and parse them.  NB this is NOT and XML parser
	//we rely on a lot of newlines.
	//also, most of the time we'll only be receiving the <spots> </spots> part, so we short-circuit the other string searches in
	//this case.
	int n=0, ret=0;
	bool only_spots=false; //if the message only contains spots, this flags it up so we can skip other stuff.
	char* pos=strstr(buffer,"<spots>\n"); //find the start of the spots
	char* endpos=strstr(buffer,"</spots>"); //find the end of the spots
	if(pos!=NULL && endpos!=NULL){
		only_spots = (pos-buffer)<15 && (buffer+messagelength-endpos)<20; //does the message only contains spots?
		pos=skipLineBreak(pos); //skip to the next line
		string spotstring = string(pos,(size_t) (endpos-pos));
		istringstream spotstream = istringstream(spotstring);
	
		float x=0,y=0,z=0,I=1,phi=0,nax=0,nay=0,nar=0,lx=0,ly=0,lz=0,lpg=0;
		int l=0;
		float* spots=gpgpu->getSpots(); //access the spots array directly
		n=0;
		while(n<MAX_N_SPOTS){ //for elegance, spotstream should go in here.
			float* spot=&spots[n*SPOT_ELEMENTS];
			spotstream >> x >> y >> z >> I >> l >> phi;
#ifdef NA_MANIPULATION
			spotstream >> nax >> nay >> nar;
#endif
#ifdef LINETRAP3D
			spotstream >> lx >> ly >> lz >> lpg;
#endif
			if(spotstream){
				spot[0]=x;
				spot[1]=y;
				spot[2]=z;
				spot[3]=(float)l;
				spot[4]=I;
				spot[5]=phi; //for the moment, this MUST be there
#ifdef NA_MANIPULATION
				spot[8]=nax;
				spot[9]=nay;
				spot[10]=nar;
#endif
#ifdef LINETRAP3D
				spot[12]=lx;
				spot[13]=ly;
				spot[14]=lz;
				spot[15]=lpg;
#endif
				n++;	//if we got a spot, increment n
			}else{
				break; //stop when we run out of input
			}
#ifdef ZERNIKE_PERSPOT
			if(spotstream){
				int i=12;
				while(spotstream && i<SPOT_ELEMENTS){
					spotstream >> z;
					spot[i]=z;
					i++;
				}
				//!TODO! this should fail gracefully if not enough Zernike modes are specified, rather than causing the last spot to disappear...
				//must find a way of skipping whitespace but NOT newlines...
			}
#endif			
			spotstream >> skipline; //then go to the next line
		}
		
		if(n>0){
			gpgpu->setNSpots(n);
		}
	}
	
	if(!only_spots){
		pos=strstr(buffer,"<blazing>\n");
		if(pos!=NULL){//if we've got an updated blazing function, pass it on
		pos=skipLineBreak(pos); //skip to the next line
			endpos=strstr(buffer,"</blazing>");
			float b;
			int i=0;
			float* blazingTable = gpgpu->getBlazing(); //get the blazing table array
			while(pos != NULL && pos<endpos && P_SSCANF(pos,"%f",&b)==1 && i<BLAZING_TABLE_LENGTH){ //iterate through and set blazing
				blazingTable[i]=b;
				i++;
				pos=skipLineBreak(pos); //skip to the next line
			}
		}

		pos=strstr(buffer,"<zernike>\n");
		if(pos!=NULL){//if we've got an updated blazing function, pass it on
		pos=skipLineBreak(pos); //skip to the next line
			endpos=strstr(buffer,"</zernike>");
			float z;
			int i=0;
			float* zernikeCoefficients = gpgpu->getZernike(); //get the array
			while(pos != NULL && pos<endpos && P_SSCANF(pos,"%f",&z)==1 && i<ZERNIKE_COEFFICIENTS_LENGTH){ //iterate through and set blazing
				zernikeCoefficients[i]=z;
				i++;
				pos=skipLineBreak(pos); //skip to the next line
			}
		}

		pos=strstr(buffer,"<geometry>\n");
		if(pos!=NULL){//this bit updates the geometry parameters (size of the hologram and focal length
			pos=skipLineBreak(pos); //skip to the next line
			float hh,hw,f,k; //we want three floats; hologram width, hologram height, focal length
			if(P_SSCANF(pos,"%f, %f, %f, %f",&hw,&hh,&f,&k)==4){ //if we've got 4 numbers
				if(k>0.0) gpgpu->setGeometry(hw,hh,f,k);
			}
		}

		pos=strstr(buffer,"<aspect>\n");
		if(pos!=NULL){//if we've been asked to move, do so
			pos=skipLineBreak(pos); //skip to the next line
			float physical,screen; //we want two floats; the physical aspect of the hologram, and the pixel aspect
			if(P_SSCANF(pos,"%f, %f",&physical,&screen)==2){ //if we've got 2 numbers
				if(physical>0.0) gpgpu->setPhysicalAspect(physical);
				if(screen>0.0) gpgpu->setScreenAspect(screen);
			}
		}

		pos=strstr(buffer,"<totalA>\n");
		if(pos!=NULL){//if it's there
			pos=skipLineBreak(pos); //skip to the next line
			float totalA; //we want one float; the total intensity
			if(P_SSCANF(pos,"%f",&totalA)==1){ //if we've got 1 numbers
				if(totalA>=0.0) gpgpu->setTotalA(totalA);
			}
		}

		pos=strstr(buffer,"<na_smoothness>\n");
		if(pos!=NULL){//if it's there
			pos=skipLineBreak(pos); //skip to the next line
			float nasm; //we want one float; the total intensity
			if(P_SSCANF(pos,"%f",&nasm)==1){ //if we've got 1 numbers
				gpgpu->setNASmoothness(nasm);
			}
		}

		pos=strstr(buffer,"<window_rect>\n");
		if(pos!=NULL){//if we've been asked to move, do so
			pos=skipLineBreak(pos); //skip to the next line
			int x,y,w,h,m; //get the blazing table array
			if(P_SSCANF(pos,"%d, %d, %d, %d",&x,&y,&w,&h)==4){ //if we've got four numbers
				moveAndSizeWindow(x,y,w,h);
			}else if(P_SSCANF(pos,"all monitor %d",&m)==1){ //if we've got a request for fullscreen
				fullScreen(m);
			}
		}

		pos=strstr(buffer,"<centre>\n");
		if(pos!=NULL){//if we've been asked to move, do so
			pos=skipLineBreak(pos); //skip to the next line
			float x,y; //get the x and y coords of the centre
			if(P_SSCANF(pos,"%f, %f",&x,&y)==2){ //if we've got 2 numbers
				gpgpu->setCentre(x,y);
			}
		}

		pos=strstr(buffer,"<network_reply>\n");
		if(pos!=NULL){//set up responding to packets
			pos=skipLineBreak(pos); //skip to the next line
			int y; //whether or not we reply
			if(P_SSCANF(pos,"%d",&y)==1){ //if we've got a number
				networkReply = y==1;
			}
		}

	}
	return n;
}


// this writes a packet to a file, which will be read on startup.  This can set the blazing function and other options.
void saveBufferToDefaultsFile(char* buffer, int messagelength){
	FILE* f=NULL;
#ifdef WIN32
	if(fopen_s(&f,DEFAULTS_FILENAME,"w+") ==0){
#else
	if((f=fopen(DEFAULTS_FILENAME,"w"))!=NULL){
#endif
		if(f==NULL) return;
		fwrite(buffer, sizeof(char),messagelength, f);
		fclose(f);
	}
}

// this function updates the spots from the network.
int updateSpotsFromNetwork(int timeout){
	int n=0, ret=0, messagelength;
	messagelength=udpServer->receive(buffer, MAX_UDP_BUFFER_LENGTH, timeout, "</data>"); //try to recieve a packet

	if(messagelength>0){ //if we've got something (which ends with </data>),
		if(strstr(buffer, "<save")!=NULL){
			saveBufferToDefaultsFile(buffer,messagelength);
			gpgpu->dumptofile();
		}
		return updateSpotsFromBuffer(buffer, messagelength);
	}
	else return 0;
}

int updateSpotsFromDefaultsFile(){
	FILE* f;
	int messagelength=0;
#ifdef WIN32
	if(fopen_s(&f,DEFAULTS_FILENAME,"r+") ==0){
#else
	if((f=fopen(DEFAULTS_FILENAME,"r"))!=NULL){
#endif
		messagelength=fread(buffer, sizeof(char), MAX_UDP_BUFFER_LENGTH, f);
		fclose(f);
	}
	if(messagelength>0){ //if we've got something,
		return updateSpotsFromBuffer(buffer, messagelength);
	}
	else return 0;
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
		if(n>0) somebodylovesyou=true; //flag the fact that we've recieved something at least once (because n=0 when we
						 //leave the loop)
		firstTime=false; //subsequent iterations shouldn't wait as long.
	}while(n>0);
	if(somebodylovesyou){ //postRedisplay(); //update the screen if we've recieved spot coordinates
		iterationTimer->startRender();
		gpgpu->update();         //redraw the window using calls to OpenGL
		iterationTimer->stopRender();
		iterationTimer->startBufferSwap();
		gpgpu->getHologramAsU8(hologramBuffer,512,512); //render and save
		boulderSLM->loadHologram(hologramBuffer);
		iterationTimer->stopBufferSwap();
	}
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

void deleteGlobalObjects(){
	delete boulderSLM;
	delete udpServer;
	delete iterationTimer;
	delete gpgpu;
	free(hologramBuffer);
}

void createGlobalObjects(){
	//create the objects for GPGPU stuff and network comms
	gpgpu = new GPGPU(false);
	udpServer = new UDPServer(PORT);
	iterationTimer = new IterationTimer(udpServer);
	boulderSLM = new BoulderSLM();
	hologramBuffer = (unsigned char *) malloc(512*512);
}