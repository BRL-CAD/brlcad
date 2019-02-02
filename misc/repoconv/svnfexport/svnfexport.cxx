#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "sha1.hpp"

enum svn_node_kind_t { nkerr, nfile, ndir };
enum svn_node_action_t { naerr, nchange, nadd, ndelete, nreplace };

std::map<std::string,std::pair<size_t, long int>> sha1_blobs;

std::set<std::string> exec_paths;
std::map<std::string,std::string> current_sha1;

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

struct svn_repo_data {
    /* Commits not associated with any project/branch */
    std::set<long int> unaffiliated;

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
};

struct svn_repo {
    std::string dump_file;
    int dump_format_version;
    std::string uuid;
    std::map<long int, struct svn_revision> revs;
    std::map<std::string, std::string> pp;
    std::map<std::string, std::string> bp;
    std::map<std::string, std::string> tp;
    std::set<std::string> active_projects;
    struct svn_repo_data *rd;
};

/**
 * Parses a dump file and returns a repository information
 * structure, or NULL on failure.
 *
 * Assumes dump file format as documented here:
 * http://svn.apache.org/repos/asf/subversion/trunk/notes/dump-load-format.txt
 */
int load_dump(const char *f, struct svn_repo *r, std::string *llog);


/**
 * Given a populated svn_repo structure, performs a variety of analysis
 * operations to build up useful data sets.
 *
 * Some information is stored in the svn_repo containers for ease of access,
 * and some sets are built up in the svn_repo_data container.
 *
 * Analyze uses the maps in svn_repo, if populated, to identify specific
 * substrings of the subversion paths as special case rules.  Generally, the
 * flow is as follow:
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
int analyze_dump(struct svn_repo *r, const char *default_project);


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
	char vbuff[v+1] = {'\0'};
	infile.read(vbuff, v);
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
    int set_exec = 0;

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
	    set_exec = 1;
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
	char vbuff[v+1] = {'\0'};
	infile.read(vbuff, v);
	value = std::string(vbuff);

	// Have key value
	n->node_props[key] = value;
    }

    // Make the exec_paths set match the path's current prop state.
    if (set_exec) {
	exec_paths.insert(n->path);
    } else {
	std::set<std::string>::iterator e_it = exec_paths.find(n->path);
	if (e_it != exec_paths.end()) {
	    exec_paths.erase(n->path);
	}
    }
}


void
print_revision(struct svn_revision &rev)
{
    std::cout << "Revision: " << rev.revision_number << std::endl;
    std::cout << "svn-author: " << rev.author << std::endl;
    std::cout << "svn-date: " << rev.timestamp << std::endl;
    std::cout << "svn-log: " << rev.commit_msg << std::endl;
    if (rev.nodes.size() > 0) {
	std::cout << "Node paths: " << std::endl;
	for (size_t n = 0; n < rev.nodes.size(); n++) {
	    std::cout << "     " << rev.nodes[n].path << std::endl;
	}
    }
}

void
print_node(struct svn_node &n)
{
    std::cout << "Revision: " << n.revision_number << std::endl;
    std::cout << "Node-path: " << n.path << std::endl;
    std::cout << "Node-kind: " << n.kind << std::endl;
    std::cout << "Node-action: " << n.action << std::endl;
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


    if (n.path == "brlcad/trunk/autogen.sh") {
	std::cout << rev->revision_number << ": " << n.text_content_sha1 << "\n";
    }


    // If we have content, store the file offset and jump the seek beyond it
    if (n.text_content_length > 0) {
	// TODO - maybe as an option, check the md5 and sha1 of the found
	// content against the stored properties as a data integrity and
	// accuracy guarantee
	n.content_start = infile.tellg();
	if (n.path == "brlcad/trunk/autogen.sh" && rev->revision_number == 28910) {
	    std::cout << rev->revision_number << ": " << n.text_content_sha1 << "\n";
	    std::ofstream outfile2("autogen.sh", std::ios::out | std::ios::binary);
	    char *buffer = new char [n.text_content_length];;
	    infile.read(buffer, n.text_content_length);
	    outfile2.write(buffer, n.text_content_length);
	    outfile2.close();
	}
	sha1_blobs.insert(std::pair<std::string,std::pair<size_t, long int>>(n.text_content_sha1, std::pair<size_t, long int>(n.content_start, n.text_content_length)));
	size_t after_content = n.content_start + n.text_content_length + 1;
	infile.seekg(after_content);
    }

    //print_node(n);

    // Have at least some node contents (last node in file?), return
    rev->nodes.push_back(n);
    return 1;
}

/* Return 1 if we successfully processed a revision, else 0 */
int
process_revision(std::ifstream &infile, struct svn_repo *r)
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

    if (rev.revision_number == 29887) {
	std::cout << "At rev " << rev.revision_number << "\n";
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

    r->revs.insert(std::pair<long int, struct svn_revision>(rev.revision_number, rev));
    return success;
}

