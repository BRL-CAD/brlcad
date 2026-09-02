/*               E M B E D D E D _ C H E C K . C X X
 * BRL-CAD
 *
 * Copyright (c) 2018-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** @file embedded_check.cxx
 *
 * Check file for certain copyright/license signatures, and for
 * those that are 3rd party check that the appropriate license
 * information is included.
 */

#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <map>
#include <string>
#include <utility>
#include <vector>

/* For performance, we don't read the entire file looking for
 * the copyright/license information. */
#define MAX_LINES_CHECK 100

struct RegexExemption {
    std::string pattern;
    std::regex expression;
    bool matched;

    explicit RegexExemption(std::string p)
	: pattern(std::move(p)), expression(pattern), matched(false) {}
};

struct EmbeddedCheckConfig {
    std::vector<RegexExemption> file_exemptions;
    std::vector<RegexExemption> public_domain_exemptions;
};

static bool
match_exemptions(std::vector<RegexExemption> &exemptions, const std::string &path)
{
    bool matched = false;
    for (auto &exemption : exemptions) {
	if (std::regex_match(path, exemption.expression)) {
	    exemption.matched = true;
	    matched = true;
	}
    }
    return matched;
}

static int
report_unmatched_exemptions(const char *kind,
			    const std::vector<RegexExemption> &exemptions)
{
    int unmatched = 0;
    for (const auto &exemption : exemptions) {
	if (!exemption.matched) {
	    std::cerr << "Unmatched " << kind << " exemption pattern: "
		      << exemption.pattern << "\n";
	    ++unmatched;
	}
    }
    return unmatched;
}

static void
init_embedded_check_config(EmbeddedCheckConfig &cfg)
{
    /* These files are outside the scope of the embedded license check or
     * carry their own third-party licensing information. */
    const char *file_exempt[] = {
	"misc/tools/.*",
	"misc/CMake/.*",
	"misc/opencl-raytracer-tests/version1/other/OpenCL/cl[.]hpp$",
	"doc/.*",
	"regress/licenses/embedded_check[.]cpp$",
	nullptr
    };
    for (int i = 0; file_exempt[i]; ++i) {
	cfg.file_exemptions.emplace_back(std::string(".*[\\\\/]") +
					 file_exempt[i]);
    }

    /* Public-domain input data mentioned by gaia.c is not the license for
     * the BRL-CAD source file itself. */
    cfg.public_domain_exemptions.emplace_back(".*/src/proc-db/gaia[.]c$");
}

int
process_file(const std::string &f,
	     EmbeddedCheckConfig &cfg,
	     std::map<std::string, std::string> &file_to_license)
{
    std::regex cad_regex(".*BRL-CAD.*");
    std::regex copyright_regex(".*[Cc]opyright.*[1-2][0-9][0-9][0-9].*");
    std::regex gov_regex(".*United[ ]States[ ]Government.*");
    std::regex pd_regex(".*[Pp]ublic[ ][Dd]omain.*");
    std::string sline;
    std::ifstream fs;
    fs.open(f);
    if (!fs.is_open()) {
	std::cerr << "Unable to open " << f << " for reading, skipping\n";
	return -1;
    }
    int lcnt = 0;
    bool brlcad_file = false;
    bool gov_copyright = false;
    bool other_copyright = false;
    bool public_domain = false;

    // Check the first MAX_LINES_CHECK lines for copyright statements.
    while (std::getline(fs, sline) && lcnt < MAX_LINES_CHECK) {
	if (std::regex_match(sline, cad_regex)) {
	    brlcad_file = true;
	}
	if (std::regex_match(sline, copyright_regex)) {
	    if (std::regex_match(sline, gov_regex)) {
		gov_copyright = true;
	    } else {
		other_copyright = true;
	    }
	} else {
	    if (std::regex_match(sline, pd_regex)) {
		public_domain = true;
	    }
	}
	lcnt++;
    }
    fs.close();

    bool public_domain_exempt =
	match_exemptions(cfg.public_domain_exemptions, f);
    bool public_domain_check = public_domain && !public_domain_exempt;

    if (gov_copyright && public_domain_check) {
	if (file_to_license.find(f) == file_to_license.end()) {
	    std::cerr << "FILE " << f << " has no associated reference in a license file! (gov copyright + public domain references)\n";
	    return 1;
	}
	return 0;
    }
    if (gov_copyright && other_copyright) {
	if (file_to_license.find(f) == file_to_license.end()) {
	    std::cerr << "FILE " << f << " has gov copyright + additional copyrights, bot no documenting file in doc/legal/embedded\n";
	    return 1;
	}
	return 0;
    }
    if (other_copyright) {
	if (file_to_license.find(f) == file_to_license.end()) {
	    std::cerr << "FILE " << f << " has no associated reference in a license file!\n";
	    return 1;
	}
	return 0;
    }

    if (public_domain_check) {
	if (!brlcad_file) {
	    if (file_to_license.find(f) == file_to_license.end()) {
		std::cout << f << " references the public domain, is not a BRL-CAD file, but has no documenting file in doc/legal/embedded\n";
		return 1;
	    }
	}
	return 0;
    }
    if (!gov_copyright && !other_copyright && !public_domain) {
	if (file_to_license.find(f) == file_to_license.end()) {
	    std::cout << "FILE " << f << " has no info\n";
	    return 1;
	} else {
	    std::cout << f << " has no embedded info but is referenced by license file " << file_to_license[f] << "\n";
	    return 1;
	}
    }
    return 0;
}

