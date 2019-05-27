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
std::map<long int,std::string> svn_rev_to_msg;
std::map<std::string,std::string> git_msg_to_sha1;
std::set<std::string> svn_msg_non_unique;

std::map<std::string,long int> svn_msg_to_time;
std::map<std::string,long int> git_msg_to_time;
std::map<long int,std::string> svn_time_to_msg;
std::map<long int,long int> svn_time_to_rev;
std::map<long int,std::string> git_time_to_msg;
std::map<long int,std::string> git_time_to_sha1;
std::set<long int> git_time_nonuniq;

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
	svn_time_to_msg[tstp] = cmsg;
	svn_time_to_rev[tstp] = rev;
	svn_rev_to_msg[rev] = cmsg;
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

    git_time_to_msg[tstp] = cmsg;
    if (git_time_to_sha1.find(tstp) != git_time_to_sha1.end()) {
	git_time_nonuniq.insert(tstp);
    }
    git_time_to_sha1[tstp] = sha1;
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

void write_note(long int rev, std::string gsha1, long int commit_time)
{
    std::string fi_file = std::to_string(rev) + std::string("-note.fi");
    std::ofstream outfile(fi_file.c_str(), std::ios::out | std::ios::binary);
    std::string svn_id_str = std::string("svn:revision:") + std::to_string(rev);

    outfile << "blob" << "\n";
    outfile << "mark :1" << "\n";
    outfile << "data " << svn_id_str.length() << "\n";
    outfile << svn_id_str << "\n";
    outfile << "commit refs/notes/commits" << "\n";
    outfile << "committer CVS_SVN_GIT Mapper <cvs_svn_git@brlcad.org> " <<  commit_time << " +0000\n";

    std::string svn_id_commit_msg = std::string("Note SVN revision ") + std::to_string(rev);
    outfile << "data " << svn_id_commit_msg.length() << "\n";
    outfile << svn_id_commit_msg << "\n";


    std::string git_sha1_cmd = std::string("cd cvs_git && git show-ref refs/notes/commits > ../nsha1.txt && cd ..");
    if (std::system(git_sha1_cmd.c_str())) {
        std::cout << "git_sha1_cmd failed: refs/notes/commits\n";
        exit(1);
    }
    std::ifstream hfile("nsha1.txt");
    if (!hfile.good()) {
        std::cout << "couldn't open nsha1.txt\n";
        exit(1);
    }
    std::string line;
    std::getline(hfile, line);
    size_t spos = line.find_first_of(" ");
    std::string nsha1 = line.substr(0, spos);

    outfile << "from " << nsha1 << "\n";
    outfile << "N :1 " << gsha1 << "\n";
    outfile.close();

    std::string git_fi = std::string("cd cvs_git && cat ../") + fi_file + std::string(" | git fast-import && git reset --hard HEAD && cd ..");
    if (std::system(git_fi.c_str())) {
        std::cout << "Fatal - could not apply fi file to working repo " << fi_file << "\n";
        exit(1);
    }

}

