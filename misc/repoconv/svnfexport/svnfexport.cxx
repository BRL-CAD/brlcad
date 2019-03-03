#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <locale>
#include <iomanip>
#include "sha1.hpp"
#include "svn_date.h"

long int starting_rev;

enum svn_node_kind_t { nkerr, nfile, ndir };
enum svn_node_action_t { naerr, nchange, nadd, ndelete, nreplace };

struct svn_node {
    /* Dump file contents */
    long int revision_number;
    std::string path;
    svn_node_kind_t kind;
    svn_node_action_t action;
    long int copyfrom_rev;
    std::string copyfrom_path;
    std::string text_copy_source_md5;
    std::string text_copy_source_sha1;
    std::string text_content_md5;
    std::string text_content_sha1;
    long int text_content_length;
    long int prop_content_length;
    long int content_length;
    std::map<std::string, std::string> node_props;
    size_t content_start; // offset position in dump file
    /* Information from analysis */
    int exec_path;
    int exec_change;
    int crlf_content;
    int move_edit;
    int tag_edit;
    int branch_add;
    int branch_delete;
    int tag_add;
    int tag_delete;
    int tag_path;
    std::string project;
    std::string branch;
    std::string tag;
    std::string local_path;
};

struct svn_revision {
    /* Dump file contents */
    long int revision_number;
    std::string project;
    std::string author;
    std::string timestamp;
    std::string commit_msg;
    std::vector<struct svn_node> nodes;
    /* Information from analysis */
    std::set<std::string> merges; // TODO - can this info live at the node level?
    int move_edit;  // Has revision level implications - set if any child nodes are set
    int tag_edit; // Has revision level implications - set if any child nodes are set
    int merge;
    std::string merged_from;
    long int merged_rev;
};


std::map<std::string,std::pair<size_t, long int>> sha1_blobs;
std::set<std::string> exec_paths;
std::map<std::string,std::string> current_sha1;
std::map<std::string,std::string> branch_head_ids;
std::map<std::string,std::string> tag_ids;
std::map<std::string,std::string> svn_sha1_to_git_sha1;

std::set<std::string> cvs_blob_sha1;
std::map<std::string,std::string> author_map;

std::map<std::string, std::map<long int, std::set<struct svn_node *> *>> path_states;
std::map<std::string, std::map<long int, std::set<struct svn_node *> *>>::iterator ps_it;
std::set<long int> rebuild_revs;


int verify_repos(long int rev, std::string branch_svn, std::string branch_git)
{
    std::string cleanup_cmd = std::string("rm -rf brlcad_svn_checkout cvs_git_working brlcad_git_checkout");
    std::string svn_cmd = std::string("svn co -q -r") + std::to_string(rev) + std::string(" file:///home/cyapp/brlcad_repo/repo_dercs/brlcad/") + branch_svn + std::string(" brlcad_svn_checkout");
    std::string svn_emptydir_rm = std::string("cd brlcad_svn_checkout && find . -type d -empty -print0 |xargs -0 rmdir && cd ..");
    std::string git_setup = std::string("rm -rf cvs_git_working && cp -r cvs_git cvs_git_working");
    std::string git_fi = std::string("cd cvs_git_working && cat ../brlcad-svn-export.fi | git fast-import && git reset --hard HEAD && cd ..");
    std::string git_clone = std::string("git clone --single-branch --branch ") + branch_git + std::string(" ./cvs_git_working/.git brlcad_git_checkout");
    std::string repo_diff = std::string("diff -qrw -I '\\$Id' -I '\\$Revision' -I'\\$Header' -I'$Source' -I'$Date' -I'$Log' -I'$Locker' --exclude \".cvsignore\" --exclude \".gitignore\" --exclude \"terra.dsp\" --exclude \".git\" --exclude \".svn\" brlcad_svn_checkout brlcad_git_checkout");
    std::cout << "Verifying r" << rev << ", branch " << branch_svn << "\n";
    std::system(cleanup_cmd.c_str());
    std::system(svn_cmd.c_str());
    std::system(svn_emptydir_rm.c_str());
    std::system(git_setup.c_str());
    if (rev > starting_rev) {
	std::system(git_fi.c_str());
    }
    std::system(git_clone.c_str());
    int diff_ret = std::system(repo_diff.c_str());
    if (diff_ret) {
	std::cout << "diff test failed, r" << rev << ", branch " << branch_svn << "\n";
	exit(1);
    }
    std::system(cleanup_cmd.c_str());
    return 0;
}



/* Branches */
std::set<std::string> branches;
/* Tags */
std::set<std::string> tags;

/* Subversion allows as a technical matter the editing of tags, since
 * they are just more paths copied into specific directories.  Build
 * up some sets with relevant information */
std::set<std::string> edited_tags;
/* The last subversion revision that makes a change to the tag */
std::map<std::string, long int> edited_tag_max_rev;
std::map<std::string, long int> edited_tag_first_rev;

std::map<long int, struct svn_revision> revs;


/* Holds strings identifying all known projects in the svn repo */
std::set<std::string> valid_projects;

std::set<long int> brlcad_revs;

std::map<std::string, std::string> branch_mappings;
std::map<std::string, std::string> tag_mappings;

#define sfcmp(_s1, _s2) _s1.compare(0, _s2.size(), _s2) && _s1.size() >= _s2.size()
#define svn_str(_s1, _s2) (!sfcmp(_s1, _s2)) ? _s1.substr(_s2.size(), _s1.size()-_s2.size()) : std::string()
#define dir_is_root(_dir, _fpath) !_fpath.compare(0, _dir.size(), _dir) && _fpath.size() > _dir.size() && _fpath.c_str()[_dir.size()] == '/'

std::string dir_parent(std::string curr_path)
{
    size_t spos = curr_path.find_last_of("/");
    if (spos != std::string::npos) {
	return curr_path.substr(0, spos);
    } else {
	return std::string("");
    }
}


svn_node_kind_t svn_node_kind(std::string nk)
{
    std::string nf("file");
    std::string nd("dir");
    if (!nk.compare(nf)) return nfile;
    if (!nk.compare(nd)) return ndir;
    std::cerr << "Error - unknown node kind: " << nk << std::endl;
    return nkerr;
}

std::string print_node_kind(svn_node_kind_t nt)
{
    if (nt == nfile) return std::string("file");
    if (nt == ndir) return std::string("dir");
    std::cerr << "Error - unknown node kind: " << nt << std::endl;
    return std::string("");
}
long int svn_lint(std::string s1, std::string s2)
{
    if (!s1.length() || !s2.length()) return -1;
    return std::stol(svn_str(s1, s2));
}

svn_node_action_t svn_node_action(std::string naction)
{
    std::string nc("change");
    std::string na("add");
    std::string nd("delete");
    std::string nr("replace");
    if (!naction.compare(nc)) return nchange;
    if (!naction.compare(na)) return nadd;
    if (!naction.compare(nd)) return ndelete;
    if (!naction.compare(nr)) return nreplace;
    std::cerr << "Error - unknown node action: " << naction << std::endl;
    return naerr;
}

std::string print_node_action(svn_node_action_t nat)
{
    if (nat == nchange) return std::string("change");
    if (nat == nadd) return std::string("add");
    if (nat == ndelete) return std::string("delete");
    if (nat == nreplace) return std::string("replace");
    return std::string("unknown");
}


void
print_node(struct svn_node &n)
{
    std::cout << "Revision: " << n.revision_number << std::endl;
    std::cout << "Node-path: " << n.path << std::endl;
    std::cout << "Node-kind: " << print_node_kind(n.kind) << std::endl;
    std::cout << "Node-action: " << print_node_action(n.action) << std::endl;
    std::cout << "Node-copyfrom-rev: " << n.copyfrom_rev << std::endl;
    std::cout << "Node-copyfrom-path: " << n.copyfrom_path << std::endl;
    std::cout << "Text-copy-source-md5: " << n.text_copy_source_md5 << std::endl;
    std::cout << "Text-copy-source-sha1: " << n.text_copy_source_sha1  << std::endl;
    std::cout << "Text-content-md5: " << n.text_content_md5 << std::endl;
    std::cout << "Text-content-sha1: " << n.text_content_sha1 << std::endl;
    std::cout << "Text-content-length: " << n.text_content_length << std::endl;
    std::cout << "Prop-content-length: " << n.prop_content_length << std::endl;
    std::cout << "Content-length: " << n.content_length << std::endl;
    if (n.node_props.size() > 0) {
	std::cout << "Properies (" << n.node_props.size() << "): " << n.content_length << std::endl;
	std::map<std::string, std::string>::iterator m_it;
	for (m_it = n.node_props.begin(); m_it != n.node_props.end(); m_it++) {
	    std::cout << "   " << m_it->first << " -> " << m_it->second << std::endl;
	}
    }
    std::cout << "exec_path: " << n.exec_path << std::endl;
    std::cout << "exec_change: " << n.exec_change << std::endl;
    std::cout << "crlf_content: " << n.crlf_content << std::endl;
    std::cout << "project: " << n.project << std::endl;
    std::cout << "branch: " << n.branch << std::endl;
    std::cout << "local_path: " << n.local_path << std::endl;
}

