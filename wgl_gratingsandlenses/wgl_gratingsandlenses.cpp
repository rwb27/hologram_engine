#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <AtlBase.h>
// C RunTime Header Files
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>


// windows-only globals
HINSTANCE hInstance;							// current instance
LPCTSTR szTitle = _T("Hologram Engine");
HWND windowHandle;		//this gives us a handle on all things graphical

#define GLEW_STATIC 1
#include <GL/glew.h>
#include <gl/gl.h>
#include "wglext.h"

#include "callbacks.cpp"



// Forward declarations of functions included in this code module:
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// allow changing port from command line

	char allCmdLine[256];
	wcstombs(allCmdLine, lpCmdLine, 256);
	int portnumber=0;
	 if(strstr(allCmdLine,"port")){              //change the port we listen on - useful for multiple SLMs
         sscanf(strstr(allCmdLine,"port"),"port%d",&portnumber);
         printf("listening on port %d",portnumber);
	}

	//now we need to convert this to a string
	char * port = (char *)malloc(8);
    memcpy(port, PORT, 6); //default port
    if(portnumber<10000000 && portnumber >0) sprintf(port,"%05d",portnumber);

 	// TODO: Place code here.
	MSG msg;				//we use this for handling and dispatching messages
	WNDCLASS windowClass;	//we use this to describe the window we want

	//set up the OpenGL window
	//first, make the settings in the window class
	windowClass.style			= CS_HREDRAW | CS_VREDRAW | CS_OWNDC;	//look at chapter 19 of the SuperBible
	windowClass.lpfnWndProc		= (WNDPROC) WndProc;					//this sets the callback function
	windowClass.cbClsExtra		= 0;
	windowClass.cbWndExtra		= 0;
	windowClass.hInstance		= hInstance;
	windowClass.hIcon			= NULL;
	windowClass.hCursor			= LoadCursor(NULL, IDC_ARROW);
	windowClass.hbrBackground	= NULL;									//OpenGL needs no background
	windowClass.lpszMenuName	= NULL;
	windowClass.lpszClassName	= szTitle;

	if(RegisterClass(&windowClass) == 0) return FALSE; //register our window class

	//create the window
	windowHandle = CreateWindow(
		szTitle,
		szTitle,
		WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		100,100, //position
		512,512, //size
		NULL,
		NULL,
		hInstance,
		NULL);
	if(windowHandle==NULL) return FALSE;

	//using GLEW, make sure the relevant extensions exist
	graphics_initialize();

	//create the objects for GPGPU stuff and network comms
	gpgpu = new GPGPU(false);
	udpServer = new UDPServer(port);
#ifdef BNS_PCIE
	setupBNS();
#endif

	//display our window
	ShowWindow(windowHandle,SW_SHOW);
	UpdateWindow(windowHandle);

/*	PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB;
	wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC) wglGetProcAddress("wglGetExtensionsStringARB");

	//output extensions
	LPCSTR wglExtensionsString = wglGetExtensionsStringARB(GetDC(windowHandle)); //the extensions as a const char*
	CA2W WwglExtensionsString(wglExtensionsString);
	MessageBox(windowHandle,WwglExtensionsString,_T("WGL Extensions"),MB_OK);//*/

	// Main message loop: we alternate listening for network packets with checking the Windows
	//message queue.  This should ensure minimal latency in recieving the packets, without being
	//too unresponsive to Windows- we wouldn't want it to get upset now...
	BOOL bRet = 1;
	while (bRet != 0)
	{	//fast hologram generation waits for no man!
		clearNetworkBacklogAndUpdate(); //this listens for incoming packets 
		//(and makes sure we've got no queued packets left) then updates the parameters and refreshes the display.
		while(PeekMessage(&msg,NULL,0,0,PM_NOREMOVE)>0){ //if there's a message in the queue
			bRet=GetMessage(&msg,NULL,0,0); //take the message out the queue
			TranslateMessage(&msg);	//pass the buck
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}





// This callback function actually "does" all the stuff in Windows.
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static HGLRC hRC = NULL;	//OpenGL rendering context
	static HDC hDC = NULL;		//GDI Device Context
	static PIXELFORMATDESCRIPTOR pfd = { //we use this in setting up the window.
			sizeof(PIXELFORMATDESCRIPTOR), 1, //size and version
			PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_TYPE_RGBA, //doublebuffered OpenGL to screen
			32, //colour depth
			0,0,0,0,0,0,0,0,0,0,0,0,0, //boring
			16, //depth buffer size (no, I don't understand either)
			0,0,0,0,0,0,0}; //also boring

	switch (message)
	{
	case WM_CREATE:
		hDC=GetDC(hWnd);	//get the Windows device context;
		
		//Set the pixel format
		SetPixelFormat(hDC, ChoosePixelFormat(hDC, &pfd), &pfd);

		//Create and activate the rendering context
		hRC = wglCreateContext(hDC);
		wglMakeCurrent(hDC, hRC);
		break;
	case WM_COMMAND: //quietly do nothing- we don't have a menu to process commands for anyway.
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	case WM_SIZE: //the window has been resized
		reshape(LOWORD(lParam),HIWORD(lParam)); //adjust the OpenGL settings to keep aspect ratios correct
		InvalidateRect(hWnd,NULL,FALSE);        //remind Windows to ask us to redraw the window
		break;
	case WM_PAINT: //we've been asked to redraw the window
		gpgpu->update();         //redraw the window using calls to OpenGL
		swapBuffersAndWait(hDC); //swap the buffers (NB this function will return after the next screen refresh 
								 //(rate is usually about 60Hz, so this function can only be executed that often, 
								 //which is a good thing- but see clearNetworkBacklogAndUpdate().
		ValidateRect(hWnd,NULL); //stop the panic
		break;
	case WM_DESTROY:
		KillTimer(hWnd,101); //stop animating

		//Stop OpenGL
		wglMakeCurrent(hDC,NULL);
		wglDeleteContext(hRC);

		PostQuitMessage(0); //quit the application
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

