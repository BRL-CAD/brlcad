/*             L I N E E D I T _ C O M P L E T I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the U.S.
 * Army Research Laboratory.
 */

#include "common.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "bu/lineedit.h"
#include "bu/malloc.h"
#include "bu/str.h"


namespace {

const size_t completion_column_gap = 2;


struct completion_trie_node {
    std::string prefix;
    std::string frontier_prefix;
    std::string terminal;
    size_t count = 0;
    std::map<uint32_t, std::unique_ptr<completion_trie_node>> children;
};


struct completion_frontier {
    size_t width = 0;
    size_t score = 0;
    size_t covered = 0;
    std::vector<std::string> labels;
};


struct completion_summary_item {
    const completion_trie_node *node = NULL;
    bool terminal = false;
};


struct completion_summary_split {
    size_t index = 0;
    size_t score = 0;
    size_t item_count = 0;
};


static std::string
completion_display_escape(const char *input)
{
    std::string output;
    if (!input)
	return output;
    const unsigned char *cp = (const unsigned char *)input;
    while (*cp) {
	if (*cp == '\n') {
	    output += "\\n";
	    cp++;
	    continue;
	}
	if (*cp == '\r') {
	    output += "\\r";
	    cp++;
	    continue;
	}
	if (*cp == '\t') {
	    output += "\\t";
	    cp++;
	    continue;
	}
	if (*cp < 0x20 || *cp == 0x7f) {
	    const char digits[] = "0123456789abcdef";
	    char escaped[5] = {'\\', 'x', digits[*cp >> 4], digits[*cp & 0x0f], '\0'};
	    output += escaped;
	    cp++;
	    continue;
	}
	size_t length = 1;
	if ((*cp & 0xe0) == 0xc0) length = 2;
	else if ((*cp & 0xf0) == 0xe0) length = 3;
	else if ((*cp & 0xf8) == 0xf0) length = 4;
	bool valid = length > 1;
	for (size_t i = 1; valid && i < length; i++)
	    if (!cp[i] || (cp[i] & 0xc0) != 0x80) valid = false;
	if (length == 1 && *cp < 0x80)
	    valid = true;
	if (!valid) {
	    const char digits[] = "0123456789abcdef";
	    char escaped[5] = {'\\', 'x', digits[*cp >> 4], digits[*cp & 0x0f], '\0'};
	    output += escaped;
	    cp++;
	    continue;
	}
	output.append((const char *)cp, length);
	cp += length;
    }
    return output;
}


static size_t
completion_utf8_next(const std::string &input, size_t offset, uint32_t *codepoint)
{
    const unsigned char *p = (const unsigned char *)input.data() + offset;
    size_t remaining = input.size() - offset;
    if (!remaining)
	return 0;
    if (p[0] < 0x80) {
	*codepoint = p[0];
	return 1;
    }
    size_t length = 0;
    uint32_t value = 0;
    if ((p[0] & 0xe0) == 0xc0) { length = 2; value = p[0] & 0x1f; }
    else if ((p[0] & 0xf0) == 0xe0) { length = 3; value = p[0] & 0x0f; }
    else if ((p[0] & 0xf8) == 0xf0) { length = 4; value = p[0] & 0x07; }
    else { *codepoint = p[0]; return 1; }
    if (length > remaining) { *codepoint = p[0]; return 1; }
    for (size_t i = 1; i < length; i++) {
	if ((p[i] & 0xc0) != 0x80) { *codepoint = p[0]; return 1; }
	value = (value << 6) | (p[i] & 0x3f);
    }
    if ((length == 2 && value < 0x80) || (length == 3 && value < 0x800) ||
	(length == 4 && value < 0x10000) || value > 0x10ffff ||
	(value >= 0xd800 && value <= 0xdfff)) {
	*codepoint = p[0];
	return 1;
    }
    *codepoint = value;
    return length;
}


static size_t
completion_codepoint_width(uint32_t cp)
{
    if (cp < 0x20 || (cp >= 0x7f && cp < 0xa0))
	return 0;
    if ((cp >= 0x0300 && cp <= 0x036f) || (cp >= 0x1ab0 && cp <= 0x1aff) ||
	(cp >= 0x1dc0 && cp <= 0x1dff) || (cp >= 0x20d0 && cp <= 0x20ff) ||
	(cp >= 0xfe20 && cp <= 0xfe2f))
	return 0;
    if ((cp >= 0x1100 && cp <= 0x115f) || cp == 0x2329 || cp == 0x232a ||
	(cp >= 0x2e80 && cp <= 0x303e) || (cp >= 0x3040 && cp <= 0xa4cf) ||
	(cp >= 0xac00 && cp <= 0xd7a3) || (cp >= 0xf900 && cp <= 0xfaff) ||
	(cp >= 0xfe10 && cp <= 0xfe19) || (cp >= 0xfe30 && cp <= 0xfe6f) ||
	(cp >= 0xff00 && cp <= 0xff60) || (cp >= 0xffe0 && cp <= 0xffe6) ||
	(cp >= 0x1f300 && cp <= 0x1faff))
	return 2;
    return 1;
}


static size_t
completion_cell_width(const std::string &input)
{
    size_t width = 0;
    for (size_t offset = 0; offset < input.size();) {
	uint32_t cp = 0;
	size_t length = completion_utf8_next(input, offset, &cp);
	if (!length)
	    break;
	width += completion_codepoint_width(cp);
	offset += length;
    }
    return width;
}


static std::string
completion_clip(const std::string &input, size_t width)
{
    if (completion_cell_width(input) <= width)
	return input;
    size_t content_width = width > 3 ? width - 3 : width;
    std::string output;
    size_t used = 0;
    for (size_t offset = 0; offset < input.size();) {
	uint32_t cp = 0;
	size_t length = completion_utf8_next(input, offset, &cp);
	size_t cp_width = completion_codepoint_width(cp);
	if (!length || used + cp_width > content_width)
	    break;
	output.append(input, offset, length);
	used += cp_width;
	offset += length;
    }
    if (width > 3)
	output += "...";
    return output;
}


static std::string
completion_bin_label(const completion_trie_node *node)
{
    if (!node)
	return std::string();
    if (node->count == 1 && !node->terminal.empty() && node->children.empty())
	return node->terminal;

    std::string label = node->prefix.empty() ? "*" : node->prefix;
    label += " (" + std::to_string(node->count);
    label += (node->count == 1) ? " match)" : " matches)";
    return label;
}


static void
completion_trie_insert(completion_trie_node *root, const std::string &candidate)
{
    completion_trie_node *node = root;
    node->count++;

    for (size_t i = 0; i < candidate.size();) {
	uint32_t key = 0;
	size_t length = completion_utf8_next(candidate, i, &key);
	if (!length)
	    break;
	auto &child = node->children[key];
	if (!child) {
	    child.reset(new completion_trie_node());
	    child->prefix = candidate.substr(0, i + length);
	    child->frontier_prefix = child->prefix;
	}
	node = child.get();
	node->count++;
	i += length;
    }
    node->terminal = candidate;
}


/* Remove single-child nonterminal paths.  Such nodes do not describe a useful
 * partition boundary, and retaining them needlessly expands the width DP.
 * Keep frontier_prefix at the original branch boundary: the multi-row summary
 * can start with that compact grouping and show the longer common prefix only
 * when the available space permits it. */
static void
completion_trie_compress(completion_trie_node *node)
{
    if (!node)
	return;
    for (auto &entry : node->children)
	completion_trie_compress(entry.second.get());

    while (node->terminal.empty() && node->children.size() == 1) {
	auto only = node->children.begin();
	std::unique_ptr<completion_trie_node> child = std::move(only->second);
	node->prefix = child->prefix;
	node->terminal = child->terminal;
	node->children = std::move(child->children);
    }
}


static bool
completion_frontier_better(const completion_frontier &candidate,
	const completion_frontier &current)
{
    if (candidate.score != current.score)
	return candidate.score > current.score;
    if (candidate.covered != current.covered)
	return candidate.covered > current.covered;
    if (candidate.labels.size() != current.labels.size())
	return candidate.labels.size() > current.labels.size();
    return candidate.labels < current.labels;
}


/* Return the nondominated prefix frontiers for this subtree.  Width is small
 * (normally one terminal row), so an exact bounded knapsack is both fast and
 * deterministic while avoiding locally-greedy bin choices. */
static std::vector<completion_frontier>
completion_frontiers(const completion_trie_node *node, size_t width_limit,
	bool allow_partial = false)
{
    std::map<size_t, completion_frontier> by_width;
    if (allow_partial)
	by_width[0] = completion_frontier();
    std::string label = completion_bin_label(node);
    if (!label.empty() && completion_cell_width(label) <= width_limit) {
	completion_frontier unsplit;
	unsplit.width = completion_cell_width(label);
	unsplit.score = node->count * node->prefix.size();
	if (node->count == 1 && !node->terminal.empty())
	    unsplit.score = node->terminal.size();
	unsplit.covered = node->count;
	unsplit.labels.push_back(label);
	by_width[unsplit.width] = unsplit;
    }

    std::vector<std::vector<completion_frontier>> parts;
    bool split_possible = true;
    if (!node->terminal.empty() && !node->children.empty()) {
	completion_frontier terminal;
	terminal.width = completion_cell_width(node->terminal);
	terminal.score = node->terminal.size();
	terminal.covered = 1;
	terminal.labels.push_back(node->terminal);
	if (terminal.width <= width_limit) {
	    std::vector<completion_frontier> terminal_options;
	    if (allow_partial)
		terminal_options.push_back(completion_frontier());
	    terminal_options.push_back(terminal);
	    parts.push_back(std::move(terminal_options));
	}
	else if (allow_partial)
	    parts.push_back(std::vector<completion_frontier>(1, completion_frontier()));
	else
	    split_possible = false;
    }
    for (const auto &entry : node->children) {
	auto child_options = completion_frontiers(entry.second.get(), width_limit, allow_partial);
	if (!child_options.empty())
	    parts.push_back(std::move(child_options));
	else
	    split_possible = false;
    }

    if (split_possible && !parts.empty()) {
	std::vector<completion_frontier> combined(1);
	for (const auto &part : parts) {
	    std::map<size_t, completion_frontier> next;
	    for (const auto &left : combined) {
		for (const auto &right : part) {
		    completion_frontier joined = left;
		    size_t gap = joined.labels.empty() ? 0 : completion_column_gap;
		    if (joined.width + gap + right.width > width_limit)
			continue;
		    joined.width += gap + right.width;
		    joined.score += right.score;
		    joined.covered += right.covered;
		    joined.labels.insert(joined.labels.end(), right.labels.begin(), right.labels.end());
		    auto found = next.find(joined.width);
		    if (found == next.end() || completion_frontier_better(joined, found->second))
			next[joined.width] = std::move(joined);
		}
	    }
	    combined.clear();
	    for (auto &entry : next)
		combined.push_back(std::move(entry.second));
	    if (combined.empty())
		break;
	}
	for (auto &candidate : combined) {
	    auto found = by_width.find(candidate.width);
	    if (found == by_width.end() || completion_frontier_better(candidate, found->second))
		by_width[candidate.width] = std::move(candidate);
	}
    }

    std::vector<completion_frontier> result;
    for (auto &entry : by_width) {
	/* A wider frontier is dominated only if an earlier state supplies at
	 * least as much information and covers at least as many candidates. */
	bool dominated = false;
	for (const completion_frontier &prior : result) {
	    if (prior.score >= entry.second.score && prior.covered >= entry.second.covered) {
		dominated = true;
		break;
	    }
	}
	if (dominated)
	    continue;
	result.push_back(std::move(entry.second));
    }
    return result;
}


static bool
completion_full_layout(std::vector<std::string> *lines,
	const std::vector<std::string> &candidates, size_t width, size_t max_lines)
{
    if (!lines || candidates.empty())
	return true;

    /* Width calculation decodes UTF-8 and applies terminal cell rules.  It is
     * independent of the candidate row assignment, so doing it in every row
     * trial made this routine O(candidate_count * max_lines) in expensive
     * Unicode work. */
    std::vector<size_t> candidate_widths;
    candidate_widths.reserve(candidates.size());
    for (const std::string &candidate : candidates)
	candidate_widths.push_back(completion_cell_width(candidate));

    size_t row_limit = std::min(max_lines, candidates.size());
    auto layout_fits = [&](size_t rows, std::vector<size_t> *widths) {
	size_t columns = (candidates.size() + rows - 1) / rows;
	std::vector<size_t> column_widths(columns, 0);
	for (size_t column = 0; column < columns; column++) {
	    for (size_t row = 0; row < rows; row++) {
		    size_t index = column * rows + row;
		    if (index < candidates.size())
			column_widths[column] = std::max(column_widths[column], candidate_widths[index]);
	    }
	}
	size_t needed = columns > 0 ? completion_column_gap * (columns - 1) : 0;
	for (size_t column_width : column_widths)
	    needed += column_width;
	if (needed > width)
	    return false;
	if (widths)
	    *widths = std::move(column_widths);
	return true;
    };

    /* Exact scans are cheap for normal viewports.  For unusually tall
     * viewports, bound the number of whole-candidate passes: retain a known
     * fitting layout, probe powers of two, and refine the first fitting
     * bracket.  This changes the pathological N-by-N search into O(N log N). */
    size_t best_rows = 0;
    if (row_limit && layout_fits(row_limit, NULL))
	best_rows = row_limit;
    size_t exact_limit = std::min(row_limit, (size_t)64);
    for (size_t rows = 1; rows <= exact_limit; rows++) {
	if (layout_fits(rows, NULL)) {
	    best_rows = rows;
	    break;
	}
    }
    if (!best_rows || best_rows > exact_limit) {
	size_t lower = exact_limit;
	size_t upper = best_rows;
	for (size_t rows = exact_limit ? exact_limit * 2 : 1;
		rows < row_limit; rows = std::min(row_limit, rows * 2)) {
	    if (layout_fits(rows, NULL)) {
		upper = rows;
		best_rows = rows;
		break;
	    }
	    lower = rows;
	    if (rows > row_limit / 2)
		break;
	}
	/* Width feasibility can be non-monotonic for column-major partitions.
	 * Binary probes are therefore an optimization only: best_rows always
	 * remains a layout which was actually checked. */
	for (size_t probes = 0; upper > lower + 1 && probes < 24; probes++) {
	    size_t middle = lower + (upper - lower) / 2;
	    if (layout_fits(middle, NULL)) {
		best_rows = middle;
		upper = middle;
	    } else {
		lower = middle;
	    }
	}
    }
    if (!best_rows)
	return false;

    std::vector<size_t> column_widths;
    if (!layout_fits(best_rows, &column_widths))
	return false;
    size_t rows = best_rows;
    size_t columns = column_widths.size();

	for (size_t row = 0; row < rows; row++) {
	    std::string line;
	    for (size_t column = 0; column < columns; column++) {
		size_t index = column * rows + row;
		if (index >= candidates.size())
		    continue;
		if (!line.empty())
		    line.append(completion_column_gap, ' ');
		line += candidates[index];
		bool have_later = false;
		for (size_t later = column + 1; later < columns; later++) {
		    if (later * rows + row < candidates.size()) {
			have_later = true;
			break;
		    }
		}
		size_t candidate_width = candidate_widths[index];
		if (have_later && candidate_width < column_widths[column])
		    line.append(column_widths[column] - candidate_width, ' ');
	    }
	    if (!line.empty())
		lines->push_back(line);
	}
    return true;
}


static std::string
completion_summary_label(const completion_summary_item &item)
{
    if (!item.node)
	return std::string();
    if (item.terminal)
	return item.node->terminal;

    std::string label = item.node->frontier_prefix.empty() ? "*" :
	item.node->frontier_prefix;
    label += " (" + std::to_string(item.node->count) + ")";
    return label;
}


static size_t
completion_summary_score(const completion_summary_item &item)
{
    if (!item.node)
	return 0;
    if (item.terminal)
	return item.node->terminal.size();
    return item.node->count * item.node->frontier_prefix.size();
}


static bool
completion_summary_pack(std::vector<std::string> *lines,
	const std::vector<completion_summary_item> &items, size_t width,
	size_t max_lines)
{
    std::vector<std::string> labels;
    labels.reserve(items.size());
    for (const completion_summary_item &item : items)
	labels.push_back(completion_summary_label(item));
    return completion_full_layout(lines, labels, width, max_lines);
}


/* Find a useful complete prefix frontier without scaling the exact one-row
 * width DP by the number of display rows.  Build an information-greedy series
 * of prefix frontiers, bounded by the absolute number of one-character labels
 * the display could hold, then find the largest packing frontier.  Keeping
 * packing out of the refinement loop is important for tall consoles: testing
 * every intermediate frontier makes interactive completion noticeably lag as
 * the available line count grows. */
static bool
completion_multi_summary(std::vector<std::string> *lines,
	const completion_trie_node *root, size_t width, size_t max_lines)
{
    if (!lines || !root || max_lines < 2)
	return false;

    std::vector<completion_summary_item> current(1);
    current[0].node = root;
    std::vector<std::vector<completion_summary_item>> frontiers;
    frontiers.push_back(current);

    size_t max_items = max_lines * ((width + completion_column_gap) /
	(completion_column_gap + 1));
    max_items = std::max(max_items, (size_t)2);
    /* Refinement currently keeps successive frontier snapshots for a bounded
     * packing search.  An arbitrarily tall and wide viewport otherwise makes
     * those vector snapshots quadratic in the theoretical number of display
     * cells.  Full candidate layouts above remain unrestricted; this cap only
     * applies after the complete list cannot fit, where 512 distinct summary
     * bins already provide substantially more detail than is useful at once. */
    max_items = std::min(max_items, (size_t)512);
    size_t max_steps = max_items * 2;

    while (frontiers.size() <= max_steps) {
	size_t current_score = 0;
	for (const completion_summary_item &item : current)
	    current_score += completion_summary_score(item);
	completion_summary_split best_split;
	bool have_split = false;
	for (size_t split_index = 0; split_index < current.size(); split_index++) {
	    const completion_summary_item &split_item = current[split_index];
	    if (split_item.terminal || !split_item.node)
		continue;
	    size_t replacement_count = split_item.node->children.size() +
		(split_item.node->terminal.empty() ? 0 : 1);
	    if (replacement_count < 1)
		continue;

	    size_t replacement_score = 0;
	    if (!split_item.node->terminal.empty()) {
		completion_summary_item terminal;
		terminal.node = split_item.node;
		terminal.terminal = true;
		replacement_score += completion_summary_score(terminal);
	    }
	    for (const auto &entry : split_item.node->children) {
		completion_summary_item child;
		child.node = entry.second.get();
		replacement_score += completion_summary_score(child);
	    }
	    completion_summary_split split;
	    split.index = split_index;
	    split.score = current_score - completion_summary_score(split_item) +
		replacement_score;
	    split.item_count = current.size() - 1 + replacement_count;
	    if (split.item_count > max_items)
		continue;
	    if (!have_split || split.score > best_split.score ||
		    (split.score == best_split.score &&
		     (split.item_count > best_split.item_count ||
		      (split.item_count == best_split.item_count &&
		       split.index < best_split.index)))) {
		best_split = split;
		have_split = true;
	    }
	}

	/* Every candidate frontier differs from current by exactly one split.
	 * Select the best viable split during that same scan.  Sorting every
	 * possible split at every refinement level made large, tall completion
	 * displays needlessly quadratic-logarithmic. */
	bool refined = false;
	if (have_split) {
	    size_t split_index = best_split.index;
	    const completion_summary_item &split_item = current[split_index];
	    size_t replacement_count = split_item.node->children.size() +
		(split_item.node->terminal.empty() ? 0 : 1);

	    std::vector<completion_summary_item> candidate;
	    candidate.reserve(current.size() - 1 + replacement_count);
	    candidate.insert(candidate.end(), current.begin(), current.begin() + split_index);
	    if (!split_item.node->terminal.empty()) {
		completion_summary_item terminal;
		terminal.node = split_item.node;
		terminal.terminal = true;
		candidate.push_back(terminal);
	    }
	    for (const auto &entry : split_item.node->children) {
		completion_summary_item child;
		child.node = entry.second.get();
		candidate.push_back(child);
	    }
	    candidate.insert(candidate.end(), current.begin() + split_index + 1, current.end());

	    current = std::move(candidate);
	    frontiers.push_back(current);
	    refined = true;
	}
	if (!refined)
	    break;
    }

    /* Refinement only adds detail and never reduces the item count.  Locate
     * the last fitting frontier with logarithmically many layout attempts.
     * Retest the immediately following few frontiers to tolerate column-major
     * packing discontinuities without returning to an unbounded search. */
    size_t first = 0;
    size_t last = frontiers.size();
    size_t best_index = 0;
    std::vector<std::string> best_lines;
    while (first < last) {
	size_t middle = first + (last - first) / 2;
	std::vector<std::string> candidate_lines;
	if (completion_summary_pack(&candidate_lines, frontiers[middle], width, max_lines)) {
	    best_index = middle;
	    best_lines = std::move(candidate_lines);
	    first = middle + 1;
	} else {
	    last = middle;
	}
    }
    size_t scan_end = std::min(frontiers.size(), best_index + 9);
    for (size_t i = best_index + 1; i < scan_end; i++) {
	std::vector<std::string> candidate_lines;
	if (!completion_summary_pack(&candidate_lines, frontiers[i], width, max_lines))
	    continue;
	best_index = i;
	best_lines = std::move(candidate_lines);
    }

    if (best_index == 0 || frontiers[best_index].size() < 2 || best_lines.empty())
	return false;
    *lines = std::move(best_lines);
    return true;
}


static void
completion_store_layout(struct bu_cmd_completion_layout *layout,
	const std::vector<std::string> &lines, int summarized)
{
    if (!layout || lines.empty())
	return;
    layout->lines = static_cast<char **>(bu_calloc(lines.size(), sizeof(char *),
		"completion layout lines"));
    layout->line_count = lines.size();
    layout->summarized = summarized;
    for (size_t i = 0; i < lines.size(); i++)
	layout->lines[i] = bu_strdup(lines[i].c_str());
}

} // namespace


