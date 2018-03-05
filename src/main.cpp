// ----------------------------------------------------------------------------
// CaptainCPS-X's gamePKG Tool
// ----------------------------------------------------------------------------
#include "main.h"
#include "misc.h"
#include "gamePKG.h"

CapApp app;

void DlgCallbackFunction(int buttonType, void */*userData*/);

#define PADBIND(_in1, _in2)	\
	_in1 = pPad->bindFilter();	\
	_in1->setChannel(_in2);	

bool CapApp::onInit(int argc, char **argv) 
{
    // always call the superclass's method
    ::FWGLApplication::onInit(argc, argv);

	dbgFontInit();

	::FWInputDevice *pPad = FWInput::getDevice(FWInput::DeviceType_Pad, 0);

	if(pPad != NULL)
	{
		PADBIND(mpSquare	, FWInput::Channel_Button_Square);
		PADBIND(mpCross		, FWInput::Channel_Button_Cross);
		PADBIND(mpCircle	, FWInput::Channel_Button_Circle);
		PADBIND(mpTriangle	, FWInput::Channel_Button_Triangle);
		PADBIND(mpSelect	, FWInput::Channel_Button_Select);
		PADBIND(mpStart		, FWInput::Channel_Button_Start);
		PADBIND(mpUp		, FWInput::Channel_Button_Up);
		PADBIND(mpDown		, FWInput::Channel_Button_Down);
		PADBIND(mpLeft		, FWInput::Channel_Button_Left);
		PADBIND(mpRight		, FWInput::Channel_Button_Right);
	}

	gamePKG = new c_gamePKG();
	gamePKG->RefreshPKGList();

    return true;
}

CapApp::CapApp()
{
	mFrame = 0;

	mIsCirclePressed = mIsCrossPressed = mIsSquarePressed = mIsTrianglePressed = false;
	mIsSelectPressed = mIsStartPressed = false;
	mIsUpPressed = mIsDownPressed = mIsLeftPressed = mIsRightPressed = false;

	squarePressedNow = crossPressedNow = circlePressedNow = trianglePressedNow = false;	
	selectPressedNow = startPressedNow = false;
	upPressedNow = downPressedNow = leftPressedNow = rightPressedNow = false;

    // set up mStartupInfo if necessary
}

void CapApp::dbgFontInit()
{
	// initialize debug font library
	int ret;

	CellDbgFontConfig cfg;
	memset(&cfg, 0, sizeof(CellDbgFontConfig));
	
	cfg.bufSize      = 2048;
	cfg.screenWidth  = mDispInfo.mWidth;
	cfg.screenHeight = mDispInfo.mHeight;
	//cfg.bgColor      = 0x000000ff;
	ret = cellDbgFontInit(&cfg);

	if (ret != CELL_OK) 
	{
		cellMsgDialogOpen2(CELL_MSGDIALOG_DIALOG_TYPE_NORMAL, "cellDbgFontInit() failed", DlgCallbackFunction, NULL, NULL);
		return;
	}

	// Debug font consoles - Unused at this moment...
 
	/*
	CellDbgFontConsoleConfig ccfg[2];
	memset(ccfg, 0, sizeof(CellDbgFontConsoleConfig) * 2);

	ccfg[0].posLeft     = 0.1f;
	ccfg[0].posTop      = 0.8f;
	ccfg[0].cnsWidth    = 32;
	ccfg[0].cnsHeight   = 4;
	ccfg[0].scale       = 1.0f;

	// ABGR -> orange
	ccfg[0].color       = 0xff0080ff;

	mDbgFontID[0] = cellDbgFontConsoleOpen(&ccfg[0]);

	if (mDbgFontID[0] < 0) 
	{
		cellMsgDialogOpen2(CELL_MSGDIALOG_DIALOG_TYPE_NORMAL, "cellDbgFontConsoleOpen() failed", DlgCallbackFunction, NULL, NULL);
		return;
	}

	ccfg[1].posLeft     = 0.25f;
	ccfg[1].posTop      = 0.2f;
	ccfg[1].cnsWidth    = 256;
	ccfg[1].cnsHeight   = 64;
	ccfg[1].scale       = 0.5f;

	// ABGR -> pale blue
	ccfg[1].color       = 0xffff8080;

	mDbgFontID[0] = cellDbgFontConsoleOpen(&ccfg[1]);

	if (mDbgFontID[1] < 0) 
	{
		cellMsgDialogOpen2(CELL_MSGDIALOG_DIALOG_TYPE_NORMAL, "cellDbgFontConsoleOpen() failed", DlgCallbackFunction, NULL, NULL);
		return;
	}
	*/
}

void CapApp::dbgFontDraw()
{
	::cellDbgFontDraw();
}

void CapApp::dbgFontExit()
{
	::cellDbgFontExit();
}

void CapApp::InputFrameStart() 
{
	squarePressedNow	= mpSquare->getBoolValue();
	crossPressedNow		= mpCross->getBoolValue();
	circlePressedNow	= mpCircle->getBoolValue();
	trianglePressedNow	= mpTriangle->getBoolValue();

	selectPressedNow	= mpSelect->getBoolValue();
	startPressedNow		= mpStart->getBoolValue();

	upPressedNow		= mpUp->getBoolValue();
	downPressedNow		= mpDown->getBoolValue();
	leftPressedNow		= mpLeft->getBoolValue();
	rightPressedNow		= mpRight->getBoolValue();
}

void CapApp::InputFrameEnd()
{
	mIsSquarePressed	= squarePressedNow;
	mIsCrossPressed		= crossPressedNow;
	mIsCirclePressed	= circlePressedNow;
	mIsTrianglePressed	= trianglePressedNow;

	mIsSelectPressed	= selectPressedNow;
	mIsStartPressed		= startPressedNow;

	mIsUpPressed		= upPressedNow;
	mIsDownPressed		= downPressedNow;
	mIsLeftPressed		= leftPressedNow;
	mIsRightPressed		= rightPressedNow;
}

bool CapApp::onUpdate()
{
	mFrame++;

	InputFrameStart();

	gamePKG->Frame();

	InputFrameEnd();

	return FWGLApplication::onUpdate();
}

void CapApp::onRender() 
{
    // again, call the superclass method
    FWGLApplication::onRender();

	dbgFontDraw();
}

void CapApp::onShutdown() 
{
	FWInputDevice *pPad = FWInput::getDevice(FWInput::DeviceType_Pad, 0);

	if(pPad != NULL)
	{
		pPad->unbindFilter(mpSquare);
		pPad->unbindFilter(mpCross);
		pPad->unbindFilter(mpCircle);
		pPad->unbindFilter(mpTriangle);
		pPad->unbindFilter(mpSelect);
		pPad->unbindFilter(mpStart);
		pPad->unbindFilter(mpUp);
		pPad->unbindFilter(mpDown);
		pPad->unbindFilter(mpLeft);
		pPad->unbindFilter(mpRight);
		
	}

    FWGLApplication::onShutdown();
}
