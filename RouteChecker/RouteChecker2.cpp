#pragma once
#include "pch.h"
#include "RouteChecker2.h"
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





#define MY_PLUGIN_NAME      "RouteChecker for VATSIM"
#define MY_PLUGIN_VERSION   "1.0"
#define MY_PLUGIN_DEVELOPER "Nils Dornbusch"
#define MY_PLUGIN_COPYRIGHT "Free to be distributed as source code"
#define MY_PLUGIN_VIEW      ""


const	int		TAG_ITEM_ROUTE_VALID = 123123;


const   int     TAG_FUNC_ROUTING = 15;



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
		(LPCSTR)&RouteTo::test, &hm) == 0)
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
	LOG_F(INFO, "We successfully started routeChecker. Great Success!");
	
	RegisterTagItemType("RouteValid", TAG_ITEM_ROUTE_VALID);

	RegisterTagItemFunction("Get routes", TAG_FUNC_ROUTING);


	
	
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
	

	// switch the different tag fields
	switch (ItemCode)
	{
	
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
			if (!routevalid)
			{
				routevalid = d.isRouteValid(tmp);
			}
			else
			{
				cruisevalid = d.isCruiseValid(FlightPlan.GetFinalAltitude());
				if (cruisevalid && routevalid)
				{
					strcpy(sItemString, "");
					return;
				}
				else {
					strcpy(sItemString, "L");
				}

			}
			cruisevalid = d.isCruiseValid(FlightPlan.GetFinalAltitude());
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

	auto fpdata = fp.GetFlightPlanData();
	std::string dep = fpdata.GetOrigin();

	// switch by the function ID
	switch (FunctionId)
	{
	
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