int
main(int argc, const char *argv[])
{
    try {

	if (argc < 4) {
	    std::cerr << "Usage: embedded_check [-v] licenses_list file_list src_root\n";
	    return -1;
	}

	std::regex f_regex("file:/(.*)");
	std::regex srcfile_regex(".*[.](c|cpp|cxx|h|hpp|hxx|tcl)$");
	std::regex svn_regex(".*[\\/][.]svn[\\/].*");
	std::string root_path(argv[3]);

	EmbeddedCheckConfig cfg;
	init_embedded_check_config(cfg);

	std::map<std::string, std::string> file_to_license;
	std::set<std::string> unused_licenses;

	int bad_ref_cnt = 0;
	std::string lfile;
	std::ifstream license_file_stream;
	license_file_stream.open(argv[1]);
	if (!license_file_stream.is_open()) {
	    std::cerr << "Unable to open license file list " << argv[1] << "\n";
	}
	while (std::getline(license_file_stream, lfile)) {
	    if (std::regex_match(lfile, svn_regex)) {
		std::cerr << "Skipping .svn file " << lfile << "\n";
		continue;
	    }
	    int valid_ref_cnt = 0;
	    std::string lline;
	    std::ifstream license_stream;
	    license_stream.open(lfile);
	    if (!license_stream.is_open()) {
		std::cerr << "Unable to open license file " << lfile << "\n";
		continue;
	    }
	    while (std::getline(license_stream, lline)) {
		if (!std::regex_match(std::string(lline), f_regex)) {
		    continue;
		}
		std::smatch lfile_ref;
		if (!std::regex_search(lline, lfile_ref, f_regex)) {
		    continue;
		}
		std::string lfile_id =  root_path + std::string("/") + std::string(lfile_ref[1]);
		std::ifstream lfile_s(lfile_id);
		if (!lfile_s.good()) {
		    std::cout << "Bad reference in license file " << lfile << ": " << lline << "\n";
		    std::cout << "    file \"" << lfile_id << "\" not found on filesystem.\n";
		    bad_ref_cnt++;
		    continue;
		}
		lfile_s.close();
		file_to_license[lfile_id] = lfile;
		valid_ref_cnt++;
	    }
	    license_stream.close();
	    if (!valid_ref_cnt) {
		std::cout << "Unused license: " << lfile << "\n";
		unused_licenses.insert(lfile);
	    }
	}
	license_file_stream.close();

	int process_fail_cnt = 0;
	std::string sfile;
	std::ifstream src_file_stream;
	src_file_stream.open(argv[2]);
	if (!src_file_stream.is_open()) {
	    std::cerr << "Unable to open source file list " << argv[2] << "\n";
	}
	while (std::getline(src_file_stream, sfile)) {
	    if (match_exemptions(cfg.file_exemptions, sfile)) {
		continue;
	    }
	    if (!std::regex_match(std::string(sfile), srcfile_regex)) {
		continue;
	    }
	    process_fail_cnt += process_file(sfile, cfg, file_to_license);
	}
	src_file_stream.close();

	int unmatched_exemption_cnt =
	    report_unmatched_exemptions("file", cfg.file_exemptions);
	unmatched_exemption_cnt += report_unmatched_exemptions(
		"public-domain", cfg.public_domain_exemptions);

	if (unused_licenses.size() || bad_ref_cnt || process_fail_cnt ||
		unmatched_exemption_cnt) {
	    return -1;
	}

    }

    catch (const std::regex_error& e) {
	std::cout << "regex error: " << e.what() << '\n';
	return -1;
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