// Don't care about deletes on old tags...
bool reject_tag(std::string rtag)
{
    if (rtag == std::string("rel-4-5")) return true;
    if (rtag == std::string("ctj-4-5-post")) return true;
    if (rtag == std::string("ctj-4-5-pre")) return true;
    if (rtag == std::string("rel-5-0")) return true;
    if (rtag == std::string("rel-5-0-beta")) return true;
    if (rtag == std::string("rel-5-1")) return true;
    if (rtag == std::string("rel-5-2")) return true;
    if (rtag == std::string("offsite-5-3-pre")) return true;
    if (rtag == std::string("rel-5-3")) return true;
    if (rtag == std::string("rel-5-4")) return true;
    return false;
}

std::string git_sha1(std::ifstream &infile, struct svn_node *n)
{
    std::string git_sha1;
    std::string go_buff;
    char *cbuffer = new char [n->text_content_length];;
    infile.read(cbuffer, n->text_content_length);
    if (n->text_content_sha1 == std::string("71fc76a4374348f844c480a375d594cd10835ab9")) {
	std::cout << "working " << n->path << ", rev " << n->revision_number << "\n";
    }
    go_buff.append("blob ");

    if (!n->crlf_content) {
	go_buff.append(std::to_string(n->text_content_length));
	go_buff.append(1, '\0');
	go_buff.append(cbuffer, n->text_content_length);
	git_sha1 = sha1_hash_hex(go_buff.c_str(), go_buff.length());
    } else {
	std::string crlf_buff;
	std::ostringstream cf(crlf_buff);
	for (int i = 0; i < n->text_content_length; i++) {
	    if (cbuffer[i] == '\n' && (i == 0 || cbuffer[i-1] != '\r')) {
		cf << '\r' << '\n';
	    } else {
		cf << cbuffer[i];
	    }
	}
	std::string crlf_file = cf.str();
	go_buff.append(std::to_string(crlf_file.length()));
	go_buff.append(1, '\0');
	go_buff.append(crlf_file.c_str(), crlf_file.length());
	git_sha1 = sha1_hash_hex(go_buff.c_str(), go_buff.length());
    }
    delete[] cbuffer;
    return git_sha1;
}

//Need to write blobs with CRLF if that's the mode we're in...
void
write_blob(std::ifstream &infile, std::ofstream &outfile, struct svn_node &node)
{
    char *buffer = new char [node.text_content_length];
    infile.seekg(node.content_start);
    infile.read(buffer, node.text_content_length);
    outfile << "blob\n";
    //std::cout << "	Adding node object for " << node.local_path << "\n";
    if (!node.crlf_content) {
	outfile << "data " << node.text_content_length << "\n";
	outfile.write(buffer, node.text_content_length);
    } else {
	std::string crlf_buff;
	std::ostringstream cf(crlf_buff);
	for (int i = 0; i < node.text_content_length; i++) {
	    if (buffer[i] == '\n' && (i == 0 || buffer[i-1] != '\r')) {
		cf << '\r' << '\n';
	    } else {
		cf << buffer[i];
	    }
	}
	std::string crlf_file = cf.str();
	outfile << "data " << crlf_file.length() << "\n";
	outfile.write(crlf_file.c_str(), crlf_file.length());
    }
    delete[] buffer;
}

std::string
branch_head_id(std::string branch, int rev){
    // If strlen of current head == 40, it's a sha1 and just return it.
    //
    // else, it's a mark and return the string with a colon in front.
    std::string curr_head = branch_head_ids[branch];
    if (!curr_head.length()) {
	std::cout << "Error - unknown branch " << branch << ", " << rev << "\n";
	exit(1);
    }
    if (curr_head.length() == 40) return curr_head;
    std::string head_mark = std::string(":") + curr_head;
    return head_mark;
}

/* Read revision properties.  Technically these are optional,
 * so don't bother with a return code - just get what we can. */
void
get_rev_props(std::ifstream &infile, struct svn_revision *rev)
{
    std::string kkey("K ");
    std::string vkey("V ");
    std::string sauth("svn:author");
    std::string sdate("svn:date");
    std::string slog("svn:log");
    std::string pend("PROPS-END");
    std::string line;

    // Go until we hit PROPS-END
    while (std::getline(infile, line) && line.compare(pend)) {
	// K <N> line is the trigger
	std::string key = svn_str(line, kkey);
	if (!key.length()) continue;

	// Key associated with K line
	std::getline(infile, key);

	// Now we need a value - bail if we suddenly get PROPS-END
	// or something other than a V <N> line
	std::getline(infile, line);
	std::string value = svn_str(line, vkey);
	if (!line.compare(pend) || !value.length()) {
	    std::cerr << "Error (r" << rev->revision_number << ") - key " << key << " has no associated value" << std::endl;
	    return;
	}
	long int v = std::stol(value);

	// Value associated with V line
	char vbuff[v+1];
	infile.read(vbuff, v);
	vbuff[v] = '\0';
	value = std::string(vbuff);

	// Have key value
	if (!key.compare(sauth)) rev->author = value;
	if (!key.compare(sdate)) rev->timestamp = value;
	if (!key.compare(slog)) rev->commit_msg = value;

	// If there is any need to store revision properties other than the
	// three standard entries, we would do so below.  For now, just print a
	// message for awareness
	if (key.compare(sauth) && key.compare(sdate) && key.compare(slog)) {
	    std::cerr << "Unknown key/value pair (r" << rev->revision_number << "): " << key << " -> " << value << std::endl;
	}
    }
}


/* Read node properties.  These are optional, so don't bother with a return
 * code - just get what we can. */
void
get_node_props(std::ifstream &infile, struct svn_node *n)
{
    std::string kkey("K ");
    std::string vkey("V ");
    std::string pend("PROPS-END");
    std::string line;
    n->exec_path = 0;
    n->exec_change = 0;
    n->crlf_content = 0;
    // Go until we hit PROPS-END
    while (std::getline(infile, line)) {

	// If we get PROPS-END, we're done
	if (!line.compare(pend)) break;

	// K <N> line is the trigger
	std::string key = svn_str(line, kkey);
	if (!key.length()) continue;

	// Key associated with K line
	std::getline(infile, key);

	if (!key.compare(std::string("svn:executable"))) {
	    n->exec_path = 1;
	}

	// Now we need a value - bail if we suddenly get PROPS-END
	// or something other than a V <N> line
	std::getline(infile, line);
	std::string value = svn_str(line, vkey);
	if (!line.compare(pend) || !value.length()) {
	    std::cerr << "Error (r" << n->revision_number << " node " << n->path << ") - key " << " has no associated value" << std::endl;
	    return;
	}
	long int v = std::stol(value);

	// Value associated with V line
	char vbuff[v+1];
	infile.read(vbuff, v);
	vbuff[v] = '\0';
	value = std::string(vbuff);


	if (!key.compare(std::string("svn:eol-style")) && value.compare(std::string("native"))) {
	    n->crlf_content = 1;
	    //std::cerr << "Warning (r" << n->revision_number << " node " << n->path << ") - key " << " has line ending style " << value << std::endl;
	}

	// Have key value
	n->node_props[key] = value;
    }

    // Make the exec_paths set match the path's current prop state.
    std::set<std::string>::iterator e_it = exec_paths.find(n->path);
    if ((n->exec_path && e_it == exec_paths.end()) || (!n->exec_path && e_it != exec_paths.end())) {
	n->exec_change = 1;
    }
    if (n->exec_path) {
	exec_paths.insert(n->path);
    } else {
	exec_paths.erase(n->path);
    }
}


