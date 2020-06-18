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
#include "./sha1.hpp"
#include "./svn_date.h"

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
std::map<std::string,std::string> svn_sha1_to_git_sha1;

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

std::string git_sha1(std::ifstream &infile, struct svn_node *n)
{
    std::string git_sha1;
    std::string go_buff;
    char *cbuffer = new char [n->text_content_length];;
    infile.read(cbuffer, n->text_content_length);
    go_buff.append("blob ");

    if (n->path.find("db/terra.dsp") != std::string::npos) {
	return std::string("b668f149e679cc373d702dee8143a3feffb2b130");
    }

    if (!n->crlf_content || !n->local_path.compare("db/terra.dsp")) {
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

    if (n.path == "brlcad/trunk/src/vfont/font.h") {
	if (rev->revision_number == 30333) {
	    std::cout << rev->revision_number << ": " << svn_sha1_to_git_sha1[n.text_content_sha1] << "\n";
	}
    }

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

    // While it is techically possible to have multi-project commits (and there
    // are some in BRL-CAD's early history) the commit range we are concerned with
    // for appending onto the CVS git conversion does not have any such commits.
    // Accordingly, can use the project from the first node path for the whole
    // revision.
    if (rev.nodes.size() > 0) {
	rev.project = rev.nodes[0].project; 
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
	    if (node.text_content_length > 0 && node.tag_path) {
		node.tag_edit = 1;
		edited_tags.insert(node.branch);
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


void write_msgs(std::ifstream &infile, std::ofstream &outfile)
{
    for (size_t n = 0; n < 29886; n++) {
	struct svn_revision &rev = revs[n];
	std::string msg = rev.commit_msg;
	msg.erase(std::remove(msg.begin(), msg.end(), '\n'), msg.end());
	msg.erase(std::remove(msg.begin(), msg.end(), '\r'), msg.end());
	std::string tstp = svn_time_to_git_time(rev.timestamp.c_str());
	std::string trimstr(" +0000");
	size_t spos = tstp.find(trimstr);
	if (spos != std::string::npos) {
	    tstp.erase(spos, trimstr.length());
	}
	if (rev.nodes.size() && rev.nodes[0].branch.length()) {
	    outfile << rev.revision_number << "," << tstp << "," << msg << "\n";
	}
    }

}

int main(int argc, const char **argv)
{
    /* Populate valid_projects */
    valid_projects.insert(std::string("brlcad"));

    /* Branch/tag name mappings */
    branch_mappings[std::string("framebuffer-experiment")] = std::string("framebuffer-experiment");

    int rev_cnt = load_dump_file(argv[1]);
    if (rev_cnt > 0) {
	std::cout << "Dump file " << argv[1] << " loaded - found " << rev_cnt << " revisions\n";
	analyze_dump();
    } else {
	std::cerr << "No revision found - quitting\n";
	return -1;
    }

    std::string tbranch;

    std::string ofile = std::string(argv[1]) + std::string(".txt");
    std::ifstream infile(argv[1]);
    std::ofstream outfile("svn_msgs.txt", std::ios::out | std::ios::binary);
    if (!outfile.good()) return -1;
    write_msgs(infile, outfile);
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
