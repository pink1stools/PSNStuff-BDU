// ---------------------------------------------------------------------
// CaptainCPS-X's gamePKG Tool (2012-2013)
// ---------------------------------------------------------------------
/*

 This application work as follows:

 1 - Scan hdd / usb directories for PKG files
 2 - Store each found entry info to "gamePKG->pkglst[x]"
 3 - When (X) is pressed:

	A) Create directory at "/dev_hdd0/vsh/game_pkg/" named as "gamePKG->pkglst[nSelectedPKG].nPKGID"
		
		Ex. "/dev_hdd0/vsh/game_pkg/80000000"

	B) Copy PKG file "gamePKG->pkglst[nSelectedPKG].path" to "/dev_hdd0/vsh/game_pkg/800000.."
	
	C) Write structured data file to "/dev_hdd0/vsh/game_pkg/800000..":
		"d0.pdb"
		"ICON_FILE"

---------------------------------------------------------------------

Tested on: 
	
	Rogero CFW 4.30 (v2.03 & v2.05) @ 1080p resolution [HDMI]

TODO:

 *	- implement easy thread creation class
	- put delete operations into separate thread

	- support re-scaning devices for new PKG files
WIP - verify HDD for previously queued files
	- support split PKG files

*/

// --------------------------------------------------------------------
#define APP_TITLE " PSNStuff BDU v1.00 - by pink1"
// --------------------------------------------------------------------

#include "main.h"
#include "misc.h"
#include "gamePKG.h"
#include "syscall8.h"
#include "types.h"
#include <sys/types.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "csvparser.h"
#include <string>

c_gamePKG* gamePKG;

void thread_FileCopy(uint64_t arg);
void shutdown_system();
uint64_t hex_to_u64(const char* hex_str);
extern unsigned char iconfile_data[0x131C9];
extern unsigned char jpg_d0top_data[177];
extern unsigned char mp4_d0top_data[177];
extern unsigned char pkg_d0top_data[177];
extern unsigned char pkg_d0end_data[110];
extern unsigned char other_d0end_data[52];
#define A 10
#define B 11
#define C 12
#define D 13
#define E 14
#define F 15

// -------------------------------------------------------

void DlgCallbackFunction(int buttonType, void */*userData*/)
{
    switch( buttonType ) 
	{
		case CELL_MSGDIALOG_BUTTON_YES:
		case CELL_MSGDIALOG_BUTTON_NO:		
		case CELL_MSGDIALOG_BUTTON_ESCAPE:
		case CELL_MSGDIALOG_BUTTON_NONE:
			break;

		default:
			break;
    }
}

/*
void shutdown_system() // X0-off X1-reboot, X=0 no prompt, X=1 prompt
{
	
		//unload_modules();
		{system_call_4(379,0x200,0,0,0);} // 0x1100/0x100 = turn off,0x1200/0x200=reboot
		//exit(0);
	
}*/


// Initialize gamePKG module
c_gamePKG::c_gamePKG()
{
	nSelectedPKG	= 0;
	nPKGListTop		= 0;
	nTotalPKG		= 0;
	bDeleting		= false;
	nStatus			= 0;
	nPKGID			= 10000002;
}

void c_gamePKG::Frame()
{
	DisplayFrame();
	InputFrame();
	DlgDisplayFrame();
}

void c_gamePKG::DisplayFrame()
{
	float xPos		= 0.05f;
	float yPos		= 0.05f;
	float yPosDiff	= 0.03f;	
	float nFontSize = 0.6f;

	::cellDbgFontPuts(xPos, yPos, nFontSize, 0xffffffff, "----------------------------------------------------------------------" );
	yPos += yPosDiff;	
	::cellDbgFontPuts(xPos, yPos, nFontSize, 0xffffffff, APP_TITLE);
	yPos += yPosDiff;
	::cellDbgFontPuts(xPos, yPos, nFontSize, 0xffffffff, "----------------------------------------------------------------------" );
	yPos += yPosDiff;
	::cellDbgFontPuts(xPos, yPos, nFontSize, 0xffffffff, "PRESS -(O)- TO EXIT AND RETURN TO XMB");
	yPos += yPosDiff;
	::cellDbgFontPuts(xPos, yPos, nFontSize, 0xffffffff, "PRESS -(X)- TO QUEUE PKG ON XMB");
	yPos += yPosDiff;
	::cellDbgFontPuts(xPos, yPos, nFontSize, 0xffffffff, "PRESS -(/\\)- TO VIEW PKG INFO");
	yPos += yPosDiff;
	::cellDbgFontPuts(xPos, yPos, nFontSize, 0xffffffff, "PRESS -([])- TO REMOVE FROM QUEUE");
	yPos += yPosDiff;
	::cellDbgFontPuts(xPos, yPos, nFontSize, 0xffffffff, "PRESS -[SELECT]- TO LOAD \"multiMAN\"");
	yPos += yPosDiff;
	::cellDbgFontPuts(xPos, yPos, nFontSize, 0xffffffff, "----------------------------------------------------------------------" );
	yPos += yPosDiff;

	// Main menu
	uint32_t block_size			= 0;
	uint64_t free_block_count	= 0;
	
	::cellFsGetFreeSize("/dev_hdd0", &block_size, &free_block_count);

	uint64_t nFreeHDDSpace = block_size * free_block_count;
	
	::cellDbgFontPrintf(xPos, yPos, nFontSize, 0xffffffff, "Free Space on /dev_hdd0/: %.2f %s (%lld bytes)", 
		GetByteUnit(nFreeHDDSpace), 
		GetByteUnitStr(nFreeHDDSpace),
		nFreeHDDSpace
	);
	yPos += yPosDiff;

	//::cellDbgFontPrintf(xPos, yPos, nFontSize, 0xffffffff, "Free Space on /dev_usb000/: %.2f %s (%lld bytes)", 
	//	GetByteUnit(nFreeUSB000Space), 
	//	GetByteUnitStr(nFreeUSB000Space),
	//	nFreeUSB000Space
	//	//GetNumWithCommas(nFreeUSB000Space)
	//);
	//yPos += yPosDiff;

	::cellDbgFontPrintf(xPos, yPos, nFontSize, 0xffffffff, 
		"Total PKG found in list: %d", nTotalPKG);

	yPos += yPosDiff;

	::cellDbgFontPuts(xPos, yPos, nFontSize, 0xffffffff, "----------------------------------------------------------------------" );
	yPos += yPosDiff;

	int nPKGListMax = 14;

	if(nSelectedPKG > nPKGListMax || nPKGListTop > 0)
	{
		if(nPKGListTop < (nSelectedPKG - nPKGListMax))
		{
			nPKGListTop = nSelectedPKG - nPKGListMax;
		}

		if(nPKGListTop > 0) 
		{
			if(nSelectedPKG < nPKGListTop)
			{
				nPKGListTop = nSelectedPKG;
			}
		} else {
			nPKGListTop = nSelectedPKG - nPKGListMax;
		}
	} else {
		nPKGListTop = nSelectedPKG - nPKGListMax;
	}

	if(nPKGListTop < 0) nPKGListTop = 0;

	int nPKG = nPKGListTop;
	
	while(nPKG <= (nPKGListTop + nPKGListMax))
	{
		if(nPKG == nTotalPKG) break;

		uint32_t nColor	= 0xffffffff;		// white
		nFontSize		= 0.6f;

		// PKG QUEUED
		if(pkglst[nPKG].bQueued) 
		{
			nColor		= 0xff00ff00;		// green
		}

		// PKG SELECTED
		if(nPKG == nSelectedPKG) 
		{
			nColor		= 0xff00ccff;		// yellow
			nFontSize	= 0.8f;

			if(pkglst[nPKG].bQueued) 
			{
				nColor	= 0xff0000ff;		// red
			}
		}

		::cellDbgFontPrintf(xPos, yPos, nFontSize, nColor, "[%d] %s [%.2f %s] [%s] [%s]", 
			nPKG+1, 
			pkglst[nPKG].title,
			GetByteUnit(pkglst[nPKG].nSize), GetByteUnitStr(pkglst[nPKG].nSize),
			pkglst[nPKG].Region,
			pkglst[nPKG].Typ,
			//pkglst[nPKG].bInternal ? "[HDD]" : "[USB]",
			pkglst[nPKG].bQueued ? "[Queued]" : " "
		);

		yPos += yPosDiff;

		nPKG++;
	}

	nFontSize = 0.6f;

	::cellDbgFontPuts(xPos, yPos, nFontSize, 0xffffffff, "----------------------------------------------------------------------" );
	yPos += yPosDiff;
}