/* Return 1 if we successfully processed a node, else 0 */
int
process_node(std::ifstream &infile, struct svn_revision *rev)
{
    std::string rkey("Revision-number: ");
    std::string npkey("Node-path: ");
    std::string nkkey("Node-kind: ");
    std::string nakey("Node-action: ");
    std::string ncrkey("Node-copyfrom-rev: ");
    std::string ncpkey("Node-copyfrom-path: ");
    std::string tcsmkey("Text-copy-source-md5: ");
    std::string tcsskey("Text-copy-source-sha1: ");
    std::string tcmkey("Text-content-md5: ");
    std::string tcskey("Text-content-sha1: ");
    std::string tclkey("Text-content-length: ");
    std::string pclkey("Prop-content-length: ");
    std::string clkey("Content-length: ");
    size_t offset = infile.tellg();
    struct svn_node n = {};
    std::string line;

    n.revision_number = rev->revision_number;

    // Find node path, or bail if we hit a new revision first
    while (!n.path.length() && std::getline(infile, line)) {
	if (!sfcmp(line, rkey)) {
	    infile.seekg(offset);  // We found another rev line, reset
	    return -1;  // Done with revision
	}
	n.path = svn_str(line, npkey);
    }

    // If no node path, no node and presumably the end of the revision
    if (!n.path.length()) return -1;

    // Have a path, so we're in a node. Find node contents, or bail if we hit a
    // new revision/path 
    while (std::getline(infile, line)) {

	// If we hit an empty line, we're done with the node itself
	// and its down to properties and content, if any.
	if (!line.length()) break;

	if (!sfcmp(line, rkey)) {
	    infile.seekg(offset);  // reset
	    rev->nodes.push_back(n);
	    return -1;  // Done with revision
	}
	if (!sfcmp(line, npkey)) {
	    infile.seekg(offset);  // reset
	    rev->nodes.push_back(n);
	    return 1; // Done with node
	}

	// Have path, get guts.
	if (!sfcmp(line, nkkey))  { n.kind = svn_node_kind(svn_str(line, nkkey)); continue; }
	if (!sfcmp(line, nakey))  { n.action = svn_node_action(svn_str(line, nakey)); continue; }
	if (!sfcmp(line, ncrkey))  { n.copyfrom_rev = svn_lint(line, ncrkey); continue; }
	if (!sfcmp(line, ncpkey))  { n.copyfrom_path = svn_str(line, ncpkey); continue; }
	if (!sfcmp(line, tcsmkey))  { n.text_copy_source_md5 = svn_str(line, tcsmkey); continue; }
	if (!sfcmp(line, tcsskey))  { n.text_copy_source_sha1 = svn_str(line, tcsskey); continue; }
	if (!sfcmp(line, tcmkey))  { n.text_content_md5 = svn_str(line, tcmkey); continue; }
	if (!sfcmp(line, tcskey))  { n.text_content_sha1 = svn_str(line, tcskey); continue; }
	if (!sfcmp(line, tclkey))  { n.text_content_length = svn_lint(line, tclkey); continue; }
	if (!sfcmp(line, pclkey))  { n.prop_content_length = svn_lint(line, pclkey); continue; }
	if (!sfcmp(line, clkey))  { n.content_length = svn_lint(line, clkey); continue; }

	// Successfully processed a line as part of the current node
	offset = infile.tellg();
    }

    // If we have properties, read them
    if (n.prop_content_length > 0) {
	get_node_props(infile, &n);
    }



    // If we have content, store the file offset and jump the seek beyond it
    if (n.text_content_length > 0) {
	/* Figure out how Git will label this blob */
	n.content_start = infile.tellg();
	std::string gsha1 = git_sha1(infile, &n);
	svn_sha1_to_git_sha1[n.text_content_sha1] = gsha1;
	/* Stash information on how to read the blob */
	sha1_blobs.insert(std::pair<std::string,std::pair<size_t, long int>>(n.text_content_sha1, std::pair<size_t, long int>(n.content_start, n.text_content_length)));
	/* Jump over the content and continue */
	size_t after_content = n.content_start + n.text_content_length + 1;
	infile.seekg(after_content);
    }


    // the commit after the starting_rev is where we will be starting our fast-export generation - until that point,
    // keep current_sha1 up to date on a rolling basis
    if (rev->revision_number <= starting_rev) {
	if (n.text_content_sha1.length()) {
	    current_sha1[n.path] = n.text_content_sha1;
	}
    }

#if 0
    if (n.path == "brlcad/trunk/autogen.sh") {
	std::cout << n.path << " " << rev->revision_number << ": " << svn_sha1_to_git_sha1[n.text_content_sha1] << "\n";
	std::cout << "curr sha1: " << n.path << " " << rev->revision_number << ": " << current_sha1[n.path] << "\n";
    }

    if (n.path == "brlcad/branches/STABLE/autogen.sh") {
	std::cout << n.path << " " << rev->revision_number << ": " << svn_sha1_to_git_sha1[n.text_content_sha1] << "\n";
	std::cout << "curr sha1: " << n.path << " " << rev->revision_number << ": " << current_sha1[n.path] << "\n";
    }
#endif

    // Have at least some node contents (last node in file?), return
    rev->nodes.push_back(n);
    return 1;
}

/* Return 1 if we successfully processed a revision, else 0 */
int
process_revision(std::ifstream &infile)
{
    std::string rkey("Revision-number: ");
    std::string ckey("Content-length: ");
    int node_ret = 0;
    int success = 0;
    std::string line;
    struct svn_revision rev = {};

    rev.revision_number = -1;
    while (rev.revision_number < 0) {
	if (!std::getline(infile, line)) return success; // No rkey and no input, no revision
	if (!sfcmp(line, rkey)) rev.revision_number = svn_lint(line, rkey);
    }
    success = 1; // For the moment, finding the revision is enough to qualify as success...

    if (rev.revision_number == 29982) {
	std::cout << "r29982\n";
    }

    // "Usually" a revision will have properties, but they are apparently not
    // technically required.  For revision properties Content-length and
    // Prop-content-length will always match if non-zero, and Content-length
    // appears to be requried by the dump file spec, so just find and use
    // Content-length
    long int rev_prop_length = -1;
    while (rev_prop_length < 0) {
	if (!std::getline(infile, line)) return success; // Rev num but no contents, no revision
	if (!sfcmp(line, ckey)) rev_prop_length = svn_lint(line, ckey);
    }
    if (rev_prop_length) get_rev_props(infile, &rev);

    //std::cerr << "Revision-number: " << rev.revision_number << ", prop length " << rev_prop_length << std::endl;

    /* Have revision number - grab nodes until we spot another one */
    while (node_ret != -1 && infile.peek() != EOF) {
	node_ret = process_node(infile, &rev);
    }

    revs.insert(std::pair<long int, struct svn_revision>(rev.revision_number, rev));
    return success;
}

std::string path_subpath(std::string &root_path, std::string &long_path)
{
    size_t spos = long_path.find(root_path);
    if (spos != std::string::npos) {
	std::string lp = long_path.substr(spos + root_path.length(), std::string::npos);
	std::cout << "local path of " << long_path << " is " << lp << "\n";
	return lp;
    }
    std::cout << "Warning: root path " << root_path << " is not a subpath of " << long_path << "\n";
    return std::string("");
}

