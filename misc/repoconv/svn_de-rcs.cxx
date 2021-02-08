#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <regex>
#include "md5.hpp"
#include "sha1.hpp"

#define sfcmp(_s1, _s2) _s1.compare(0, _s2.size(), _s2) && _s1.size() >= _s2.size()
#define svn_str(_s1, _s2) (!sfcmp(_s1, _s2)) ? _s1.substr(_s2.size(), _s1.size()-_s2.size()) : std::string()

std::map<std::string, std::string> md5_map;
std::map<std::string, std::string> sha1_map;

bool is_binary(const char *cstr, int len, std::string &npath)
{
    // https://stackoverflow.com/a/567918/2037687
    for (int i = 0; i < len; i++) {
	if (cstr[i] == '\0') {
	    return true;
	}
    }

    // We may be processing the file contents as C++ with regex - make
    // sure we can represent the contents successfully.
    std::string sbuff;
    sbuff.assign(cstr, len);
    if (sbuff.length() != len) {
	std::cout << "C++ string representation failed (size delta " << abs(sbuff.length() - len) << "): " << npath << "\n";
	return true;
    }

    return false;
}

bool skip_dercs(std::string &npath) {
    if (npath.find("/re2c/") != std::string::npos) return false;
    if (npath.find("/misc/Cakefile.defs") != std::string::npos) return false;
    if (npath.find("/misc/win32-msvc8") != std::string::npos) return false;
    if (npath.find("/misc/rcs2log") != std::string::npos) return false;
    if (npath.find("/misc/win32-msvc") != std::string::npos) return false;
    if (npath.find("/misc/archlinux/brlcad.install") != std::string::npos) return false;
    if (npath.find("/misc/brlcad.spec.in") != std::string::npos) return false;
    if (npath.find("/misc/") != std::string::npos) return true;
    if (npath.find("/src/other/step") != std::string::npos) return true;
    if (npath.find("/src/conv/step") != std::string::npos) return true;
    if (npath.find("/src/other/ext/stepcode") != std::string::npos) return true;
    if (npath.find("/ap242.exp") != std::string::npos) return true;
    return false;
}

std::string de_rcs(const char *cstr, int len)
{
    std::regex rcs_date("\\$Date:[^\\$;\"\n\r]*");
    std::regex rcs_header("\\$Header:[^\\$;\"\n\r]*");
    std::regex rcs_id("\\$Id:[^\\$;\"\n\r]*");
    std::regex rcs_log("\\$Log:[^\\$;\"\n\r]*");
    std::regex rcs_revision("\\$Revision:[^\\$;\"\n\r]*");
    std::regex rcs_source("\\$Source:[^\\$;\"\n\r]*");
    std::regex rcs_author("\\$Author:[^\\$;\"\n\r]*");
    std::regex rcs_locker("\\$Locker:[^\\$;\"\n\r]*");

    std::string buff01;
    buff01.assign(cstr, len);
    std::string buff02 = std::regex_replace(buff01, rcs_date, "$Date");
    std::string buff03 = std::regex_replace(buff02, rcs_header, "$Header");
    std::string buff04 = std::regex_replace(buff03, rcs_id, "$Id");
    std::string buff05 = std::regex_replace(buff04, rcs_log, "$Log");
    std::string buff06 = std::regex_replace(buff05, rcs_revision, "$Revision");
    std::string buff07 = std::regex_replace(buff06, rcs_source, "$Source");
    std::string buff08 = std::regex_replace(buff07, rcs_author, "$Author");
    std::string buff09 = std::regex_replace(buff08, rcs_locker, "$Locker");

#if 0
    if (buff01 != buff09) {
	std::ofstream ofile("orig.f", std::ios::out | std::ios::binary);
	ofile.write(cstr, len);
	ofile.close();
	std::ofstream nfile("new.f", std::ios::out | std::ios::binary);
	nfile << buff09;
	nfile.close();

	std::cout << "RCS stripping complete.\n";
    }
#endif
    return buff09;
}

long int svn_lint(std::string s1, std::string s2)
{
    if (!s1.length() || !s2.length()) return -1;
    return std::stol(svn_str(s1, s2));
}

/* Newer subversion doesn't like non-LF line endings in properties,
 * so strip them out */
void
skip_rev_props(std::ifstream &infile, std::ofstream &outfile)
{
    std::string kkey("K ");
    std::string pend("PROPS-END");
    std::string line;

    // Go until we hit PROPS-END
    while (std::getline(infile, line) && line.compare(pend)) {
	// K <N> line is the trigger
	std::replace(line.begin(), line.end(), '\r', '\n');
	outfile << line << "\n";
	std::string key = svn_str(line, kkey);
	if (!key.length()) continue;

	// Key associated with K line and value
	std::getline(infile, key);
	std::replace(key.begin(), key.end(), '\r', '\n');
	outfile << key << "\n";
	std::getline(infile, line);
	std::replace(line.begin(), line.end(), '\r', '\n');
	outfile << line << "\n";
    }
    outfile << "PROPS-END\n";
}