void c_gamePKG::InputFrame()
{
	static int nSelInputFrame = 0;

	// ------------------------------------------------------
	// Navigation UP/DOWN fast

	/*if( !app.mIsLeftPressed && app.leftPressedNow)
	{
		if(nSelectedPKG > 14 && nSelectedPKG < nTotalPKG) 
		{
			nSelectedPKG = nSelectedPKG - 14;
		}
		else if(nSelectedPKG <= 14 && nSelectedPKG < nTotalPKG) 
		{
			nSelectedPKG = nSelectedPKG = 0;
		}
		nSelInputFrame = 0;
	} 
	
	if( !app.mIsRightPressed && app.rightPressedNow)
	{
		if(nSelectedPKG >= 0 && nSelectedPKG < nTotalPKG-15)
		{
			nSelectedPKG = nSelectedPKG + 15;
		}
		else if(nSelectedPKG >= 0 && nSelectedPKG > nTotalPKG-15)
		{
			nSelectedPKG = nSelectedPKG = nTotalPKG;
		}
		nSelInputFrame = 0;
	}*/
	
	// Navigation UP/DOWN fast

	if( app.mIsLeftPressed && app.leftPressedNow)
	{
		if(nSelectedPKG > 2 && nSelectedPKG <= nTotalPKG) 
		{
			nSelectedPKG = nSelectedPKG - 2;
		}
		else if(nSelectedPKG > 0 && nSelectedPKG <= 2) 
		{
			nSelectedPKG = nSelectedPKG = 0;
		}
		nSelInputFrame = 0;
	} 
	
	if( app.mIsRightPressed && app.rightPressedNow)
	{
		if(nSelectedPKG >= 0 && nSelectedPKG <= nTotalPKG-3)
		{
			nSelectedPKG = nSelectedPKG+1;
		}
		else if(nSelectedPKG >= 0 && nSelectedPKG > nTotalPKG-3)
		{
			nSelectedPKG = nTotalPKG-1;
		}
		nSelInputFrame = 0;
	}
	
	// ------------------------------------------------------
	// Navigation UP/DOWN with no delay

	if( !app.mIsUpPressed && app.upPressedNow)
	{
		if(nSelectedPKG > 0 && nSelectedPKG <= nTotalPKG) 
		{
			nSelectedPKG--;
		}
		nSelInputFrame = 0;
	} 
	
	if( !app.mIsDownPressed && app.downPressedNow)
	{
		if(nSelectedPKG >= 0 && nSelectedPKG < nTotalPKG-1)
		{
			nSelectedPKG++;
		}
		nSelInputFrame = 0;
	}
	
	// ------------------------------------------------------
	// Navigation UP/DOWN with delay
	
	if(((app.mFrame + nSelInputFrame) - app.mFrame) == 5)
	{
		if( app.mIsUpPressed && app.upPressedNow)
		{
			if(nSelectedPKG > 0 && nSelectedPKG <= nTotalPKG) 
			{
				nSelectedPKG--;
			}			
		}
		if( app.mIsDownPressed && app.downPressedNow)
		{		
			if(nSelectedPKG >= 0 && nSelectedPKG < nTotalPKG-1) 
			{
				nSelectedPKG++;
			}
		}
		nSelInputFrame = 0;
	}

	nSelInputFrame++;

	// ------------------------------------------------------
	// [ ] - SQUARE

	if(!app.mIsSquarePressed && app.squarePressedNow)
	{
		RemoveFromQueue();
	}

	// ------------------------------------------------------
	// (X) - CROSS
	
	if ( !app.mIsCrossPressed && app.crossPressedNow ) 
	{
		if(pkglst[nSelectedPKG].bQueued) 
		{
			// already queued...
			::cellMsgDialogOpen2(
				CELL_MSGDIALOG_DIALOG_TYPE_NORMAL, 
				"Sorry, this PKG is already queued.",
				DlgCallbackFunction, NULL, NULL
			);
		} else {
			QueuePKG();
		}
	}	
	
	// ------------------------------------------------------
	// (O) - CIRCLE
	
	if (!app.mIsCirclePressed && app.circlePressedNow) 
	{
		app.onShutdown();
		//shutdown_system();
		exit(0);
	}

	// ------------------------------------------------------
	// (O) - triangle
	
	if (!app.mIsTrianglePressed && app.trianglePressedNow) 
	{
		cellMsgDialogAbort();

		char szMsg[256] = "";
		sprintf(
			szMsg, 
			"Title - %s  \nSize - %.2f %s \nType - %s \nPosted by - %s",
			pkglst[nSelectedPKG].title,
			GetByteUnit(pkglst[nSelectedPKG].nSize), 
			GetByteUnitStr(pkglst[nSelectedPKG].nSize),
			pkglst[nSelectedPKG].Typ,
			pkglst[nSelectedPKG].postedby
			
		);

		::cellMsgDialogOpen2(
			CELL_MSGDIALOG_DIALOG_TYPE_NORMAL | CELL_MSGDIALOG_TYPE_BUTTON_TYPE_OK | CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_ON, 
			szMsg, 
			DlgCallbackFunction, NULL, NULL
		);
	}

	// ------------------------------------------------------
	// [SELECT]

	if(!app.mIsSelectPressed && app.selectPressedNow)
	{
		app.onShutdown();
		exitspawn("/dev_hdd0/game/BLES80608/USRDIR/RELOAD.SELF", NULL, NULL, NULL, NULL, 64, SYS_PROCESS_PARAM_STACK_SIZE_MAX);
	}
}

void c_gamePKG::DlgDisplayFrame()
{

    //start
	if(nStatus == STATUS_START) 
	{
		cellMsgDialogAbort();

		char szMsg[256] = "";
		sprintf(
			szMsg, 
			"Hello." 
			//pkglst[nSelectedPKG].title,
			//GetByteUnit(pkglst[nSelectedPKG].nSize), 
			//GetByteUnitStr(pkglst[nSelectedPKG].nSize)
		);

		::cellMsgDialogOpen2(
			CELL_MSGDIALOG_DIALOG_TYPE_NORMAL | CELL_MSGDIALOG_TYPE_BUTTON_TYPE_OK | CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_ON, 
			szMsg, 
			DlgCallbackFunction, NULL, NULL
		);

		//pkglst[nSelectedPKG].bQueued = true;
	}
 
	

	// COPY STARTED
	if(nStatus == STATUS_COPY_START)
	{
		char szMsg[256] = "";
		sprintf(
			szMsg, 
			"Processing \"%s\" [%.2f %s]...\n\nPlease wait, this could take a while depending on the size of the PKG. Do not turn off the system.", 
			pkglst[nSelectedPKG].title,
			GetByteUnit(pkglst[nSelectedPKG].nSize), 
			GetByteUnitStr(pkglst[nSelectedPKG].nSize)
		);

		::cellMsgDialogOpen2(
			CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL
			|CELL_MSGDIALOG_TYPE_BUTTON_TYPE_NONE
			|CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_ON
			|CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_NONE
			|CELL_MSGDIALOG_TYPE_PROGRESSBAR_SINGLE,
			szMsg,
			DlgCallbackFunction, NULL, NULL
		);
	}

	// COPY [OK]
	if(nStatus == STATUS_COPY_OK) 
	{
		cellMsgDialogAbort();

		char szMsg[256] = "";
		sprintf(
			szMsg, 
			"Successfully added \"%s\" [%.2f %s] to queue.", 
			pkglst[nSelectedPKG].title,
			GetByteUnit(pkglst[nSelectedPKG].nSize), 
			GetByteUnitStr(pkglst[nSelectedPKG].nSize)
		);

		::cellMsgDialogOpen2(
			CELL_MSGDIALOG_DIALOG_TYPE_NORMAL | CELL_MSGDIALOG_TYPE_BUTTON_TYPE_OK | CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_ON, 
			szMsg, 
			DlgCallbackFunction, NULL, NULL
		);

		pkglst[nSelectedPKG].bQueued = true;
	}

	// COPY [ERROR]
	if(nStatus == STATUS_COPY_ERROR) 
	{
		cellMsgDialogAbort();

		char szMsg[256] = "";
		sprintf(
			szMsg, 
			"Error while processing \"%s\" [%.2f %s].", 
			pkglst[nSelectedPKG].title,
			GetByteUnit(pkglst[nSelectedPKG].nSize), 
			GetByteUnitStr(pkglst[nSelectedPKG].nSize)
		);

		::cellMsgDialogOpen2(
			CELL_MSGDIALOG_DIALOG_TYPE_NORMAL | CELL_MSGDIALOG_TYPE_BUTTON_TYPE_OK | CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_OFF, 
			szMsg, 
			DlgCallbackFunction, NULL, NULL
		);

		pkglst[nSelectedPKG].bQueued = false;

		DeletePDBFiles(pkglst[nSelectedPKG].nPKGID);
		RemovePKGDir(pkglst[nSelectedPKG].nPKGID);
	}

	if(nStatus == STATUS_RM_QUEUE_START)
	{
		::cellMsgDialogOpen2(
			CELL_MSGDIALOG_BUTTON_TYPE_NONE | CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_ON,
			"Removing selected PKG from queue, please wait. Do not turn off the system.",
			DlgCallbackFunction, NULL, NULL
		);
	}

	if(nStatus == STATUS_RM_QUEUE_OK)
	{
		cellMsgDialogAbort();

		char szMsg[256] = "";
		sprintf(
			szMsg, 
			"Successfully removed \"%s\" [%.2f %s] from queue.", 
			pkglst[nSelectedPKG].title,
			GetByteUnit(pkglst[nSelectedPKG].nSize), 
			GetByteUnitStr(pkglst[nSelectedPKG].nSize)
		);

		::cellMsgDialogOpen2(
			CELL_MSGDIALOG_DIALOG_TYPE_NORMAL | CELL_MSGDIALOG_TYPE_BUTTON_TYPE_OK | CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_ON, 
			szMsg, 
			DlgCallbackFunction, NULL, NULL
		);
	}

	nStatus = STATUS_NORMAL;
}

int c_gamePKG::GetPKGDirId()
{
	int fd;

	char szDirectory[256] = "";
	sprintf(szDirectory, "/dev_hdd0/vsh/task/%d", nPKGID);

	if(cellFsOpendir(szDirectory, &fd) == CELL_FS_SUCCEEDED)
	{
		// there is already a directory with the ID, try again...
		cellFsClosedir(fd);

		nPKGID++;
		return 0;
	}

	return nPKGID;
}

int c_gamePKG::ParsePKGListFile()
{
	    int i = 0; 
		int v = 0; 
		int ret;
        ret = cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
        if (ret) {
        printf("cellSysmoduleLoadModule() error 0x%x !\n", ret);
        sys_process_exit(1);
        }
	    cellFsChmod("/dev_hdd0/game/PSNBDU123/USRDIR/db", CELL_FS_DEFAULT_CREATE_MODE_1);
        
		
		
		FILE *fp;
		fp = fopen( "/dev_hdd0/game/PSNBDU123/USRDIR/db", "r");;
		fseek(fp, 0, SEEK_END);
        long fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

		char *string = NULL;
		string = (char*)malloc(fsize + 1);
        fread(string, fsize, 1, fp);
        fclose(fp);

        string[fsize] = 0;
		
		
		
		 
		
    //                                   file, delimiter, first_line_is_header?
    CsvParser *csvparser = CsvParser_new_from_string(string, ";", 0);
    CsvRow *row;

    while ((row = CsvParser_getRow(csvparser)) ) {
	if (CsvParser_getNumFields(row) == 11){
		
		
        const char **rowFields = CsvParser_getFields(row);
        for (i = 0 ; i < CsvParser_getNumFields(row) ; i++) {
 
			for (v = 0 ; v < 10 ; v++) {
			pkglst[nTotalPKG].ID[v] = rowFields[0][v];
			pkglst[nTotalPKG].Typ[v] = rowFields[2][v];
			pkglst[nTotalPKG].Region[v] = rowFields[3][v];
			}

			for (v = 0 ; v < 256 ; v++) {

			pkglst[nTotalPKG].title[v] = rowFields[1][v];
			pkglst[nTotalPKG].path[v] = rowFields[4][v];
			pkglst[nTotalPKG].rapname[v] = rowFields[5][v];
			pkglst[nTotalPKG].rapdata[v] = rowFields[6][v];
			pkglst[nTotalPKG].postedby[v] = rowFields[8][v];
			
			pkglst[nTotalPKG].CID[v] = rowFields[9][v];
			
			}
			
			for (v = 0 ; v < 16 ; v++) {
			pkglst[nTotalPKG].rapdatat[v] = rowFields[6][v];
			pkglst[nTotalPKG].rapdatab[v] = rowFields[6][v+16];
			}
			//sprintf(pkglst[nTotalPKG].rapdata2, "%s", rowFields[6]);				
			pkglst[nTotalPKG].rapdata2 = hex_to_u64(pkglst[nTotalPKG].rapdatat);			
			pkglst[nTotalPKG].rapdata3 = hex_to_u64(pkglst[nTotalPKG].rapdatab);
			
			
            uint64_t u;
            sscanf(rowFields[10], "%" SCNu64, &u);
			pkglst[nTotalPKG].nSize = u;
			
			
			
			
			
			
        }
	    pkglst[nTotalPKG].bInternal = true;
		
		// PKG directory ID (ex. 80000000)
		while(pkglst[nTotalPKG].nPKGID == 0)
		{
			pkglst[nTotalPKG].nPKGID = GetPKGDirId();
		}

		// PKG Size in bytes
		//pkglst[nTotalPKG].nSize = 66666;
		
		nTotalPKG++;
		nPKGID++;
        CsvParser_destroy_row(row);
		}
    }
    CsvParser_destroy(csvparser);
	return 1;
}

