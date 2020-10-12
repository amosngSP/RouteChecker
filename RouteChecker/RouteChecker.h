// GMPHelper.h : main header file for the GMPHelper DLL
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols
#include "RouteChecker2.h"


// CGMPHelperApp
// See GMPHelper.cpp for the implementation of this class
//

class CGMPHelperApp : public CWinApp
{
public:
	CGMPHelperApp();

// Overrides
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