extern "C" size_t
bu_lineedit_text_width(const char *text, size_t length)
{
    if (!text)
	return 0;
    return completion_cell_width(std::string(text, length));
}


extern "C" size_t
bu_cmd_completion_candidate_budget(size_t width, size_t max_lines)
{
    const size_t hard_limit = 65536;

    if (!width)
	width = 80;
    if (!max_lines)
	max_lines = 5;
    /* A one-cell candidate followed by the shared two-cell gap is the
     * densest layout the renderer can produce. */
    size_t columns = width >= hard_limit * (1 + completion_column_gap) ?
	hard_limit : (width + completion_column_gap) /
	(1 + completion_column_gap);
    if (!columns)
	columns = 1;
    if (max_lines > hard_limit / columns)
	return hard_limit;
    return std::min(hard_limit, std::max((size_t)2, max_lines * columns));
}


extern "C" void
bu_cmd_completion_layout_init(struct bu_cmd_completion_layout *layout)
{
    if (!layout)
	return;
    layout->line_count = 0;
    layout->lines = NULL;
    layout->summarized = 0;
}


extern "C" void
bu_cmd_completion_layout_clear(struct bu_cmd_completion_layout *layout)
{
    if (!layout)
	return;
    if (layout->lines) {
	for (size_t i = 0; i < layout->line_count; i++)
	    bu_free(layout->lines[i], "completion layout line");
	bu_free(layout->lines, "completion layout lines");
    }
    bu_cmd_completion_layout_init(layout);
}


