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
#include "loguru.cpp"
#include <unordered_map>





#define MY_PLUGIN_NAME      "GMPHelper VATSIM OMAE"
#define MY_PLUGIN_VERSION   "1.0"
#define MY_PLUGIN_DEVELOPER "Nils Dornbusch"
#define MY_PLUGIN_COPYRIGHT "Free to be distributed as source code"
#define MY_PLUGIN_VIEW      ""

//const   int     TAG_ITEM_CTOT = 1121312;
//const   int     TAG_ITEM_Sequence = 212312;
//const   int     TAG_ITEM_TOBT = 312312;
const	int		TAG_ITEM_ROUTE_VALID = 123123;

//const   int     TAG_FUNC_CTOT_MANUAL = 1;
//const   int     TAG_FUNC_CTOT_ASSIGN = 2;


//const   int     TAG_FUNC_CTOT_MANUAL_FINISH = 10;
//const   int     TAG_FUNC_CTOT_ASSIGN_SEQ = 11;
//const   int     TAG_FUNC_CTOT_ASSIGN_ASAP = 13;
const   int     TAG_FUNC_ROUTING = 15;
//const   int     TAG_FUNC_CTOT_CLEAR = 12341;

const CTimeSpan taxitime = CTimeSpan(0, 0, 20, 0);
std::unordered_map<std::string, RouteData> data;



//---CGMPHelper------------------------------------------

