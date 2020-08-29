#pragma once
#include "pch.h"
#include "GMPHelper2.h"
#include <string>
#include <ctime>
#include <algorithm>
#include <regex>
#include "resource.h"
#include <iostream>
#include <fstream>
#include <filesystem>




#define MY_PLUGIN_NAME      "GMPHelper VATSIM OMAE"
#define MY_PLUGIN_VERSION   "1.0"
#define MY_PLUGIN_DEVELOPER "Nils Dornbusch"
#define MY_PLUGIN_COPYRIGHT "Free to be distributed as source code"
#define MY_PLUGIN_VIEW      ""

const   int     TAG_ITEM_CTOT = 1121312;
const   int     TAG_ITEM_Sequence = 212312;
const   int     TAG_ITEM_TOBT = 312312;
const	int		TAG_ITEM_ROUTE_VALID = 123123;

const   int     TAG_FUNC_CTOT_MANUAL = 1;
const   int     TAG_FUNC_CTOT_ASSIGN = 2;

const   int     TAG_FUNC_CTOT_MANUAL_FINISH = 10;
const   int     TAG_FUNC_CTOT_ASSIGN_SEQ = 11;
const   int     TAG_FUNC_CTOT_ASSIGN_ASAP = 13;
const   int     TAG_FUNC_CTOT_CLEAR = 12341;
const CTimeSpan taxitime = CTimeSpan(0, 0, 20, 0);
RouteData data;



//---CGMPHelper------------------------------------------

CGMPHelper::CGMPHelper(void)
	: CPlugIn(EuroScopePlugIn::COMPATIBILITY_CODE,
		MY_PLUGIN_NAME,
		MY_PLUGIN_VERSION,
		MY_PLUGIN_DEVELOPER,
		MY_PLUGIN_COPYRIGHT)
{
	//Registering our TAG items that can be added to any list in euroscope
	RegisterTagItemType("CTOT", TAG_ITEM_CTOT);
	RegisterTagItemType("T/O sequence", TAG_ITEM_Sequence);
	RegisterTagItemType("TOBT", TAG_ITEM_TOBT);
	RegisterTagItemType("RouteValid", TAG_ITEM_ROUTE_VALID);

	//Registering our two functions the first one opens a popup menu the second one lets the user enter a ctot manually
	RegisterTagItemFunction("Assign CTOT", TAG_FUNC_CTOT_ASSIGN);
	RegisterTagItemFunction("Edit CTOT", TAG_FUNC_CTOT_MANUAL);

	//Registering our list that is displayed in euroscope
	m_TOSequenceList = RegisterFpList("T/O Sequence List");
	if (m_TOSequenceList.GetColumnNumber() == 0)
	{
		// fill in the default columns of the list in none are available from settings
		m_TOSequenceList.AddColumnDefinition("C/S", 10, true, NULL, EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN,
			NULL, EuroScopePlugIn::TAG_ITEM_FUNCTION_NO,
			NULL, EuroScopePlugIn::TAG_ITEM_FUNCTION_NO);
		m_TOSequenceList.AddColumnDefinition("CTOT", 4, true, MY_PLUGIN_NAME, TAG_ITEM_CTOT,
			NULL, EuroScopePlugIn::TAG_ITEM_FUNCTION_NO,
			NULL, EuroScopePlugIn::TAG_ITEM_FUNCTION_NO);
		m_TOSequenceList.AddColumnDefinition("#", 2, true, MY_PLUGIN_NAME, TAG_ITEM_Sequence,
			NULL, EuroScopePlugIn::TAG_ITEM_FUNCTION_NO,
			NULL, EuroScopePlugIn::TAG_ITEM_FUNCTION_NO);
		m_TOSequenceList.AddColumnDefinition("W", 1, true, NULL, EuroScopePlugIn::TAG_ITEM_TYPE_AIRCRAFT_CATEGORY,
			NULL, EuroScopePlugIn::TAG_ITEM_FUNCTION_NO,
			NULL, EuroScopePlugIn::TAG_ITEM_FUNCTION_NO);

	}
	
	
	char path[MAX_PATH];
	HMODULE hm = NULL;

	if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCSTR)&CTOTData::test, &hm) == 0)
	{
		int ret = GetLastError();
		fprintf(stderr, "GetModuleHandle failed, error = %d\n", ret);
		// Return or however you want to handle an error.
	}
	if (GetModuleFileName(hm, path, sizeof(path)) == 0)
	{
		int ret = GetLastError();
		fprintf(stderr, "GetModuleFileName failed, error = %d\n", ret);
		// Return or however you want to handle an error.
	}
	std::string dir(path);
	std::string filename("GMPHelper.dll");
	size_t pos = dir.find(filename);
	dir.replace(pos, filename.length(), "");
	dir += "omaeroutes.csv";
	io::CSVReader<4, io::trim_chars<' '>, io::no_quote_escape<','>> in(dir);
	in.read_header(io::ignore_no_column, "Dest ICAO", "Dest Name", "Level Restrictions", "Full Route");
	std::string Dest, Destname, LevelR, Routing;
	while (in.read_row(Dest, Destname, LevelR, Routing)) {
		data.Routes.push_back(RouteTo(Dest, Destname, LevelR, Routing));
		data.icaos.push_back(Dest);
	}
}