int c_gamePKG::ParsePKGList()
{
	char szDir[][256] = {
		"PKG"		, "pkg"		, "Pkg"		,
		"PACKAGE"	, "package"	, "Package"	,
		"PACKAGES"	, "packages", "Packages"
	};

	char szDevice[][256] = { 
		"dev_hdd0/",
		"/dev_usb000", "/dev_usb001", "/dev_usb002", "/dev_usb003",
		"/dev_cf",
		"/dev_sd",
		"/dev_ms"
	};

	// Scan each device for directories...
	for(unsigned int nDev = 0; nDev < (sizeof(szDevice) / 256); nDev++)
	{
		int device;
		
		// device is mounted
		if(cellFsOpendir(szDevice[nDev], &device) == CELL_FS_SUCCEEDED) 
		{
			CellFsDirent dirEntry;
			uint64_t nread = 0;

			// start scanning for directories...
			while(cellFsReaddir(device, &dirEntry, &nread) == CELL_FS_SUCCEEDED)			
			{
				if(nread == 0) break;

				// found a directory...
				if (dirEntry.d_type == CELL_FS_TYPE_DIRECTORY)
				{					
					// check if its one of the directories we are looking for...					
					for(unsigned int nDir = 0; nDir < (sizeof(szDir) / 256); nDir++) 
					{
						// if we got a winner, then look for PKG files inside the directory...
						if(strcmp(dirEntry.d_name, szDir[nDir]) == 0)
						{
							// ---
							int d;
							char szDirectory[256] = "";
							sprintf(szDirectory, "%s/%s", szDevice[nDev], szDir[nDir]);
							
							if(cellFsOpendir(szDirectory, &d) == CELL_FS_SUCCEEDED)
							{
								CellFsDirent dirEntry2;

								uint64_t nread2 = 0;

								while(cellFsReaddir(d, &dirEntry2, &nread2) == CELL_FS_SUCCEEDED)			
								{
									if (dirEntry2.d_type == CELL_FS_TYPE_DIRECTORY)
									{
										// DIRECTORY

										// we're not looking for directories here...
									
									} else {
				
										if(dirEntry2.d_type != CELL_FS_TYPE_REGULAR) break;

										// FILE

										int nNameLen = strlen(dirEntry2.d_name);
										if (nNameLen < 5) continue;

										char* pszFilename = NULL;
										
										pszFilename = (char*)malloc(nNameLen+1);										
										memcpy(pszFilename, dirEntry2.d_name, nNameLen);
										pszFilename[nNameLen] = 0;

										// Check file extension...
										if(NULL != strstr(toLowerCase(pszFilename, strlen(pszFilename)), ".pkg")) 
										{
											// Add entry values to game list

											if(NULL != strstr(szDirectory, "/dev_hdd0"))
											{
												pkglst[nTotalPKG].bInternal = true;
											}

											// PKG Path
											if(szDirectory[strlen(szDirectory)-1] == '/')
											{
												szDirectory[strlen(szDirectory)-1] = 0;
											}

											sprintf(pkglst[nTotalPKG].path, "%s/%s", szDirectory, dirEntry2.d_name);				
					
											// PKG Filename
											sprintf(pkglst[nTotalPKG].title, "%s", dirEntry2.d_name);

											// PKG directory ID (ex. 80000000)
											while(pkglst[nTotalPKG].nPKGID == 0)
											{
												pkglst[nTotalPKG].nPKGID = GetPKGDirId();
											}

											// PKG Size in bytes
											pkglst[nTotalPKG].nSize = GetPKGSize(pkglst[nTotalPKG].path);
					
											nTotalPKG++;
											nPKGID++;
										}

										if(pszFilename) {
											free(pszFilename);
											pszFilename = NULL;
										}
									}
								}
								cellFsClosedir(d);
								d = NULL;
							}
							// ---
						}
					}

				} else {
					// FILE
				}
			}
			cellFsClosedir(device);
			device = NULL;
		}
	}

	// Scan devices again, but only root directory...
	// Have to do it this way because SONY SDK doesn't have 'telldir()' and 'seekdir()' implemented

	for(unsigned int nDev = 0; nDev < (sizeof(szDevice) / 256); nDev++)
	{
		int device;		

		// device is mounted
		if(cellFsOpendir(szDevice[nDev], &device) == CELL_FS_SUCCEEDED) 
		{
			CellFsDirent dirEntry;
			uint64_t nread = 0;

			// start scanning for directories...
			while(cellFsReaddir(device, &dirEntry, &nread) == CELL_FS_SUCCEEDED)			
			{
				if(nread == 0) break;

				// found a directory...
				if (dirEntry.d_type == CELL_FS_TYPE_DIRECTORY)
				{
					// ...
				} else {
	
					if(dirEntry.d_type != CELL_FS_TYPE_REGULAR) break;

					// FILE

					int nNameLen = strlen(dirEntry.d_name);
					if (nNameLen < 5) continue;

					char* pszFilename = NULL;
										
					pszFilename = (char*)malloc(nNameLen+1);										
					memcpy(pszFilename, dirEntry.d_name, nNameLen);
					pszFilename[nNameLen] = 0;

					// Check file extension...
					if(NULL != strstr(toLowerCase(pszFilename, strlen(pszFilename)), ".pkg")) 
					{
						// Add entry values to game list

						if(NULL != strstr(szDevice[nDev], "/dev_hdd0"))
						{
							pkglst[nTotalPKG].bInternal = true;
						}

						// PKG Path
						if(szDevice[nDev][strlen(szDevice[nDev])-1] == '/')
						{
							szDevice[nDev][strlen(szDevice[nDev])-1] = 0;
						}

						sprintf(pkglst[nTotalPKG].path, "%s/%s", szDevice[nDev], dirEntry.d_name);				
					
						// PKG Filename
						sprintf(pkglst[nTotalPKG].title, "%s", dirEntry.d_name);

						// PKG directory ID (ex. 80000000)
						while(pkglst[nTotalPKG].nPKGID == 0)
						{
							pkglst[nTotalPKG].nPKGID = GetPKGDirId();
						}

						// PKG Size in bytes
						pkglst[nTotalPKG].nSize = GetPKGSize(pkglst[nTotalPKG].path);
					
						nTotalPKG++;
						nPKGID++;
					}

					if(pszFilename) {
						free(pszFilename);
						pszFilename = NULL;
					}
				}
			}
			cellFsClosedir(device);
			device = NULL;
		}
		//ParsePKGListFile();
	}

	return 1;
}

void c_gamePKG::RefreshPKGList()
{
	if(nTotalPKG > 0) 
	{
		if(pkglst) {
			free(pkglst);
			pkglst = NULL;
		}
		nTotalPKG = 0;
	}

	pkglst = (struct c_pkglist*)malloc(sizeof(struct c_pkglist) * 20000);

	//ParsePKGList();
	//qsort(pkglst, nTotalPKG, sizeof(struct c_pkglist), _FcCompareStruct);

    ParsePKGListFile();
	//qsort(pkglst, nTotalPKG, sizeof(struct c_pkglist), _FcCompareStruct);
	//qsort(pkglst.Typ, nTotalPKG, sizeof(struct c_pkglist), _FcCompareStruct);

	//nStatus = STATUS_START;
	
}

int c_gamePKG::RemovePKGDir(int nId)
{
	if(bDeleting) return 0;
	bDeleting = true;

	char szPKGDir[256] = "";
	sprintf(szPKGDir, "/dev_hdd0/vsh/task/%d", nId);
	
	CellFsErrno ret = cellFsRmdir(szPKGDir);
	
	if(ret != CELL_FS_SUCCEEDED) 
	{
		// couldn't delete the directory...
		bDeleting = false;
		return 0;
	}

	bDeleting = false;

	return 1;
}

int c_gamePKG::RemoveAllDirFiles(char* szDirectory)
{
	int fd;
	CellFsDirent dirEntry;
	
	CellFsErrno res = cellFsOpendir(szDirectory, &fd);
	
	if (res == CELL_FS_SUCCEEDED) 
	{
		uint64_t nread;

		while(cellFsReaddir(fd, &dirEntry, &nread) == CELL_FS_SUCCEEDED) 
		{
			if(nread == 0) break;

			if (dirEntry.d_type == CELL_FS_TYPE_DIRECTORY)
			{
				// DIRECTORY
			} else {
				
				if(dirEntry.d_type != CELL_FS_TYPE_REGULAR) break;
				
				// FILE
				char szFile[256] = "";
				sprintf(szFile, "%s/%s", szDirectory, dirEntry.d_name);

				cellFsUnlink(szFile);
			}
		}
	}
	cellFsClosedir(fd);

	return 1;
}

int c_gamePKG::DeletePDBFiles(int nId)
{
	if(bDeleting) return 0;
	bDeleting = true;

	char szPDB[256] = "";
	char szIconFile[256] = "";
	sprintf(szPDB, "/dev_hdd0/vsh/task/%d/d0.pdb", nId);
	sprintf(szIconFile, "/dev_hdd0/vsh/task/%d/ICON_FILE", nId);
	
	FILE *fpPDB, *fpIcon;
	
	if((fpPDB = fopen(szPDB, "r")))
	{
		fclose(fpPDB);
		fpPDB = NULL;
		cellFsUnlink(szPDB);
	}

	if((fpIcon = fopen(szIconFile, "r")))
	{
		fclose(fpIcon);
		fpIcon = NULL;
		cellFsUnlink(szIconFile);
	}

	bDeleting = false;
	
	return 1;
}

void c_gamePKG::RemovePKG(int nId)
{
	if(bDeleting) return;
	bDeleting = true;

	char szPKG[256] = "";
	sprintf(szPKG, "/dev_hdd0/vsh/task/%d/%s", nId, pkglst[nSelectedPKG].title);

	FILE *fpPKG;
	
	if((fpPKG = fopen(szPKG, "r")))
	{
		fclose(fpPKG);
		fpPKG = NULL;
		cellFsUnlink(szPKG);
	}

	bDeleting = false;
}

void c_gamePKG::RemoveFromQueue()
{
	if(bDeleting || !pkglst[nSelectedPKG].bQueued) return; // nothing to do here...

	nStatus = STATUS_RM_QUEUE_START;

	pkglst[nSelectedPKG].bQueued = false;
	
	char szDir[256] = "";
	sprintf(szDir, "/dev_hdd0/vsh/task/%d", pkglst[nSelectedPKG].nPKGID);

	RemoveAllDirFiles(szDir);
	RemovePKGDir(pkglst[nSelectedPKG].nPKGID);

	// todo: add error checking here...

	nStatus = STATUS_RM_QUEUE_OK;
}