CGMPHelper::CGMPHelper(void)
	: CPlugIn(EuroScopePlugIn::COMPATIBILITY_CODE,
		MY_PLUGIN_NAME,
		MY_PLUGIN_VERSION,
		MY_PLUGIN_DEVELOPER,
		MY_PLUGIN_COPYRIGHT)
{
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
	loguru::add_file("GMPHelper.log", loguru::Append, loguru::Verbosity_INFO);
	char logpath[1024];
	loguru::set_thread_name("GMPHelper");
	loguru::suggest_log_path(dir.c_str(), logpath, sizeof(logpath));
	loguru::add_file(logpath, loguru::FileMode::Truncate, loguru::Verbosity_INFO);
	LOG_F(INFO, "We successfully started GMPHelper. Great Success!");
	
	//Registering our TAG items that can be added to any list in euroscope
    //RegisterTagItemType("CTOT", TAG_ITEM_CTOT);
	//RegisterTagItemType("T/O sequence", TAG_ITEM_Sequence);
	//RegisterTagItemType("TOBT", TAG_ITEM_TOBT);
	RegisterTagItemType("RouteValid", TAG_ITEM_ROUTE_VALID);

	//Registering our two functions the first one opens a popup menu the second one lets the user enter a ctot manually

	//RegisterTagItemFunction("Assign CTOT", TAG_FUNC_CTOT_ASSIGN);
	//RegisterTagItemFunction("Edit CTOT", TAG_FUNC_CTOT_MANUAL);
	RegisterTagItemFunction("Get routes", TAG_FUNC_ROUTING);


	//Registering our list that is displayed in euroscope
	//m_TOSequenceList = RegisterFpList("T/O Sequence List");
	//if (m_TOSequenceList.GetColumnNumber() == 0)
	//{
	//	// fill in the default columns of the list in none are available from settings
	//	m_TOSequenceList.AddColumnDefinition("C/S", 10, true, NULL, EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN,
	//		NULL, EuroScopePlugIn::TAG_ITEM_FUNCTION_NO,
	//		NULL, EuroScopePlugIn::TAG_ITEM_FUNCTION_NO);
	//	m_TOSequenceList.AddColumnDefinition("CTOT", 4, true, MY_PLUGIN_NAME, TAG_ITEM_CTOT,
	//		NULL, EuroScopePlugIn::TAG_ITEM_FUNCTION_NO,
	//		NULL, EuroScopePlugIn::TAG_ITEM_FUNCTION_NO);
	//	m_TOSequenceList.AddColumnDefinition("#", 2, true, MY_PLUGIN_NAME, TAG_ITEM_Sequence,
	//		NULL, EuroScopePlugIn::TAG_ITEM_FUNCTION_NO,
	//		NULL, EuroScopePlugIn::TAG_ITEM_FUNCTION_NO);
	//	m_TOSequenceList.AddColumnDefinition("W", 1, true, NULL, EuroScopePlugIn::TAG_ITEM_TYPE_AIRCRAFT_CATEGORY,
	//		NULL, EuroScopePlugIn::TAG_ITEM_FUNCTION_NO,
	//		NULL, EuroScopePlugIn::TAG_ITEM_FUNCTION_NO);

	//}
	
    LOG_F(INFO, "Everything registered. Ready to go!");
	
	
	dir += "RouteChecker.csv";
	io::CSVReader<5, io::trim_chars<' '>, io::no_quote_escape<','>> in(dir);
	in.read_header(io::ignore_extra_column, "Dep", "Dest","Evenodd", "Restriction", "Route");
	std::string Dep, Dest, evenodd, LevelR, Routing;
	while (in.read_row(Dep, Dest, evenodd, LevelR, Routing)) 
	{
		auto temp = RouteTo(Dep, Dest, evenodd, LevelR, Routing);
		auto depicao = temp.mDEPICAO;
		RouteData dt;
		std::pair<std::string, RouteData> mypair (depicao, dt);
		data.insert(mypair);
		data.at(depicao).Routes.push_back(temp);
		data.at(depicao).icaos.push_back(Dest);
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
	auto fpdata = FlightPlan.GetFlightPlanData();
	std::string dep = fpdata.GetOrigin();
	// handle the case that our aircraft is not in our -local- sequence yet

	//if ((idx = _SelectAcIndex(FlightPlan)) < 0&& ItemCode!=TAG_ITEM_ROUTE_VALID)
	//{
	//	//now we check if another controller assigned a ctot we need the scratchpad string and other data for that
	//	
	//	auto remarks = fpdata.GetRemarks();
	//	const char* test = NULL;

	//	//check if the remarks contains the phrase /CTOT
	//	test = strstr(remarks, "/CTOT");
	//	
	//	if (test)
	//	{
	//		CTime temp;
	//		temp = CTime::GetCurrentTime();
	//		tm t, t1;
	//		temp.GetGmtTm(&t);
	//		temp.GetLocalTm(&t1);
	//		std::string callsign = FlightPlan.GetCallsign();
	//		std::string logstring = "We already found a valid CTOT for " + callsign;
	//		LOG_F(INFO, logstring.c_str());
	//		//if yes then a controller already assigned a ctot and the estimated dep time on the flightplan is the ctot
	//		auto ctot = fpdata.GetEstimatedDepartureTime();
	//		CTOTData newone;
	//		

	//		//calculating the timezone 
	//		CTimeSpan diff = CTime(1900 + t.tm_year, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec) - CTime(1900 + t1.tm_year, t1.tm_mon + 1, t1.tm_mday, t1.tm_hour, t1.tm_min, t1.tm_sec);
	//		
	//		//casting our string to int
	//		int input = atoi(ctot);

	//		//get the minutes (integer divison for the win)
	//		int test = input - (input / 100) * 100;

	//		//safeguard for something silly
	//		if (input >= 2400 || test >= 60) return;

	//		//construct the new CTOT object and add it to the sequence. We didnt calculate anything here just read the data from the flightplan
	//		newone.CTOT = CTime(1900 + t.tm_year, t.tm_mon + 1, t.tm_mday, input / 100, input - (input / 100) * 100, 0) - diff;
	//		newone.flightplan = FlightPlan;
	//		newone.TOBT = newone.CTOT - taxitime;
	//		if (dep == "OMDB")
	//		{
	//			m_sequence_OMDB.push_back(newone);
	//			std::sort(m_sequence_OMDB.begin(), m_sequence_OMDB.end());
	//		}
	//		if (dep == "OMSJ")
	//		{
	//			m_sequence_OMSJ.push_back(newone);
	//			std::sort(m_sequence_OMSJ.begin(), m_sequence_OMSJ.end());
	//		}
	//		if (dep == "OMDW")
	//		{
	//			m_sequence_OMDW.push_back(newone);
	//			std::sort(m_sequence_OMDW.begin(), m_sequence_OMDW.end());
	//		}
	//		if (dep == "OMAA")
	//		{
	//			m_sequence_OMDB.push_back(newone);
	//			std::sort(m_sequence_OMDB.begin(), m_sequence_OMDB.end());
	//		}
	//		
	//		
	//	}
	//	return;
	//}

	// switch the different tag fields
	switch (ItemCode)
	{
	//case TAG_ITEM_CTOT:
	//{
	//	CTime CTOT;
	//	//just take the stored CTOT and format it to look nice
	//	if(dep == "OMDB")
	//		CTOT = m_sequence_OMDB[idx].CTOT;
	//	if (dep == "OMSJ")
	//		CTOT = m_sequence_OMSJ[idx].CTOT;
	//	if (dep == "OMDW")
	//		CTOT = m_sequence_OMDW[idx].CTOT;
	//	if (dep == "OMAA")
	//		CTOT = m_sequence_OMAA[idx].CTOT;
	//	CString temp1;
	//	tm t;
	//	CTOT.GetGmtTm(&t);
	//	temp1.Format("%.4d", t.tm_hour * 100 + t.tm_min);
	//	strcpy(sItemString, temp1);
	//	updateListOMDB();
	//	updateListOMSJ();
	//	updateListOMDW();
	//	updateListOMAA();
	//	break;
	//}

	/*case TAG_ITEM_TOBT:
	{
		CTime TOBT;
		if (dep == "OMDB")
			TOBT = m_sequence_OMDB[idx].TOBT;
		if (dep == "OMSJ")
			TOBT = m_sequence_OMSJ[idx].TOBT;
		if (dep == "OMDW")
			TOBT = m_sequence_OMDW[idx].TOBT;
		if (dep == "OMAA")
			TOBT = m_sequence_OMAA[idx].TOBT;
		CTime curr;
		curr = CTime::GetCurrentTime();
		tm currt, currt1;
		curr.GetGmtTm(&currt);
		CString temp1;
		tm t;
		TOBT.GetGmtTm(&t);
		temp1.Format("%.4d", t.tm_hour * 100 + t.tm_min);
		strcpy(sItemString, temp1);
		std::string status = FlightPlan.GetGroundState();
		if (TOBT - curr < CTimeSpan(0, 0, 5, 0) && status.empty())
		{
			if (TOBT + CTimeSpan(0,0,5,0) > curr)
			{
				*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;
				*pRGB = RGB(255, 191, 0);
			}
			else
				*pColorCode = EuroScopePlugIn::TAG_COLOR_EMERGENCY;
		}

		break;
	}
	case TAG_ITEM_Sequence:
	{

		int seq = 0;
		if (dep == "OMDB")
			seq = m_sequence_OMDB[idx].sequence;
		if (dep == "OMSJ")
			seq = m_sequence_OMSJ[idx].sequence;
		if (dep == "OMDW")
			seq = m_sequence_OMDW[idx].sequence;
		if (dep == "OMAA")
			seq = m_sequence_OMAA[idx].sequence;
		CString temp3;
		temp3.Format("%d", seq);
		strcpy(sItemString, temp3);
		break;
	}*/
	case TAG_ITEM_ROUTE_VALID:
	{
		auto fpdata = FlightPlan.GetFlightPlanData();
		std::string icaodep = fpdata.GetOrigin();
		if (data.find(icaodep) == data.end())
		{
			strcpy(sItemString, "?");
			return;
		}
		std::string icaodest = fpdata.GetDestination();
		auto test = fpdata.GetPlanType();
		if (strcmp(test,"V")==0)
		{
			strcpy(sItemString, "");
			return;
		}
		bool foundRoute = false;
		for (auto temp : data.at(icaodep).icaos)
		{
			if (temp == icaodest)
			{
				std::string logstring = "Found a designated route to " + icaodest + " for " + FlightPlan.GetCallsign();
				LOG_F(INFO, logstring.c_str());

				foundRoute = true;
				break;
			}

		}
		if (!foundRoute)
		{
			for (auto temp : data.at(icaodep).icaos)
			{
				if (temp == icaodest.substr(0, 2))
				{

					icaodest = icaodest.substr(0, 2);
					std::string logstring = "Using dummy route to " + icaodest + " for " + FlightPlan.GetCallsign();
					LOG_F(INFO, logstring.c_str());

					icaodest = icaodest.substr(0, 2);

					foundRoute = true;
					break;
				}
			}
			if (!foundRoute)
			{
				for (auto temp : data.at(icaodep).icaos)
				{
					if (temp == icaodest.substr(0, 1))
					{
						icaodest = icaodest.substr(0, 1);

						foundRoute = true;
						break;
					}
				}
			}
			if (!foundRoute)
			{
				strcpy(sItemString, "?");
				std::string logstring = "No (dummy) route to " + icaodest + " found for " + FlightPlan.GetCallsign();
				LOG_F(INFO, logstring.c_str());
				return;
			}
		}
		auto dt = data.at(icaodep).getDatafromICAO(icaodest);
		bool cruisevalid = false;
		bool routevalid = false;
		for (auto d : dt) {
			std::string tmp = fpdata.GetRoute();
			std::regex rule("\\/(.+?)(\\\s+?)");
			tmp = std::regex_replace(tmp, rule, " ");
			cruisevalid = d.isCruiseValid(FlightPlan.GetFinalAltitude());
			routevalid = d.isRouteValid(tmp);
			if( cruisevalid && routevalid )
			{
				strcpy(sItemString, "");
				return;
			}	
		}
		
		if (cruisevalid && !routevalid) strcpy(sItemString, "R");
		else if (routevalid && !cruisevalid) strcpy(sItemString, "L");
		else {
			strcpy(sItemString, "X");
		}
		
		*pColorCode = EuroScopePlugIn::TAG_COLOR_EMERGENCY;
	}
	}
}

//void CGMPHelper::OnFlightPlanDisconnect(EuroScopePlugIn::CFlightPlan FlightPlan)
//{
//	//nothing fancy just remove the Flightplan from everything
//	
//	auto fpdata = FlightPlan.GetFlightPlanData();
//	CHECK_F(fpdata.GetOrigin() ==NULL);
//	//std::string logstring = "Disconnection found for " + *FlightPlan.GetCallsign();
//
//	int idx = _SelectAcIndex(FlightPlan);
//	if (idx < 0) return;
//	if (!FlightPlan.IsValid())
//		return;
//
//	auto fpdata = FlightPlan.GetFlightPlanData();
//
//	std::string dep = fpdata.GetOrigin();
//	//the usual remove erase thing in c++
//	try {
//		if (m_sequence_OMDB.at(idx).flightplan.GetCallsign() == FlightPlan.GetCallsign())
//			m_sequence_OMDB.erase(std::remove(m_sequence_OMDB.begin(), m_sequence_OMDB.end(), m_sequence_OMDB.at(idx)), m_sequence_OMDB.end());
//	}
//	catch (...) {}
//	try {
//		if (m_sequence_OMSJ.at(idx).flightplan.GetCallsign() == FlightPlan.GetCallsign())
//			m_sequence_OMSJ.erase(std::remove(m_sequence_OMSJ.begin(), m_sequence_OMSJ.end(), m_sequence_OMSJ.at(idx)), m_sequence_OMSJ.end());
//	}
//	catch (...) {}
//	try {
//		if (m_sequence_OMDW.at(idx).flightplan.GetCallsign() == FlightPlan.GetCallsign())
//			m_sequence_OMDW.erase(std::remove(m_sequence_OMDW.begin(), m_sequence_OMDW.end(), m_sequence_OMDW.at(idx)), m_sequence_OMDW.end());
//	}
//	catch (...) {}
//	try {
//		if (m_sequence_OMAA.at(idx).flightplan.GetCallsign() == FlightPlan.GetCallsign())
//			m_sequence_OMAA.erase(std::remove(m_sequence_OMAA.begin(), m_sequence_OMAA.end(), m_sequence_OMAA.at(idx)), m_sequence_OMAA.end());
//	}
//	catch (...) {}
//	m_TOSequenceList.RemoveFpFromTheList(FlightPlan);
//	updateListOMDB();
//	updateListOMSJ();
//	updateListOMDW();
//	updateListOMAA();
//	recalculateCTOT(*(m_sequence_OMDB.begin()));
//	recalculateCTOT(*(m_sequence_OMSJ.begin()));
//	recalculateCTOT(*(m_sequence_OMDW.begin()));
//	recalculateCTOT(*(m_sequence_OMAA.begin()));
//}
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
	
	if (m_sequence_OMDB.empty() && m_sequence_OMSJ.empty() && m_sequence_OMDW.empty() && m_sequence_OMAA.empty())
	{
		return -1;
	}
	for (CTOTData test : m_sequence_OMDB)
	{
		try {
			CString temp1 = test.flightplan.GetCallsign();
			if (temp1.Compare(temp2) == 0)
			{
				return  i;
			}
			i++;
		}
		catch (const std::exception) { continue; }
	}
	i = 0 ;
	for (CTOTData test : m_sequence_OMSJ)
	{
		try {
			CString temp1 = test.flightplan.GetCallsign();
			if (temp1.Compare(temp2) == 0)
			{
				return  i;
			}
			i++;
		}
		catch (const std::exception) { continue; }
	}
	i = 0;
	for (CTOTData test : m_sequence_OMDW)
	{
		try {
			CString temp1 = test.flightplan.GetCallsign();
			if (temp1.Compare(temp2) == 0)
			{
				return  i;
			}
			i++;
		}
		catch (const std::exception) { continue; }
	}
	i = 0;
	for (CTOTData test : m_sequence_OMAA)
	{
		try {
			CString temp1 = test.flightplan.GetCallsign();
			if (temp1.Compare(temp2) == 0)
			{
				return  i;
			}
			i++;
		}
		catch (const std::exception) { continue; }
	}
	return -1;
}