CGMPHelper::~CGMPHelper(void)
{
}

void CGMPHelper::OnGetTagItem(EuroScopePlugIn::CFlightPlan FlightPlan,
	EuroScopePlugIn::CRadarTarget RadarTarget,
	int ItemCode,
	int TagData,
	char sItemString[16],
	int * pColorCode,
	COLORREF * pRGB,
	double * pFontSize)
{
	int     idx;


	// only for flight plans
	if (!FlightPlan.IsValid())
		return;

	// handle the case that our aircraft is not in our -local- sequence yet
	if ((idx = _SelectAcIndex(FlightPlan)) < 0&& ItemCode!=TAG_ITEM_ROUTE_VALID)
	{
		//now we check if another controller assigned a ctot we need the scratchpad string and other data for that
		auto fpdata = FlightPlan.GetFlightPlanData();
		auto remarks = fpdata.GetRemarks();
		const char* test = NULL;

		//check if the remarks contains the phrase /CTOT
		test = strstr(remarks, "/CTOT");
		if (test)
		{
			//if yes then a controller already assigned a ctot and the estimated dep time on the flightplan is the ctot
			auto ctot = fpdata.GetEstimatedDepartureTime();
			CTOTData newone;
			CTime temp;
			temp = CTime::GetCurrentTime();
			tm t, t1;
			temp.GetGmtTm(&t);
			temp.GetLocalTm(&t1);

			//calculating the timezone 
			CTimeSpan diff = CTime(1900 + t.tm_year, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec) - CTime(1900 + t1.tm_year, t1.tm_mon + 1, t1.tm_mday, t1.tm_hour, t1.tm_min, t1.tm_sec);
			
			//casting our string to int
			int input = atoi(ctot);

			//get the minutes (integer divison for the win)
			int test = input - (input / 100) * 100;

			//safeguard for something silly
			if (input >= 2400 || test >= 60) return;

			//construct the new CTOT object and add it to the sequence. We didnt calculate anything here just read the data from the flightplan
			newone.CTOT = CTime(1900 + t.tm_year, t.tm_mon + 1, t.tm_mday, input / 100, input - (input / 100) * 100, 0) - diff;
			newone.flightplan = FlightPlan;
			newone.TOBT = newone.CTOT - taxitime;
			m_sequence.push_back(newone);
			std::sort(m_sequence.begin(), m_sequence.end());
		}
		return;
	}


	// switch the different tag fields
	switch (ItemCode)
	{
	case TAG_ITEM_CTOT:
	{
		//just take the stored CTOT and format it to look nice
		CTime CTOT = m_sequence[idx].CTOT;
		CString temp1;
		tm t;
		CTOT.GetGmtTm(&t);
		temp1.Format("%.4d", t.tm_hour * 100 + t.tm_min);
		strcpy(sItemString, temp1);
		updateList();
		break;
	}

	case TAG_ITEM_TOBT:
	{
		CTime TOBT = m_sequence[idx].TOBT;
		CString temp1;
		tm t;
		TOBT.GetGmtTm(&t);
		temp1.Format("%.4d", t.tm_hour * 100 + t.tm_min);
		strcpy(sItemString, temp1);
		break;
	}
	case TAG_ITEM_Sequence:
	{
		int seq = m_sequence[idx].sequence;
		CString temp3;
		temp3.Format("%d", seq);
		strcpy(sItemString, temp3);
		break;
	}
	case TAG_ITEM_ROUTE_VALID:
	{
		auto fpdata = FlightPlan.GetFlightPlanData();
		auto icao = fpdata.GetDestination();
		auto test = fpdata.GetPlanType();
		if (strcmp(test,"V")==0)
		{
			strcpy(sItemString, "");
			return;
		}
		bool foundRoute = false;
		for (auto temp : data.icaos)
		{
			if (temp == icao)
			{
				foundRoute = true;
				break;
			}

		}
		if (!foundRoute)
		{
			strcpy(sItemString, "?");
			return;
		}
		auto dt = data.getDatafromICAO(icao);
		bool cruisevalid = false;
		bool routevalid = false;
		for (auto d : dt) {
			std::string tmp = fpdata.GetRoute();
			std::regex rule("\\/(.+?)(\\\s+?)");
			tmp = std::regex_replace(tmp, rule, " ");
			cruisevalid = d.isCruiseValid(FlightPlan.GetFinalAltitude());
			bool routevalid = d.isRouteValid(tmp);
			if( cruisevalid && routevalid)
			{
				strcpy(sItemString, "");
				return;
			}
				
		}
		if (cruisevalid) strcpy(sItemString, "R");
		else if (routevalid) strcpy(sItemString, "L");
		else {
			strcpy(sItemString, "X");
		}
		
		*pColorCode = EuroScopePlugIn::TAG_COLOR_EMERGENCY;
	}
	}
}