uint64_t c_gamePKG::GetPKGSize(char* szFilePath)
{
	CellFsStat sb;
	memset(&sb, 0, sizeof(CellFsStat));

	cellFsStat(szFilePath, &sb);
	
	return sb.st_size;
}

/* ---------------------------------------------------------------------

UPDATE [2012-12-21]:
	
- Rewrote whole function [Thanks to Dean & Deroad for the PDB structure info]

 --------------------------------------------------------------------- */
int c_gamePKG::CreatePKGPDBFiles()
{
	// Create files	
	char szPDBFile[256] = "";
	char szIconFile[256] = "";
	//char szpkgFile[256] = "";
	char szrapFile[256] = "";
	
	sprintf(szPDBFile, "/dev_hdd0/vsh/task/%d/d0.pdb", pkglst[nSelectedPKG].nPKGID);
	sprintf(szIconFile, "/dev_hdd0/vsh/task/%d/ICON_FILE", pkglst[nSelectedPKG].nPKGID);
	sprintf(szrapFile, "/dev_usb000/exdata/%s", pkglst[nSelectedPKG].rapname);
	
	
	
	FILE *fpPDB, *fpIcon, *fprap;
	fpPDB	= fopen(szPDBFile, "wb");
	fpIcon	= fopen(szIconFile, "wb");
	if(!fpPDB || !fpIcon) 
	{
		// failed to create files
		return 0;
	}

	// ------------------------------------------------------------------------
	// write - d0.pdb
	//

	fwrite(pkg_d0top_data, 177, 1, fpPDB);
	
	// 000000CE - Download expected size (in bytes)
	char dl_size[12] = {
		0x00, 0x00, 0x00, 0xCE,
		0x00, 0x00, 0x00, 0x08,
		0x00, 0x00, 0x00, 0x08
	};
	fwrite(dl_size, 12, 1, fpPDB);
	fwrite((char*) &pkglst[nSelectedPKG].nSize, 8, 1, fpPDB);

	// 000000CB - PKG file name
	char filename[4] = {
		0x00, 0x00, 0x00, 0xCB
	};
	fwrite(filename, 4, 1, fpPDB);
	
	unsigned int filename_len = strlen(pkglst[gamePKG->nSelectedPKG].CID);
	//
	filename_len = filename_len+5;
	fwrite((char*) &filename_len, 4, 1, fpPDB);
	fwrite((char*) &filename_len, 4, 1, fpPDB);
	
	char filenamePDB[256] ="";
	sprintf(
		filenamePDB, 
		"%s.pkg",  
		gamePKG->pkglst[gamePKG->nSelectedPKG].CID 
	);
	
	fwrite(filenamePDB, filename_len, 1, fpPDB);

	// 000000CC - time
	char pkg_time[42] = {
	  0x00, 0x00, 0x00, 0xCC, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0x1E,
	  0x4D, 0x6F, 0x6E, 0x2C, 0x20, 0x31, 0x31, 0x20, 0x44, 0x65, 0x63, 0x20,
	  0x32, 0x30, 0x31, 0x37, 0x20, 0x31, 0x31, 0x3A, 0x34, 0x35, 0x3A, 0x31,
	  0x30, 0x20, 0x47, 0x4D, 0x54, 0x00
	};
	fwrite(pkg_time, 42, 1, fpPDB);

	// 000000CA - PKG Link name
	char pkg_link[4] = {
		0x00, 0x00, 0x00, 0xCA
	};
	fwrite(pkg_link, 4, 1, fpPDB);
	
	unsigned int pkg_link_len = strlen(pkglst[gamePKG->nSelectedPKG].path);
	//
	pkg_link_len = pkg_link_len+1;
	fwrite((char*) &pkg_link_len, 4, 1, fpPDB);
	fwrite((char*) &pkg_link_len, 4, 1, fpPDB);
	
	char pkg_linkPDB[256] ="";
	sprintf(
		pkg_linkPDB,  
		gamePKG->pkglst[gamePKG->nSelectedPKG].path 
	);
	
	fwrite(pkg_linkPDB, pkg_link_len, 1, fpPDB);

	// 0000006A - Icon location / path (PNG w/o extension) 
	char iconpath[4] ={
		0x00, 0x00, 0x00, 0x6A
	};
	fwrite(iconpath, 4, 1, fpPDB);

	unsigned int iconpath_len = strlen(szIconFile) + 1;
	fwrite((char*) &iconpath_len, 4, 1, fpPDB);
	fwrite((char*) &iconpath_len, 4, 1, fpPDB);
	fwrite(szIconFile, iconpath_len, 1, fpPDB);

	// 00000069 - Display title
	char title[4] = {
		0x00, 0x00, 0x00, 0x69
	};
	fwrite(title, 4, 1, fpPDB);
	
	char title_str[256] = "";
	sprintf(title_str, "\xE2\x98\x85 Install \x22%s\x22", pkglst[nSelectedPKG].title);
	
	unsigned int title_len = strlen(title_str) + 1;
	fwrite((char*) &title_len, 4, 1, fpPDB);
	fwrite((char*) &title_len, 4, 1, fpPDB);
	fwrite(title_str, title_len, 1, fpPDB);
	
	fwrite(pkg_d0end_data, 110, 1, fpPDB);

	
	
	// 00000000 - Header
	/*char header[4] = {
		0x00, 0x00, 0x00, 0x00
	};
	fwrite(header, 4, 1, fpPDB);

	// 00000064 - Unknown
	char unknown64[16] = {
		0x00, 0x00, 0x00, 0x64,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x00
	};
	fwrite(unknown64, 16, 1, fpPDB);

	// 00000065 - Unknown
	char unknown1[16] = {
		0x00, 0x00, 0x00, 0x65,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x00
	};
	fwrite(unknown1, 16, 1, fpPDB);

	// 00000066 - Unknown
	char unknown66[16] = {
		0x00, 0x00, 0x00, 0x66,
		0x00, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x01,
		0x00
	};
	fwrite(unknown66, 13, 1, fpPDB);

	// 0000006B - Unknown
	char unknown2[16] = {
		0x00, 0x00, 0x00, 0x6B,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x03
	};
	fwrite(unknown2, 16, 1, fpPDB);

	// 00000068 - Status of download
	char dl_status[16] = {
		0x00, 0x00, 0x00, 0x68,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x00
	};
	fwrite(dl_status, 16, 1, fpPDB);

	// 000000D0 - Download current size (in bytes)
	char dl_progress[12] = {
		0x00, 0x00, 0x00, 0xD0,
		0x00, 0x00, 0x00, 0x08,
		0x00, 0x00, 0x00, 0x08
	};
	fwrite(dl_progress, 12, 1, fpPDB);
	fwrite((char*) &pkglst[nSelectedPKG].nSize, 8, 1, fpPDB);

	// 000000CE - Download expected size (in bytes)
	char dl_size[12] = {
		0x00, 0x00, 0x00, 0xCE,
		0x00, 0x00, 0x00, 0x08,
		0x00, 0x00, 0x00, 0x08
	};
	fwrite(dl_size, 12, 1, fpPDB);
	fwrite((char*) &pkglst[nSelectedPKG].nSize, 8, 1, fpPDB);

	// 00000069 - Display title
	char title[4] = {
		0x00, 0x00, 0x00, 0x69
	};
	fwrite(title, 4, 1, fpPDB);
	
	char title_str[256] = "";
	sprintf(title_str, "\xE2\x98\x85 Install \x22%s\x22", pkglst[nSelectedPKG].title);
	
	unsigned int title_len = strlen(title_str) + 1;
	fwrite((char*) &title_len, 4, 1, fpPDB);
	fwrite((char*) &title_len, 4, 1, fpPDB);
	fwrite(title_str, title_len, 1, fpPDB);

	// 000000CB - PKG file name
	char filename[4] = {
		0x00, 0x00, 0x00, 0xCB
	};
	fwrite(filename, 4, 1, fpPDB);
	
	unsigned int filename_len = strlen(pkglst[gamePKG->nSelectedPKG].CID);
	//
	filename_len = filename_len+5;
	fwrite((char*) &filename_len, 4, 1, fpPDB);
	fwrite((char*) &filename_len, 4, 1, fpPDB);
	
	char filenamePDB[256] ="";
	sprintf(
		filenamePDB, 
		"%s.pkg",  
		gamePKG->pkglst[gamePKG->nSelectedPKG].CID 
	);
	
	fwrite(filenamePDB, filename_len, 1, fpPDB);

	// 0000006A - Icon location / path (PNG w/o extension) 
	char iconpath[4] ={
		0x00, 0x00, 0x00, 0x6A
	};
	fwrite(iconpath, 4, 1, fpPDB);

	unsigned int iconpath_len = strlen(szIconFile) + 1;
	fwrite((char*) &iconpath_len, 4, 1, fpPDB);
	fwrite((char*) &iconpath_len, 4, 1, fpPDB);
	fwrite(szIconFile, iconpath_len, 1, fpPDB);

	fclose(fpPDB);
*/
	// ------------------------------------------------------------------------
	// write - ICON_FILE
	//

	fwrite(iconfile_data, 0x131C9, 1, fpIcon);

	fclose(fpIcon);
	
	char rapname_str[256] = "";
	sprintf(rapname_str, "%s", pkglst[nSelectedPKG].rapname);
	unsigned int rapname_len = strlen(rapname_str);
	
	if (!rapname_len == 0)
	{
	 int fd;
	if(!cellFsOpendir("/dev_usb000/exdata", &fd) == CELL_FS_SUCCEEDED)
	{
	CellFsErrno ret;
	ret = cellFsMkdir("/dev_usb000/exdata", CELL_FS_S_IFDIR | 0777);
	
	  if(ret != CELL_FS_SUCCEEDED)
	    {
		::cellMsgDialogOpen2(
			CELL_MSGDIALOG_DIALOG_TYPE_NORMAL, 
			"[Error] Could not create /dev_usb000/exdata.", 
			DlgCallbackFunction, NULL, NULL
		);
		return 1;
	}
	}
	fprap = fopen(szrapFile, "wb");
	char rapdata_str[256] = "";
	
	/*int size = 32;
	char arr_ascii[size/2];

	int i, j;
	for(j=i=0; j < size/2; i+=2){
	printf("%c", arr_ascii[j++] = pkglst[nSelectedPKG].rapdata[i]*16 + pkglst[nSelectedPKG].rapdata[i+1]);
	}*/
	//sprintf(rapdata_str, "%s", pkglst[nSelectedPKG].rapdata2);
	//fwrite(rapdata_str, 8, 1, fprap);
	fwrite( (char*) &pkglst[nSelectedPKG].rapdata2, 8, 1, fprap);
	fwrite( (char*) &pkglst[nSelectedPKG].rapdata3, 8, 1, fprap);
	
	
	fclose(fprap);
	cellFsClosedir(fd);
	};
	
	
	return 1;
}