inline void CGMPHelper::OnFunctionCall(int FunctionId,	const char * sItemString,	POINT Pt,RECT Area)
{
	//handle our registered functions
	EuroScopePlugIn::CFlightPlan  fp;
	CString                         str;


	// get the flightplan we are dealing with
	fp = FlightPlanSelectASEL();
	if (!fp.IsValid())
		return;

	// select it from the sequence
	int idx = _SelectAcIndex(fp);
	auto fpdata = fp.GetFlightPlanData();
	std::string dep = fpdata.GetOrigin();

	// switch by the function ID
	switch (FunctionId)
	{
	// case TAG_FUNC_CTOT_MANUAL: // TAG function
	// {

		// // open a popup editor there
		// OpenPopupEdit(Area,
			// TAG_FUNC_CTOT_MANUAL_FINISH,
			// "");
		// break;
	// }
	// case TAG_FUNC_CTOT_MANUAL_FINISH: // when finished editing
	// {
		// //same thing as in getTagItem
		// CTime temp;
		// temp = CTime::GetCurrentTime();
		// tm t, t1;
		// temp.GetGmtTm(&t);
		// temp.GetLocalTm(&t1);
		// CTimeSpan diff = CTime(1900 + t.tm_year, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec) - CTime(1900 + t1.tm_year, t1.tm_mon + 1, t1.tm_mday, t1.tm_hour, t1.tm_min, t1.tm_sec);
		// //as our ctot we use the manually entered 4 digits
		// int input = atoi(sItemString);
		// int test = input - (input / 100) * 100;
		// if (input >= 2400 || test >= 60) return;
		// auto data = fp.GetFlightPlanData();
		// auto remarks = data.GetRemarks();
		// const char* test1 = NULL;
		// std::string temp2 = remarks;
		// test1 = strstr(remarks, "/CTOT");
		// //if /CTOT is not present in the remarks we append it to it
		// if (!test1)
		// {
			// std::string temp2;
			// temp2 = remarks;
			// temp2 += "/CTOT";
			// data.SetRemarks(temp2.c_str());
			// data.AmendFlightPlan();
		// }
		// // if the aircraft was in the sequence already we modify its CTOTData otherwise we make a new one
		// if (idx >= 0)
		// {
			// if (dep == "OMDB")
			// {
				// try {
					// m_sequence_OMDB[idx].CTOT = CTime(1900 + t.tm_year, t.tm_mon + 1, t.tm_mday, input / 100, input - (input / 100) * 100, 0) - diff;
					// m_sequence_OMDB[idx].TOBT = m_sequence_OMDB[idx].CTOT - taxitime;
					// m_sequence_OMDB[idx].manual = true;
					// std::sort(m_sequence_OMDB.begin(), m_sequence_OMDB.end());

				// }
				// catch (...)
				// {
				// }
			// }
			// if (dep == "OMSJ")
			// {
				// try {
					// m_sequence_OMSJ[idx].CTOT = CTime(1900 + t.tm_year, t.tm_mon + 1, t.tm_mday, input / 100, input - (input / 100) * 100, 0) - diff;
					// m_sequence_OMSJ[idx].TOBT = m_sequence_OMSJ[idx].CTOT - taxitime;
					// m_sequence_OMSJ[idx].manual = true;
					// std::sort(m_sequence_OMSJ.begin(), m_sequence_OMSJ.end());

				// }
				// catch (...)
				// {
				// }
			// }
			// if (dep == "OMDW")
			// {
				// try {
					// m_sequence_OMDW[idx].CTOT = CTime(1900 + t.tm_year, t.tm_mon + 1, t.tm_mday, input / 100, input - (input / 100) * 100, 0) - diff;
					// m_sequence_OMDW[idx].TOBT = m_sequence_OMDW[idx].CTOT - taxitime;
					// m_sequence_OMDW[idx].manual = true;
					// std::sort(m_sequence_OMDW.begin(), m_sequence_OMDW.end());

				// }
				// catch (...)
				// {
				// }
			// }
			// if (dep == "OMAA")
			// {
				// try {
					// m_sequence_OMAA[idx].CTOT = CTime(1900 + t.tm_year, t.tm_mon + 1, t.tm_mday, input / 100, input - (input / 100) * 100, 0) - diff;
					// m_sequence_OMAA[idx].TOBT = m_sequence_OMAA[idx].CTOT - taxitime;
					// m_sequence_OMAA[idx].manual = true;
					// std::sort(m_sequence_OMAA.begin(), m_sequence_OMAA.end());

				// }
				// catch (...)
				// {
				// }
			// }
		// }
		// else {
			// CTOTData temp1;
			// temp1.flightplan = fp;
			// try {
				// temp1.CTOT = CTime(1900 + t.tm_year, t.tm_mon + 1, t.tm_mday, input / 100, input - (input / 100) * 100, 0) - diff;
				// temp1.TOBT = temp1.CTOT - taxitime;
				// temp1.manual = true;
				// if (dep == "OMDB")
				// {
					// m_sequence_OMDB.push_back(temp1);
					// std::sort(m_sequence_OMDB.begin(), m_sequence_OMDB.end());
				// }
				// if (dep == "OMSJ")
				// {
					// m_sequence_OMSJ.push_back(temp1);
					// std::sort(m_sequence_OMSJ.begin(), m_sequence_OMSJ.end());
				// }
				// if (dep == "OMDW")
				// {
					// m_sequence_OMDW.push_back(temp1);
					// std::sort(m_sequence_OMDW.begin(), m_sequence_OMDW.end());
				// }
				// if (dep == "OMAA")
				// {
					// m_sequence_OMAA.push_back(temp1);
					// std::sort(m_sequence_OMAA.begin(), m_sequence_OMAA.end());
				// }
			// }
			// catch (...)
			// {
			// }
		// }
		// // at last we modify the estimated departure time in the flightplan so other controller's plugin can parse the value themselves
		// idx = _SelectAcIndex(fp);
		// auto fpdata = fp.GetFlightPlanData();
		// tm t2 = {0};
		// if (dep == "OMDB")
			// m_sequence_OMDB[idx].CTOT.GetGmtTm(&t2);
		// if (dep == "OMSJ")
			// m_sequence_OMSJ[idx].CTOT.GetGmtTm(&t2);
		// if (dep == "OMDW")
			// m_sequence_OMDW[idx].CTOT.GetGmtTm(&t2);
		// if (dep == "OMAA")
			// m_sequence_OMAA[idx].CTOT.GetGmtTm(&t2);
		// CString cstring;
		// cstring.Format("%.4d", t2.tm_hour * 100 + t2.tm_min);
		// fpdata.SetEstimatedDepartureTime(cstring);
		// fpdata.AmendFlightPlan();
		// //we recalculate the CTOT for all aircraft in sequence after the manually assigned one
		// break;
	// }
	// case TAG_FUNC_CTOT_ASSIGN: // TAG function
	// {
		// // start a popup list
		// OpenPopupList(Area, "Assign CTOT", 1);


		// AddPopupListElement("Assign in sequence", "", TAG_FUNC_CTOT_ASSIGN_SEQ);
		// AddPopupListElement("Assign ASAP", "", TAG_FUNC_CTOT_ASSIGN_ASAP);


		// // finally add a fixed element to clear the ctot

		// AddPopupListElement("Clear", "",
			// TAG_FUNC_CTOT_CLEAR);
		// break;
	// }
	// case TAG_FUNC_CTOT_ASSIGN_SEQ: // user selected assign in sequence
	// {
		// //dont assign if we already have assigned one. Safeguards against missclicks
		// if (idx >= 0) return;
		// // get the remarks and modify it
		// auto data = fp.GetFlightPlanData();
		// auto remarks = data.GetRemarks();
		// const char* test = NULL;
		// std::string temp = remarks;
		// test = strstr(remarks, "/CTOT");
		// if (!test)
		// {
			// std::string temp;
			// temp = remarks;
			// temp += "/CTOT";
			// data.SetRemarks(temp.c_str());
			// data.AmendFlightPlan();
		// }
		// CGMPHelper::assignCTOT(false, fp);

		// //again modify the estimated departure time in the flightplan
		// idx = _SelectAcIndex(fp);
		// auto fpdata = fp.GetFlightPlanData();
		// std::string dep = fpdata.GetOrigin();
		// tm t;
		// if (dep == "OMDB")
			// m_sequence_OMDB[idx].CTOT.GetGmtTm(&t);
		// if (dep == "OMSJ")
			// m_sequence_OMSJ[idx].CTOT.GetGmtTm(&t);
		// if (dep == "OMDW")
			// m_sequence_OMDW[idx].CTOT.GetGmtTm(&t);
		// if (dep == "OMAA")
			// m_sequence_OMAA[idx].CTOT.GetGmtTm(&t);
		// CString cstring;
		// cstring.Format("%.4d", t.tm_hour * 100 + t.tm_min);
		// fpdata.SetEstimatedDepartureTime(cstring);
		// fpdata.AmendFlightPlan();
		// break;
	// }
	// case TAG_FUNC_CTOT_ASSIGN_ASAP: // user selected asap
	// {
		// //all is the same but we call assignCTOT with the asap flag
		// if (idx >= 0) return;
		// auto data = fp.GetFlightPlanData();
		// auto remarks = data.GetRemarks();
		// const char* test = NULL;
		// std::string temp = remarks;
		// test = strstr(remarks, "/CTOT");
		// if (!test)
		// {
			// std::string temp;
			// temp = remarks;
			// temp += "/CTOT";
			// data.SetRemarks(temp.c_str());
			// data.AmendFlightPlan();
		// }
		// CGMPHelper::assignCTOT(true, fp);
		// idx = _SelectAcIndex(fp);
		// auto fpdata = fp.GetFlightPlanData();
		// std::string dep = fpdata.GetOrigin();
		// tm t;
		// if (dep == "OMDB")
			// m_sequence_OMDB[idx].CTOT.GetGmtTm(&t);
		// if (dep == "OMSJ")
			// m_sequence_OMSJ[idx].CTOT.GetGmtTm(&t);
		// if (dep == "OMDW")
			// m_sequence_OMDW[idx].CTOT.GetGmtTm(&t);
		// if (dep == "OMAA")
			// m_sequence_OMAA[idx].CTOT.GetGmtTm(&t);
		// CString cstring;
		// cstring.Format("%.4d", t.tm_hour * 100 + t.tm_min);
		// fpdata.SetEstimatedDepartureTime(cstring);
		// fpdata.AmendFlightPlan();
		// break;
	// }
	// case TAG_FUNC_CTOT_CLEAR: // clear the ctot
	// {
		// // simply clear
		// if (idx < 0) break;
		// auto cadata = fp.GetFlightPlanData();
		// std::string dep = cadata.GetOrigin();
		// auto remarks = cadata.GetRemarks();
		// const char* test = NULL;
		// std::string temp = remarks;
		// test = strstr(remarks, "/CTOT");
		// if (test)
		// {
			// //find and replace /CTOT from scratchpad
			// temp = std::regex_replace(temp, std::regex("\\/CTOT"), "");
			// cadata.SetRemarks(temp.c_str());
			// cadata.AmendFlightPlan();
		// }
		// if (dep == "OMDB")
			// m_sequence_OMDB.erase(std::remove(m_sequence_OMDB.begin(), m_sequence_OMDB.end(), m_sequence_OMDB.at(_SelectAcIndex(fp))), m_sequence_OMDB.end());
		// if (dep == "OMSJ")
			// m_sequence_OMSJ.erase(std::remove(m_sequence_OMSJ.begin(), m_sequence_OMSJ.end(), m_sequence_OMSJ.at(_SelectAcIndex(fp))), m_sequence_OMSJ.end());
		// if (dep == "OMDW")
			// m_sequence_OMDW.erase(std::remove(m_sequence_OMDW.begin(), m_sequence_OMDW.end(), m_sequence_OMDW.at(_SelectAcIndex(fp))), m_sequence_OMDW.end());
		// if (dep == "OMAA")
			// m_sequence_OMAA.erase(std::remove(m_sequence_OMAA.begin(), m_sequence_OMAA.end(), m_sequence_OMAA.at(_SelectAcIndex(fp))), m_sequence_OMAA.end());
		// m_TOSequenceList.RemoveFpFromTheList(fp);
		// updateListOMDB();
		// updateListOMSJ();
		// updateListOMDW();
		// updateListOMAA();
		// if (dep == "OMDB")
		// {
			// if (m_sequence_OMDB.size() == idx) break;
			// if (idx == 0) recalculateCTOT(*m_sequence_OMDB.begin());
			// else recalculateCTOT(m_sequence_OMDB[idx - 1]);
			// break;
		// }
		// if (dep == "OMSJ")
		// {
			// if (m_sequence_OMSJ.size() == idx) break;
			// if (idx == 0) recalculateCTOT(*m_sequence_OMSJ.begin());
			// else recalculateCTOT(m_sequence_OMSJ[idx - 1]);
			// break;
		// }
		// if (dep == "OMDW")
		// {
			// if (m_sequence_OMDW.size() == idx) break;
			// if (idx == 0) recalculateCTOT(*m_sequence_OMDW.begin());
			// else recalculateCTOT(m_sequence_OMDW[idx - 1]);
			// break;
		// }
		// if (dep == "OMAA")
		// {
			// if (m_sequence_OMAA.size() == idx) break;
			// if (idx == 0) recalculateCTOT(*m_sequence_OMAA.begin());
			// else recalculateCTOT(m_sequence_OMAA[idx - 1]);
			// break;
		// }
	// }
	case TAG_FUNC_ROUTING:
	{
		std::string handlername = "Route for ";
		handlername += fp.GetCallsign();
		std::string dest = fpdata.GetDestination();
		auto routes = data[fpdata.GetOrigin()].getDatafromICAO(dest);
		if (routes.empty())
		{
			routes = data[fpdata.GetOrigin()].getDatafromICAO(dest.substr(0, 2));
			if (routes.empty())
			{
				routes = data[fpdata.GetOrigin()].getDatafromICAO(dest.substr(0, 1));
				if(routes.empty())
					DisplayUserMessage(handlername.c_str(), "", "No route found", true, true, true, true, true);
			}
			
		}
		for (auto temp : routes)
		{
			std::string routing = "Valid routes to ";
			routing += dest;
			routing += ": ";
			routing += temp.mRoute;
			routing += ". Flightlevel is ";
			if (temp.mLevelR.empty())
				routing += "not restricted";
			else
			{
				routing += "restricted to ";
				routing += temp.mLevelR;
			}
			routing += ". The direction of flight dictates an ";
			routing += temp.mEvenOdd;
			routing += " cruise level.";
			DisplayUserMessage(handlername.c_str(), "", routing.c_str(), true, true, true, true, true);
		}
	}

	}// switch by the function ID
}