void
node_path_split(std::string &node_path, std::string &project, std::string &branch, std::string &etag, std::string &local_path, int *is_tag)
{
    std::string t("trunk");
    std::string b("branches");
    std::string tag("tags");
    std::string wpath = node_path;
    std::string bs, rp;

    /* First, find the project */
    size_t spos = wpath.find_first_of("/");
    if (spos != std::string::npos) {
	/* If we need to split the path, do so */
	std::string ps = wpath.substr(0, spos);
	if (valid_projects.find(ps) != valid_projects.end()) {
	    project = ps;
	    if (wpath.length() > spos + 1 && spos != std::string::npos) {
		wpath = wpath.substr(spos + 1, std::string::npos);
	    } else {
		wpath = std::string();
	    }
	} else {
	    // If we don't have an explicit project string, but we do have
	    // one of the trunk/branches/tags paths, it's brlcad
	    if (!ps.compare(t) || !ps.compare(b) || !ps.compare(tag)) {
		project = std::string("brlcad");
		if (wpath.length() > spos + 1 && spos != std::string::npos) {
		    wpath = wpath.substr(spos + 1, std::string::npos);
		} else {
		    wpath = std::string();
		}
	    }
	}
    } else {
	/* No subpaths - if we know a project, it's the string itself... */
	if (valid_projects.find(wpath) != valid_projects.end()) {
	    project = wpath;
	    wpath = std::string();
	}
    }
    /* Next, find the branch */
    *is_tag = 0;

    // Is it trunk?
    if (!wpath.compare(0, t.length(), t)) {
	if (wpath.length() > t.length() + 1) {
	    wpath = wpath.substr(t.length() + 1, std::string::npos);
	} else {
	    wpath = std::string();
	}
	branch = std::string("refs/heads/master");
    }

    // Is it a branch?
    if (!wpath.compare(0, b.length(), b)) {
	if (wpath.length() > b.length()) {
	    wpath = wpath.substr(b.length() + 1, std::string::npos);
	} else {
	    // Huh?? branches with nothing after it??
	    std::cerr << "Warning - branch path with nothing after branch dir: " << node_path << "\n";
	}
	spos = wpath.find_first_of("/");
	if (spos != std::string::npos) {
	    bs = wpath.substr(0, spos);
	    wpath = (wpath.length() > bs.length()) ? wpath.substr(spos + 1, std::string::npos) : std::string();
	} else {
	    bs = wpath;
	    wpath = std::string();
	}
	branch = std::string("refs/heads/") + bs;
    }

    // Is it a tag?
    if (!wpath.compare(0, tag.length(), tag)) {
	*is_tag = 1;
	if (wpath.length() > tag.length()) {
	    wpath = wpath.substr(tag.length() + 1, std::string::npos);
	} else {
	    // Huh?? tags with nothing after it??
	    std::cerr << "Warning - tag path with nothing after tag dir: " << node_path << "\n";
	}
	spos = wpath.find_first_of("/");
	if (spos != std::string::npos) {
	    bs = wpath.substr(0, spos);
	    wpath = (wpath.length() > bs.length()) ? wpath.substr(spos + 1, std::string::npos) : std::string();
	} else {
	    bs = wpath;
	    wpath = std::string();
	}
	etag = bs;
    }

    // Not one of the standards, see if it's called out in the maps
    spos = wpath.find_first_of("/");
    bs = (spos != std::string::npos) ? wpath.substr(0, spos) : wpath;
    if (branch_mappings.find(bs) != branch_mappings.end() || tag_mappings.find(bs) != tag_mappings.end()) {
	wpath = (spos == std::string::npos) ? std::string() : wpath.substr(bs.length() + 1, std::string::npos);
	if (tag_mappings.find(bs) != tag_mappings.end()) {
	    *is_tag = 1;
	    tag = tag_mappings[bs];
	}
     	branch = branch_mappings[bs];
    }

    local_path = wpath;
}

void
update_node(std::set<struct svn_node *> *nstate, struct svn_node *nnode)
{
    std::set<struct svn_node *>::iterator n_it;
    // Any old nodes with the same path are out.  If the new one is an
    // add or mod it's in, but regardless all old matches are out
    for (n_it = nstate->begin(); n_it != nstate->end(); ) {
	struct svn_node *onode = *n_it;
	if (onode->path == nnode->path) {
	    //std::cout << "Erasing old node: " << nnode->path << "\n";
	    n_it = nstate->erase(n_it);
	} else {
	    n_it++;
	}
    }

    if (nnode->action != ndelete) {
	nstate->insert(nnode);
    }
}

/**
 * Performs a variety of analysis operations to build up useful data sets.
 *
 * Generally, the flow is as follow:
 *
 *  1. If the first element of the path matches a project_path key, the commit
 *  and path are noted as being part of that project and the rest of the path
 *  is processed as follows.
 *
 *  2. If the path's first element matches trunk, branches or tags it is so
 *  categorized.  For trunk, the remainder of the path is the local file system
 *  path.  For branches and tags, the next element in the path is the associated
 *  branch or tag.
 *
 *  3. If no match to trunk, branches, or tags is found for the second element,
 *  the branch_path and tag_path maps are checked for any special case mappings.
 *  If any are found, the second element is mapped to the branch/tag and the
 *  remainder of the path is considered the local file system path.
 *
 *  A node will always map uniquely to at most one project and branch.  A
 *  revision may map (technically speaking) to more than one of either, since
 *  the groupings are implicit in paths.
 *
 */
void
analyze_dump()
{
    std::map<std::string, long int> path_last_commit;
    std::map<long int, struct svn_revision>::iterator r_it;

    // First pass - find all copyfrom_path instances where we are going to need to track
    // the contents at one or more specific revisions
    for (r_it = revs.begin(); r_it != revs.end(); r_it++) {
	struct svn_revision &rev = r_it->second;
	std::set<std::pair<std::string, long int>> dnodes;
	std::set<std::pair<std::string, long int>>::iterator d_it;
	for (size_t n = 0; n != rev.nodes.size(); n++) {
	    struct svn_node &node = rev.nodes[n];
	    if (node.copyfrom_path.length() && !node.text_copy_source_sha1.length()) {
		//std::cout << "(r" << rev.revision_number << "): copying directory " << node.path << " from " << node.copyfrom_path << " at revision " << node.copyfrom_rev << "\n";
		int cp_is_tag, tp_is_tag;
		std::string cpproject, cpbranch, cplocal_path, cptag;
		std::string tpproject, tpbranch, tplocal_path, tptag;
		node_path_split(node.copyfrom_path, cpproject, cpbranch, cptag, cplocal_path, &cp_is_tag);
		node_path_split(node.copyfrom_path, tpproject, tpbranch, tptag, tplocal_path, &tp_is_tag);


		// If the copy is from the immediately previous commit in the
		// same branch, we haven't had time to make any changes that
		// would invalidate a straight-up "C" operation in git.  If
		// it's from an older state, "C" won't work and we need to
		// explicitly reassemble the older directory state.

		if (cpbranch != tpbranch || node.copyfrom_rev < path_last_commit[node.copyfrom_path]) {
		    dnodes.insert(std::pair<std::string, long int>(node.copyfrom_path, node.copyfrom_rev));
		}
	    }
	}
	// For every path we touched, update its last commit number
	for (size_t n = 0; n != rev.nodes.size(); n++) {
	    struct svn_node &node = rev.nodes[n];
	    path_last_commit[node.path] = rev.revision_number;
	    std::string npath = dir_parent(node.path);
	    while (npath.length()) {
		path_last_commit[npath] = rev.revision_number;
		npath = dir_parent(npath);
	    }
	}

	for (d_it = dnodes.begin(); d_it != dnodes.end(); d_it++) {
	    std::cout << "(r" << rev.revision_number << "): copying directory " << d_it->first << " at revision " << d_it->second << "\n";
	    path_states[d_it->first][d_it->second] = new std::set<struct svn_node *>;
	    rebuild_revs.insert(rev.revision_number);
	}
    }

    // Second pass - assemble node sets for the paths and revisions we will need to know
    for (r_it = revs.begin(); r_it != revs.end(); r_it++) {
	struct svn_revision &rev = r_it->second;
	for (size_t n = 0; n != rev.nodes.size(); n++) {
	    struct svn_node &node = rev.nodes[n];
	    for (ps_it = path_states.begin(); ps_it != path_states.end(); ps_it++) {
		std::string psp = ps_it->first;
		if (dir_is_root(psp, node.path)) {
		    std::map<long int, std::set<struct svn_node *> *>::iterator r_it;
		    for (r_it = ps_it->second.begin(); r_it != ps_it->second.end(); r_it++) {
			if (rev.revision_number <= r_it->first) {
			    /*if (psp != std::string("brlcad/trunk")) {
				std::cout << "Path " << node.path << ", r" << rev.revision_number << " is relevant for " << psp << ",r" << r_it->first << "\n";
			    }*/
			    update_node(r_it->second, &node);
			}
		    }
		}
	    }
	}
    }

    for (r_it = revs.begin(); r_it != revs.end(); r_it++) {
	long int maxrev = 0;
	struct svn_revision &rev = r_it->second;
	for (size_t n = 0; n != rev.nodes.size(); n++) {
	    struct svn_node &node = rev.nodes[n];
	    // Break up paths and build tag/branch lists
	    node_path_split(node.path, node.project, node.branch, node.tag, node.local_path, &(node.tag_path));

	    if (node.copyfrom_path.length() && node.kind == ndir && node.action == nadd) {
		std::cout << "(r" << rev.revision_number << "): copying directory " << node.path << " from " << node.copyfrom_path << " at revision " << node.copyfrom_rev << "\n";
	    }


	    if (node.branch.length()) {
		// Map dmtogl-branch names to dmtogl
		if (node.branch == std::string("refs/heads/dmtogl-branch")) {
		    node.branch = std::string("refs/heads/dmtogl");
		    std::cout << "Mapping dmtogl-branch rev " << rev.revision_number << " to dmtogl\n";
		}
		if (node.tag_path) {
		    tags.insert(node.branch);
		} else {
		    branches.insert(node.branch);
		}
	    }
	    // Find move + edit operations
	    if (node.copyfrom_path.length() > 0 && node.text_content_length > 0 && node.copyfrom_rev == rev.revision_number) {
		node.move_edit = 1;
		rev.move_edit = 1;
	    }
	    // Tag add
	    if (node.kind == ndir && node.action == nadd && node.tag.length() && !node.local_path.length() && node.tag_path) {
		node.tag_add = 1;
	    }
	    // Tag delete
	    if (node.action == ndelete && node.tag.length() && !node.local_path.length() && node.tag_path) {
		node.tag_delete = 1;
	    }
	    // Spot tag edits
	    if (node.tag_path) {
		if (node.text_content_length > 0 || node.text_copy_source_sha1.length() || (node.action == ndelete && !node.tag_delete) || (edited_tags.find(node.tag) != edited_tags.end() && node.action == nadd)) {
		    node.tag_edit = 1;
		    if (edited_tags.find(node.tag) == edited_tags.end()) {
			edited_tag_first_rev[node.tag] = rev.revision_number;
		    }
		    edited_tags.insert(node.tag);
		    std::cout << "Edited tag(" << rev.revision_number << "): " << node.tag << "\n";
		    edited_tag_max_rev[node.tag] = rev.revision_number;
		    node.branch = std::string("refs/heads/") + node.tag;
		}
	    }
	    // Branch add
	    if (node.kind == ndir && node.action == nadd && node.branch.length() && !node.local_path.length() && !node.tag_path) {
		node.branch_add = 1;
	    }
	    // Branch delete
	    if (node.action == ndelete && node.branch.length() && !node.local_path.length() && !node.tag_path) {
		node.branch_delete = 1;
	    }
	    // Deal with merges
	    if (node.node_props.find("svn:mergeinfo") != node.node_props.end()) {
		rev.merge = 1;
		std::string mprop = node.node_props.find("svn:mergeinfo")->second;
		if (mprop.length()) {
		    std::string pline;
		    std::string mfrom;
		    std::stringstream ss(mprop);
		    while (std::getline(ss, pline, '\n')) {
			if (!pline.length()) continue;
			int is_tag = 0;
			size_t cpos = pline.find_first_of(":");
			std::string mpath = pline.substr(1, cpos - 1);
			std::string mproject, cbranch, mlocal_path, ctag;
			node_path_split(mpath, mproject, cbranch, ctag, mlocal_path, &is_tag);
			std::string revs = pline.substr(cpos+1, std::string::npos);
			size_t nspos = revs.find_last_of(",-");
			std::string hrev = (nspos == std::string::npos) ? revs : revs.substr(nspos+1, std::string::npos);
			long int crev = std::stol(hrev);
			if (crev > maxrev && node.branch.compare(cbranch)) {
			    rev.merged_from = cbranch;
			    maxrev = crev;
			    rev.merged_rev = maxrev;
			}
		    }
		}
	    }
	}
	// While it is techically possible to have multi-project commits (and there
	// are some in BRL-CAD's early history) the commit range we are concerned with
	// for appending onto the CVS git conversion does not have any such commits.
	// Accordingly, can use the project from the first node path for the whole
	// revision.
	if (rev.nodes.size() > 0) {
	    rev.project = rev.nodes[0].project; 
	}
    }
}