int c_gamePKG::CreateVideoPDBFiles()
{
	// Create files	
	char szPDBFile[256] = "";
	char szIconFile[256] = "";
	//char szpkgFile[256] = "";
	//char szrapFile[256] = "";
	
	sprintf(szPDBFile, "/dev_hdd0/vsh/task/%d/d0.pdb", pkglst[nSelectedPKG].nPKGID);
	sprintf(szIconFile, "/dev_hdd0/vsh/task/%d/ICON_FILE", pkglst[nSelectedPKG].nPKGID);
	//sprintf(szrapFile, "/dev_usb000/exdata/%s", pkglst[nSelectedPKG].rapname);
	
	
	
	FILE *fpPDB, *fpIcon;
	fpPDB	= fopen(szPDBFile, "wb");
	fpIcon	= fopen(szIconFile, "wb");
	//fppkg	= fopen(szpkgFile, "wb");

	if(!fpPDB || !fpIcon) 
	{
		// failed to create files
		return 0;
	}

	// ------------------------------------------------------------------------
	// write - d0.pdb
	//

	fwrite(mp4_d0top_data, 177, 1, fpPDB);
	
	// 000000CE - Download expected size (in bytes)
	char dl_size[12] = {
		0x00, 0x00, 0x00, 0xCE,
		0x00, 0x00, 0x00, 0x08,
		0x00, 0x00, 0x00, 0x08
	};
	fwrite(dl_size, 12, 1, fpPDB);
	fwrite((char*) &pkglst[nSelectedPKG].nSize, 8, 1, fpPDB);

	// 000000CB - PKG file name
	char filename[4] = {
		0x00, 0x00, 0x00, 0xCB
	};
	fwrite(filename, 4, 1, fpPDB);
	
	unsigned int filename_len = strlen(pkglst[gamePKG->nSelectedPKG].title);
	//
	filename_len = filename_len+5;
	fwrite((char*) &filename_len, 4, 1, fpPDB);
	fwrite((char*) &filename_len, 4, 1, fpPDB);
	
	char filenamePDB[256] ="";
	sprintf(
		filenamePDB, 
		"%s.mp4",  
		gamePKG->pkglst[gamePKG->nSelectedPKG].title 
	);
	
	fwrite(filenamePDB, filename_len, 1, fpPDB);

	// 000000CC - time
	char pkg_time[42] = {
	  0x00, 0x00, 0x00, 0xCC, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0x1E,
	  0x4D, 0x6F, 0x6E, 0x2C, 0x20, 0x31, 0x31, 0x20, 0x44, 0x65, 0x63, 0x20,
	  0x32, 0x30, 0x31, 0x37, 0x20, 0x31, 0x31, 0x3A, 0x34, 0x35, 0x3A, 0x31,
	  0x30, 0x20, 0x47, 0x4D, 0x54, 0x00
	};
	fwrite(pkg_time, 42, 1, fpPDB);

	// 000000CA - PKG Link name
	char pkg_link[4] = {
		0x00, 0x00, 0x00, 0xCA
	};
	fwrite(pkg_link, 4, 1, fpPDB);
	
	unsigned int pkg_link_len = strlen(pkglst[gamePKG->nSelectedPKG].path);
	//
	pkg_link_len = pkg_link_len+1;
	fwrite((char*) &pkg_link_len, 4, 1, fpPDB);
	fwrite((char*) &pkg_link_len, 4, 1, fpPDB);
	
	char pkg_linkPDB[256] ="";
	sprintf(
		pkg_linkPDB,  
		gamePKG->pkglst[gamePKG->nSelectedPKG].path 
	);
	
	fwrite(pkg_linkPDB, pkg_link_len, 1, fpPDB);

	// 0000006A - Icon location / path (PNG w/o extension) 
	char iconpath[4] ={
		0x00, 0x00, 0x00, 0x6A
	};
	fwrite(iconpath, 4, 1, fpPDB);

	unsigned int iconpath_len = strlen(szIconFile) + 1;
	fwrite((char*) &iconpath_len, 4, 1, fpPDB);
	fwrite((char*) &iconpath_len, 4, 1, fpPDB);
	fwrite(szIconFile, iconpath_len, 1, fpPDB);

	// 00000069 - Display title
	char title[4] = {
		0x00, 0x00, 0x00, 0x69
	};
	fwrite(title, 4, 1, fpPDB);
	
	char title_str[256] = "";
	sprintf(title_str, "\xE2\x98\x85 Install \x22%s\x22", pkglst[nSelectedPKG].title);
	
	unsigned int title_len = strlen(title_str) + 1;
	fwrite((char*) &title_len, 4, 1, fpPDB);
	fwrite((char*) &title_len, 4, 1, fpPDB);
	fwrite(title_str, title_len, 1, fpPDB);
	
	fwrite(other_d0end_data, 52, 1, fpPDB);

	
	
	// 00000000 - Header
	/*char header[4] = {
		0x00, 0x00, 0x00, 0x00
	};
	fwrite(header, 4, 1, fpPDB);

	// 00000064 - Unknown
	char unknown64[16] = {
		0x00, 0x00, 0x00, 0x64,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x00
	};
	fwrite(unknown64, 16, 1, fpPDB);

	// 00000065 - Unknown
	char unknown1[16] = {
		0x00, 0x00, 0x00, 0x65,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x00
	};
	fwrite(unknown1, 16, 1, fpPDB);

	// 00000066 - Unknown
	char unknown66[16] = {
		0x00, 0x00, 0x00, 0x66,
		0x00, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x01,
		0x00
	};
	fwrite(unknown66, 13, 1, fpPDB);

	// 0000006B - Unknown
	char unknown2[16] = {
		0x00, 0x00, 0x00, 0x6B,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x03
	};
	fwrite(unknown2, 16, 1, fpPDB);

	// 00000068 - Status of download
	char dl_status[16] = {
		0x00, 0x00, 0x00, 0x68,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x00
	};
	fwrite(dl_status, 16, 1, fpPDB);

	// 000000D0 - Download current size (in bytes)
	char dl_progress[12] = {
		0x00, 0x00, 0x00, 0xD0,
		0x00, 0x00, 0x00, 0x08,
		0x00, 0x00, 0x00, 0x08
	};
	fwrite(dl_progress, 12, 1, fpPDB);
	fwrite((char*) &pkglst[nSelectedPKG].nSize, 8, 1, fpPDB);

	// 000000CE - Download expected size (in bytes)
	char dl_size[12] = {
		0x00, 0x00, 0x00, 0xCE,
		0x00, 0x00, 0x00, 0x08,
		0x00, 0x00, 0x00, 0x08
	};
	fwrite(dl_size, 12, 1, fpPDB);
	fwrite((char*) &pkglst[nSelectedPKG].nSize, 8, 1, fpPDB);

	// 00000069 - Display title
	char title[4] = {
		0x00, 0x00, 0x00, 0x69
	};
	fwrite(title, 4, 1, fpPDB);
	
	char title_str[256] = "";
	sprintf(title_str, "\xE2\x98\x85 Install \x22%s\x22", pkglst[nSelectedPKG].title);
	
	unsigned int title_len = strlen(title_str) + 1;
	fwrite((char*) &title_len, 4, 1, fpPDB);
	fwrite((char*) &title_len, 4, 1, fpPDB);
	fwrite(title_str, title_len, 1, fpPDB);

	// 000000CB - PKG file name
	char filename[4] = {
		0x00, 0x00, 0x00, 0xCB
	};
	fwrite(filename, 4, 1, fpPDB);
	
	unsigned int filename_len = strlen(pkglst[gamePKG->nSelectedPKG].CID);
	//
	filename_len = filename_len+5;
	fwrite((char*) &filename_len, 4, 1, fpPDB);
	fwrite((char*) &filename_len, 4, 1, fpPDB);
	
	char filenamePDB[256] ="";
	sprintf(
		filenamePDB, 
		"%s.pkg",  
		gamePKG->pkglst[gamePKG->nSelectedPKG].CID 
	);
	
	fwrite(filenamePDB, filename_len, 1, fpPDB);

	// 0000006A - Icon location / path (PNG w/o extension) 
	char iconpath[4] ={
		0x00, 0x00, 0x00, 0x6A
	};
	fwrite(iconpath, 4, 1, fpPDB);

	unsigned int iconpath_len = strlen(szIconFile) + 1;
	fwrite((char*) &iconpath_len, 4, 1, fpPDB);
	fwrite((char*) &iconpath_len, 4, 1, fpPDB);
	fwrite(szIconFile, iconpath_len, 1, fpPDB);

	fclose(fpPDB);
*/
	// ------------------------------------------------------------------------
	// write - ICON_FILE
	//

	fwrite(iconfile_data, 0x131C9, 1, fpIcon);

	fclose(fpIcon);
	
	
	
	
	return 1;
}

