#include "BoulderSLM.h"
#include <stdio.h>
#include <direct.h>
#include <string.h>
#include <fstream>

BoulderSLM::BoulderSLM(void)
{
	char buffer[_MAX_PATH];
	char buffer2[_MAX_PATH];
	bool Init = true;
	bool TestEnable = false;
	bool bRAMWriteEnable = false;
	theBoard = new CPCIeBoard(&buffer[0],&buffer2[0], Init,
TestEnable, bRAMWriteEnable);

	theBoard->SetTrueFrames(3);
	//read in and load the LUT
	_getcwd(buffer, _MAX_PATH);
	strcat_s((char *)buffer,_MAX_PATH , "\\slm8777.LUT");
	CString LUTPath = (CString) buffer;
	unsigned char* LUT = new unsigned char[256];
	readLUTFile(LUT, LUTPath);
	theBoard->WriteLUT(LUT);
}

BoulderSLM::~BoulderSLM(void)
{
	theBoard->SetPower(0);
	delete theBoard;
}

void BoulderSLM::loadHologram(unsigned char *hologram)
{
	theBoard->WriteFrameBuffer(hologram);
}

bool BoulderSLM::readLUTFile(unsigned char* LUT, CString LUTFileName)
{
	int i, seqnum, scanCount, tmpLUT;
	char inbuf[_MAX_PATH];
	bool errorFlag = false;
/*
	std::ifstream LUTFile(LUTFileName);
	if (!LUTFile.is_open())
	{	
		AfxMessageBox((LPCTSTR) "LUT File is currently open - could not open file",0,0);
		return false;
	}

	//read in each line of the file
	i = 0;
	while (LUTFile.getline(inbuf, _MAX_PATH, '\n'))
	{
		// parse out the Image Entries
		scanCount=sscanf(inbuf, "%d %d", &seqnum, &tmpLUT);

		//if scanCount is 0, then no fields were assigned. Thisis an invalid row
		//Or if scanCount does not equal 15, then there were not15 items in that
		//row. This would indicate an invalid row. In either case we "continue" to break
		//out of the while loop
		if ((scanCount != 2)||(seqnum != i)||(tmpLUT < 0) ||(tmpLUT > 255))
		{
			errorFlag = true;		
			continue;
		}

		LUT[seqnum] = (unsigned char) tmpLUT;
		i++;
	}
	LUTFile.close();
*/
//	if (errorFlag == true)                    
//	{
		for (i=0; i<256; i++)
			LUT[i]=i;
		return false;
//	}

	return TRUE;
} 