int main(int argc, const char **argv)
{
    read_svn_info();
    read_git_info();
#if 1
    std::map<std::pair<std::string, long int>,long int>::iterator s_it;
    for (s_it = svn_msgtime_to_rev.begin(); s_it != svn_msgtime_to_rev.end(); s_it++) {
	std::map<std::pair<std::string, long int>,std::string>::iterator g_it;
	g_it = git_msgtime_to_sha1.find((*s_it).first);
	if (g_it != git_msgtime_to_sha1.end()) {
	    // Unique time,message mapping
	    std::cout << (*s_it).second << " -> " << (*g_it).second << "\n";
	    //write_note((*s_it).second , (*g_it).second, (*g_it).first.second);
	} else {
	    std::string cmsg = (*s_it).first.first;
	    if (svn_msg_non_unique.find(cmsg) == svn_msg_non_unique.end() &&
		    git_msg_non_unique.find(cmsg) == git_msg_non_unique.end()) {
		// Unique message
		std::map<std::string,long int>::iterator r_it = svn_msg_to_rev.find(cmsg);
		std::map<std::string,std::string>::iterator g2_it = git_msg_to_sha1.find(cmsg);
		if (r_it != svn_msg_to_rev.end() && g2_it != git_msg_to_sha1.end()) {
		    // Unique msg has matching revision, but not matching time
		    std::cout << (*r_it).second << " -> " << (*g2_it).second << " (time offset)\n";
		    //write_note((*r_it).second , (*g2_it).second, git_msg_to_time[cmsg]);
		} else {
		    if (git_time_to_msg.find((*s_it).first.second) != git_time_to_msg.end()) {
			// Unique msg is unmapped, but there is a matching timestamp in the git history
			long int timestamp = (*s_it).first.second;

			if (git_time_nonuniq.find(timestamp) != git_time_nonuniq.end()) {
			    std::cerr << (*s_it).first.second << " " << (*s_it).second << " [unique, unmapped, timestamp not unique in git] : " << cmsg << "\n";
			}

			// There are two timestamp ranges where SVN is known to be out of wack - in those ranges,
			// a timestamp match is not enough.  Otherwise, assume a match

			if ((timestamp > 524275754) && (timestamp < 625839678)) {
			    std::cerr << (*s_it).first.second << " " << (*s_it).second << " [unique, unmapped, timestamp in unreliable range] : " << cmsg << "\n";
			} else {
			    std::cout << (*s_it).first.second << " " << (*s_it).second << " [unique, unmapped, but timestamp match] : " << cmsg << " -> " << git_time_to_msg[(*s_it).first.second]  << "\n";
			    //write_note((*s_it).second , git_time_to_sha1[(*s_it).first.second], git_msg_to_time[cmsg]);
			}
		    } else {
			// Unique msg is unmapped, and there is no matching timestamp in the git history
			std::cerr << (*s_it).first.second << " " << (*s_it).second << " [unique, unmapped] : " << cmsg << "\n";
		    }
		}
	    } else {
		// Non-unique message
		if (git_time_to_msg.find((*s_it).first.second) != git_time_to_msg.end()) {
		    // Have a timestamp match, even though the non-unique git message doesn't match
		    long int timestamp = (*s_it).first.second;

		    if (git_time_nonuniq.find(timestamp) != git_time_nonuniq.end()) {
			std::cerr << (*s_it).first.second << " " << (*s_it).second << " [non-unique, timestamp not unique in git] : " << cmsg << "\n";
		    }

		    if (cmsg == std::string("Initial revision")) {
			if ((timestamp > 524275754) && (timestamp < 625839678)) {
			    std::cerr << (*s_it).second << " -> " << git_time_to_sha1[(*s_it).first.second] << " [\"Initial revision\" timestamp match, timestamp in unreliable range]\n";
			} else {
			    std::cout << (*s_it).second << " -> " << git_time_to_sha1[(*s_it).first.second] << " [\"Initial revision\" timestamp match]\n";
			    //write_note((*s_it).second , git_time_to_sha1[(*s_it).first.second], (*s_it).first.second);
			}
		    } else {
			if ((timestamp > 524275754) && (timestamp < 625839678)) {
			    std::cerr << (*s_it).first.second << " " << (*s_it).second << " [non-unique, has exact timestamp match, timestamp in unreliable range] : " << cmsg << " -> [" << git_time_to_sha1[(*s_it).first.second] << "] " << git_time_to_msg[(*s_it).first.second] << "\n";
			} else {
			    std::cerr << (*s_it).first.second << " " << (*s_it).second << " [non-unique, has exact timestamp match] : " << cmsg << " -> [" << git_time_to_sha1[(*s_it).first.second] << "] " << git_time_to_msg[(*s_it).first.second] << "\n";
			}
		    }
		} else {
		    std::cerr << (*s_it).first.second << " " << (*s_it).second << " [non-unique,no exact timestamp match] : " << cmsg << "\n";
		}
	    }
	}
    }
#endif

#if 0
    // Write out diffable timestamp sorted msg histories from git and svn
    std::map<std::pair<std::string, long int>,std::string>::iterator gm_it;
    std::ofstream gitatimes("git_all_times.txt", std::ios::out | std::ios::binary);
    for (gm_it = git_msgtime_to_sha1.begin(); gm_it != git_msgtime_to_sha1.end(); gm_it++) {
	gitatimes << (*gm_it).first.second << " " << (*gm_it).first.first << "\n";
    }
    gitatimes.close();

    std::map<long int,std::string>::iterator tmsg_it;
    std::ofstream gittimes("git_times.txt", std::ios::out | std::ios::binary);

    for (tmsg_it = git_time_to_msg.begin(); tmsg_it != git_time_to_msg.end(); tmsg_it++) {
	gittimes << (*tmsg_it).first << " " << (*tmsg_it).second << "\n";
    }
    gittimes.close();

    std::ofstream svntimes("svn_times.txt", std::ios::out | std::ios::binary);
    for (tmsg_it = svn_time_to_msg.begin(); tmsg_it != svn_time_to_msg.end(); tmsg_it++) {
	svntimes << (*tmsg_it).first << " " << (*tmsg_it).second << "\n";
    }
    svntimes.close();
#endif

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