void CGMPHelper::OnFlightPlanDisconnect(EuroScopePlugIn::CFlightPlan FlightPlan)
{
	//nothing fancy just remove the Flightplan from everything
	int idx = _SelectAcIndex(FlightPlan);
	if (idx < 0) return;
	//the usual remove erase thing in c++
	m_sequence.erase(std::remove(m_sequence.begin(), m_sequence.end(), m_sequence.at(idx)), m_sequence.end());
	m_TOSequenceList.RemoveFpFromTheList(FlightPlan);
	updateList();
	recalculateCTOT(*(m_sequence.begin()));
}
inline  bool CGMPHelper::OnCompileCommand(const char * sCommandLine)
{
	//show the t/o sequence list if we type the command
	if (std::strcmp(sCommandLine, ".showtolist") == 0)
	{
		m_TOSequenceList.ShowFpList(true);
		return true;
	}
	return false;
}

int     CGMPHelper::_SelectAcIndex(EuroScopePlugIn::CFlightPlan flightplan)
{
	// helper function to search our sequence for "flightplan"
	int i = 0;
	CString temp2 = flightplan.GetCallsign();
	if (m_sequence.empty())
	{
		return -1;
	}
	for (CTOTData test : m_sequence)
	{
		try {
			CString temp1 = test.flightplan.GetCallsign();
			if (temp1.Compare(temp2) == 0)
			{
				return  i;
			}
			i++;
		}
		catch (const std::exception &e) { continue; }
	}
	return -1;
}
void    CGMPHelper::OnFunctionCall(int FunctionId,
	const char * sItemString,
	POINT Pt,
	RECT Area)
{
	//handle our registered functions
	EuroScopePlugIn::CFlightPlan  fp;
	int                             i;
	CString                         str;


	// get the flightplan we are dealing with
	fp = FlightPlanSelectASEL();
	if (!fp.IsValid())
		return;

	// select it from the sequence
	int idx = _SelectAcIndex(fp);


	// switch by the function ID
	switch (FunctionId)
	{
	case TAG_FUNC_CTOT_MANUAL: // TAG function
	{

		// open a popup editor there
		OpenPopupEdit(Area,
			TAG_FUNC_CTOT_MANUAL_FINISH,
			"");
		break;
	}
	case TAG_FUNC_CTOT_MANUAL_FINISH: // when finished editing
	{
		//same thing as in getTagItem
		CTime temp;
		temp = CTime::GetCurrentTime();
		tm t, t1;
		temp.GetGmtTm(&t);
		temp.GetLocalTm(&t1);
		CTimeSpan diff = CTime(1900 + t.tm_year, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec) - CTime(1900 + t1.tm_year, t1.tm_mon + 1, t1.tm_mday, t1.tm_hour, t1.tm_min, t1.tm_sec);
		//as our ctot we use the manually entered 4 digits
		int input = atoi(sItemString);
		int test = input - (input / 100) * 100;
		if (input >= 2400 || test >= 60) return;
		auto data = fp.GetFlightPlanData();
		auto remarks = data.GetRemarks();
		const char* test1 = NULL;
		std::string temp2 = remarks;
		test1 = strstr(remarks, "/CTOT");
		//if /CTOT is not present in the remarks we append it to it
		if (!test1)
		{
			std::string temp2;
			temp2 = remarks;
			temp2 += "/CTOT";
			data.SetRemarks(temp2.c_str());
			data.AmendFlightPlan();
		}
		// if the aircraft was in the sequence already we modify its CTOTData otherwise we make a new one
		if (idx >= 0)
		{
			try {
				m_sequence[idx].CTOT = CTime(1900 + t.tm_year, t.tm_mon + 1, t.tm_mday, input / 100, input - (input / 100) * 100, 0) - diff;
				m_sequence[idx].TOBT = m_sequence[idx].CTOT - taxitime;
				m_sequence[idx].manual = true;
				std::sort(m_sequence.begin(), m_sequence.end());

			}
			catch (...)
			{
			}
		}
		else {
			CTOTData temp1;
			temp1.flightplan = fp;
			try {
				temp1.CTOT = CTime(1900 + t.tm_year, t.tm_mon + 1, t.tm_mday, input / 100, input - (input / 100) * 100, 0) - diff;
				temp1.TOBT = temp1.CTOT - taxitime;
				temp1.manual = true;
				m_sequence.push_back(temp1);
				std::sort(m_sequence.begin(), m_sequence.end());
			}
			catch (...)
			{
			}
		}
		// at last we modify the estimated departure time in the flightplan so other controller's plugin can parse the value themselves
		idx = _SelectAcIndex(fp);
		auto fpdata = fp.GetFlightPlanData();
		tm t2;
		m_sequence[idx].CTOT.GetGmtTm(&t2);
		CString cstring;
		cstring.Format("%.4d", t2.tm_hour * 100 + t2.tm_min);
		fpdata.SetEstimatedDepartureTime(cstring);
		fpdata.AmendFlightPlan();
		//we recalculate the CTOT for all aircraft in sequence after the manually assigned one
		break;
	}
	case TAG_FUNC_CTOT_ASSIGN: // TAG function

		// start a popup list
		OpenPopupList(Area, "Assign CTOT", 1);


		AddPopupListElement("Assign in sequence", "", TAG_FUNC_CTOT_ASSIGN_SEQ);
		AddPopupListElement("Assign ASAP", "", TAG_FUNC_CTOT_ASSIGN_ASAP);


		// finally add a fixed element to clear the ctot

		AddPopupListElement("Clear", "",
			TAG_FUNC_CTOT_CLEAR);
		break;

	case TAG_FUNC_CTOT_ASSIGN_SEQ: // user selected assign in sequence
	{
		//dont assign if we already have assigned one. Safeguards against missclicks
		if (idx >= 0) return;
		// get the scratchpad data and modify it
		auto data = fp.GetFlightPlanData();
		auto remarks = data.GetRemarks();
		const char* test = NULL;
		std::string temp = remarks;
		test = strstr(remarks, "/CTOT");
		if (!test)
		{
			std::string temp;
			temp = remarks;
			temp += "/CTOT";
			data.SetRemarks(temp.c_str());
			data.AmendFlightPlan();
		}
		CGMPHelper::assignCTOT(false, fp);

		//again modify the estimated departure time in the flightplan
		idx = _SelectAcIndex(fp);
		auto fpdata = fp.GetFlightPlanData();
		tm t;
		m_sequence[idx].CTOT.GetGmtTm(&t);
		CString cstring;
		cstring.Format("%.4d", t.tm_hour * 100 + t.tm_min);
		fpdata.SetEstimatedDepartureTime(cstring);
		fpdata.AmendFlightPlan();
		break;
	}
	case TAG_FUNC_CTOT_ASSIGN_ASAP: // user selected asap
	{
		//all is the same but we call assignCTOT with the asap flag
		if (idx >= 0) return;
		auto data = fp.GetFlightPlanData();
		auto remarks = data.GetRemarks();
		const char* test = NULL;
		std::string temp = remarks;
		test = strstr(remarks, "/CTOT");
		if (!test)
		{
			std::string temp;
			temp = remarks;
			temp += "/CTOT";
			data.SetRemarks(temp.c_str());
			data.AmendFlightPlan();
		}
		CGMPHelper::assignCTOT(true, fp);
		idx = _SelectAcIndex(fp);
		auto fpdata = fp.GetFlightPlanData();
		tm t;
		m_sequence[idx].CTOT.GetGmtTm(&t);
		CString cstring;
		cstring.Format("%.4d", t.tm_hour * 100 + t.tm_min);
		fpdata.SetEstimatedDepartureTime(cstring);
		fpdata.AmendFlightPlan();
		break;
	}
	case TAG_FUNC_CTOT_CLEAR: // clear the ctot

		// simply clear
		if (idx < 0) break;
		auto cadata = fp.GetFlightPlanData();
		auto remarks = cadata.GetRemarks();
		const char* test = NULL;
		std::string temp = remarks;
		test = strstr(remarks, "/CTOT");
		if (test)
		{
			//find and replace /CTOT from scratchpad
			temp = std::regex_replace(temp, std::regex("\\/CTOT"), "");
			cadata.SetRemarks(temp.c_str());
			cadata.AmendFlightPlan();
		}
		m_sequence.erase(std::remove(m_sequence.begin(), m_sequence.end(), m_sequence.at(_SelectAcIndex(fp))), m_sequence.end());
		m_TOSequenceList.RemoveFpFromTheList(fp);
		updateList();
		if (m_sequence.size() == idx) break;
		if (idx == 0) recalculateCTOT(*m_sequence.begin());
		else recalculateCTOT(m_sequence[idx-1]);
		break;

	}// switch by the function ID
}

