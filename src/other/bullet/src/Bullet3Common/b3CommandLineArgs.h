#ifndef COMMAND_LINE_ARGS_H
#define COMMAND_LINE_ARGS_H

/******************************************************************************
 * Command-line parsing
 ******************************************************************************/
#include <map>
#include <algorithm>
#include <string>
#include <cstring>
#include <sstream>
class b3CommandLineArgs
{
protected:

	std::map<std::string, std::string> pairs;

public:

	// Constructor
	b3CommandLineArgs(int argc, char **argv)
	{
		addArgs(argc,argv);
	}

	void addArgs(int argc, char**argv)
	{
		using namespace std;

	    for (int i = 1; i < argc; i++)
	    {
	        string arg = argv[i];

	        if ((arg[0] != '-') || (arg[1] != '-')) {
	        	continue;
	        }

        	string::size_type pos;
		    string key, val;
	        if ((pos = arg.find( '=')) == string::npos) {
	        	key = string(arg, 2, arg.length() - 2);
	        	val = "";
	        } else {
	        	key = string(arg, 2, pos - 2);
	        	val = string(arg, pos + 1, arg.length() - 1);
	        }
			
			//only add new keys, don't replace existing
			if(pairs.find(key) == pairs.end())
			{
        		pairs[key] = val;
			}
	    }
	}

	bool CheckCmdLineFlag(const char* arg_name)
	{
		using namespace std;
		map<string, string>::iterator itr;
		if ((itr = pairs.find(arg_name)) != pairs.end()) {
			return true;
	    }
		return false;
	}

	template <typename T>
	bool GetCmdLineArgument(const char *arg_name, T &val);

	int ParsedArgc()
	{
		return pairs.size();
	}
};

template <typename T>
inline bool b3CommandLineArgs::GetCmdLineArgument(const char *arg_name, T &val)
{
	using namespace std;
	map<string, string>::iterator itr;
	if ((itr = pairs.find(arg_name)) != pairs.end()) {
		istringstream strstream(itr->second);
		strstream >> val;
		return true;
    }
	return false;
}

template <>
inline bool b3CommandLineArgs::GetCmdLineArgument<char*>(const char* arg_name, char* &val)
{
	using namespace std;
	map<string, string>::iterator itr;
	if ((itr = pairs.find(arg_name)) != pairs.end()) {

		string s = itr->second;
		val = (char*) malloc(sizeof(char) * (s.length() + 1));
		std::strcpy(val, s.c_str());
		return true;
	} else {
    	val = NULL;
	}
	return false;
}


#endif //COMMAND_LINE_ARGS_H
