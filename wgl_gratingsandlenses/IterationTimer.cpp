#include "IterationTimer.h"


#ifndef WIN32
int sprintf_s(){} //this needs to be fixed.
#endif

IterationTimer::IterationTimer(UDPServer *feedback)
{
	udpserver=feedback;
	_smoothedFrameTime=-1.0;
	_smoothedTimeToSwap=-1.0;
	_smoothedSwapTime=0.0; //this shouldn't be negative, might cause problems.
	bufferSwapStart=-1;
}

void IterationTimer::startBufferSwap(){
	 lastBufferSwapStart=bufferSwapStart; 
	 bufferSwapStart=clock(); 

	 if(lastBufferSwapStart>=0){ //these only start to work after the first couple of iterations.
		 //update the smoothed frame timer and time_to_swap.
		 if(_smoothedFrameTime==-1.0 || _smoothedTimeToSwap==-1.0){
			 _smoothedFrameTime=((float) (bufferSwapStart - lastBufferSwapStart))/CLOCKS_PER_SEC;
			 _smoothedTimeToSwap=((float) (bufferSwapStart - iterationStart))/CLOCKS_PER_SEC;
		 }else{
			 _smoothedFrameTime=0.95f*_smoothedFrameTime+0.05f*((float) (bufferSwapStart - lastBufferSwapStart))/CLOCKS_PER_SEC;
			 _smoothedTimeToSwap=0.95f*_smoothedTimeToSwap+0.05f*((float) (bufferSwapStart - iterationStart))/CLOCKS_PER_SEC;
		 }
	 }
}

void IterationTimer::stopBufferSwap(){ 
	 bufferSwapStop=clock(); 
	 //update the smoothed frame timer and time_to_swap.
	 _lastSwapTime=((float)(bufferSwapStop - bufferSwapStart))/CLOCKS_PER_SEC;
	 if(_smoothedSwapTime<0.0f){
		 _smoothedSwapTime=_lastSwapTime;
	 }else{
		 _smoothedSwapTime=0.95f*_smoothedSwapTime+0.05f*_lastSwapTime;
	 }
}

void IterationTimer::networkReply(){
	int pos=0;
	pos += sprintf_s(buffer + pos, TIMER_OUTPUT_BUFFER_LENGTH-pos-1, "Iteration Length: %f seconds\n", ((float) (iterationStop-iterationStart))/CLOCKS_PER_SEC);
	pos += sprintf_s(buffer + pos, TIMER_OUTPUT_BUFFER_LENGTH-pos-1, "Network Part: %f to %f seconds\n", ((float) (networkStart-iterationStart))/CLOCKS_PER_SEC, ((float) (networkStop-iterationStart))/CLOCKS_PER_SEC);
	pos += sprintf_s(buffer + pos, TIMER_OUTPUT_BUFFER_LENGTH-pos-1, "Render Part: %f to %f seconds\n", ((float) (renderStart-iterationStart))/CLOCKS_PER_SEC, ((float) (renderStop-iterationStart))/CLOCKS_PER_SEC);
	pos += sprintf_s(buffer + pos, TIMER_OUTPUT_BUFFER_LENGTH-pos-1, "Buffer Swap Part: %f to %f seconds\n", ((float) (bufferSwapStart-iterationStart))/CLOCKS_PER_SEC, ((float) (bufferSwapStop-iterationStart))/CLOCKS_PER_SEC);
	pos += sprintf_s(buffer + pos, TIMER_OUTPUT_BUFFER_LENGTH-pos-1, "Frame Rate: %f fps\n", smoothedFrameRate());
	pos += sprintf_s(buffer + pos, TIMER_OUTPUT_BUFFER_LENGTH-pos-1, "Time to Swap: %f seconds\n", _smoothedTimeToSwap);
	pos += sprintf_s(buffer + pos, TIMER_OUTPUT_BUFFER_LENGTH-pos-1, "Swap Time: %f seconds\n", _smoothedSwapTime);
	pos += sprintf_s(buffer + pos, TIMER_OUTPUT_BUFFER_LENGTH-pos-1, "Timer Resolution: %f seconds\n", 1.0f/((float) CLOCKS_PER_SEC));
	buffer[pos] = '\0'; //terminated.

	udpserver->reply(buffer,pos+1);
}


IterationTimer::~IterationTimer(void)
{
}
