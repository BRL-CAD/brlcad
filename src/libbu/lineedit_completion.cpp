/*             L I N E E D I T _ C O M P L E T I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the U.S.
 * Army Research Laboratory.
 */

#include "common.h"

#include <algorithm>
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
    std::map<unsigned char, std::unique_ptr<completion_trie_node>> children;
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


static std::string
completion_clip(const std::string &input, size_t width)
{
    if (input.size() <= width)
	return input;
    if (width <= 3)
	return input.substr(0, width);
    return input.substr(0, width - 3) + "...";
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
    for (size_t i = 0; i < candidate.size(); i++) {
	unsigned char key = static_cast<unsigned char>(candidate[i]);
	auto &child = node->children[key];
	if (!child) {
	    child.reset(new completion_trie_node());
	    child->prefix = candidate.substr(0, i + 1);
	    child->frontier_prefix = child->prefix;
	}
	node = child.get();
	node->count++;
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
    if (!label.empty() && label.size() <= width_limit) {
	completion_frontier unsplit;
	unsplit.width = label.size();
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
	terminal.width = node->terminal.size();
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

    size_t row_limit = std::min(max_lines, candidates.size());
    for (size_t rows = 1; rows <= row_limit; rows++) {
	size_t columns = (candidates.size() + rows - 1) / rows;
	std::vector<size_t> column_widths(columns, 0);
	for (size_t column = 0; column < columns; column++) {
	    for (size_t row = 0; row < rows; row++) {
		size_t index = column * rows + row;
		if (index < candidates.size())
		    column_widths[column] = std::max(column_widths[column], candidates[index].size());
	    }
	}
	size_t needed = columns > 0 ? completion_column_gap * (columns - 1) : 0;
	for (size_t column_width : column_widths)
	    needed += column_width;
	if (needed > width)
	    continue;

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
		if (have_later && candidates[index].size() < column_widths[column])
		    line.append(column_widths[column] - candidates[index].size(), ' ');
	    }
	    if (!line.empty())
		lines->push_back(line);
	}
	return true;
    }
    return false;
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
 * width DP by the number of display rows.  Starting from the root, repeatedly
 * choose the information-maximizing split whose full replacement frontier
 * still packs in the requested rows.  The visible frontier stays small, so
 * this remains interactive even for databases with thousands of objects. */
static bool
completion_multi_summary(std::vector<std::string> *lines,
	const completion_trie_node *root, size_t width, size_t max_lines)
{
    if (!lines || !root || max_lines < 2)
	return false;

    std::vector<completion_summary_item> current(1);
    current[0].node = root;
    std::vector<std::string> current_lines;

    while (true) {
	std::vector<completion_summary_item> best_items;
	std::vector<std::string> best_lines;
	size_t best_score = 0;
	for (size_t split_index = 0; split_index < current.size(); split_index++) {
	    const completion_summary_item &split_item = current[split_index];
	    if (split_item.terminal || !split_item.node)
		continue;
	    size_t replacement_count = split_item.node->children.size() +
		(split_item.node->terminal.empty() ? 0 : 1);
	    if (replacement_count < 1)
		continue;

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

	    std::vector<std::string> candidate_lines;
	    if (!completion_summary_pack(&candidate_lines, candidate, width, max_lines))
		continue;
	    size_t score = 0;
	    for (const completion_summary_item &item : candidate)
		score += completion_summary_score(item);
	    if (best_items.empty() || score > best_score ||
		    (score == best_score && candidate.size() > best_items.size())) {
		best_score = score;
		best_items = std::move(candidate);
		best_lines = std::move(candidate_lines);
	    }
	}
	if (best_items.empty())
	    break;
	current = std::move(best_items);
	current_lines = std::move(best_lines);
    }

    if (current.size() < 2 || current_lines.empty())
	return false;
    *lines = std::move(current_lines);
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
    layout->line_count = 0;
    layout->lines = NULL;
    layout->summarized = 0;
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
	    unique.insert(std::string(candidates[i]));
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
	if (width > reserve.size() + completion_column_gap) {
	    size_t partial_width = width - reserve.size() - completion_column_gap;
	    std::vector<completion_frontier> partial_options =
		completion_frontiers(&root, partial_width, true);
	    completion_frontier partial_best;
	    for (completion_frontier candidate : partial_options) {
		if (!candidate.covered || candidate.covered >= root.count)
		    continue;
		std::string remainder = "... (" +
		    std::to_string(root.count - candidate.covered) + " more)";
		candidate.width += completion_column_gap + remainder.size();
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
