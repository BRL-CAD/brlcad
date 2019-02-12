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
    std::string branch; // For tags, the "branch" is the tag
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
};


std::map<std::string,std::pair<size_t, long int>> sha1_blobs;
std::set<std::string> exec_paths;
std::map<std::string,std::string> current_sha1;
std::map<std::string,std::string> branch_head_ids;
std::map<std::string,std::string> svn_sha1_to_git_sha1;

std::set<std::string> cvs_blob_sha1;
std::map<std::string,std::string> author_map;

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


std::map<std::string, std::string> branch_mappings;
std::map<std::string, std::string> tag_mappings;

#define sfcmp(_s1, _s2) _s1.compare(0, _s2.size(), _s2) && _s1.size() >= _s2.size()
#define svn_str(_s1, _s2) (!sfcmp(_s1, _s2)) ? _s1.substr(_s2.size(), _s1.size()-_s2.size()) : std::string()

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


std::string git_sha1(std::ifstream &infile, struct svn_node *n)
{
    std::string git_sha1;
    std::string go_buff;
    char *cbuffer = new char [n->text_content_length];;
    infile.read(cbuffer, n->text_content_length);
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
	    if (buffer[i] == '\n') {
		cf << '\r' << '\n';
	    } else {
		cf << buffer[i];
	    }
	}
	std::string crlf_file = cf.str();
	outfile << "data " << crlf_file.length() << "\n";
	outfile.write(crlf_file.c_str(), crlf_file.length());
    }
}