/* Newer subversion doesn't like non-LF line endings in properties,
 * so strip them out */
void
skip_node_props(std::ifstream &infile, std::vector<std::string> &node_lines)
{
    std::string kkey("K ");
    std::string pend("PROPS-END");
    std::string line;

    // Go until we hit PROPS-END
    while (std::getline(infile, line)) {
	std::replace(line.begin(), line.end(), '\r', '\n');
	node_lines.push_back(line);

	// If we get PROPS-END, we're done
	if (!line.compare(pend)) {
	    return;
	}

	// K <N> line is the trigger
	std::string key = svn_str(line, kkey);
	if (!key.length()) continue;

	// Key and value line associated with K line
	std::getline(infile, key);
	std::replace(key.begin(), key.end(), '\r', '\n');
	node_lines.push_back(key);
	std::getline(infile, line);
	std::replace(line.begin(), line.end(), '\r', '\n');
	node_lines.push_back(line);
    }
}

int curr_md5_line(std::string line, std::string key, std::ofstream &outfile)
{
    if (!sfcmp(line, key))  {
	std::map<std::string, std::string>::iterator m_it;
	std::string old_md5 = svn_str(line, key);
	m_it = md5_map.find(old_md5);
	if (m_it != md5_map.end()) {
	    outfile << key << m_it->second << "\n";
	} else {
	    outfile << line << "\n";
	}
	return 1;
    }
    return 0;
}


int curr_sha1_line(std::string line, std::string key, std::ofstream &outfile)
{
    if (!sfcmp(line, key))  {
	std::map<std::string, std::string>::iterator m_it;
	std::string old_sha1 = svn_str(line, key);
	m_it = sha1_map.find(old_sha1);
	if (m_it != sha1_map.end()) {
	    outfile << key << m_it->second << "\n";
	} else {
	    outfile << line << "\n";
	}
	return 1;
    }
    return 0;
}



int curr_md5_line2(std::string line, std::string key, std::string &oval, std::vector<std::string> &node_lines)
{
    if (!sfcmp(line, key))  {
	std::map<std::string, std::string>::iterator m_it;
	std::string old_md5 = svn_str(line, key);
	if (old_md5.length()) {
	    m_it = md5_map.find(old_md5);
	    if (m_it != md5_map.end()) {
		std::string nline = key + m_it->second;
		node_lines.push_back(nline);
		oval = m_it->second;
	    } else {
		oval = old_md5;
		node_lines.push_back(line);
	    }
	} else {
	    oval = old_md5;
	    node_lines.push_back(line);
	}
	return 1;
    }
    return 0;
}


int curr_sha1_line2(std::string line, std::string key, std::string &oval, std::vector<std::string> &node_lines)
{
    if (!sfcmp(line, key))  {
	std::map<std::string, std::string>::iterator m_it;
	std::string old_sha1 = svn_str(line, key);
	m_it = sha1_map.find(old_sha1);
	if (old_sha1.length()) {
	    if (m_it != sha1_map.end()) {
		std::string nline = key + m_it->second;
		node_lines.push_back(nline);
		oval = m_it->second;
	    } else {
		oval = old_sha1;
		node_lines.push_back(line);
	    }
	} else {
	    oval = old_sha1;
	    node_lines.push_back(line);
	}
	return 1;
    }
    return 0;
}