void CGMPHelper::assignCTOT(bool asap, EuroScopePlugIn::CFlightPlan flightplan)
{
	CTime time = CTime::GetCurrentTime();
	CTime ctot;
	CTime tobt;
	CTOTData temp;
	//if there is none in sequence or we want asap we dont care about separation
	if (m_sequence.empty() || asap)
	{

		temp.flightplan = flightplan;
		temp.sequence = -1;
		ctot = time + CTimeSpan(0, 0, 25, 0);
		tobt = ctot - taxitime;
		temp.CTOT = ctot;
		temp.TOBT = tobt;

	}
	else
	{
		//get the last aircraft in sequence
		CTOTData &end = *(m_sequence.end() - 1);
		CTimeSpan increment = getIncrement(end.flightplan, flightplan);
		temp.flightplan = flightplan;
		temp.sequence = -1;
		//assign at earliest 25 minutes after now 
		ctot = max(time + CTimeSpan(0, 0, 25, 0), end.CTOT + increment);
		tobt = ctot - taxitime;
		temp.CTOT = ctot;
		temp.TOBT = tobt;


	}

	m_sequence.push_back(temp);
	std::sort(m_sequence.begin(), m_sequence.end());
	if (asap)
	{
		if(int ind= _SelectAcIndex(temp.flightplan)>0) recalculateCTOT(m_sequence.at(ind-1));
		else recalculateCTOT(*(m_sequence.begin()));
		
	}

}
void CGMPHelper::updateList()
{
	//some housekeeping 
	for (CTOTData i : m_sequence)
	{
		//add all aircraft in sequence to the euroscope list
		m_TOSequenceList.AddFpToTheList(i.flightplan);
	}
	for (CTOTData i : m_sequence)
	{
		//if any aircraft takes off remove it automatically
		auto fp = i.flightplan;
		EuroScopePlugIn::CRadarTarget rt = fp.GetCorrelatedRadarTarget();
		auto cad = fp.GetFlightPlanData();
		auto remarks = cad.GetRemarks();
		const char* test = NULL;
		test = strstr(remarks, "/CTOT");
		if (rt.GetGS() > 80||!test)
		{
			m_TOSequenceList.RemoveFpFromTheList(fp);
			m_sequence.erase(std::remove(m_sequence.begin(), m_sequence.end(), m_sequence.at(_SelectAcIndex(fp))), m_sequence.end());
		}
	}
	std::sort(m_sequence.begin(), m_sequence.end());
	for (CTOTData &i : m_sequence)
	{
		//get the number in sequence for everyone
		i.sequence = _SelectAcIndex(i.flightplan) + 1;
	}
}

