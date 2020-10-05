#pragma once
#pragma warning(disable : 4996)
#include "EuroScopePlugIn.h"
#include "pch.h"
#include <vector>
#include "csv.h"
#include "loguru.hpp"
#include <sstream>
#include <set>

class CTOTData
	//this class is used as a storage. Each aircraft that gets a ctot assigned will be put in a CTOTData object. It contains the flightplan the CTOT and TOBT a switch if it was manually assigned
{
public:
	CTOTData(){}
	EuroScopePlugIn::CFlightPlan flightplan;
	CTime CTOT, TOBT;
	int sequence;
	bool manual = false;
	bool operator==(const CTOTData& rhs)
		//overwriting the == and < operator to make use of the STL algorithms for sorting and finding lateron
	{
		if ((this->flightplan.GetCallsign() == rhs.flightplan.GetCallsign()) && this->CTOT == rhs.CTOT && this->sequence == rhs.sequence && this->TOBT == rhs.TOBT)
		{
			return true;
		}
		return false;
	}
	bool operator<(const CTOTData& rhs)
	{
		if (this->CTOT < rhs.CTOT)
		{
			return true;
		}
		return false;
	}
	static bool test()
	{
		return true;
	}

};
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
	
		CTOTData::test();
	}

	bool isCruiseValid(int Flightlevel)
	{
		bool returnval = false;
		
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
					if (((Flightlevel / 1000) % 2 == 1) && Flightlevel <= std::stoi(restr)) return true;
					else return false;
				}
				if (this->mEvenOdd == "EVEN")
				{
					if (((Flightlevel / 1000) % 2 == 0) && Flightlevel <= std::stoi(restr)*100) return true;
					else return false;
				}
			}
			else
			{
				int restr = std::stoi(this->mLevelR);
				return (Flightlevel == restr*100);
			}
		}
		return returnval;
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
	std::string makeAirwaysUnique(std::string Route)
	{
	    std::string buf;                 // Have a buffer string
		std::stringstream ss(Route);       // Insert the string into a stream
		std::vector<std::string> tokens; // Create vector to hold our words

		while (ss >> buf)
			tokens.push_back(buf);
		auto tokenscopy = tokens;
		RemoveDuplicatesInVector(tokens);
		for (int i = 0; i < tokens.size(); i++)
		{
			if (tokens.at(i) != tokenscopy.at(i))
			{
				tokens.erase(std::remove(tokens.begin(), tokens.end(), tokens.at(i-1)), tokens.end());
				break;
			}
		}
		tokens.shrink_to_fit();
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
		for (auto value: vec)
		{
			auto check = values.find(value);
			if(values.end() == check)
				values.insert(value);
			else
			{
				auto check1 = std::find(vec.begin(), vec.end(), *check);
				if (check1 != vec.end() && *(check1 + 2) == *check)
					vec.erase(std::remove(vec.begin(),check1+1,*check1));
			}
		}
		vec.shrink_to_fit();
		return vec;
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
	EuroScopePlugIn::CFlightPlanList  m_TOSequenceList;
	//this vector holds a CTOTData for each aircraft
	std::vector<CTOTData> m_sequence_OMDB;
	std::vector<CTOTData> m_sequence_OMSJ;
	std::vector<CTOTData> m_sequence_OMDW;
	std::vector<CTOTData> m_sequence_OMAA;
	

	CGMPHelper(void);



	virtual ~CGMPHelper(void);

	inline  virtual bool    OnCompileCommand(const char * sCommandLine);
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
	

	//virtual void OnFlightPlanDisconnect(EuroScopePlugIn::CFlightPlan FlightPlan);

	/*
	This function overrides a Euroscope function. It makes sure that when a user disconnects, that his flightplan is deleted from our list and from the sequence
	Input: FlightPlan (the flightplan of the disconnected user)
	*/

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

	int     _SelectAcIndex(EuroScopePlugIn::CFlightPlan flightplan); 
		//This function searches our sequence for the input "flightplan" and returns the index



	virtual void    OnFunctionCall(int FunctionId,
			const char * sItemString,
			POINT Pt,
			RECT Area);

	/*This function overrides a Euroscope function. It handles the user interface interaction. So it shows the popup menu on left click on the respective TAG fields and so on 
	  detailed info on the input and output values can be found in the EuroScopePlugIn.h header
	*/

	void assignCTOT(bool asap, EuroScopePlugIn::CFlightPlan);
	/*This function is called when a CTOT should be assigned to an aircraft. It constructs the new CTOTData object and redoes the sequence
		Input: bool asap (if an aircraft should be assigned a ctot that is as close to now as possible or at the end of the sequence)
			   CFlightPlan flightplan (the corresponding flightplan)
	*/


	void updateListOMDB();
	//This function is called from various other functions to do housekeeping on the actual euroscope list
	void updateListOMSJ();
	//This function is called from various other functions to do housekeeping on the actual euroscope list
	void updateListOMDW();
	//This function is called from various other functions to do housekeeping on the actual euroscope list
	void updateListOMAA();
	//This function is called from various other functions to do housekeeping on the actual euroscope list

	CTimeSpan getIncrement(EuroScopePlugIn::CFlightPlan fp1, EuroScopePlugIn::CFlightPlan fp2);


	/*This function is the heart of the implementation. It determines which separation fp2 needs to maintain to the preceeding aircraft fp1.
	  It takes into account the sids and wake turbulence category of the aircraft
	*/


	void recalculateCTOT(CTOTData inserted);

	/* This function is called when we change the order of the sequence either by assigning an aircraft and asap ctot or by manually assigning one.
	   The function recalculates all CTOTs that follow the "inserted" so the modified one.
	*/
};