std::string
branch_head_id(std::string branch){
    // If strlen of current head == 40, it's a sha1 and just return it.
    //
    // else, it's a mark and return the string with a colon in front.
    std::string curr_head = branch_head_ids[branch];
    if (!curr_head.length()) {
	std::cout << "Error - unknown branch " << branch << "\n";
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


    // r29887 is where we will be starting our fast-export generation - until that point,
    // keep current_sha1 up to date on a rolling basis
    if (rev->revision_number <= 29886) {
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


void
node_path_split(std::string &node_path, std::string &project, std::string &branch, std::string &local_path, int *is_tag)
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
	branch = bs;
    }

    // Not one of the standards, see if it's called out in the maps
    spos = wpath.find_first_of("/");
    bs = (spos != std::string::npos) ? wpath.substr(0, spos) : wpath;
    if (branch_mappings.find(bs) != branch_mappings.end() || tag_mappings.find(bs) != tag_mappings.end()) {
	wpath = (spos == std::string::npos) ? std::string() : wpath.substr(bs.length() + 1, std::string::npos);
	if (tag_mappings.find(bs) != tag_mappings.end()) {
	    *is_tag = 1;
	    branch = tag_mappings[bs];
	}
     	branch = branch_mappings[bs];
    }

    local_path = wpath;
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
    std::map<long int, struct svn_revision>::iterator r_it;

    for (r_it = revs.begin(); r_it != revs.end(); r_it++) {
	long int maxrev = 0;
	struct svn_revision &rev = r_it->second;
	for (size_t n = 0; n != rev.nodes.size(); n++) {
	    struct svn_node &node = rev.nodes[n];
	    // Break up paths and build tag/branch lists
	    node_path_split(node.path, node.project, node.branch, node.local_path, &(node.tag_path));
	    if (node.branch.length()) {
		// Map dmtogl-branch names to dmtogl
		if (node.branch == std::string("refs/heads/dmtogl-branch")) {
		    node.branch = std::string("refs/heads/dmtogl");
		    std::cout << "Updating rev " << rev.revision_number << "\n";
		}
		if (node.tag_path) {
		    tags.insert(node.branch);
		} else {
		    branches.insert(node.branch);
		}
	    }
	    // Find move + edit operations
	    if (node.copyfrom_path.length() > 0 && node.text_content_length > 0) {
		node.move_edit = 1;
		rev.move_edit = 1;
	    }
	    // Spot tag edits
	    if ((node.text_content_length > 0 || node.text_copy_source_sha1.length()) && node.tag_path) {
		node.tag_edit = 1;
		if (edited_tags.find(node.branch) == edited_tags.end()) {
		    edited_tag_first_rev[node.branch] = rev.revision_number;
		}
		edited_tags.insert(node.branch);
		std::cout << "Edited tag(" << rev.revision_number << "): " << node.branch << "\n";
		edited_tag_max_rev[node.branch] = rev.revision_number;
	    }
	    // Branch add
	    if (node.kind == ndir && node.action == nadd && node.branch.length() && !node.local_path.length() && !node.tag_path) {
		node.branch_add = 1;
	    }
	    // Branch delete
	    if (node.action == ndelete && node.branch.length() && !node.local_path.length() && !node.tag_path) {
		node.branch_delete = 1;
	    }
	    // Tag add
	    if (node.kind == ndir && node.action == nadd && node.branch.length() && !node.local_path.length() && node.tag_path) {
		node.tag_add = 1;
	    }
	    // Tag delete
	    if (node.action == ndelete && node.branch.length() && !node.local_path.length() && node.tag_path) {
		node.tag_delete = 1;
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
			std::string mproject, cbranch, mlocal_path;
			node_path_split(mpath, mproject, cbranch, mlocal_path, &is_tag);
			std::string revs = pline.substr(cpos+1, std::string::npos);
			size_t nspos = revs.find_last_of(",-");
			std::string hrev = (nspos == std::string::npos) ? revs : revs.substr(nspos+1, std::string::npos);
			long int crev = std::stol(hrev);
			if (crev > maxrev && node.branch.compare(cbranch)) {
			    rev.merged_from = cbranch;
			    maxrev = crev;
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

void rev_fast_export(std::ifstream &infile, std::ofstream &outfile, long int rev_num_min, long int rev_num_max)
{
    std::string empty_sha1("da39a3ee5e6b4b0d3255bfef95601890afd80709");
    std::map<long int, struct svn_revision>::iterator r_it;
    for (r_it = revs.begin(); r_it != revs.end(); r_it++) {
	struct svn_revision &rev = r_it->second;

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
	    std::string rbranch = rev.nodes[0].branch;
	    if (rev.revision_number == 66223) {
		std::cout << "at 66223\n";
	    }

	    for (size_t n = 0; n != rev.nodes.size(); n++) {
		struct svn_node &node = rev.nodes[n];

		if (node.tag_add || node.tag_edit) {
		    std::cout << "Processing revision " << rev.revision_number << "\n";
		    std::string ppath, bbpath, llpath;
		    int is_tag;
		    node_path_split(node.copyfrom_path, ppath, bbpath, llpath, &is_tag);

		    int edited_tag_minr = -1;
		    int edited_tag_maxr = -1;

		    // For edited tags - first tag create branch, final tag is "real" tag,
		    // else just branch commits
		    if (edited_tags.find(node.branch) != edited_tags.end()) {
			edited_tag_minr = edited_tag_first_rev[node.branch];
			edited_tag_maxr = edited_tag_max_rev[node.branch];
			std::cout << node.branch << ": " << edited_tag_minr << " " << edited_tag_maxr << "\n";

			if (rev.revision_number < edited_tag_minr) {
			    std::string tbstr = std::string("refs/heads/") +  node.branch;
			    std::cout << "Adding tag branch " << tbstr << " from " << bbpath << ", r" << rev.revision_number <<"\n";
			    std::cout << rev.commit_msg << "\n";
			    outfile << "reset " << tbstr << "\n";
			    outfile << "from " << branch_head_id(bbpath) << "\n";
			    branch_head_ids[tbstr] = branch_head_ids[bbpath];
			    have_commit = 1;
			    continue;
			} else {
			    if (rev.revision_number == edited_tag_maxr) {
				tag_after_commit = 1;
				std::string tbstr = std::string("refs/heads/") +  node.branch;
				node.branch = tbstr;
				rbranch = tbstr;
				continue;
			    } else {
				std::cout << "Non-final tag edit, processing normally: " << node.branch << ", r" << rev.revision_number<< "\n";
				std::string tbstr = std::string("refs/heads/") +  node.branch;
				node.branch = tbstr;
				rbranch = tbstr;
			    }
			}
		    } else {
			std::cout << "[TODO] Adding tag " << node.branch << " from " << bbpath << ", r" << rev.revision_number << "\n";
			have_commit = 1;
			continue;
		    }

		}

		if (node.branch_add) {
		    std::cout << "Processing revision " << rev.revision_number << "\n";
		    std::string ppath, bbpath, llpath;
		    int is_tag;
		    node_path_split(node.copyfrom_path, ppath, bbpath, llpath, &is_tag);
		    std::cout << "Adding branch " << node.branch << " from " << bbpath << ", r" << rev.revision_number <<"\n";
		    std::cout << rev.commit_msg << "\n";
		    outfile << "reset " << node.branch << "\n";
		    outfile << "from " << branch_head_id(bbpath) << "\n";
		    branch_head_ids[node.branch] = branch_head_ids[bbpath];
		    have_commit = 1;
		    continue;
		}

		if (node.branch_delete) {
		    //std::cout << "Processing revision " << rev.revision_number << "\n";
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
		if (author_map.find(rev.author) == author_map.end()) {
		    std::cout << "Error - couldn't find author map for author " << rev.author << " on revision " << rev.revision_number << "\n";
		    exit(1);
		}
		outfile << "commit " << rbranch << "\n";
		outfile << "mark :" << rev.revision_number << "\n";
		outfile << "committer " << author_map[rev.author] << " " << svn_time_to_git_time(rev.timestamp.c_str()) << "\n";
		outfile << "data " << rev.commit_msg.length() << "\n";
		outfile << rev.commit_msg << "\n";
		outfile << "from " << branch_head_id(rbranch) << "\n";
		branch_head_ids[rbranch] = std::to_string(rev.revision_number);

		for (size_t n = 0; n != rev.nodes.size(); n++) {
		    struct svn_node &node = rev.nodes[n];
		    /* Don't add directory nodes themselves - git works on files */
		    if (node.kind == ndir && node.action == nadd) continue;
		    if (node.kind == ndir && node.action == ndelete) {
			std::cerr << "deleting " << node.path << "\n";
			std::cerr << "TODO - do we get individual file deletes, or do we need to figure out what was in the directory?  If the latter, we're going to have to walk the currently active paths and delete any that match this node path...\n";
		    }

		    //std::cout << "	Processing node " << print_node_action(node.action) << " " << node.local_path << " into branch " << node.branch << "\n";
		    std::string gsha1;
		    if (node.action == nchange || node.action == nadd || node.action == nreplace) {
			if (node.exec_change || node.copyfrom_path.length() || node.text_content_length || node.text_content_sha1.length()) {
			    std::string tpath = std::string("brlcad/trunk/") + node.local_path;
			    gsha1 = svn_sha1_to_git_sha1[current_sha1[node.path]];
			    if (gsha1.length() < 40) {
				if (node.copyfrom_rev) {
				    gsha1 = svn_sha1_to_git_sha1[node.text_copy_source_sha1];
				}
				if (gsha1.length() < 40) {
				    // TODO - this isn't (necessarily) right - need to do this by looking up the copyfrom_path of the current branch
				    // and going the path lookup using the origin branch.  If the source branch isn't trunk, this could be flat
				    // out wrong.
				    gsha1 = svn_sha1_to_git_sha1[current_sha1[tpath]];
				    if (gsha1.length() < 40) {
					std::cout << "Fatal - could not find git sha1 - r" << rev.revision_number << ", node: " << node.path << "\n";
					std::cout << "current sha1: " << current_sha1[node.path] << "\n";
					std::cout << "trunk sha1: " << current_sha1[tpath] << "\n";
					std::cout << "Revision merged from: " << rev.merged_from << "\n";
					print_node(node);
					exit(1);
				    } else {
					std::cout << "Warning - couldn't find SHA1 for " << node.path << ", using node from " << tpath << "\n";
				    }
				}
			    }
			} else {
			    std::cout << "Warning skipping " << node.path << " - r" << rev.revision_number << " - no git applicable change found.\n";
			}
			continue;
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
		    if (node.exec_path) {
			outfile << "100755 ";
		    } else {
			outfile << "100644 ";
		    }

		    if (node.action == ndelete) {
			outfile << "\"" << node.local_path << "\"\n";
			continue;
		    }
		    if (node.action == nchange || node.action == nadd || node.action == nreplace) { 
			outfile << gsha1 << " \"" << node.local_path << "\"\n";
			continue;
		    }

		    std::cout << "Error - unhandled node action: " << print_node_action(node.action) << "\n";
		    exit(1);
		}
		if (tag_after_commit) {
		    // Note - in this situation, we need to both build a commit and do a tag.  Will probably
		    // take some refactoring.  Merge information will also be a factor.
		    std::cout << "[TODO] Adding final commit and tag " << rbranch << ", r" << rev.revision_number<< "\n";
		}

	    } else {
		if (!branch_delete && !have_commit) {
		    std::cout << "Skipping SVN commit r" << rev.revision_number << " - no git applicable changes\n";
		    std::cout << rev.commit_msg << "\n";
		}
	    }
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

void
load_branch_head_sha1s(const char *f)
{
    std::ifstream hfile(f);
    std::string line;
    while (std::getline(hfile, line)) {
	size_t spos = line.find_first_of(" ");
	std::string hsha1 = line.substr(0, spos);
	std::string hbranch = line.substr(spos+1, std::string::npos);
	branch_head_ids[hbranch] = hsha1;
    }
}

void
load_blob_sha1s(const char *f)
{

    std::ifstream bfile(f);
    std::string line;
    while (std::getline(bfile, line)) {
	size_t spos = line.find_first_of(" ");
	std::string hsha1 = line.substr(0, spos);
	cvs_blob_sha1.insert(hsha1);
    }
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
    if (argc < 4) {
	std::cerr << "svnfexport dumpfile author_map head_sha1s blob_sha1s\n";
	return 1;
    }

    /* Populate valid_projects */
    valid_projects.insert(std::string("brlcad"));
    /*
    valid_projects.insert(std::string("gct"));
    valid_projects.insert(std::string("geomcore"));
    valid_projects.insert(std::string("iBME"));
    valid_projects.insert(std::string("isst"));
    valid_projects.insert(std::string("jbrlcad"));
    valid_projects.insert(std::string("osl"));
    valid_projects.insert(std::string("ova"));
    valid_projects.insert(std::string("rt^3"));
    valid_projects.insert(std::string("rtcmp"));
    valid_projects.insert(std::string("web"));
    valid_projects.insert(std::string("webcad"));
    */

    /* SVN has some empty files - tell our setup how to handle them */
    svn_sha1_to_git_sha1[std::string("da39a3ee5e6b4b0d3255bfef95601890afd80709")] = std::string("e69de29bb2d1d6434b8b29ae775ad8c2e48c5391");


    /* Branch/tag name mappings */
    branch_mappings[std::string("framebuffer-experiment")] = std::string("framebuffer-experiment");

    /* Read in pre-existing branch sha1 heads from git */
    load_author_map(argv[2]);

    /* Read in pre-existing branch sha1 heads from git */
    load_branch_head_sha1s(argv[3]);

    /* Read in pre-existing blob sha1s from git */
    load_blob_sha1s(argv[4]);

    int rev_cnt = load_dump_file(argv[1]);
    if (rev_cnt > 0) {
	std::cout << "Dump file " << argv[1] << " loaded - found " << rev_cnt << " revisions\n";
	analyze_dump();
    } else {
	std::cerr << "No revision found - quitting\n";
	return -1;
    }

    std::ifstream infile(argv[1]);
    std::ofstream outfile1("r29887-r31038.fi", std::ios::out | std::ios::binary);
    if (!outfile1.good()) return -1;
    rev_fast_export(infile, outfile1, 29887, 31038);
    outfile1.close();

    // r31039 needs special handling

    infile.seekg(0);

    std::ofstream outfile2("r31040-r32313.fi", std::ios::out | std::ios::binary);
    if (!outfile2.good()) return -1;
    rev_fast_export(infile, outfile2, 31040, 32313);
    outfile2.close();


    // r32314 needs special handling

    infile.seekg(0);

    std::ofstream outfile3("r32315-r36471.fi", std::ios::out | std::ios::binary);
    if (!outfile3.good()) return -1;
    rev_fast_export(infile, outfile3, 32315, 36471);
    outfile3.close();

     // r36472 needs special handling

   infile.seekg(0);

    std::ofstream outfile4("r36473-r36632.fi", std::ios::out | std::ios::binary);
    if (!outfile4.good()) return -1;
    rev_fast_export(infile, outfile4, 36473, 36632);
    outfile4.close();

    // r36633 needs special handling

  infile.seekg(0);

    std::ofstream outfile5("r36634-r36842.fi", std::ios::out | std::ios::binary);
    if (!outfile5.good()) return -1;
    rev_fast_export(infile, outfile5, 36634, 36842);
    outfile5.close();

    // r36843 needs special handling

    infile.seekg(0);

    std::ofstream outfile6("r36844-r39464.fi", std::ios::out | std::ios::binary);
    if (!outfile6.good()) return -1;
    rev_fast_export(infile, outfile6, 36844, 39464);
    outfile6.close();

    // r39465 needs special handling

    infile.seekg(0);

    std::ofstream outfile7("r39466-r73000.fi", std::ios::out | std::ios::binary);
    if (!outfile7.good()) return -1;
    rev_fast_export(infile, outfile7, 39466, 73000);
    outfile7.close();


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
