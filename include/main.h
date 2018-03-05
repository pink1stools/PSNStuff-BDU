// ---------------------------------------------------------------------
// CaptainCPS-X's gamePKG Tool
// ---------------------------------------------------------------------
#ifndef MAIN_H
#define MAIN_H

#include <cell/dbgfont.h>
#include <cell/cell_fs.h>
#include <sys/paths.h>
#include <sys/process.h>
#include <sys/ppu_thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <sysutil/sysutil_msgdialog.h>
#include <cell/sysmodule.h>
#include <sys/memory.h>

#include <dirent.h>

#include "psgl/FWGLApplication.h"
#include "psgl/FWGLGrid.h"
#include "FWInput.h"
#include "FWDebugFont.h"

class CapApp : public FWGLApplication
{
public:
	
	CapApp();

	virtual bool onInit(int argc, char **argv);
	virtual void onRender();
	virtual void onShutdown();
	virtual bool onUpdate();

	virtual void InputFrameStart();
	virtual void InputFrameEnd();

	virtual void dbgFontInit();
	virtual void dbgFontDraw();
	virtual void dbgFontExit();

	unsigned int mFrame;

	bool	mIsCirclePressed, mIsCrossPressed, mIsSquarePressed, mIsTrianglePressed;
	bool	mIsSelectPressed, mIsStartPressed;
	bool	mIsUpPressed, mIsDownPressed, mIsLeftPressed, mIsRightPressed;

	bool	squarePressedNow, crossPressedNow, circlePressedNow, trianglePressedNow;	
	bool	selectPressedNow, startPressedNow;
	bool	upPressedNow, downPressedNow, leftPressedNow, rightPressedNow;

	FWInputFilter   *mpCircle, *mpCross, *mpSquare, *mpTriangle;		
	FWInputFilter   *mpSelect, *mpStart;
	FWInputFilter   *mpUp, *mpDown, *mpLeft, *mpRight;

	//CellDbgFontConsoleId mDbgFontID[2];

protected:
private:

};

extern CapApp app;

#endif // MAIN_H