void CGMPHelper::recalculateCTOT(CTOTData inserted)
//recalculates the CTOT for each flightplan in sequence following and inculding "inserted"
{
	//get the number in sequence for our inserted fp
	auto pos = std::find(m_sequence.begin(), m_sequence.end(), inserted);
	//safeguard if not found due to whatever 
	if (pos == m_sequence.end())
	{
		return;
	}
	//iterate over the sequence starting from "inserted"
	for (auto &it = pos; it != m_sequence.end()-1; it++)
	{
		CTOTData temp1 = *it ;
		CString test = temp1.flightplan.GetCallsign();
		CTOTData &temp2 = *(it+1);
		CString test2 = temp2.flightplan.GetCallsign();
		if (temp2.manual) continue;
		CTimeSpan inc = getIncrement(temp1.flightplan, temp2.flightplan);
		temp2.CTOT = temp1.CTOT + inc;
		temp2.TOBT = temp2.CTOT - taxitime;
	}

}
CTimeSpan CGMPHelper::getIncrement(EuroScopePlugIn::CFlightPlan fp1, EuroScopePlugIn::CFlightPlan fp2)
//returns the required difference in CTOT when fp2 is trailing fp1
{
	CTimeSpan increment;
	CString sid1 = fp1.GetFlightPlanData().GetSidName();
	CString sid2 = fp2.GetFlightPlanData().GetSidName();
	char wtc1 = fp1.GetFlightPlanData().GetAircraftWtc();
	char wtc2 = fp2.GetFlightPlanData().GetAircraftWtc();
	switch (wtc1)
	{
	case 'J':
	{
		if (wtc2 == 'H')
		{
			increment = CTimeSpan(0, 0, 2, 0);
		}
		if (wtc2 == 'M' || wtc2 == 'L')
		{
			increment = CTimeSpan(0, 0, 3, 0);
		}
		break;
	}
	case 'H':
	{
		if (wtc2 == 'M' || wtc2 == 'L')
		{
			increment = CTimeSpan(0, 0, 2, 0);
		}
		else {

		}
		break;
	}
	case 'M':
	{
		if (wtc2 == 'L')
		{
			increment = CTimeSpan(0, 0, 2, 0);
		}
		break;
	}
	}
	if (increment == NULL)
	{
		if (sid1 != sid2)
		{
			//RRSM
			increment = CTimeSpan(0, 0, 1, 0);
		}
		else
			increment = CTimeSpan(0, 0, 2, 0);
	}
	return increment;
}






