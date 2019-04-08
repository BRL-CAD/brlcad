#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <locale>
#include <iomanip>

std::map<std::string,long int> svn_msg_to_rev;
std::map<std::string,std::string> git_msg_to_sha1;
std::set<std::string> svn_msg_non_unique;

std::map<std::string,long int> svn_msg_to_time;
std::map<std::string,long int> git_msg_to_time;
std::set<std::string> git_msg_non_unique;

std::map<std::pair<std::string, long int>,long int> svn_msgtime_to_rev;
std::map<std::pair<std::string, long int>,std::string> git_msgtime_to_sha1;

// Generate input file with ./svn_msgs brlcad.dump
void
read_svn_info()
{
    std::ifstream infile("svn_msgs.txt");
    if (!infile.good()) exit(1);
    std::string line;
    while (std::getline(infile, line)) {
	size_t spos = line.find_first_of(",");
	std::string rev_str = line.substr(0, spos);
	long int rev = std::stol(rev_str);
	line = line.substr(spos + 1, std::string::npos);
	spos = line.find_first_of(",");
	std::string timestamp = line.substr(0, spos);
	long int tstp = std::stol(timestamp);
	std::string cmsg = line.substr(spos+1, std::string::npos);
	if (svn_msg_to_rev.find(cmsg) == svn_msg_to_rev.end()) {
	    svn_msg_to_rev[cmsg] = rev;
	    svn_msg_to_time[cmsg] = tstp;
	} else {
	    svn_msg_to_rev.erase(cmsg);
	    svn_msg_to_time.erase(cmsg);
	    svn_msg_non_unique.insert(cmsg);
	}
	svn_msgtime_to_rev[std::pair<std::string,long int>(cmsg, tstp)] = rev;
    }
    infile.close();
}

void
read_git_line(std::string &line)
{
    size_t spos = line.find_first_of(",");
    std::string sha1 = line.substr(0, spos);
    line = line.substr(spos + 1, std::string::npos);
    spos = line.find_first_of(",");
    std::string timestamp = line.substr(0, spos);
    std::string trimstr(" +0000");
    {
	size_t spos2 = timestamp.find(trimstr);
	if (spos2 != std::string::npos) {
	    timestamp.erase(spos2, trimstr.length());
	}
    }
    long int tstp = std::stol(timestamp);
    std::string cmsg = line.substr(spos+1, std::string::npos);

    if (git_msg_to_sha1.find(cmsg) == git_msg_to_sha1.end()) {
	git_msg_to_sha1[cmsg] = sha1;
	git_msg_to_time[cmsg] = tstp;
    } else {
	git_msg_to_sha1.erase(cmsg);
	git_msg_to_time.erase(cmsg);
	git_msg_non_unique.insert(cmsg);
    }

    git_msgtime_to_sha1[std::pair<std::string,long int>(cmsg, tstp)] = sha1;
}

// Generate input file with:
//
// git log --all --pretty=format:"GITMSG%n%H,%ct +0000,%B%nGITMSGEND%n" > ../git.log
void
read_git_info()
{
    std::string line;
    std::string newline;
    std::string bstr("GITMSG");
    std::string estr("GITMSGEND");

    std::ifstream infile("git.log");
    if (!infile.good()) exit(-1);

    while (std::getline(infile, line)) {
	if (!line.length()) continue;
	if (line == bstr) {
	    newline = std::string("");
	    continue;
	}

	if (line == estr) {
	    read_git_line(newline);
	    continue;
	} else {
	    newline.append(line);
	}
    }
    infile.close();
}

int main(int argc, const char **argv)
{
    read_svn_info();
    read_git_info();

    std::map<std::pair<std::string, long int>,long int>::iterator s_it;
    for (s_it = svn_msgtime_to_rev.begin(); s_it != svn_msgtime_to_rev.end(); s_it++) {
	std::map<std::pair<std::string, long int>,std::string>::iterator g_it;
	g_it = git_msgtime_to_sha1.find((*s_it).first);
	if (g_it != git_msgtime_to_sha1.end()) {
	    std::cout << (*s_it).second << " -> " << (*g_it).second << "\n";
	} else {
	    std::string cmsg = (*s_it).first.first;
	    if (svn_msg_non_unique.find(cmsg) == svn_msg_non_unique.end() &&
		    git_msg_non_unique.find(cmsg) == git_msg_non_unique.end()) {
		std::map<std::string,long int>::iterator r_it = svn_msg_to_rev.find(cmsg);
		std::map<std::string,std::string>::iterator g2_it = git_msg_to_sha1.find(cmsg);
		if (r_it != svn_msg_to_rev.end() && g2_it != git_msg_to_sha1.end()) {
		    std::cout << (*r_it).second << " -> " << (*g2_it).second << " (time offset)\n";
		} else {
		    std::cerr << (*s_it).second << " [unmapped] : " << cmsg << "\n";
		}
	    } else {
		std::cerr << (*s_it).second << " [non-unique,no exact timestamp match] : " << cmsg << "\n";
	    }
	}
    }

    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