void full_sync_commit(std::ofstream &outfile, struct svn_revision &rev, std::string &bsrc, std::string &bdest)
{
    outfile << "commit " << bdest << "\n";
    outfile << "mark :" << rev.revision_number << "\n";
    outfile << "committer " << author_map[rev.author] << " " << svn_time_to_git_time(rev.timestamp.c_str()) << "\n";
    outfile << "data " << rev.commit_msg.length() << "\n";
    outfile << rev.commit_msg << "\n";
    outfile << "from " << branch_head_id(bdest, rev.revision_number) << "\n";
    outfile << "deleteall\n";
    // TODO - merge info
    branch_head_ids[bdest] = std::to_string(rev.revision_number);

    std::string line;
    std::string ifile = std::to_string(rev.revision_number) + std::string(".txt");
    std::ifstream infile(ifile);
    if (!infile.good()) {
	std::cerr << "Fatal error: could not open file " << ifile << " for special commit handling, exiting\n";
	exit(1);
    }
    while (std::getline(infile, line)) {
	outfile << line << "\n";
    }
    outfile << "\n";
}

void move_only_commit(std::ofstream &outfile, struct svn_revision &rev, std::string &rbranch)
{
    if (author_map.find(rev.author) == author_map.end()) {
	std::cout << "Error - couldn't find author map for author " << rev.author << " on revision " << rev.revision_number << "\n";
	exit(1);
    }

    // first, check if we truly have path changes between the copyfrom and dest local paths
    int have_real_rename = 0;
    for (size_t n = 0; n != rev.nodes.size(); n++) {
	struct svn_node &node = rev.nodes[n];
	if (node.copyfrom_path.length() > 0) {
	    int is_tag;
	    std::string mproject, cbranch, mlocal_path, ctag;
	    node_path_split(node.copyfrom_path, mproject, cbranch, ctag, mlocal_path, &is_tag);
	    if ((mlocal_path != node.local_path) && (cbranch == node.branch)) {
		have_real_rename = 1;
	    }
	}
    }

    if (!have_real_rename) {
	return;
    }

    std::string ncmsg = rev.commit_msg + std::string("(preliminiary file move commit)");

    std::string mvmark = std::string("1111") + std::to_string(rev.revision_number);

    outfile << "commit " << rbranch << "\n";
    outfile << "mark :" << mvmark << "\n";
    outfile << "committer " << author_map[rev.author] << " " << svn_time_to_git_time(rev.timestamp.c_str()) << "\n";
    outfile << "data " << ncmsg.length() << "\n";
    outfile << ncmsg << "\n";
    outfile << "from " << branch_head_id(rbranch, rev.revision_number) << "\n";
    branch_head_ids[rbranch] = mvmark;

    if (rev.merged_from.length()) {
	if (brlcad_revs.find(rev.merged_rev) != brlcad_revs.end()) {
	    std::cout << "Revision " << rev.revision_number << " merged from: " << rev.merged_from << "(" << rev.merged_rev << "), id " << branch_head_id(rev.merged_from, rev.merged_rev) << "\n";
	    std::cout << "Revision " << rev.revision_number << "        from: " << rbranch << "\n";
	    outfile << "merge :" << rev.merged_rev << "\n";
	} else {
	    std::cout << "Warning: r" << rev.revision_number << " is referencing a commit id (" << rev.merged_rev << ") that is not a BRL-CAD commit\n";
	}
    }

    for (size_t n = 0; n != rev.nodes.size(); n++) {
	struct svn_node &node = rev.nodes[n];
	/* Don't add directory nodes themselves - git works on files */
	if (node.kind == ndir) continue;
	if (node.copyfrom_path.length() > 0) {
	    int is_tag;
	    std::string mproject, cbranch, mlocal_path, ctag;
	    node_path_split(node.copyfrom_path, mproject, cbranch, ctag, mlocal_path, &is_tag);
	    if (mlocal_path != node.local_path) {
		std::cout << "(r" << rev.revision_number << ") - renaming " << mlocal_path << " to " << node.local_path << "\n";
		outfile << "R " << mlocal_path << " " << node.local_path << "\n";
	    }
	}
    }

}