void CGMPHelper::assignCTOT(bool asap, EuroScopePlugIn::CFlightPlan flightplan)
{
	CTime time = CTime::GetCurrentTime();
	CTime ctot;
	CTime tobt;
	CTOTData temp;
	auto cadata = flightplan.GetFlightPlanData();
	std::string dep = cadata.GetOrigin();
	//if there is none in sequence or we want asap we dont care about separation
	if (dep == "OMDB")
	{
		if (m_sequence_OMDB.empty() || asap)
		{

			temp.flightplan = flightplan;
			temp.sequence = -1;
			ctot = time + CTimeSpan(0, 0, 30, 0);
			tobt = ctot - taxitime;
			temp.CTOT = ctot;
			temp.TOBT = tobt;

		}
		else
		{
			//get the last aircraft in sequence
			CTOTData &end = *(m_sequence_OMDB.end() - 1);
			CTimeSpan increment = getIncrement(end.flightplan, flightplan);
			temp.flightplan = flightplan;
			temp.sequence = -1;
			//assign at earliest 30 minutes after now 
			ctot = std::max(time + CTimeSpan(0, 0, 30, 0), end.CTOT + increment);
			tobt = ctot - taxitime;
			temp.CTOT = ctot;
			temp.TOBT = tobt;


		}

		m_sequence_OMDB.push_back(temp);
		std::sort(m_sequence_OMDB.begin(), m_sequence_OMDB.end());
		if (asap)
		{
			if (int ind = _SelectAcIndex(temp.flightplan) > 0) recalculateCTOT(m_sequence_OMDB.at(ind - 1));
			else recalculateCTOT(*(m_sequence_OMDB.begin()));

		}
	}
	if (dep == "OMSJ")
	{
		if (m_sequence_OMSJ.empty() || asap)
		{

			temp.flightplan = flightplan;
			temp.sequence = -1;
			ctot = time + CTimeSpan(0, 0, 30, 0);
			tobt = ctot - taxitime;
			temp.CTOT = ctot;
			temp.TOBT = tobt;

		}
		else
		{
			//get the last aircraft in sequence
			CTOTData &end = *(m_sequence_OMSJ.end() - 1);
			CTimeSpan increment = getIncrement(end.flightplan, flightplan);
			temp.flightplan = flightplan;
			temp.sequence = -1;
			//assign at earliest 30 minutes after now 
			ctot = std::max(time + CTimeSpan(0, 0, 30, 0), end.CTOT + increment);
			tobt = ctot - taxitime;
			temp.CTOT = ctot;
			temp.TOBT = tobt;


		}

		m_sequence_OMSJ.push_back(temp);
		std::sort(m_sequence_OMSJ.begin(), m_sequence_OMSJ.end());
		if (asap)
		{
			if (int ind = _SelectAcIndex(temp.flightplan) > 0) recalculateCTOT(m_sequence_OMSJ.at(ind - 1));
			else recalculateCTOT(*(m_sequence_OMSJ.begin()));

		}
	}
	if (dep == "OMDW")
	{
		if (m_sequence_OMDW.empty() || asap)
		{

			temp.flightplan = flightplan;
			temp.sequence = -1;
			ctot = time + CTimeSpan(0, 0, 30, 0);
			tobt = ctot - taxitime;
			temp.CTOT = ctot;
			temp.TOBT = tobt;

		}
		else
		{
			//get the last aircraft in sequence
			CTOTData &end = *(m_sequence_OMDW.end() - 1);
			CTimeSpan increment = getIncrement(end.flightplan, flightplan);
			temp.flightplan = flightplan;
			temp.sequence = -1;
			//assign at earliest 30 minutes after now 
			ctot = std::max(time + CTimeSpan(0, 0, 30, 0), end.CTOT + increment);
			tobt = ctot - taxitime;
			temp.CTOT = ctot;
			temp.TOBT = tobt;


		}

		m_sequence_OMDW.push_back(temp);
		std::sort(m_sequence_OMDW.begin(), m_sequence_OMDW.end());
		if (asap)
		{
			if (int ind = _SelectAcIndex(temp.flightplan) > 0) recalculateCTOT(m_sequence_OMDW.at(ind - 1));
			else recalculateCTOT(*(m_sequence_OMDW.begin()));

		}
	}
	if (dep == "OMAA")
	{
		if (m_sequence_OMAA.empty() || asap)
		{

			temp.flightplan = flightplan;
			temp.sequence = -1;
			ctot = time + CTimeSpan(0, 0, 30, 0);
			tobt = ctot - taxitime;
			temp.CTOT = ctot;
			temp.TOBT = tobt;

		}
		else
		{
			//get the last aircraft in sequence
			CTOTData &end = *(m_sequence_OMAA.end() - 1);
			CTimeSpan increment = getIncrement(end.flightplan, flightplan);
			temp.flightplan = flightplan;
			temp.sequence = -1;
			//assign at earliest 30 minutes after now 
			ctot = std::max(time + CTimeSpan(0, 0, 30, 0), end.CTOT + increment);
			tobt = ctot - taxitime;
			temp.CTOT = ctot;
			temp.TOBT = tobt;


		}

		m_sequence_OMAA.push_back(temp);
		std::sort(m_sequence_OMAA.begin(), m_sequence_OMAA.end());
		if (asap)
		{
			if (int ind = _SelectAcIndex(temp.flightplan) > 0) recalculateCTOT(m_sequence_OMAA.at(ind - 1));
			else recalculateCTOT(*(m_sequence_OMAA.begin()));

		}
	}
}
void CGMPHelper::updateListOMDB()
{
	//some housekeeping 
	for (CTOTData i : m_sequence_OMDB)
	{
		//add all aircraft in sequence to the euroscope list
		m_TOSequenceList.AddFpToTheList(i.flightplan);
	}
	for (CTOTData i : m_sequence_OMDB)
	{
		EuroScopePlugIn::CFlightPlan first = FlightPlanSelectFirst();
		EuroScopePlugIn::CFlightPlan temp = FlightPlanSelectNext(first);
		bool found = false;
		if (first.GetCallsign() == i.flightplan.GetCallsign()) found = true;
		while (temp.IsValid() && !found)
		{
			if (temp.GetCallsign() == i.flightplan.GetCallsign())
			{
				found = true;
				break;
			}
			temp = FlightPlanSelectNext(temp);
		}
		if (!found)
		{
			m_TOSequenceList.RemoveFpFromTheList(i.flightplan);
			m_sequence_OMDB.erase(std::remove(m_sequence_OMDB.begin(), m_sequence_OMDB.end(), m_sequence_OMDB.at(_SelectAcIndex(i.flightplan))), m_sequence_OMDB.end());
			continue;
		}
		
		
		//if any aircraft takes off remove it automatically
		auto fp = i.flightplan;
		EuroScopePlugIn::CRadarTarget rt = fp.GetCorrelatedRadarTarget();
		auto cad = fp.GetFlightPlanData();
		std::string remarks = cad.GetRemarks();
		const char* test = NULL;
		test = strstr(remarks.c_str(), "/CTOT");
		if (rt.GetGS() > 80||!test)
		{
			remarks = std::regex_replace(remarks, std::regex("\\/CTOT"), "");
			cad.SetRemarks(remarks.c_str());
			cad.AmendFlightPlan();
			m_TOSequenceList.RemoveFpFromTheList(fp);
			m_sequence_OMDB.erase(std::remove(m_sequence_OMDB.begin(), m_sequence_OMDB.end(), m_sequence_OMDB.at(_SelectAcIndex(fp))), m_sequence_OMDB.end());
		}
	}
	std::sort(m_sequence_OMDB.begin(), m_sequence_OMDB.end());
	for (CTOTData &i : m_sequence_OMDB)
	{
		//get the number in sequence for everyone
		i.sequence = _SelectAcIndex(i.flightplan) + 1;
	}
	if (m_sequence_OMDB.empty())
	{
		return;
	}
	recalculateCTOT(*m_sequence_OMDB.begin());
}
void CGMPHelper::updateListOMSJ()
{
	//some housekeeping 
	for (CTOTData i : m_sequence_OMSJ)
	{
		//add all aircraft in sequence to the euroscope list
		m_TOSequenceList.AddFpToTheList(i.flightplan);
	}
	for (CTOTData i : m_sequence_OMSJ)
	{
		EuroScopePlugIn::CFlightPlan first = FlightPlanSelectFirst();
		EuroScopePlugIn::CFlightPlan temp = FlightPlanSelectNext(first);
		bool found = false;
		if (first.GetCallsign() == i.flightplan.GetCallsign()) found = true;
		while (temp.IsValid() && !found)
		{
			if (temp.GetCallsign() == i.flightplan.GetCallsign())
			{
				found = true;
				break;
			}
			temp = FlightPlanSelectNext(temp);
		}
		if (!found)
		{
			m_TOSequenceList.RemoveFpFromTheList(i.flightplan);
			m_sequence_OMSJ.erase(std::remove(m_sequence_OMSJ.begin(), m_sequence_OMSJ.end(), m_sequence_OMSJ.at(_SelectAcIndex(i.flightplan))), m_sequence_OMSJ.end());
			continue;
		}


		//if any aircraft takes off remove it automatically
		auto fp = i.flightplan;
		EuroScopePlugIn::CRadarTarget rt = fp.GetCorrelatedRadarTarget();
		auto cad = fp.GetFlightPlanData();
		std::string remarks = cad.GetRemarks();
		const char* test = NULL;
		test = strstr(remarks.c_str(), "/CTOT");
		if (rt.GetGS() > 80 || !test)
		{
			remarks = std::regex_replace(remarks, std::regex("\\/CTOT"), "");
			cad.SetRemarks(remarks.c_str());
			cad.AmendFlightPlan();
			m_TOSequenceList.RemoveFpFromTheList(fp);
			m_sequence_OMSJ.erase(std::remove(m_sequence_OMSJ.begin(), m_sequence_OMSJ.end(), m_sequence_OMSJ.at(_SelectAcIndex(fp))), m_sequence_OMSJ.end());
		}
	}
	std::sort(m_sequence_OMSJ.begin(), m_sequence_OMSJ.end());
	for (CTOTData &i : m_sequence_OMSJ)
	{
		//get the number in sequence for everyone
		i.sequence = _SelectAcIndex(i.flightplan) + 1;
	}
	if (m_sequence_OMSJ.empty())
	{
		return;
	}
	recalculateCTOT(*m_sequence_OMSJ.begin());
}
void CGMPHelper::updateListOMDW()
{
	//some housekeeping 
	for (CTOTData i : m_sequence_OMDW)
	{
		//add all aircraft in sequence to the euroscope list
		m_TOSequenceList.AddFpToTheList(i.flightplan);
	}
	for (CTOTData i : m_sequence_OMDW)
	{
		EuroScopePlugIn::CFlightPlan first = FlightPlanSelectFirst();
		EuroScopePlugIn::CFlightPlan temp = FlightPlanSelectNext(first);
		bool found = false;
		if (first.GetCallsign() == i.flightplan.GetCallsign()) found = true;
		while (temp.IsValid() && !found)
		{
			if (temp.GetCallsign() == i.flightplan.GetCallsign())
			{
				found = true;
				break;
			}
			temp = FlightPlanSelectNext(temp);
		}
		if (!found)
		{
			m_TOSequenceList.RemoveFpFromTheList(i.flightplan);
			m_sequence_OMDW.erase(std::remove(m_sequence_OMDW.begin(), m_sequence_OMDW.end(), m_sequence_OMDW.at(_SelectAcIndex(i.flightplan))), m_sequence_OMDW.end());
			continue;
		}


		//if any aircraft takes off remove it automatically
		auto fp = i.flightplan;
		EuroScopePlugIn::CRadarTarget rt = fp.GetCorrelatedRadarTarget();
		auto cad = fp.GetFlightPlanData();
		std::string remarks = cad.GetRemarks();
		const char* test = NULL;
		test = strstr(remarks.c_str(), "/CTOT");
		if (rt.GetGS() > 80 || !test)
		{
			remarks = std::regex_replace(remarks, std::regex("\\/CTOT"), "");
			cad.SetRemarks(remarks.c_str());
			cad.AmendFlightPlan();
			m_TOSequenceList.RemoveFpFromTheList(fp);
			m_sequence_OMDW.erase(std::remove(m_sequence_OMDW.begin(), m_sequence_OMDW.end(), m_sequence_OMDW.at(_SelectAcIndex(fp))), m_sequence_OMDW.end());
		}
	}
	std::sort(m_sequence_OMDW.begin(), m_sequence_OMDW.end());
	for (CTOTData &i : m_sequence_OMDW)
	{
		//get the number in sequence for everyone
		i.sequence = _SelectAcIndex(i.flightplan) + 1;
	}
	if (m_sequence_OMDW.empty())
	{
		return;
	}
	recalculateCTOT(*m_sequence_OMDW.begin());
}
void CGMPHelper::updateListOMAA()
{
	//some housekeeping 
	for (CTOTData i : m_sequence_OMAA)
	{
		//add all aircraft in sequence to the euroscope list
		m_TOSequenceList.AddFpToTheList(i.flightplan);
	}
	for (CTOTData i : m_sequence_OMAA)
	{
		EuroScopePlugIn::CFlightPlan first = FlightPlanSelectFirst();
		EuroScopePlugIn::CFlightPlan temp = FlightPlanSelectNext(first);
		bool found = false;
		if (first.GetCallsign() == i.flightplan.GetCallsign()) found = true;
		while (temp.IsValid() && !found)
		{
			if (temp.GetCallsign() == i.flightplan.GetCallsign())
			{
				found = true;
				break;
			}
			temp = FlightPlanSelectNext(temp);
		}
		if (!found)
		{
			m_TOSequenceList.RemoveFpFromTheList(i.flightplan);
			m_sequence_OMAA.erase(std::remove(m_sequence_OMAA.begin(), m_sequence_OMAA.end(), m_sequence_OMAA.at(_SelectAcIndex(i.flightplan))), m_sequence_OMAA.end());
			continue;
		}


		//if any aircraft takes off remove it automatically
		auto fp = i.flightplan;
		EuroScopePlugIn::CRadarTarget rt = fp.GetCorrelatedRadarTarget();
		auto cad = fp.GetFlightPlanData();
		std::string remarks = cad.GetRemarks();
		const char* test = NULL;
		test = strstr(remarks.c_str(), "/CTOT");
		if (rt.GetGS() > 80 || !test)
		{
			remarks = std::regex_replace(remarks, std::regex("\\/CTOT"), "");
			cad.SetRemarks(remarks.c_str());
			cad.AmendFlightPlan();
			m_TOSequenceList.RemoveFpFromTheList(fp);
			m_sequence_OMAA.erase(std::remove(m_sequence_OMAA.begin(), m_sequence_OMAA.end(), m_sequence_OMAA.at(_SelectAcIndex(fp))), m_sequence_OMAA.end());
		}
	}
	std::sort(m_sequence_OMAA.begin(), m_sequence_OMAA.end());
	for (CTOTData &i : m_sequence_OMAA)
	{
		//get the number in sequence for everyone
		i.sequence = _SelectAcIndex(i.flightplan) + 1;
	}
	if (m_sequence_OMAA.empty())
	{
		return;
	}
	recalculateCTOT(*m_sequence_OMAA.begin());
}

