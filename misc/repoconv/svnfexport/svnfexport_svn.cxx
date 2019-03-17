
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
	branch = std::string("master");
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
	branch = bs;
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

// TODO - we're going to have to special case r29982 - we need the current
// state of that branch at that revision to update the executable properties
// of .sh scripts, and the current hack of pulling the master state won't
// work because trunk's copies at that revision don't match STABLE.

void
analyze_dump()
{
    std::map<std::string, long int> path_last_commit;
    std::map<long int, struct svn_revision>::iterator r_it;

    // First pass - find all copyfrom_path instances where we are going to need to track
    // the contents at one or more specific revisions
    for (r_it = revs.begin(); r_it != revs.end(); r_it++) {
	struct svn_revision &rev = r_it->second;
	if (rev.revision_number == 32046) {
	    std::cout << "32046\n";
	}
	std::set<std::pair<std::string, long int>> dnodes;
	std::set<std::pair<std::string, long int>>::iterator d_it;
	for (size_t n = 0; n != rev.nodes.size(); n++) {
	    struct svn_node &node = rev.nodes[n];
	    if (node.copyfrom_path.length() && !node.text_copy_source_sha1.length()) {
		//std::cout << "(r" << rev.revision_number << "): copying directory " << node.path << " from " << node.copyfrom_path << " at revision " << node.copyfrom_rev << "\n";
		int cp_is_tag, tp_is_tag;
		std::string cpproject, cpbranch, cplocal_path, cptag;
		std::string tpproject, tpbranch, tplocal_path, tptag;
		node_path_split(node.path, cpproject, cpbranch, cptag, cplocal_path, &cp_is_tag);
		node_path_split(node.copyfrom_path, tpproject, tpbranch, tptag, tplocal_path, &tp_is_tag);


		// If the copy is from the immediately previous commit in the
		// same branch, we haven't had time to make any changes that
		// would invalidate a straight-up "C" operation in git.  If
		// it's from an older state, "C" won't work and we need to
		// explicitly reassemble the older directory state.

		if ((cpbranch != tpbranch &&  node.local_path.length())|| node.copyfrom_rev < path_last_commit[node.copyfrom_path]) {
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
	    std::cout << "(r" << rev.revision_number << "): copying subsequently edited directory " << d_it->first << " at revision " << d_it->second << "\n";
	    path_states[d_it->first][d_it->second] = new std::set<struct svn_node *>;
	    rebuild_revs.insert(rev.revision_number);
	}
    }

    // Second pass - assemble node sets for the paths and revisions we will need to know
    for (r_it = revs.begin(); r_it != revs.end(); r_it++) {
	struct svn_revision &rev = r_it->second;
	if (rev.revision_number % 1000 == 0) {
	    std::cout << "Pass 2: " << rev.revision_number << "\n";
	}
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
		if (node.branch == std::string("dmtogl-branch")) {
		    node.branch = std::string("dmtogl");
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
		    node.branch = node.tag;
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

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