// TODO - support verbose option that will do timed reporting (every n seconds,
// print out current processing revision)
int
load_dump(const char *f, struct svn_repo *r, std::string *llog)
{
    int revs = 0;
    r->dump_file = std::string(f);
    std::ifstream infile(f);
    if (!infile.good()) return -1;
    std::string line;
    std::string fmtkey("SVN-fs-dump-format-version: ");
    int dump_format_version = -1;
    // The first non-empty line has to be the format version, or we're done.
    while (std::getline(infile, line) && !line.length());
    if (line.compare(0, fmtkey.length(), fmtkey)) {
	if (llog) (*llog).append("SVN: load_dump error, no format found\n");
	return -1;
    }

    // Grab the format number
    r->dump_format_version = svn_lint(line, fmtkey);

    while (!r->uuid.length() && std::getline(infile, line)) {
	r->uuid = svn_str(line, std::string("UUID: "));
    }

    /* As long as we're not done, read revisions */
    while (infile.peek() != EOF) {
	revs += process_revision(infile, r);
    }

    infile.close();

    return revs;
}

std::string
node_project(const char *d, std::map<std::string, std::string> &pp, std::string &wpath)
{
    std::string t("trunk");
    std::string b("branches");
    std::string tag("tags");
    std::string proj;
    size_t spos = wpath.find_first_of("/");
    if (spos != std::string::npos) {
	std::string ps = wpath.substr(0, spos);
	if (pp.find(ps) != pp.end()) {
	    proj = pp[ps];
	    if (wpath.length() > spos + 1 && spos != std::string::npos) {
		wpath = wpath.substr(spos + 1, std::string::npos);
	    } else {
		wpath = std::string();
	    }
	} else {
	    if (!ps.compare(t) || !ps.compare(b) || !ps.compare(tag)) {
		proj = std::string(d);
		if (wpath.length() > spos + 1 && spos != std::string::npos) {
		    wpath = wpath.substr(spos + 1, std::string::npos);
		} else {
		    wpath = std::string();
		}
	    }
	}
    } else {
	if (pp.find(wpath) != pp.end()) {
	    proj = pp[wpath];
	    wpath = std::string();
	}
    }
    return proj;
}


std::string
node_branch(std::map<std::string, std::string> &bp, std::map<std::string, std::string> &tp, std::string &wpath, int *is_tag)
{
    size_t spos;
    std::string bs, rp;
    std::string t("trunk");
    std::string b("branches");
    std::string tag("tags");
    *is_tag = 0;

    // Is it trunk?
    if (!wpath.compare(0, t.length(), t)) {
	if (wpath.length() > t.length() + 1) {
	    wpath = wpath.substr(t.length() + 1, std::string::npos);
	} else {
	    wpath = std::string();
	}
	return std::string("refs/heads/master");
    }

    // Is it a branch?
    if (!wpath.compare(0, b.length(), b)) {
	if (wpath.length() > b.length()) {
	    wpath = wpath.substr(b.length() + 1, std::string::npos);
	} else {
	    // Huh?? branches with nothing after it??
	    return std::string();
	}
	spos = wpath.find_first_of("/");
	if (spos != std::string::npos) {
	    bs = wpath.substr(0, spos);
	    wpath = (wpath.length() > bs.length()) ? wpath.substr(spos + 1, std::string::npos) : std::string();
	} else {
	    bs = wpath;
	    wpath = std::string();
	}
	return std::string("refs/heads/") + bs;
    }

    // Is it a tag?
    if (!wpath.compare(0, tag.length(), tag)) {
	*is_tag = 1;
	if (wpath.length() > tag.length()) {
	    wpath = wpath.substr(tag.length() + 1, std::string::npos);
	} else {
	    // Huh?? tags with nothing after it??
	    return std::string();
	}
	spos = wpath.find_first_of("/");
	if (spos != std::string::npos) {
	    bs = wpath.substr(0, spos);
	    wpath = (wpath.length() > bs.length()) ? wpath.substr(spos + 1, std::string::npos) : std::string();
	} else {
	    bs = wpath;
	    wpath = std::string();
	}
	return bs;
    }

    // Not one of the standards, see if it's called out in the maps
    spos = wpath.find_first_of("/");
    bs = (spos != std::string::npos) ? wpath.substr(0, spos) : wpath;
    if (bp.find(bs) != bp.end() || tp.find(bs) != tp.end()) {
	wpath = (spos == std::string::npos) ? std::string() : wpath.substr(bs.length() + 1, std::string::npos);
	if (tp.find(bs) != tp.end()) {
	    *is_tag = 1;
	    return tp[bs];
	}
     	return bp[bs];
    }

    return std::string();
}