int c_gamePKG::CreateIMGPDBFiles()
{
	// Create files	
	char szPDBFile[256] = "";
	char szIconFile[256] = "";
	//char szpkgFile[256] = "";
	//char szrapFile[256] = "";
	
	sprintf(szPDBFile, "/dev_hdd0/vsh/task/%d/d0.pdb", pkglst[nSelectedPKG].nPKGID);
	sprintf(szIconFile, "/dev_hdd0/vsh/task/%d/ICON_FILE", pkglst[nSelectedPKG].nPKGID);
	//sprintf(szrapFile, "/dev_usb000/exdata/%s", pkglst[nSelectedPKG].rapname);
	
	
	
	FILE *fpPDB, *fpIcon;
	fpPDB	= fopen(szPDBFile, "wb");
	fpIcon	= fopen(szIconFile, "wb");
	//fppkg	= fopen(szpkgFile, "wb");

	if(!fpPDB || !fpIcon) 
	{
		// failed to create files
		return 0;
	}

	// ------------------------------------------------------------------------
	// write - d0.pdb
	//

	fwrite(jpg_d0top_data, 177, 1, fpPDB);
	
	// 000000CE - Download expected size (in bytes)
	char dl_size[12] = {
		0x00, 0x00, 0x00, 0xCE,
		0x00, 0x00, 0x00, 0x08,
		0x00, 0x00, 0x00, 0x08
	};
	fwrite(dl_size, 12, 1, fpPDB);
	fwrite((char*) &pkglst[nSelectedPKG].nSize, 8, 1, fpPDB);

	// 000000CB - PKG file name
	char filename[4] = {
		0x00, 0x00, 0x00, 0xCB
	};
	fwrite(filename, 4, 1, fpPDB);
	
	unsigned int filename_len = strlen(pkglst[gamePKG->nSelectedPKG].title);
	//
	filename_len = filename_len+5;
	fwrite((char*) &filename_len, 4, 1, fpPDB);
	fwrite((char*) &filename_len, 4, 1, fpPDB);
	
	char filenamePDB[256] ="";
	sprintf(
		filenamePDB, 
		"%s.jpg",  
		gamePKG->pkglst[gamePKG->nSelectedPKG].title 
	);
	
	fwrite(filenamePDB, filename_len, 1, fpPDB);

	// 000000CC - time
	char pkg_time[42] = {
	  0x00, 0x00, 0x00, 0xCC, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0x1E,
	  0x4D, 0x6F, 0x6E, 0x2C, 0x20, 0x31, 0x31, 0x20, 0x44, 0x65, 0x63, 0x20,
	  0x32, 0x30, 0x31, 0x37, 0x20, 0x31, 0x31, 0x3A, 0x34, 0x35, 0x3A, 0x31,
	  0x30, 0x20, 0x47, 0x4D, 0x54, 0x00
	};
	fwrite(pkg_time, 42, 1, fpPDB);

	// 000000CA - PKG Link name
	char pkg_link[4] = {
		0x00, 0x00, 0x00, 0xCA
	};
	fwrite(pkg_link, 4, 1, fpPDB);
	
	unsigned int pkg_link_len = strlen(pkglst[gamePKG->nSelectedPKG].path);
	//
	pkg_link_len = pkg_link_len+1;
	fwrite((char*) &pkg_link_len, 4, 1, fpPDB);
	fwrite((char*) &pkg_link_len, 4, 1, fpPDB);
	
	char pkg_linkPDB[256] ="";
	sprintf(
		pkg_linkPDB,  
		gamePKG->pkglst[gamePKG->nSelectedPKG].path 
	);
	
	fwrite(pkg_linkPDB, pkg_link_len, 1, fpPDB);

	// 0000006A - Icon location / path (PNG w/o extension) 
	char iconpath[4] ={
		0x00, 0x00, 0x00, 0x6A
	};
	fwrite(iconpath, 4, 1, fpPDB);

	unsigned int iconpath_len = strlen(szIconFile) + 1;
	fwrite((char*) &iconpath_len, 4, 1, fpPDB);
	fwrite((char*) &iconpath_len, 4, 1, fpPDB);
	fwrite(szIconFile, iconpath_len, 1, fpPDB);

	// 00000069 - Display title
	char title[4] = {
		0x00, 0x00, 0x00, 0x69
	};
	fwrite(title, 4, 1, fpPDB);
	
	char title_str[256] = "";
	sprintf(title_str, "\xE2\x98\x85 Install \x22%s\x22", pkglst[nSelectedPKG].title);
	
	unsigned int title_len = strlen(title_str) + 1;
	fwrite((char*) &title_len, 4, 1, fpPDB);
	fwrite((char*) &title_len, 4, 1, fpPDB);
	fwrite(title_str, title_len, 1, fpPDB);
	
	fwrite(other_d0end_data, 52, 1, fpPDB);

	
	
	// 00000000 - Header
	/*char header[4] = {
		0x00, 0x00, 0x00, 0x00
	};
	fwrite(header, 4, 1, fpPDB);

	// 00000064 - Unknown
	char unknown64[16] = {
		0x00, 0x00, 0x00, 0x64,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x00
	};
	fwrite(unknown64, 16, 1, fpPDB);

	// 00000065 - Unknown
	char unknown1[16] = {
		0x00, 0x00, 0x00, 0x65,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x00
	};
	fwrite(unknown1, 16, 1, fpPDB);

	// 00000066 - Unknown
	char unknown66[16] = {
		0x00, 0x00, 0x00, 0x66,
		0x00, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x01,
		0x00
	};
	fwrite(unknown66, 13, 1, fpPDB);

	// 0000006B - Unknown
	char unknown2[16] = {
		0x00, 0x00, 0x00, 0x6B,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x03
	};
	fwrite(unknown2, 16, 1, fpPDB);

	// 00000068 - Status of download
	char dl_status[16] = {
		0x00, 0x00, 0x00, 0x68,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x00
	};
	fwrite(dl_status, 16, 1, fpPDB);

	// 000000D0 - Download current size (in bytes)
	char dl_progress[12] = {
		0x00, 0x00, 0x00, 0xD0,
		0x00, 0x00, 0x00, 0x08,
		0x00, 0x00, 0x00, 0x08
	};
	fwrite(dl_progress, 12, 1, fpPDB);
	fwrite((char*) &pkglst[nSelectedPKG].nSize, 8, 1, fpPDB);

	// 000000CE - Download expected size (in bytes)
	char dl_size[12] = {
		0x00, 0x00, 0x00, 0xCE,
		0x00, 0x00, 0x00, 0x08,
		0x00, 0x00, 0x00, 0x08
	};
	fwrite(dl_size, 12, 1, fpPDB);
	fwrite((char*) &pkglst[nSelectedPKG].nSize, 8, 1, fpPDB);

	// 00000069 - Display title
	char title[4] = {
		0x00, 0x00, 0x00, 0x69
	};
	fwrite(title, 4, 1, fpPDB);
	
	char title_str[256] = "";
	sprintf(title_str, "\xE2\x98\x85 Install \x22%s\x22", pkglst[nSelectedPKG].title);
	
	unsigned int title_len = strlen(title_str) + 1;
	fwrite((char*) &title_len, 4, 1, fpPDB);
	fwrite((char*) &title_len, 4, 1, fpPDB);
	fwrite(title_str, title_len, 1, fpPDB);

	// 000000CB - PKG file name
	char filename[4] = {
		0x00, 0x00, 0x00, 0xCB
	};
	fwrite(filename, 4, 1, fpPDB);
	
	unsigned int filename_len = strlen(pkglst[gamePKG->nSelectedPKG].CID);
	//
	filename_len = filename_len+5;
	fwrite((char*) &filename_len, 4, 1, fpPDB);
	fwrite((char*) &filename_len, 4, 1, fpPDB);
	
	char filenamePDB[256] ="";
	sprintf(
		filenamePDB, 
		"%s.pkg",  
		gamePKG->pkglst[gamePKG->nSelectedPKG].CID 
	);
	
	fwrite(filenamePDB, filename_len, 1, fpPDB);

	// 0000006A - Icon location / path (PNG w/o extension) 
	char iconpath[4] ={
		0x00, 0x00, 0x00, 0x6A
	};
	fwrite(iconpath, 4, 1, fpPDB);

	unsigned int iconpath_len = strlen(szIconFile) + 1;
	fwrite((char*) &iconpath_len, 4, 1, fpPDB);
	fwrite((char*) &iconpath_len, 4, 1, fpPDB);
	fwrite(szIconFile, iconpath_len, 1, fpPDB);

	fclose(fpPDB);
*/
	// ------------------------------------------------------------------------
	// write - ICON_FILE
	//

	fwrite(iconfile_data, 0x131C9, 1, fpIcon);

	fclose(fpIcon);
	
	
	
	
	return 1;
}

#define BUFF_SIZE	0x300000 // 3MB

double _round(double r) {
    return (r > 0.0) ? floor(r + 0.5) : ceil(r - 0.5);
}

void thread_Createmp4(uint64_t fsize/*arg*/)
{	
	gamePKG->nStatus = STATUS_COPY_START;

	//char *tf = NULL;
	FILE *filer			= NULL;
	FILE *filew			= NULL;
	uint64_t numr		= 0;	// elements read
	uint64_t numw		= 0;	// elements wrote	
	uint64_t nTotalRead = 0;
	double nCopyPct		= 0.0f;
	double nTotDelta	= 0.0f;
	uint32_t nApproxTotDelta = 0;
	//uint64_t fsize		= 0;	


	char *buffer = NULL;
	buffer = (char*)malloc(BUFF_SIZE);

	/*
	char szFilein[256] ="";
	sprintf(
		szFilein, 
		"/dev_hdd0/tmp/%d/%s", 
		gamePKG->pkglst[gamePKG->nSelectedPKG].nPKGID, 
		gamePKG->pkglst[gamePKG->nSelectedPKG].title
	);
	//gamePKG->nStatus = STATUS_COPY_START;working
	 
	//pkglst[nSelectedPKG].nSize;
	//char *pkgfill[gamePKG->pkglst[gamePKG->nSelectedPKG].nSize];
	//gamePKG->nStatus = STATUS_COPY_START; not working
	//filer = fopen(szFilein,"rb");
    //uint64_t fsize = sizeof(pkgfill);
	
	    //gamePKG->nStatus = STATUS_COPY_START; not working
*/
	char szFileOut[256] ="";
	sprintf(
		szFileOut, 
		"/dev_hdd0/vsh/task/%d/%s.mp4", 
		gamePKG->pkglst[gamePKG->nSelectedPKG].nPKGID, 
		gamePKG->pkglst[gamePKG->nSelectedPKG].title 
	);
	filew = fopen(szFileOut, "wb");

	//if(pkgfill != NULL && filew != NULL)
	if(filew != NULL)
	{
		bool bCopyError = false;

		
		
	    //gamePKG->nStatus = STATUS_COPY_START;

		while(fsize > 0)
		{
			/*// read
			if((numr = fread(buffer, 1, BUFF_SIZE, pkgfill)) != BUFF_SIZE) 
			{
				if(ferror(pkgfill) != 0) 
				{
					bCopyError = true;
					break;
				}
				else if(feof(pkgfill) != 0)
				{
					// ...
				}
			}*/
			//numr = BUFF_SIZE;
			if(fsize > BUFF_SIZE)
			{
			numr = BUFF_SIZE;
			fsize = fsize - BUFF_SIZE;
			}
			else if(fsize < BUFF_SIZE)
			{
			numr = fsize;
			fsize = fsize - numr;
			}
			
			// write
			if((numw = fwrite(buffer, 1, numr, filew)) != numr) 
			{
				bCopyError = true;
				break;
			}
			
			nTotalRead += numr;			

			nCopyPct = (double)(((double)numr / (double)gamePKG->pkglst[gamePKG->nSelectedPKG].nSize) * 100.0f);
			double nTotalPct = (double)((double)nTotalRead / (double)gamePKG->pkglst[gamePKG->nSelectedPKG].nSize) * 100.0f;

			nTotDelta += nCopyPct;
			nApproxTotDelta += (uint32_t)round(nCopyPct);

			if((double)nApproxTotDelta < nTotalPct) 
			{
				// Compensate loss of float/double data, as for example: approx 70% vs. precise 95%
				nApproxTotDelta += (uint32_t)(nTotalPct - (double)nApproxTotDelta);
				nCopyPct += (nTotalPct - (double)nApproxTotDelta);				
			}
			cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, (uint32_t)nCopyPct);

			char msg[256] = "";
			sprintf(
				msg, 
				//"%.2f %s / %.2f %s (%.1f %%)(delta: %d %%)",
				"%.2f %s / %.2f %s",
				GetByteUnit(nTotalRead),
				GetByteUnitStr(nTotalRead),
				GetByteUnit(gamePKG->pkglst[gamePKG->nSelectedPKG].nSize),
				GetByteUnitStr(gamePKG->pkglst[gamePKG->nSelectedPKG].nSize)	//,
				//nTotalPct,
				//nApproxTotDelta
			);
			cellMsgDialogProgressBarSetMsg(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, msg);
		}

		nCopyPct = (double)(((double)numr / (double)gamePKG->pkglst[gamePKG->nSelectedPKG].nSize) * 100.0f);
		double nTotalPct = (double)((double)nTotalRead / (double)gamePKG->pkglst[gamePKG->nSelectedPKG].nSize) * 100.0f;

		nTotDelta += nCopyPct;
		nApproxTotDelta += (uint32_t)round(nCopyPct);

		if((double)nApproxTotDelta < nTotalPct) 
		{
			// Compensate loss of float/double data, as for example: approx 70% vs. precise 95%
			nApproxTotDelta += (uint32_t)(nTotalPct - (double)nApproxTotDelta);
			nCopyPct += (nTotalPct - (double)nApproxTotDelta);
		}
		cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, (uint32_t)nCopyPct);

		char msg[256] = "";
		sprintf(
			msg, 
			//"%.2f %s / %.2f %s (%.1f %%)(delta: %d %%)",
			"%.2f %s / %.2f %s",
			GetByteUnit(nTotalRead),
			GetByteUnitStr(nTotalRead),
			GetByteUnit(gamePKG->pkglst[gamePKG->nSelectedPKG].nSize),
			GetByteUnitStr(gamePKG->pkglst[gamePKG->nSelectedPKG].nSize)	//,
			//nTotalPct,
			//nApproxTotDelta
		);
		cellMsgDialogProgressBarSetMsg(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, msg);

		if(filer) fclose(filer);
		if(filew) fclose(filew);
		
		if(bCopyError)
		{
			cellFsUnlink(szFileOut); // delete file

			gamePKG->nStatus = STATUS_COPY_ERROR;
			sys_ppu_thread_exit(0);
			return;
		}

	} else {

		if(buffer) free(buffer);
		buffer = NULL;

		if(filer) fclose(filer);
		if(filew) 
		{
			fclose(filew);		
			cellFsUnlink(szFileOut); // delete file
		}

		gamePKG->nStatus = STATUS_COPY_ERROR;
		sys_ppu_thread_exit(0);
		return;
	}
	
	if(buffer) 
	{
		free(buffer);
		buffer = NULL;
	}

	sys_timer_usleep(1000000); // wait 1 second

	gamePKG->nStatus = STATUS_COPY_OK;
	sys_ppu_thread_exit(0);
}