/* Return 1 if we successfully processed a node, else 0 */
int
process_node(std::ifstream &infile, std::ofstream &outfile)
{
    std::string text_copy_source_md5;
    std::string text_copy_source_sha1;
    std::string text_content_md5;
    std::string text_content_sha1;
    long int text_content_length = 0;
    long int prop_content_length = 0;
    long int content_length = 0;
    std::string npath("");
    std::string rkey("Revision-number: ");
    std::string npkey("Node-path: ");
    std::string pclkey("Prop-content-length: ");
    std::string tcsmkey("Text-copy-source-md5: ");
    std::string tcsskey("Text-copy-source-sha1: ");
    std::string tcmkey("Text-content-md5: ");
    std::string tcskey("Text-content-sha1: ");
    std::string tclkey("Text-content-length: ");
    std::string clkey("Content-length: ");
    std::string line;
    std::vector<std::string> node_lines;
    std::vector<std::string>::iterator nl_it;

    // Find node path, or bail if we hit a new revision first
    size_t line_start = infile.tellg();
    while (!npath.length() && std::getline(infile, line)) {
	if (!sfcmp(line, rkey)) {
	    infile.seekg(line_start);
	    return -1;  // Done with revision
	}
	node_lines.push_back(line);
	npath = svn_str(line, npkey);
    }

    // If no node path, no node and presumably the end of the revision
    if (!npath.length()) return -1;


    // Have a path, so we're in a node. Find node contents, or bail if we hit a
    // new revision/path
    while (std::getline(infile, line)) {

	// If we hit an empty line, we're done with the node itself
	// and its down to properties and content, if any.
	if (!line.length()) break;

	if (!sfcmp(line, rkey)) {
	    return -1;  // Done with revision
	}
	if (!sfcmp(line, npkey)) {
	    return 1; // Done with node
	}

	// Have path, get guts.
	if (curr_md5_line2(line, tcsmkey, text_copy_source_md5, node_lines)) {
	    continue;
	}
	if (curr_sha1_line2(line, tcsskey, text_copy_source_sha1, node_lines)) {
	    continue;
	}
	if (!sfcmp(line, tcmkey))  {
	    text_content_md5 = svn_str(line, tcmkey);
	    node_lines.push_back(line);
	    continue;
	}
	if (!sfcmp(line, tcskey))  {
	    text_content_sha1 = svn_str(line, tcskey);
	    node_lines.push_back(line);
	    continue;
	}
	if (!sfcmp(line, tclkey))  {
	    text_content_length = svn_lint(line, tclkey);
	    node_lines.push_back(line);
	    continue;
	}
	if (!sfcmp(line, clkey))  {
	    content_length = svn_lint(line, clkey);
	    node_lines.push_back(line);
	    node_lines.push_back(std::string(""));
	    continue;
	}
	if (!sfcmp(line, pclkey))  {
	    prop_content_length = svn_lint(line, pclkey);
	    node_lines.push_back(line);
	    continue;
       	}

	node_lines.push_back(line);
    }

    // If we have properties, skip beyond them
    if (prop_content_length > 0) {
	skip_node_props(infile, node_lines);
    }

    // If we have neither properties nor content, we're done
    if (!prop_content_length && !text_content_length) {
	for (nl_it = node_lines.begin(); nl_it != node_lines.end(); nl_it++) {
	    outfile << *nl_it << "\n";
	}
	outfile << "\n";
	return 1;
    }

    // If we have content, store the file offset, process the content
    // for possible RCS edits, set up the new values for md5 and sha1,
    // and jump the seek beyond the old content.
    char *buffer = NULL;
    std::string new_content("");
    size_t oldpos;
    size_t after_content;
    if (text_content_length > 0) {
	oldpos = infile.tellg();
	after_content = oldpos + text_content_length + 1;
	buffer = new char [text_content_length];
	infile.read(buffer, text_content_length);
	infile.seekg(after_content);
    }

    if (buffer && !skip_dercs(npath)) {
	if (!is_binary(buffer, text_content_length, npath)) {
	    std::string calc_md5 = md5_hash_hex(buffer, text_content_length);
	    std::string calc_sha1 = sha1_hash_hex(buffer, text_content_length);
	    if (text_content_md5 != calc_md5 || text_content_sha1 != calc_sha1) {
		std::cout << "Stored vs. calculated mismatch: " << npath << "\n";
		std::cout << "Read md5       : " << text_content_md5 << "\n";
		std::cout << "Calculated md5 : " << calc_md5 << "\n";
		std::cout << "Read sha1      : " << text_content_sha1 << "\n";
		std::cout << "Calculated sha1: " << calc_sha1 << "\n";
		/*
		   if (npath == std::string("brlcad/trunk/misc/vfont/fix.6r")) {
		   std::ofstream cfile("fix-extracted.6r", std::ios::out | std::ios::binary);
		   cfile.write(buffer, text_content_length);
		   cfile.close();
		   }
		   */
	    }
	    new_content = de_rcs(buffer, text_content_length);
	    std::string new_md5 = md5_hash_hex(new_content.c_str(), new_content.length());
	    std::string new_sha1 = sha1_hash_hex(new_content.c_str(), new_content.length());
	    if (text_content_md5 != new_md5 || text_content_sha1 != new_sha1) {
		std::cout << "Altered: " << npath << "\n";
		std::cout << "Original md5   : " << text_content_md5 << "\n";
		std::cout << "Calculated md5 : " << new_md5 << "\n";
		std::cout << "Original sha1  : " << text_content_sha1 << "\n";
		std::cout << "Calculated sha1: " << new_sha1 << "\n";
		md5_map.insert(std::pair<std::string,std::string>(text_content_md5, new_md5));
		sha1_map.insert(std::pair<std::string,std::string>(text_content_sha1, new_sha1));
	    }
	}
    }

#if 0
    std::regex cvsignore(".*cvsignore$");

    if (!std::regex_match(npath, cvsignore)) {
#endif
	// Write out the node lines and content.
	std::map<std::string, std::string>::iterator m_it;
	for (nl_it = node_lines.begin(); nl_it != node_lines.end(); nl_it++) {
	    if (skip_dercs(npath)) {
		outfile << *nl_it << "\n";
		continue;
	    }
	    line = *nl_it;
	    // Text-copy-source-md5
	    if (curr_md5_line(line, tcsmkey, outfile)) {
		continue;
	    }
	    // Text-copy-source-sha1
	    if (curr_sha1_line(line, tcsskey, outfile)) {
		continue;
	    }

	    // Text-content-md5
	    if (curr_md5_line(line, tcmkey, outfile)) {
		continue;
	    }

	    // Text-content-sha1
	    if (curr_sha1_line(line, tcskey, outfile)) {
		continue;
	    }

	    // Text-content-length
	    if (!sfcmp(line, tclkey))  {
		if (new_content.length()) {
		    outfile << "Text-content-length: " << new_content.length() << "\n";
		} else {
		    outfile << *nl_it << "\n";
		}
		continue;
	    }

	    // Content-length
	    if (!sfcmp(line, clkey))  {
		if (new_content.length()) {
		    outfile << "Content-length: " << new_content.length() + prop_content_length << "\n";
		} else {
		    outfile << *nl_it << "\n";
		}
		continue;
	    }

	    outfile << *nl_it << "\n";
	}
	if (buffer) {
	    if (new_content.length()) {
		outfile << new_content;
	    } else {
		outfile.write(buffer, text_content_length);
	    }
	    outfile << "\n";
	}
#if 0
    } else {
	std::cout << "Skipping " << npath << "\n";
    }
#endif

    if (buffer) {
	delete buffer;
    }

    return 1;
}