int
analyze_dump(struct svn_repo *rp, const char *default_project)
{
    std::map<long int, struct svn_revision>::iterator r_it;
    for (r_it = rp->revs.begin(); r_it != rp->revs.end(); r_it++) {
	long int maxrev = 0;
	struct svn_revision &rev = r_it->second;
	for (size_t n = 0; n != rev.nodes.size(); n++) {
	    struct svn_node &node = rev.nodes[n];
	    // Break up paths and build tag/branch lists
	    std::string wpath = node.path;
	    node.project = node_project(default_project, rp->pp, wpath);
	    node.branch = node_branch(rp->bp, rp->tp, wpath, &(node.tag_path));
	    node.local_path = wpath;
	    if (node.branch.length()) {
		if (node.tag_path) {
		    rp->rd->tags.insert(node.branch);
		} else {
		    rp->rd->branches.insert(node.branch);
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
		rp->rd->edited_tags.insert(node.branch);
		rp->rd->edited_tag_max_rev[node.branch] = rev.revision_number;
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
			std::string project = node_project(default_project, rp->pp, mpath);
			std::string revs = pline.substr(cpos+1, std::string::npos);
			std::string cbranch = node_branch(rp->bp, rp->tp, mpath, &is_tag);
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

void analyze_svn(struct svn_repo *r) {
    r->pp["brlcad"] = "brlcad";
    r->pp["gct"] = "gct";
    r->pp["geomcore"] = "geomcore";
    r->pp["iBME"] = "iBME";
    r->pp["isst"] = "isst";
    r->pp["jbrlcad"] = "jbrlcad";
    r->pp["osl"] = "osl";
    r->pp["ova"] = "ova";
    r->pp["rt^3"] = "rt_3";
    r->pp["rtcmp"] = "rtcmp";
    r->pp["web"] = "web";
    r->pp["webcad"] = "webcad";
    r->bp["framebuffer-experiment"] = "framebuffer-experiment";
    analyze_dump(r, "brlcad");
}


void rev_fast_export(std::ifstream &infile, std::ofstream &outfile,  struct svn_repo *rp, long int rev_num)
{
    std::map<long int, struct svn_revision>::iterator r_it;
    for (r_it = rp->revs.begin(); r_it != rp->revs.end(); r_it++) {
	struct svn_revision &rev = r_it->second;

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

	std::map<struct svn_node *, std::string> git_sha1_map;
	if (rev_num == rev.revision_number) {
	    std::cout << "Processing revision " << rev.revision_number << "\n";
	    for (size_t n = 0; n != rev.nodes.size(); n++) {
		struct svn_node &node = rev.nodes[n];
		std::pair<size_t, long int> sha1_info = sha1_blobs[current_sha1[node.path]];
		char *buffer = new char [sha1_info.second];
		infile.seekg(sha1_info.first);
		infile.read(buffer, sha1_info.second);
		if (node.text_content_length > 0) {
		    std::cout << "	Adding node object for " << node.local_path << "\n";
		    outfile << "blob\n";
		    outfile << "data " << sha1_info.second << "\n";
		    outfile.write(buffer, sha1_info.second);
		}
		/* get SHA1 for git - see https://stackoverflow.com/a/7225329/2037687 */
		std::string go_buff;
		go_buff.append("blob ");
		go_buff.append(std::to_string(sha1_info.second));
		go_buff.append(1, '\0');
		go_buff.append(buffer, sha1_info.second);
		std::string git_sha1 = sha1_hash_hex(go_buff.c_str(), go_buff.length());
		git_sha1_map[&node] = git_sha1;
		delete buffer;
	    }

	    outfile << "commit " << rev.nodes[0].branch << "\n";
	    outfile << "mark :" << rev.revision_number << "\n";
	    outfile << "committer " << "unknown <brlcad@unknown> 1200077647 -0500" << "\n";
	    outfile << "data " << rev.commit_msg.length() << "\n";
	    outfile << rev.commit_msg << "\n";
	    outfile << "from :" << rev.revision_number - 1 << "\n";

	    for (size_t n = 0; n != rev.nodes.size(); n++) {
		struct svn_node &node = rev.nodes[n];
		std::cout << "	Processing node " << print_node_action(node.action) << " " << node.local_path << " into branch " << node.branch << "\n";
		outfile << "M 100755 " << git_sha1_map[&node] << " \"" << node.local_path << "\"\n";
	    }
	}
    }
}

int main(int argc, const char **argv)
{
    std::string msgs;
    struct svn_repo r = {}; // Initialize to empty
    struct svn_repo_data d = {}; // Initialize to empty
    r.rd = &d;
    if (argc == 1) return 1;
    int rev_cnt = load_dump(argv[1], &r, &msgs);
    if (rev_cnt > 0) {
	std::cout << "Dump file " << argv[1] << " loaded - found " << rev_cnt << " revisions\n";
	analyze_svn(&r);
    } else {
	std::cerr << "No revision found - quitting\n";
	return -1;
    }

    std::ifstream infile(argv[1]);
    std::ofstream outfile("29887.fi", std::ios::out | std::ios::binary);
    if (!outfile.good()) return -1;
    rev_fast_export(infile, outfile,  &r, 29887);
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