void thread_Createjpg(uint64_t fsize/*arg*/)
{	
	gamePKG->nStatus = STATUS_COPY_START;

	//char *tf = NULL;
	FILE *filer			= NULL;
	FILE *filew			= NULL;
	uint64_t numr		= 0;	// elements read
	uint64_t numw		= 0;	// elements wrote	
	uint64_t nTotalRead = 0;
	double nCopyPct		= 0.0f;
	double nTotDelta	= 0.0f;
	uint32_t nApproxTotDelta = 0;
	//uint64_t fsize		= 0;	


	char *buffer = NULL;
	buffer = (char*)malloc(BUFF_SIZE);

	/*
	char szFilein[256] ="";
	sprintf(
		szFilein, 
		"/dev_hdd0/tmp/%d/%s", 
		gamePKG->pkglst[gamePKG->nSelectedPKG].nPKGID, 
		gamePKG->pkglst[gamePKG->nSelectedPKG].title
	);
	//gamePKG->nStatus = STATUS_COPY_START;working
	 
	//pkglst[nSelectedPKG].nSize;
	//char *pkgfill[gamePKG->pkglst[gamePKG->nSelectedPKG].nSize];
	//gamePKG->nStatus = STATUS_COPY_START; not working
	//filer = fopen(szFilein,"rb");
    //uint64_t fsize = sizeof(pkgfill);
	
	    //gamePKG->nStatus = STATUS_COPY_START; not working
*/
	char szFileOut[256] ="";
	sprintf(
		szFileOut, 
		"/dev_hdd0/vsh/task/%d/%s.jpg", 
		gamePKG->pkglst[gamePKG->nSelectedPKG].nPKGID, 
		gamePKG->pkglst[gamePKG->nSelectedPKG].title 
	);
	filew = fopen(szFileOut, "wb");

	//if(pkgfill != NULL && filew != NULL)
	if(filew != NULL)
	{
		bool bCopyError = false;

		
		
	    //gamePKG->nStatus = STATUS_COPY_START;

		while(fsize > 0)
		{
			/*// read
			if((numr = fread(buffer, 1, BUFF_SIZE, pkgfill)) != BUFF_SIZE) 
			{
				if(ferror(pkgfill) != 0) 
				{
					bCopyError = true;
					break;
				}
				else if(feof(pkgfill) != 0)
				{
					// ...
				}
			}*/
			//numr = BUFF_SIZE;
			if(fsize > BUFF_SIZE)
			{
			numr = BUFF_SIZE;
			fsize = fsize - BUFF_SIZE;
			}
			else if(fsize < BUFF_SIZE)
			{
			numr = fsize;
			fsize = fsize - numr;
			}
			
			// write
			if((numw = fwrite(buffer, 1, numr, filew)) != numr) 
			{
				bCopyError = true;
				break;
			}
			
			nTotalRead += numr;			

			nCopyPct = (double)(((double)numr / (double)gamePKG->pkglst[gamePKG->nSelectedPKG].nSize) * 100.0f);
			double nTotalPct = (double)((double)nTotalRead / (double)gamePKG->pkglst[gamePKG->nSelectedPKG].nSize) * 100.0f;

			nTotDelta += nCopyPct;
			nApproxTotDelta += (uint32_t)round(nCopyPct);

			if((double)nApproxTotDelta < nTotalPct) 
			{
				// Compensate loss of float/double data, as for example: approx 70% vs. precise 95%
				nApproxTotDelta += (uint32_t)(nTotalPct - (double)nApproxTotDelta);
				nCopyPct += (nTotalPct - (double)nApproxTotDelta);				
			}
			cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, (uint32_t)nCopyPct);

			char msg[256] = "";
			sprintf(
				msg, 
				//"%.2f %s / %.2f %s (%.1f %%)(delta: %d %%)",
				"%.2f %s / %.2f %s",
				GetByteUnit(nTotalRead),
				GetByteUnitStr(nTotalRead),
				GetByteUnit(gamePKG->pkglst[gamePKG->nSelectedPKG].nSize),
				GetByteUnitStr(gamePKG->pkglst[gamePKG->nSelectedPKG].nSize)	//,
				//nTotalPct,
				//nApproxTotDelta
			);
			cellMsgDialogProgressBarSetMsg(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, msg);
		}

		nCopyPct = (double)(((double)numr / (double)gamePKG->pkglst[gamePKG->nSelectedPKG].nSize) * 100.0f);
		double nTotalPct = (double)((double)nTotalRead / (double)gamePKG->pkglst[gamePKG->nSelectedPKG].nSize) * 100.0f;

		nTotDelta += nCopyPct;
		nApproxTotDelta += (uint32_t)round(nCopyPct);

		if((double)nApproxTotDelta < nTotalPct) 
		{
			// Compensate loss of float/double data, as for example: approx 70% vs. precise 95%
			nApproxTotDelta += (uint32_t)(nTotalPct - (double)nApproxTotDelta);
			nCopyPct += (nTotalPct - (double)nApproxTotDelta);
		}
		cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, (uint32_t)nCopyPct);

		char msg[256] = "";
		sprintf(
			msg, 
			//"%.2f %s / %.2f %s (%.1f %%)(delta: %d %%)",
			"%.2f %s / %.2f %s",
			GetByteUnit(nTotalRead),
			GetByteUnitStr(nTotalRead),
			GetByteUnit(gamePKG->pkglst[gamePKG->nSelectedPKG].nSize),
			GetByteUnitStr(gamePKG->pkglst[gamePKG->nSelectedPKG].nSize)	//,
			//nTotalPct,
			//nApproxTotDelta
		);
		cellMsgDialogProgressBarSetMsg(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, msg);

		if(filer) fclose(filer);
		if(filew) fclose(filew);
		
		if(bCopyError)
		{
			cellFsUnlink(szFileOut); // delete file

			gamePKG->nStatus = STATUS_COPY_ERROR;
			sys_ppu_thread_exit(0);
			return;
		}

	} else {

		if(buffer) free(buffer);
		buffer = NULL;

		if(filer) fclose(filer);
		if(filew) 
		{
			fclose(filew);		
			cellFsUnlink(szFileOut); // delete file
		}

		gamePKG->nStatus = STATUS_COPY_ERROR;
		sys_ppu_thread_exit(0);
		return;
	}
	
	if(buffer) 
	{
		free(buffer);
		buffer = NULL;
	}

	sys_timer_usleep(1000000); // wait 1 second

	gamePKG->nStatus = STATUS_COPY_OK;
	sys_ppu_thread_exit(0);
}