void write_git_node(std::ofstream &outfile, struct svn_revision &rev, struct svn_node &node)
{
    /* Don't add directory nodes themselves - git works on files.
     *
     * However, there is an important exception here for directory copies and deletes!
     * */
    if (node.kind == ndir && node.action == nadd && node.copyfrom_path.length()) {
	int is_tag;
	std::string mproject, cbranch, mlocal_path, ctag;
	node_path_split(node.copyfrom_path, mproject, cbranch, ctag, mlocal_path, &is_tag);
	if (mlocal_path != node.local_path) {
	    std::cout << "(r" << rev.revision_number << ") - renaming " << mlocal_path << " to " << node.local_path << "\n";
	    outfile << "C " << mlocal_path << " " << node.local_path << "\n";
	}
	return;
    }
    if (node.kind == ndir && node.action == nadd) return;

    // If it's a straight-up path delete, do it and return
    if (node.kind == nkerr && node.action == ndelete) {
	std::cerr << "Revision r" << rev.revision_number << " delete: " << node.local_path << "\n";
	outfile << "D \"" << node.local_path << "\"\n";
	return;
    }

    //std::cout << "	Processing node " << print_node_action(node.action) << " " << node.local_path << " into branch " << node.branch << "\n";
    std::string gsha1;
    if (node.action == nchange || node.action == nadd || node.action == nreplace) {
	if (node.exec_change || node.copyfrom_path.length() || node.text_content_length || node.text_content_sha1.length()) {
	    if (node.text_copy_source_sha1.length()) {
		gsha1 = svn_sha1_to_git_sha1[node.text_copy_source_sha1];
	    } else {
		gsha1 = svn_sha1_to_git_sha1[current_sha1[node.path]];
	    }
	    if (gsha1.length() < 40) {
		std::string tpath;
		if (node.copyfrom_path.length()) {
		    tpath = node.copyfrom_path;
		} else {
		    tpath = std::string("brlcad/trunk/") + node.local_path;
		}
		gsha1 = svn_sha1_to_git_sha1[current_sha1[tpath]];
		if (gsha1.length() < 40) {
		    std::cout << "Fatal - could not find git sha1 - r" << rev.revision_number << ", node: " << node.path << "\n";
		    std::cout << "current sha1: " << current_sha1[node.path] << "\n";
		    std::cout << "trunk sha1: " << current_sha1[tpath] << "\n";
		    std::cout << "Revision merged from: " << rev.merged_from << "\n";
		    print_node(node);
		    exit(1);
		} else {
		    std::cout << "Warning(r" << rev.revision_number << ") - couldn't find SHA1 for " << node.path << ", using node from " << tpath << "\n";
		}
	    }
	} else {
	    std::cout << "Warning(r" << rev.revision_number << ") - skipping " << node.path << " - no git applicable change found.\n";
	    return;
	}
    }

    switch (node.action) {
	case nchange:
	    outfile << "M ";
	    break;
	case nadd:
	    outfile << "M ";
	    break;
	case nreplace:
	    outfile << "M ";
	    break;
	case ndelete:
	    outfile << "D ";
	    break;
	default:
	    std::cerr << "Unhandled node action type " << print_node_action(node.action) << "\n";
	    outfile << "? ";
	    outfile << "\"" << node.local_path << "\"\n";
	    std::cout << "Fatal - unhandled node action at r" << rev.revision_number << ", node: " << node.path << "\n";
	    exit(1);
    }

    if (node.action != ndelete) {
	if (node.exec_path) {
	    outfile << "100755 ";
	} else {
	    outfile << "100644 ";
	}
    }

    if (node.action == nchange || node.action == nadd || node.action == nreplace) {
	outfile << gsha1 << " \"" << node.local_path << "\"\n";
	return;
    }

    if (node.action == ndelete) {
	outfile << "\"" << node.local_path << "\"\n";
	return;
    }

    std::cout << "Error(r" << rev.revision_number << ") - unhandled node action: " << print_node_action(node.action) << "\n";
    print_node(node);
    exit(1);
}


void old_references_commit(std::ofstream &outfile, struct svn_revision &rev, std::string &rbranch)
{
    if (author_map.find(rev.author) == author_map.end()) {
	std::cout << "Error - couldn't find author map for author " << rev.author << " on revision " << rev.revision_number << "\n";
	exit(1);
    }
    outfile << "commit " << rbranch << "\n";
    outfile << "mark :" << rev.revision_number << "\n";
    outfile << "committer " << author_map[rev.author] << " " << svn_time_to_git_time(rev.timestamp.c_str()) << "\n";
    outfile << "data " << rev.commit_msg.length() << "\n";
    outfile << rev.commit_msg << "\n";
    outfile << "from " << branch_head_id(rbranch, rev.revision_number) << "\n";
    branch_head_ids[rbranch] = std::to_string(rev.revision_number);

    if (rev.merged_from.length()) {
	if (brlcad_revs.find(rev.merged_rev) != brlcad_revs.end()) {
	    std::cout << "Revision " << rev.revision_number << " merged from: " << rev.merged_from << "(" << rev.merged_rev << "), id " << branch_head_id(rev.merged_from, rev.merged_rev) << "\n";
	    std::cout << "Revision " << rev.revision_number << "        from: " << rbranch << "\n";
	    outfile << "merge :" << rev.merged_rev << "\n";
	} else {
	    std::cout << "Warning: r" << rev.revision_number << " is referencing a commit id (" << rev.merged_rev << ") that is not a BRL-CAD commit\n";
	}
    }

    brlcad_revs.insert(rev.revision_number);

    for (size_t n = 0; n != rev.nodes.size(); n++) {
	struct svn_node &node = rev.nodes[n];

	if (node.kind == ndir && node.action == nadd && node.copyfrom_path.length() && node.copyfrom_rev < rev.revision_number - 1) {
	    std::cout << "Non-standard DIR handling needed: " << node.path << "\n";
	    std::string ppath, bbpath, llpath, tpath;
	    int is_tag;
	    node_path_split(node.copyfrom_path, ppath, bbpath, tpath, llpath, &is_tag);
	    std::set<struct svn_node *>::iterator n_it;
	    std::set<struct svn_node *> *node_set = path_states[node.copyfrom_path][node.copyfrom_rev];
	    for (n_it = node_set->begin(); n_it != node_set->end(); n_it++) {
		if ((*n_it)->kind == ndir) {
		    std::cout << "Stashed dir node: " << (*n_it)->path << "\n";
		    exit(1);
		}
		std::cout << "Stashed file node: " << (*n_it)->path << "\n";
		std::string rp = path_subpath(llpath, (*n_it)->local_path);
		std::string gsha1 = svn_sha1_to_git_sha1[(*n_it)->text_content_sha1];
		std::string exestr = ((*n_it)->exec_path) ? std::string("100755 ") : std::string("100644 ");
		outfile << "M " << exestr << gsha1 << " \"" << node.local_path << rp << "\"\n";
	    }
	    continue;
	}

	// Not one of the special cases, handle normally
	write_git_node(outfile, rev, node);

    }
}

void standard_commit(std::ofstream &outfile, struct svn_revision &rev, std::string &rbranch)
{
    if (author_map.find(rev.author) == author_map.end()) {
	std::cout << "Error - couldn't find author map for author " << rev.author << " on revision " << rev.revision_number << "\n";
	exit(1);
    }

    brlcad_revs.insert(rev.revision_number);

    outfile << "commit " << rbranch << "\n";
    outfile << "mark :" << rev.revision_number << "\n";
    outfile << "committer " << author_map[rev.author] << " " << svn_time_to_git_time(rev.timestamp.c_str()) << "\n";
    outfile << "data " << rev.commit_msg.length() << "\n";
    outfile << rev.commit_msg << "\n";
    outfile << "from " << branch_head_id(rbranch, rev.revision_number) << "\n";
    branch_head_ids[rbranch] = std::to_string(rev.revision_number);

    if (rev.merged_from.length()) {
	if (brlcad_revs.find(rev.merged_rev) != brlcad_revs.end()) {
	    std::cout << "Revision " << rev.revision_number << " merged from: " << rev.merged_from << "(" << rev.merged_rev << "), id " << branch_head_id(rev.merged_from, rev.merged_rev) << "\n";
	    std::cout << "Revision " << rev.revision_number << "        from: " << rbranch << "\n";
	    outfile << "merge :" << rev.merged_rev << "\n";
	} else {
	    std::cout << "Warning: r" << rev.revision_number << " is referencing a commit id (" << rev.merged_rev << ") that is not a BRL-CAD commit\n";
	}
    }


    // Check for directory deletes and copyfrom in the same node set.  If this happens,
    // we need to defer the delete of the old dir until after the copy is made.  This is
    // related to the problem of pulling old dir contents, but if we're in this function
    // the assumption is that the working dir's contents are what we need here.
    std::set<struct svn_node *> deferred_deletes;
    std::set<struct svn_node *> deletes;
    std::set<struct svn_node *>::iterator d_it;
    for (size_t n = 0; n != rev.nodes.size(); n++) {
	struct svn_node &node = rev.nodes[n];
	if (node.action == ndelete) {
	    deletes.insert(&node);
	}
    }
    for (size_t n = 0; n != rev.nodes.size(); n++) {
	struct svn_node &node = rev.nodes[n];
	if (node.kind != nfile && node.copyfrom_path.length()) {
	    for (d_it = deletes.begin(); d_it != deletes.end(); d_it++) {
		if (node.copyfrom_path == (*d_it)->path) {
		    std::cout << "Deferring delete of " << node.copyfrom_path << "\n";
		    deferred_deletes.insert(*d_it);
		}
	    }
	}
    }

    for (size_t n = 0; n != rev.nodes.size(); n++) {
	struct svn_node &node = rev.nodes[n];
	if (node.action != ndelete || deferred_deletes.find(&node) == deferred_deletes.end()) {
	    write_git_node(outfile, rev, node);
	}
    }

    for (d_it = deferred_deletes.begin(); d_it != deferred_deletes.end(); d_it++) {
	write_git_node(outfile, rev, **d_it);
    }

}

