#ifndef __ITERATION_TIMER
#define __ITERATION_TIMER 1

#include "UDPServer.h"
#include <time.h>
#define TIMER_OUTPUT_BUFFER_LENGTH 512

class IterationTimer
{
public:
	IterationTimer(UDPServer * udpserver);
	~IterationTimer(void);

	void startIteration() { iterationStart=clock(); }
	void startNetwork() { networkStart=clock(); }
	void stopNetwork() { networkStop=clock(); }
	void startRender() { renderStart=clock(); }
	void stopRender() { renderStop=clock(); }
	void startBufferSwap();
	void stopBufferSwap();
	void stopIteration() { iterationStop=clock(); }
	float smoothedFrameRate() {return 1.0f/_smoothedFrameTime;}
	float smoothedTimeToSwap() {return _smoothedTimeToSwap; }
	float smoothedSwapTime() {return _smoothedSwapTime; }
	float lastSwapTime() {return _lastSwapTime; }
	void networkReply();

protected:
	UDPServer* udpserver;
	char buffer[TIMER_OUTPUT_BUFFER_LENGTH];
	clock_t iterationStart;
	clock_t networkStart;
	clock_t renderStart;
	clock_t bufferSwapStart;
	clock_t iterationStop;
	clock_t networkStop;
	clock_t renderStop;
	clock_t bufferSwapStop;
	clock_t lastBufferSwapStart;
	float _smoothedFrameTime;
	float _smoothedTimeToSwap;
	float _smoothedSwapTime;
	float _lastSwapTime;
};

#endif