void thread_CreateC00(uint64_t fsize/*arg*/)
{	
	gamePKG->nStatus = STATUS_COPY_START;

	//char *tf = NULL;
	FILE *filer			= NULL;
	FILE *filew			= NULL;
	uint64_t numr		= 0;	// elements read
	uint64_t numw		= 0;	// elements wrote	
	uint64_t nTotalRead = 0;
	double nCopyPct		= 0.0f;
	double nTotDelta	= 0.0f;
	uint32_t nApproxTotDelta = 0;
	//uint64_t fsize		= 0;	


	char *buffer = NULL;
	buffer = (char*)malloc(BUFF_SIZE);

	/*
	char szFilein[256] ="";
	sprintf(
		szFilein, 
		"/dev_hdd0/tmp/%d/%s", 
		gamePKG->pkglst[gamePKG->nSelectedPKG].nPKGID, 
		gamePKG->pkglst[gamePKG->nSelectedPKG].title
	);
	//gamePKG->nStatus = STATUS_COPY_START;working
	 
	//pkglst[nSelectedPKG].nSize;
	//char *pkgfill[gamePKG->pkglst[gamePKG->nSelectedPKG].nSize];
	//gamePKG->nStatus = STATUS_COPY_START; not working
	//filer = fopen(szFilein,"rb");
    //uint64_t fsize = sizeof(pkgfill);
	
	    //gamePKG->nStatus = STATUS_COPY_START; not working
*/
	char szFileOut[256] ="";
	sprintf(
		szFileOut, 
		"/dev_hdd0/vsh/task/%d/%s.pkg", 
		gamePKG->pkglst[gamePKG->nSelectedPKG].nPKGID, 
		gamePKG->pkglst[gamePKG->nSelectedPKG].CID 
	);
	filew = fopen(szFileOut, "wb");

	//if(pkgfill != NULL && filew != NULL)
	if(filew != NULL)
	{
		bool bCopyError = false;

		
		
	    //gamePKG->nStatus = STATUS_COPY_START;

		while(fsize > 0)
		{
			/*// read
			if((numr = fread(buffer, 1, BUFF_SIZE, pkgfill)) != BUFF_SIZE) 
			{
				if(ferror(pkgfill) != 0) 
				{
					bCopyError = true;
					break;
				}
				else if(feof(pkgfill) != 0)
				{
					// ...
				}
			}*/
			//numr = BUFF_SIZE;
			if(fsize > BUFF_SIZE)
			{
			numr = BUFF_SIZE;
			fsize = fsize - BUFF_SIZE;
			}
			else if(fsize < BUFF_SIZE)
			{
			numr = fsize;
			fsize = fsize - numr;
			}
			
			// write
			if((numw = fwrite(buffer, 1, numr, filew)) != numr) 
			{
				bCopyError = true;
				break;
			}
			
			nTotalRead += numr;			

			nCopyPct = (double)(((double)numr / (double)gamePKG->pkglst[gamePKG->nSelectedPKG].nSize) * 100.0f);
			double nTotalPct = (double)((double)nTotalRead / (double)gamePKG->pkglst[gamePKG->nSelectedPKG].nSize) * 100.0f;

			nTotDelta += nCopyPct;
			nApproxTotDelta += (uint32_t)round(nCopyPct);

			if((double)nApproxTotDelta < nTotalPct) 
			{
				// Compensate loss of float/double data, as for example: approx 70% vs. precise 95%
				nApproxTotDelta += (uint32_t)(nTotalPct - (double)nApproxTotDelta);
				nCopyPct += (nTotalPct - (double)nApproxTotDelta);				
			}
			cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, (uint32_t)nCopyPct);

			char msg[256] = "";
			sprintf(
				msg, 
				//"%.2f %s / %.2f %s (%.1f %%)(delta: %d %%)",
				"%.2f %s / %.2f %s",
				GetByteUnit(nTotalRead),
				GetByteUnitStr(nTotalRead),
				GetByteUnit(gamePKG->pkglst[gamePKG->nSelectedPKG].nSize),
				GetByteUnitStr(gamePKG->pkglst[gamePKG->nSelectedPKG].nSize)	//,
				//nTotalPct,
				//nApproxTotDelta
			);
			cellMsgDialogProgressBarSetMsg(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, msg);
		}

		nCopyPct = (double)(((double)numr / (double)gamePKG->pkglst[gamePKG->nSelectedPKG].nSize) * 100.0f);
		double nTotalPct = (double)((double)nTotalRead / (double)gamePKG->pkglst[gamePKG->nSelectedPKG].nSize) * 100.0f;

		nTotDelta += nCopyPct;
		nApproxTotDelta += (uint32_t)round(nCopyPct);

		if((double)nApproxTotDelta < nTotalPct) 
		{
			// Compensate loss of float/double data, as for example: approx 70% vs. precise 95%
			nApproxTotDelta += (uint32_t)(nTotalPct - (double)nApproxTotDelta);
			nCopyPct += (nTotalPct - (double)nApproxTotDelta);
		}
		cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, (uint32_t)nCopyPct);

		char msg[256] = "";
		sprintf(
			msg, 
			//"%.2f %s / %.2f %s (%.1f %%)(delta: %d %%)",
			"%.2f %s / %.2f %s",
			GetByteUnit(nTotalRead),
			GetByteUnitStr(nTotalRead),
			GetByteUnit(gamePKG->pkglst[gamePKG->nSelectedPKG].nSize),
			GetByteUnitStr(gamePKG->pkglst[gamePKG->nSelectedPKG].nSize)	//,
			//nTotalPct,
			//nApproxTotDelta
		);
		cellMsgDialogProgressBarSetMsg(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, msg);

		if(filer) fclose(filer);
		if(filew) fclose(filew);
		
		if(bCopyError)
		{
			cellFsUnlink(szFileOut); // delete file

			gamePKG->nStatus = STATUS_COPY_ERROR;
			sys_ppu_thread_exit(0);
			return;
		}

	} else {

		if(buffer) free(buffer);
		buffer = NULL;

		if(filer) fclose(filer);
		if(filew) 
		{
			fclose(filew);		
			cellFsUnlink(szFileOut); // delete file
		}

		gamePKG->nStatus = STATUS_COPY_ERROR;
		sys_ppu_thread_exit(0);
		return;
	}
	
	if(buffer) 
	{
		free(buffer);
		buffer = NULL;
	}

	sys_timer_usleep(1000000); // wait 1 second

	gamePKG->nStatus = STATUS_COPY_OK;
	sys_ppu_thread_exit(0);
}

int c_gamePKG::QueuePKG()
{
	char pszPKGDir[256] ="";
	sprintf(pszPKGDir, "/dev_hdd0/vsh/task/%d", pkglst[nSelectedPKG].nPKGID);
    

		
	bool isC00  = (std::strcmp(pkglst[nSelectedPKG].Typ, "C00") == 0);
	bool isPSN  = (std::strcmp(pkglst[nSelectedPKG].Typ, "PSN") == 0);
	bool isDLC  = (std::strcmp(pkglst[nSelectedPKG].Typ, "DLC") == 0);	
	bool isDemo = (std::strcmp(pkglst[nSelectedPKG].Typ, "Demo") == 0);
	bool isVideo  = (std::strcmp(pkglst[nSelectedPKG].Typ, "Video") == 0);
	bool isTheme  = (std::strcmp(pkglst[nSelectedPKG].Typ, "Theme") == 0);
	bool isWallpaper  = (std::strcmp(pkglst[nSelectedPKG].Typ, "Wallpaper") == 0);
	bool isPS2  = (std::strcmp(pkglst[nSelectedPKG].Typ, "PS2") == 0);
	bool isPS1  = (std::strcmp(pkglst[nSelectedPKG].Typ, "PS1") == 0);
	bool isPSVita  = (std::strcmp(pkglst[nSelectedPKG].Typ, "PSVita") == 0);
	bool isMini  = (std::strcmp(pkglst[nSelectedPKG].Typ, "Mini") == 0);
	bool isPSP  = (std::strcmp(pkglst[nSelectedPKG].Typ, "PSP") == 0);
	bool isEDAT  = (std::strcmp(pkglst[nSelectedPKG].Typ, "EDAT") == 0);
	bool isSoundtrack  = (std::strcmp(pkglst[nSelectedPKG].Typ, "Soundtrack") == 0);
	bool isUpdate  = (std::strcmp(pkglst[nSelectedPKG].Typ, "Update") == 0);
	bool isAvatar  = (std::strcmp(pkglst[nSelectedPKG].Typ, "Avatar") == 0);


	
    if(isC00 == true || isPSN == true || isDLC == true || isDemo == true || isTheme == true 
	|| isPS2 == true || isPS1 == true || isPSVita == true || isMini == true || isPSP == true 
	|| isEDAT == true || isSoundtrack == true || isUpdate == true || isAvatar == true)
	{
	CellFsErrno ret;
	ret = cellFsMkdir("/dev_hdd0/vsh/task", CELL_FS_S_IFDIR | 0777);
	ret = cellFsMkdir(pszPKGDir, CELL_FS_S_IFDIR | 0777);

	if(ret != CELL_FS_SUCCEEDED)
	{
		::cellMsgDialogOpen2(
			CELL_MSGDIALOG_DIALOG_TYPE_NORMAL, 
			"[Error] Could not create the required directory on HDD.", 
			DlgCallbackFunction, NULL, NULL
		);
		pkglst[nSelectedPKG].bQueued = false;
		return 0;
	}
	
    
	if(!CreatePKGPDBFiles())
	{
		::cellMsgDialogOpen2(
			CELL_MSGDIALOG_DIALOG_TYPE_NORMAL, 
			"[Error] Could not create / write needed files to HDD.", 
			DlgCallbackFunction, NULL, NULL
		);
		pkglst[nSelectedPKG].bQueued = false;
		return 0;
	}
	

	sys_ppu_thread_t thread_id;

	sys_ppu_thread_create(
		&thread_id,
		thread_CreateC00,				// callback function
		//0x1337,							// arg
		pkglst[nSelectedPKG].nSize, 
		1500,							// priority
		0x1000,							// stack size
		SYS_PPU_THREAD_CREATE_JOINABLE, //
		"File Copy"						// name
	);
	}
	
	else if(isVideo == true)
	{
	CellFsErrno ret;
	ret = cellFsMkdir("/dev_hdd0/vsh/task", CELL_FS_S_IFDIR | 0777);
	ret = cellFsMkdir(pszPKGDir, CELL_FS_S_IFDIR | 0777);

	if(ret != CELL_FS_SUCCEEDED)
	{
		::cellMsgDialogOpen2(
			CELL_MSGDIALOG_DIALOG_TYPE_NORMAL, 
			"[Error] Could not create the required directory on HDD.", 
			DlgCallbackFunction, NULL, NULL
		);
		pkglst[nSelectedPKG].bQueued = false;
		return 0;
	}
    
	if(!CreateVideoPDBFiles())
	{
		::cellMsgDialogOpen2(
			CELL_MSGDIALOG_DIALOG_TYPE_NORMAL, 
			"[Error] Could not create / write needed files to HDD.", 
			DlgCallbackFunction, NULL, NULL
		);
		pkglst[nSelectedPKG].bQueued = false;
		return 0;
	}
	

	sys_ppu_thread_t thread_id;

	sys_ppu_thread_create(
		&thread_id,
		thread_Createmp4,				// callback function
		//0x1337,							// arg
		pkglst[nSelectedPKG].nSize, 
		1500,							// priority
		0x1000,							// stack size
		SYS_PPU_THREAD_CREATE_JOINABLE, //
		"File Copy"						// name
	);
	}
	
	else if(isWallpaper == true)
	{
	CellFsErrno ret;
	ret = cellFsMkdir("/dev_hdd0/vsh/task", CELL_FS_S_IFDIR | 0777);
	ret = cellFsMkdir(pszPKGDir, CELL_FS_S_IFDIR | 0777);

	if(ret != CELL_FS_SUCCEEDED)
	{
		::cellMsgDialogOpen2(
			CELL_MSGDIALOG_DIALOG_TYPE_NORMAL, 
			"[Error] Could not create the required directory on HDD.", 
			DlgCallbackFunction, NULL, NULL
		);
		pkglst[nSelectedPKG].bQueued = false;
		return 0;
	}
    
	if(!CreateIMGPDBFiles())
	{
		::cellMsgDialogOpen2(
			CELL_MSGDIALOG_DIALOG_TYPE_NORMAL, 
			"[Error] Could not create / write needed files to HDD.", 
			DlgCallbackFunction, NULL, NULL
		);
		pkglst[nSelectedPKG].bQueued = false;
		return 0;
	}
	

	sys_ppu_thread_t thread_id;

	sys_ppu_thread_create(
		&thread_id,
		thread_Createjpg,				// callback function
		//0x1337,							// arg
		pkglst[nSelectedPKG].nSize, 
		1500,							// priority
		0x1000,							// stack size
		SYS_PPU_THREAD_CREATE_JOINABLE, //
		"File Copy"						// name
	);
	}
	
	return 1;
}