void CGMPHelper::recalculateCTOT(CTOTData inserted)
//recalculates the CTOT for each flightplan in sequence following and inculding "inserted"
{
	std::string dep = inserted.flightplan.GetFlightPlanData().GetOrigin();
	//get the number in sequence for our inserted fp
	if (dep == "OMDB")
	{
		auto pos = std::find(m_sequence_OMDB.begin(), m_sequence_OMDB.end(), inserted);
		//safeguard if not found due to whatever 
		if (pos == m_sequence_OMDB.end())
		{
			return;
		}
		//iterate over the sequence starting from "inserted"
		for (auto &it = pos; it != m_sequence_OMDB.end() - 1; it++)
		{
			CTime curr = CTime::GetCurrentTime();
			CTOTData temp1 = *it;
			CString test = temp1.flightplan.GetCallsign();
			CTOTData &temp2 = *(it + 1);
			CString test2 = temp2.flightplan.GetCallsign();
			if (temp2.manual) continue;
			CTimeSpan inc = getIncrement(temp1.flightplan, temp2.flightplan);
			temp2.CTOT = std::max(temp2.CTOT,temp1.CTOT + inc);
			temp2.TOBT = temp2.CTOT - taxitime;
		}
	}
	if (dep == "OMSJ")
	{
		auto pos = std::find(m_sequence_OMSJ.begin(), m_sequence_OMSJ.end(), inserted);
		//safeguard if not found due to whatever 
		if (pos == m_sequence_OMSJ.end())
		{
			return;
		}
		//iterate over the sequence starting from "inserted"
		for (auto &it = pos; it != m_sequence_OMSJ.end() - 1; it++)
		{
			CTime curr = CTime::GetCurrentTime();
			CTOTData temp1 = *it;
			CString test = temp1.flightplan.GetCallsign();
			CTOTData &temp2 = *(it + 1);
			CString test2 = temp2.flightplan.GetCallsign();
			if (temp2.manual) continue;
			CTimeSpan inc = getIncrement(temp1.flightplan, temp2.flightplan);
			temp2.CTOT = std::max(temp2.CTOT, temp1.CTOT + inc);
			temp2.TOBT = temp2.CTOT - taxitime;
		}
	}
	if (dep == "OMDW")
	{
		auto pos = std::find(m_sequence_OMDW.begin(), m_sequence_OMDW.end(), inserted);
		//safeguard if not found due to whatever 
		if (pos == m_sequence_OMDW.end())
		{
			return;
		}
		//iterate over the sequence starting from "inserted"
		for (auto &it = pos; it != m_sequence_OMDW.end() - 1; it++)
		{
			CTime curr = CTime::GetCurrentTime();
			CTOTData temp1 = *it;
			CString test = temp1.flightplan.GetCallsign();
			CTOTData &temp2 = *(it + 1);
			CString test2 = temp2.flightplan.GetCallsign();
			if (temp2.manual) continue;
			CTimeSpan inc = getIncrement(temp1.flightplan, temp2.flightplan);
			temp2.CTOT = std::max(temp2.CTOT, temp1.CTOT + inc);
			temp2.TOBT = temp2.CTOT - taxitime;
		}
	}
	if (dep == "OMAA")
	{
		auto pos = std::find(m_sequence_OMAA.begin(), m_sequence_OMAA.end(), inserted);
		//safeguard if not found due to whatever 
		if (pos == m_sequence_OMAA.end())
		{
			return;
		}
		//iterate over the sequence starting from "inserted"
		for (auto &it = pos; it != m_sequence_OMAA.end() - 1; it++)
		{
			CTime curr = CTime::GetCurrentTime();
			CTOTData temp1 = *it;
			CString test = temp1.flightplan.GetCallsign();
			CTOTData &temp2 = *(it + 1);
			CString test2 = temp2.flightplan.GetCallsign();
			if (temp2.manual) continue;
			CTimeSpan inc = getIncrement(temp1.flightplan, temp2.flightplan);
			temp2.CTOT = std::max(temp2.CTOT, temp1.CTOT + inc);
			temp2.TOBT = temp2.CTOT - taxitime;
		}
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
	if (sid1 == sid2 && (strstr(sid1, "ANVIX") || strstr(sid1, "IVURO")))
		increment = std::max(increment, CTimeSpan(0, 0, 3, 0));

	return increment;
}