extern "C" int
bu_cmd_completion_layout_create(struct bu_cmd_completion_layout *layout,
	const char * const *candidates, size_t candidate_count,
	size_t width, size_t max_lines)
{
    if (!layout || (candidate_count && !candidates))
	return BRLCAD_ERROR;
    bu_cmd_completion_layout_clear(layout);
    if (!candidate_count)
	return BRLCAD_OK;
    if (!width)
	width = 80;
    if (!max_lines)
	max_lines = 5;

    std::set<std::string> unique;
    for (size_t i = 0; i < candidate_count; i++) {
	if (candidates[i] && candidates[i][0])
	    unique.insert(completion_display_escape(candidates[i]));
    }
    std::vector<std::string> values(unique.begin(), unique.end());
    if (values.empty())
	return BRLCAD_OK;

    std::vector<std::string> lines;
    if (completion_full_layout(&lines, values, width, max_lines)) {
	completion_store_layout(layout, lines, 0);
	return BRLCAD_OK;
    }

    completion_trie_node root;
    for (const std::string &value : values)
	completion_trie_insert(&root, value);
    completion_trie_compress(&root);

    /* Prefer a complete prefix frontier spread over the available rows.  The
     * exact one-line logic below remains the fallback when even a useful broad
     * partition cannot fit. */
    std::vector<std::string> multi_lines;
    if (completion_multi_summary(&multi_lines, &root, width, max_lines)) {
	completion_store_layout(layout, multi_lines, 1);
	return BRLCAD_OK;
    }

    std::vector<completion_frontier> options = completion_frontiers(&root, width);
    completion_frontier best;
    for (const completion_frontier &candidate : options) {
	if (best.labels.empty() || completion_frontier_better(candidate, best))
	    best = candidate;
    }

    /* A broad candidate set may have too many top-level branches for even
     * one bin per branch.  In that case, replace the uninformative root bin
     * with the most informative non-overlapping bins that fit and explicitly
     * account for all candidates not shown. */
    if (best.labels.size() == 1 && best.covered == root.count) {
	std::string reserve = "... (" + std::to_string(root.count) + " more)";
    if (width > completion_cell_width(reserve) + completion_column_gap) {
	    size_t partial_width = width - completion_cell_width(reserve) - completion_column_gap;
	    std::vector<completion_frontier> partial_options =
		completion_frontiers(&root, partial_width, true);
	    completion_frontier partial_best;
	    for (completion_frontier candidate : partial_options) {
		if (!candidate.covered || candidate.covered >= root.count)
		    continue;
		std::string remainder = "... (" +
		    std::to_string(root.count - candidate.covered) + " more)";
		candidate.width += completion_column_gap + completion_cell_width(remainder);
		if (candidate.width > width)
		    continue;
		candidate.labels.push_back(remainder);
		if (partial_best.labels.empty() ||
			completion_frontier_better(candidate, partial_best))
		    partial_best = std::move(candidate);
	    }
	    if (!partial_best.labels.empty())
		best = std::move(partial_best);
	}
    }

    std::string summary;
    for (const std::string &entry : best.labels) {
	if (!summary.empty())
	    summary.append(completion_column_gap, ' ');
	summary += entry;
    }
    if (summary.empty())
	summary = completion_clip(completion_bin_label(&root), width);
    else
	summary = completion_clip(summary, width);
    lines.push_back(summary);
    completion_store_layout(layout, lines, 1);
    return BRLCAD_OK;
}
