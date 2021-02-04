#pragma once
#pragma warning(disable : 4996)
#include "EuroScopePlugIn.h"
#include "pch.h"
#include <vector>
#include "csv.h"
#include "loguru.hpp"
#include <sstream>
#include <set>

class RouteTo
	//This class stores the different standard routes to a destination icao.
{
public:
	std::string mDEPICAO, mDestICAO, mEvenOdd, mLevelR, mRoute;
	std::vector<std::string> endpoints;
	RouteTo(std::string DepICAO, std::string DestICAO, std::string evenodd, std::string LevelR, std::string Route)
	{
		mDEPICAO = DepICAO;
		mDestICAO = DestICAO;
		mEvenOdd = evenodd;
		mLevelR = LevelR;
		mRoute = Route;
		RouteTo::test();
		
	}

	bool isCruiseValid(int Flightlevel)
	{
		bool returnval = false;
		if (Flightlevel > 40000)
		{
			if (this->mLevelR.empty())
			{
				if (this->mEvenOdd == "ODD")
				{
					if ((Flightlevel / 1000) % 4 == 1) return true;
					else return false;
				}
				if (this->mEvenOdd == "EVEN")
				{
					if ((Flightlevel / 1000) % 4 == 3) return true;
					else return false;
				}
			}
			else
			{

				if (this->mLevelR.at(0) == '<')
				{
					std::string restr = this->mLevelR.substr(1, 3);
					if (this->mEvenOdd == "ODD")
					{
						if (((Flightlevel / 1000) % 4 == 1) && Flightlevel <= std::stoi(restr)) return true;
						else return false;
					}
					if (this->mEvenOdd == "EVEN")
					{
						if (((Flightlevel / 1000) % 4 == 3) && Flightlevel <= std::stoi(restr) * 100) return true;
						else return false;
					}
				}
				else
				{
					int restr = std::stoi(this->mLevelR);
					return (Flightlevel == restr * 100);
				}
			}
		}
		else {
			if (this->mLevelR.empty())
			{
				if (this->mEvenOdd == "ODD")
				{
					if ((Flightlevel / 1000) % 2 == 1) return true;
					else return false;
				}
				if (this->mEvenOdd == "EVEN")
				{
					if ((Flightlevel / 1000) % 2 == 0) return true;
					else return false;
				}
			}
			else
			{

				if (this->mLevelR.at(0) == '<')
				{
					std::string restr = this->mLevelR.substr(1, 3);
					if (this->mEvenOdd == "ODD")
					{
						if (((Flightlevel / 1000) % 2 == 1) && Flightlevel <= std::stoi(restr) * 100) return true;
						else return false;
					}
					if (this->mEvenOdd == "EVEN")
					{
						if (((Flightlevel / 1000) % 2 == 0) && Flightlevel <= std::stoi(restr) * 100) return true;
						else return false;
					}
				}
				else
				{
					int restr = std::stoi(this->mLevelR);
					return (Flightlevel == restr * 100);
				}
			}
			return returnval;
		}
	}
	bool isRouteValid(std::string Route)
	{
		auto temp = makeAirwaysUnique(findAndReplaceAll(Route, " DCT ", " "));
		auto check = temp.find(mRoute);
		if (check == std::string::npos)
		{
			auto temp1 = findAndReplaceAll(mRoute, " DCT ", " ");
			if (temp.find(temp1) == std::string::npos)
				return false;
			else
				return true;
		}
			
		else return true;
	}
	static bool test()
	{
		return true;
	}
	std::string makeAirwaysUnique(std::string Route)
	{
	    std::string buf;                 // Have a buffer string
		std::stringstream ss(Route);       // Insert the string into a stream
		std::vector<std::string> tokens; // Create vector to hold our words

		while (ss >> buf)
			tokens.push_back(buf);
		auto tokenscopy = tokens;
		tokens = RemoveDuplicatesInVector(tokens);
		std::string result;
		for (auto temp : tokens)
		{

			result += temp;
			result += " ";
		}
		return result;
	}
	std::vector<std::string> RemoveDuplicatesInVector(std::vector<std::string> vec)
	{
		std::set<std::string> values;
		//vec.erase(std::remove_if(vec.begin(), vec.end(), [&](const std::string & value) { return !values.insert(value).second; }), vec.end());
		std::vector<std::string> vec2 = vec;
		for (auto value: vec)
		{
			auto check = values.find(value);
			if(values.end() == check)
				values.insert(value);
			else
			{
				auto check1 = std::find(vec2.begin(), vec2.end(), *check);
				if (check1 != vec2.end() && *(check1 + 2) == *check)
				{
					vec2.erase(std::remove(vec2.begin(), check1 + 2, *(check1 + 1)));
					auto check2 = std::find(vec2.begin(), vec2.end(), *check);
					vec2.erase(std::remove(vec2.begin(), check2 + 1, *check2));
				}
			}
		}
		vec2.shrink_to_fit();
		return vec2;
	}
	std::string findAndReplaceAll(std::string data, std::string toSearch, std::string replaceStr)
	{
		// Get the first occurrence
		size_t pos = data.find(toSearch);
		// Repeat till end is reached
		while (pos != std::string::npos)
		{
			// Replace this occurrence of Sub String
			data.replace(pos, toSearch.size(), replaceStr);
			// Get the next occurrence from the current position
			pos = data.find(toSearch, pos + replaceStr.size());
		}
		return data;
	}
};

class RouteData
	//this Class holds all RouteTos
{
public:
	RouteData(){}
	std::vector<RouteTo> Routes;
	std::vector<std::string> icaos;

	std::vector<RouteTo> getDatafromICAO(std::string icao)
	{
		std::vector<RouteTo> routes;
		for (auto temp : Routes)
		{
			if (icao == temp.mDestICAO)
				routes.push_back(temp);
		}
		return routes;
	}
};

class CGMPHelper :
	//The class that holds all our functions 
	public EuroScopePlugIn::CPlugIn
{
public:
	//the list displayed in euroscope
	
	

	CGMPHelper(void);



	virtual ~CGMPHelper(void);

	
	/*
	This function overrides a Euroscope function. If you type ".showtolist" in the euroscope textbox it will show the t/o sequence list
	Input: sCommandLine (the textbox string)
	*/
	bool fileExists(const std::string& filename)
	{
		struct stat buf;
		if (stat(filename.c_str(), &buf) != -1)
		{
			return true;
		}
		return false;
	}
	

	

	virtual void  OnGetTagItem(EuroScopePlugIn::CFlightPlan FlightPlan,
		EuroScopePlugIn::CRadarTarget RadarTarget,
		int ItemCode,
		int TagData,
		char sItemString[16],
		int * pColorCode,
		COLORREF * pRGB,
		double * pFontSize);
	/*This function overrides a Euroscope function. It determines what value to show for our new TAG items "CTOT" and "TOBT" and is called automatically by euroscope every few seconds
	  detailed info on the input and output values can be found in the EuroScopePlugIn.h header
	*/

	

	virtual void    OnFunctionCall(int FunctionId,
			const char * sItemString,
			POINT Pt,
			RECT Area);

	


};