void rev_fast_export(std::ifstream &infile, std::ofstream &outfile, long int rev_num_min, long int rev_num_max)
{
    std::set<int> special_revs;
    std::string empty_sha1("da39a3ee5e6b4b0d3255bfef95601890afd80709");
    std::map<long int, struct svn_revision>::iterator r_it;


    // particular revisions that will have to be special cased:
    special_revs.insert(31039);
    special_revs.insert(32314);
    special_revs.insert(36633);
    special_revs.insert(36843);
    special_revs.insert(39465);


    for (r_it = revs.begin(); r_it != revs.end(); r_it++) {
	struct svn_revision &rev = r_it->second;

	if (rev.revision_number == 40098) {
	    std::cout << "at 40098\n";
	}

	if (rev.project != std::string("brlcad")) {
	    //std::cout << "Revision " << rev.revision_number << " is not part of brlcad, skipping\n";
	    continue;
	}

	// If we have a text content sha1, update the map
	// to the current path state
	if (rev.nodes.size() > 0) {
	    for (size_t n = 0; n != rev.nodes.size(); n++) {
		struct svn_node &node = rev.nodes[n];
		if (node.text_content_sha1.length()) {
		    current_sha1[node.path] = node.text_content_sha1;
		}
	    }
	}

	if (rev.revision_number > rev_num_max) return;

	if (rev.revision_number == 30760) {
	    std::cout << "Skipping r30760 - premature tagging of rel-7-12-2.  Will be tagged by 30792\n";
	    continue;
	}

	if (rev.revision_number == 36472) {
	    std::cout << "Skipping r36472 - branch rename, handled by mapping the original name in the original commit (dmtogl-branch) to the desired branch name (dmtogl)\n";
	    continue;
	}

	if (rev.revision_number >= rev_num_min) {
#if 0
	    std::cout << "Processing revision " << rev.revision_number << "\n";
	    if (rev.merged_from.length()) {
		std::cout << "Note: merged from " << rev.merged_from << "\n";
	    }
#endif
	    int git_changes = 0;
	    int have_commit = 0;
	    int tag_after_commit = 0;
	    int branch_delete = 0;
	    std::string ctag, cfrom;
	    std::string rbranch = rev.nodes[0].branch;

	    if (reject_tag(rev.nodes[0].tag)) {
		std::cout << "Skipping " << rev.revision_number << " - edit to old tag:\n" << rev.commit_msg << "\n";
		continue;
	    }

	    if (special_revs.find(rev.revision_number) != special_revs.end()) {
		std::string bsrc, bdest;
		brlcad_revs.insert(rev.revision_number);
		std::cout << "Revision " << rev.revision_number << " requires special handling\n";
		if (rev.revision_number == 36633 || rev.revision_number == 39465) {
		    bdest = std::string("refs/heads/dmtogl");
		    bsrc = branch_head_id(std::string("refs/heads/master"), rev.revision_number);
		} else {
		    bdest = std::string("refs/heads/STABLE");
		}
		if (rev.revision_number == 31039) {
		    bsrc = tag_ids[std::string("rel-7-12-2")];
		}
		if (rev.revision_number == 32314 || rev.revision_number == 36843) {
		    bsrc = tag_ids[std::string("rel-7-12-6")];
		}
		full_sync_commit(outfile, rev, bsrc, bdest);

		// Check trunk after every commit - this will eventually expand to branch specific checks as well, but for now we
		// need to get trunk building correctly
		outfile.flush();
		verify_repos(rev.revision_number, std::string("trunk"), std::string("master"));

		continue;
	    }

	    if (rebuild_revs.find(rev.revision_number) != rebuild_revs.end()) {
		std::cout << "Revision " << rev.revision_number << " references non-current SVN info, needs special handling\n";
		old_references_commit(outfile, rev, rbranch);

		// Check trunk after every commit - this will eventually expand to branch specific checks as well, but for now we
		// need to get trunk building correctly
		outfile.flush();
		verify_repos(rev.revision_number, std::string("trunk"), std::string("master"));


		continue;
	    }

	    if (rev.revision_number == 30332) {
		std::cout << "at 30332\n";
	    }

	    for (size_t n = 0; n != rev.nodes.size(); n++) {
		struct svn_node &node = rev.nodes[n];

		if (node.tag_add || node.tag_edit) {
		    std::cout << "Processing revision " << rev.revision_number << "\n";
		    std::string ppath, bbpath, llpath, tpath;
		    int is_tag;
		    node_path_split(node.copyfrom_path, ppath, bbpath, tpath, llpath, &is_tag);

		    int edited_tag_minr = -1;
		    int edited_tag_maxr = -1;

		    // For edited tags - first tag create branch, final tag is "real" tag,
		    // else just branch commits
		    if (edited_tags.find(node.tag) != edited_tags.end()) {
			edited_tag_minr = edited_tag_first_rev[node.tag];
			edited_tag_maxr = edited_tag_max_rev[node.tag];
			std::cout << node.tag << ": " << edited_tag_minr << " " << edited_tag_maxr << "\n";
			brlcad_revs.insert(rev.revision_number);

			if (rev.revision_number < edited_tag_minr) {
			    std::string nbranch = std::string("refs/heads/") + node.tag;
			    std::cout << "Adding tag branch " << nbranch << " from " << bbpath << ", r" << rev.revision_number <<"\n";
			    std::cout << rev.commit_msg << "\n";
			    outfile << "reset " << nbranch << "\n";
			    outfile << "from " << branch_head_id(bbpath, rev.revision_number) << "\n";
			    branch_head_ids[nbranch] = branch_head_ids[bbpath];
			    continue;
			}
			if (rev.revision_number == edited_tag_maxr) {
			    tag_after_commit = 1;
			    rbranch = node.branch;
			    ctag = node.tag;
			    cfrom = node.branch;
			}
			std::cout << "Non-final tag edit, processing normally: " << node.branch << ", r" << rev.revision_number<< "\n";
			rbranch = node.branch;
			//git_changes = 1;
		    } else {
			brlcad_revs.insert(rev.revision_number);
			std::cout << "Adding tag " << node.tag << " from " << bbpath << ", r" << rev.revision_number << "\n";
			have_commit = 1;
			outfile << "tag " << node.tag << "\n";
			outfile << "from " << branch_head_id(bbpath, rev.revision_number) << "\n";
			outfile << "tagger " << author_map[rev.author] << " " << svn_time_to_git_time(rev.timestamp.c_str()) << "\n";
			outfile << "data " << rev.commit_msg.length() << "\n";
			outfile << rev.commit_msg << "\n";
			tag_ids[node.tag] = branch_head_id(bbpath, rev.revision_number);
			continue;
		    }

		}

		if (node.tag_delete) {
		    branch_delete = 1;
		    std::cout << "processing revision " << rev.revision_number << "\n";
		    std::cout << "TODO - delete tag: " << node.tag << "\n";
		    continue;
		}

		if (node.branch_add) {
		    brlcad_revs.insert(rev.revision_number);
		    std::cout << "Processing revision " << rev.revision_number << "\n";
		    std::string ppath, bbpath, llpath, tpath;
		    int is_tag;
		    node_path_split(node.copyfrom_path, ppath, bbpath, tpath, llpath, &is_tag);
		    std::cout << "Adding branch " << node.branch << " from " << bbpath << ", r" << rev.revision_number <<"\n";
		    std::cout << rev.commit_msg << "\n";
		    outfile << "reset " << node.branch << "\n";
		    outfile << "from " << branch_head_id(bbpath, rev.revision_number) << "\n";
		    branch_head_ids[node.branch] = branch_head_ids[bbpath];
		    have_commit = 1;

		    // TODO - make an empty commit on the new branch with the commit message from SVN, but no changes

		    continue;
		}

		if (node.branch_delete) {
		    //std::cout << "processing revision " << rev.revision_number << "\n";
		    //std::cout << "Delete branch instruction: " << node.branch << " - deferring.\n";
		    branch_delete = 1;
		    continue;
		}
		if (node.text_content_length > 0) {
		    //std::cout << "	Adding node object for " << node.local_path << "\n";
		    write_blob(infile, outfile, node);
		    git_changes = 1;
		}
		if (node.text_content_sha1.length()) {
		    git_changes = 1;
		}
		if (node.text_copy_source_sha1.length()) {
		    git_changes = 1;
		}
		if (node.exec_change) {
		    git_changes = 1;
		}
		if (node.action == ndelete) {
		    git_changes = 1;
		}
	    }

	    if (git_changes) {
		if (have_commit) {
		    std::cout << "Error - more than one commit generated for revision " << rev.revision_number << "\n";
		}

		if (rev.move_edit) {
		    move_only_commit(outfile, rev, rbranch);
		}

		standard_commit(outfile, rev, rbranch);

		if (tag_after_commit) {
		    // Note - in this situation, we need to both build a commit and do a tag.  Will probably
		    // take some refactoring.  Merge information will also be a factor.
		    std::cout << "Adding tag after final tag branch commit: " << ctag << " from " << cfrom << ", r" << rev.revision_number << "\n";
		    outfile << "tag " << ctag << "\n";
		    outfile << "from " << branch_head_id(cfrom, rev.revision_number) << "\n";
		    outfile << "tagger " << author_map[rev.author] << " " << svn_time_to_git_time(rev.timestamp.c_str()) << "\n";
		    outfile << "data " << rev.commit_msg.length() << "\n";
		    outfile << rev.commit_msg << "\n";
		}
	    } else {
		if (!branch_delete && !have_commit) {
		    std::cout << "Warning(r" << rev.revision_number << ") - skipping SVN commit, no git applicable changes found\n";
		    std::cout << rev.commit_msg << "\n";
		}
	    }

	    // Check trunk after every commit - this will eventually expand to branch specific checks as well, but for now we
	    // need to get trunk building correctly
	    outfile.flush();
	    verify_repos(rev.revision_number, std::string("trunk"), std::string("master"));
	}
    }
}


