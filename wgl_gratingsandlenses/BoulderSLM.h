#pragma once
#include "mystdafx.h"
#include "PCIeBoard.h"

class BoulderSLM
{
public:
	BoulderSLM(void);
	~BoulderSLM(void);

	void loadHologram(unsigned char * hologram);
private:
	CPCIeBoard* theBoard;
	static bool readLUTFile(unsigned char* LUT, CString LUTFileName);
};