/* Return 1 if we successfully processed a revision, else 0 */
int
process_revision(std::ifstream &infile, std::ofstream &outfile)
{
    std::string rkey("Revision-number: ");
    std::string ckey("Content-length: ");
    int node_ret = 0;
    int success = 0;
    std::string line;
    long int revision_number = -1;

    while (revision_number < 0) {
	if (!std::getline(infile, line)) return success; // No rkey and no input, no revision
	outfile << line << "\n";
	outfile.flush();
	if (!sfcmp(line, rkey)) revision_number = svn_lint(line, rkey);
    }
    success = 1; // For the moment, finding the revision is enough to qualify as success...

    // "Usually" a revision will have properties, but they are apparently not
    // technically required.  For revision properties Content-length and
    // Prop-content-length will always match if non-zero, and Content-length
    // appears to be requried by the dump file spec, so just find and use
    // Content-length
    long int rev_prop_length = -1;
    while (rev_prop_length < 0) {
	if (!std::getline(infile, line)) return success; // Rev num but no contents, no revision
	outfile << line << "\n";
	outfile.flush();
	if (!sfcmp(line, ckey)) rev_prop_length = svn_lint(line, ckey);
    }
    if (rev_prop_length) skip_rev_props(infile, outfile);

    //std::cerr << "Revision-number: " << revision_number << ", prop length " << rev_prop_length << std::endl;

    /* Have revision number - grab nodes until we spot another one */
    while (node_ret != -1 && infile.peek() != EOF) {
	node_ret = process_node(infile, outfile);
    }

    outfile.flush();
    std::cout << "Processed r" << revision_number << "\n";

    return success;
}

int
main(int argc, const char **argv)
{
    std::string uuid;
    long int dump_format_version = -1;
    std::ifstream infile(argv[1]);
    std::ofstream outfile(argv[2], std::ios::out | std::ios::binary);
    if (!infile.good()) return -1;
    if (!outfile.good()) return -1;
    std::string line;
    std::string fmtkey("SVN-fs-dump-format-version: ");
    // The first non-empty line has to be the format version, or we're done.
    while (std::getline(infile, line) && !line.length()) {
	outfile << line;
    };
    if (line.compare(0, fmtkey.length(), fmtkey)) {
	return -1;
    }
    outfile << line << "\n";

    // Grab the format number
    dump_format_version = svn_lint(line, fmtkey);

    while (!uuid.length() && std::getline(infile, line)) {
	outfile << line << "\n";
	uuid = svn_str(line, std::string("UUID: "));
    }

    outfile.flush();

    /* As long as we're not done, read revisions */
    while (infile.peek() != EOF) {
	process_revision(infile, outfile);
    }

    outfile << "\n";
    outfile << "\n";

    infile.close();
    outfile.close();

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