/**
 * Parses a dump file and returns a repository information
 * structure, or NULL on failure.
 *
 * Assumes dump file format as documented here:
 * http://svn.apache.org/repos/asf/subversion/trunk/notes/dump-load-format.txt
 */
int
load_dump_file(const char *f)
{
    std::string line;
    std::string uuid;
    int rev_cnt = 0;
    int dump_format_version = -1;
    std::string fmtkey("SVN-fs-dump-format-version: ");

    std::ifstream infile(f);
    if (!infile.good()) return -1;

    // The first non-empty line has to be the format version, or we're done.
    while (std::getline(infile, line) && !line.length());
    if (line.compare(0, fmtkey.length(), fmtkey)) {
	std::cerr << "SVN: load_dump error, no format found\n";
	return -1;
    }

    // Grab and check the format number
    dump_format_version = svn_lint(line, fmtkey);
    if (dump_format_version != 2) {
	std::cerr << "SVN: need dump format 2, but have " << dump_format_version << "\n";
	return -1;
    }

    // Get past the uuid
    while (!uuid.length() && std::getline(infile, line)) {
	uuid = svn_str(line, std::string("UUID: "));
    }

    /* As long as we're not done, read revisions */
    while (infile.peek() != EOF) {
	rev_cnt += process_revision(infile);
    }

    infile.close();

    return rev_cnt;
}

// TODO run system command to generate these at time of run, rather than using static file
void
load_branch_head_sha1s()
{
    std::string git_sha1_heads_cmd = std::string("rm -f brlcad_cvs_git_heads_sha1.txt && cd cvs_git && git show-ref --heads --tags > ../brlcad_cvs_git_heads_sha1.txt && cd ..");
    std::system(git_sha1_heads_cmd.c_str());
    std::ifstream hfile("brlcad_cvs_git_heads_sha1.txt");
    if (!hfile.good()) exit(-1);
    std::string line;
    while (std::getline(hfile, line)) {
	size_t spos = line.find_first_of(" ");
	std::string hsha1 = line.substr(0, spos);
	std::string hbranch = line.substr(spos+1, std::string::npos);
	branch_head_ids[hbranch] = hsha1;
    }
    hfile.close();
    //remove("brlcad_cvs_git_heads_sha1.txt");
}

// TODO run system command to generate these at time of run, rather than using static file
void
load_blob_sha1s()
{
    std::string git_sha1_heads_cmd = std::string("rm -f brlcad_cvs_git_all_blob_sha1.txt && cd cvs_git && git rev-list --objects --all | git cat-file --batch-check='%(objectname) %(objecttype) %(rest)' | grep '^[^ ]* blob' | cut -d\" \" -f1,3- > ../brlcad_cvs_git_all_blob_sha1.txt && echo \"e69de29bb2d1d6434b8b29ae775ad8c2e48c5391 src/other/tkhtml3/AUTHORS\n\" >> ../brlcad_cvs_git_heads_sha1.txt && cd ..");
    std::system(git_sha1_heads_cmd.c_str());
    std::ifstream bfile("brlcad_cvs_git_all_blob_sha1.txt");
    if (!bfile.good()) exit(-1);
    std::string line;
    while (std::getline(bfile, line)) {
	size_t spos = line.find_first_of(" ");
	std::string hsha1 = line.substr(0, spos);
	cvs_blob_sha1.insert(hsha1);
    }
    bfile.close();
    //remove("brlcad_cvs_git_all_blob_sha1.txt");
}


void
load_author_map(const char *f)
{

    std::ifstream afile(f);
    std::string line;
    while (std::getline(afile, line)) {
	size_t spos = line.find_first_of(" ");
	std::string svnauthor = line.substr(0, spos);
	std::string gitauthor = line.substr(spos+1, std::string::npos);
	author_map[svnauthor] = gitauthor;
    }
}

int main(int argc, const char **argv)
{
    if (argc < 3) {
	std::cerr << "svnfexport dumpfile author_map\n";
	return 1;
    }

    //starting_rev = 29886;
    starting_rev = 29939;

    // Make sure our starting point is sound
    verify_repos(starting_rev, std::string("trunk"), std::string("master"));

    /* Populate valid_projects */
    valid_projects.insert(std::string("brlcad"));

    /* SVN has some empty files - tell our setup how to handle them */
    svn_sha1_to_git_sha1[std::string("da39a3ee5e6b4b0d3255bfef95601890afd80709")] = std::string("e69de29bb2d1d6434b8b29ae775ad8c2e48c5391");

    /* Branch/tag name mappings */
    branch_mappings[std::string("framebuffer-experiment")] = std::string("framebuffer-experiment");

    /* Read in pre-existing branch sha1 heads from git */
    load_author_map(argv[2]);

    /* Read in pre-existing branch sha1 heads from git */
    load_branch_head_sha1s();

    /* Read in pre-existing blob sha1s from git */
    load_blob_sha1s();

    int rev_cnt = load_dump_file(argv[1]);
    if (rev_cnt > 0) {
	std::cout << "Dump file " << argv[1] << " loaded - found " << rev_cnt << " revisions\n";
	analyze_dump();
    } else {
	std::cerr << "No revision found - quitting\n";
	return -1;
    }

    std::ifstream infile(argv[1]);

    std::ofstream outfile("brlcad-svn-export.fi", std::ios::out | std::ios::binary);
    if (!outfile.good()) return -1;
    //rev_fast_export(infile, outfile, 29887, 30854);
    rev_fast_export(infile, outfile, starting_rev + 1, 40000);
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
