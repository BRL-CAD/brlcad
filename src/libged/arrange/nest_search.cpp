/*                    N E S T _ S E A R C H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <numeric>
#include <set>
#include <tuple>
#include <vector>

#include "bu/parallel.h"

#include "arrange_private.h"

namespace arrange {

namespace {

std::array<int, 3>
coordinates(const Cell &cell)
{
    return {{cell.x, cell.y, cell.z}};
}

std::size_t
candidate_budget(Quality quality)
{
    switch (quality) {
        case Quality::FAST:
            return FAST_CANDIDATE_BUDGET;
        case Quality::BALANCED:
            return BALANCED_CANDIDATE_BUDGET;
        case Quality::THOROUGH:
            return THOROUGH_CANDIDATE_BUDGET;
    }
    return BALANCED_CANDIDATE_BUDGET;
}

bool
intersects_shifted_rows(const Raster &target, const Raster &object,
        const Cell &position)
{
    int destination_word_offset = position.x / 64;
    int bit_offset = position.x & 63;
    for (int z = 0; z < object.dims[Z]; ++z) {
        for (int y = 0; y < object.dims[Y]; ++y) {
            std::size_t object_row =
                (static_cast<std::size_t>(z) * object.dims[Y] + y) *
                object.row_words;
            std::size_t target_row =
                (static_cast<std::size_t>(z + position.z) * target.dims[Y] +
                y + position.y) * target.row_words;
            for (int word_index = 0; word_index < object.row_words; ++word_index) {
                std::uint64_t word = object.bits[object_row + word_index];
                if (!word)
                    continue;
                int destination_word = destination_word_offset + word_index;
                if ((word << bit_offset) & target.bits[target_row + destination_word])
                    return true;
                if (bit_offset && destination_word + 1 < target.row_words &&
                        (word >> (64 - bit_offset)) &
                        target.bits[target_row + destination_word + 1])
                    return true;
            }
        }
    }
    return false;
}

bool
contained_in_shifted_rows(const Raster &target, const Raster &object,
        const Cell &position)
{
    int destination_word_offset = position.x / 64;
    int bit_offset = position.x & 63;
    for (int z = 0; z < object.dims[Z]; ++z) {
        for (int y = 0; y < object.dims[Y]; ++y) {
            std::size_t object_row =
                (static_cast<std::size_t>(z) * object.dims[Y] + y) *
                object.row_words;
            std::size_t target_row =
                (static_cast<std::size_t>(z + position.z) * target.dims[Y] +
                y + position.y) * target.row_words;
            for (int word_index = 0; word_index < object.row_words; ++word_index) {
                std::uint64_t word = object.bits[object_row + word_index];
                if (!word)
                    continue;
                int destination_word = destination_word_offset + word_index;
                if ((word << bit_offset) & ~target.bits[target_row + destination_word])
                    return false;
                if (bit_offset) {
                    std::uint64_t high = word >> (64 - bit_offset);
                    if (high && (high &
                            ~target.bits[target_row + destination_word + 1]))
                        return false;
                }
            }
        }
    }
    return true;
}

std::size_t
envelope_size(const SearchState &state, const OrientedRaster &orientation,
        const Cell &position, Dimension dimension, int projection_axis)
{
    std::array<int, 3> minimum = state.envelope_min;
    std::array<int, 3> maximum = state.envelope_max;
    std::array<int, 3> base = coordinates(position);
    for (int axis = 0; axis < 3; ++axis) {
        int object_minimum = base[axis] + orientation.pad[axis];
        int object_maximum = object_minimum + orientation.shape.dims[axis] - 1;
        if (maximum[axis] < minimum[axis]) {
            minimum[axis] = object_minimum;
            maximum[axis] = object_maximum;
        } else {
            minimum[axis] = std::min(minimum[axis], object_minimum);
            maximum[axis] = std::max(maximum[axis], object_maximum);
        }
    }

    std::size_t size = 1;
    for (int axis = 0; axis < 3; ++axis) {
        if (dimension == Dimension::PLANAR && axis == projection_axis)
            continue;
        size *= static_cast<std::size_t>(maximum[axis] - minimum[axis] + 1);
    }
    return size;
}

int
contact_faces(const Raster &allowed, const SearchState &state,
        const OrientedRaster &orientation, const Cell &position,
        Dimension dimension, int projection_axis)
{
    const int directions[6][3] = {
        {-1, 0, 0}, {1, 0, 0}, {0, -1, 0},
        {0, 1, 0}, {0, 0, -1}, {0, 0, 1}
    };
    int contacts = 0;
    for (const Cell &cell : orientation.shape.cells) {
        int x = position.x + orientation.pad[X] + cell.x;
        int y = position.y + orientation.pad[Y] + cell.y;
        int z = position.z + orientation.pad[Z] + cell.z;
        for (int direction = 0; direction < 6; ++direction) {
            int axis = direction / 2;
            if (dimension == Dimension::PLANAR && axis == projection_axis)
                continue;
            int local_x = cell.x + directions[direction][X];
            int local_y = cell.y + directions[direction][Y];
            int local_z = cell.z + directions[direction][Z];
            if (orientation.shape.get(local_x, local_y, local_z))
                continue;
            int neighbor_x = x + directions[direction][X];
            int neighbor_y = y + directions[direction][Y];
            int neighbor_z = z + directions[direction][Z];
            if (state.occupied.get(neighbor_x, neighbor_y, neighbor_z) ||
                    !allowed.get(neighbor_x, neighbor_y, neighbor_z))
                ++contacts;
        }
    }
    return contacts;
}

bool
better_placement(const Placement &left, const Placement &right)
{
    if (left.contact_faces != right.contact_faces)
        return left.contact_faces > right.contact_faces;
    if (left.envelope != right.envelope)
        return left.envelope < right.envelope;
    if (left.orientation_index != right.orientation_index)
        return left.orientation_index < right.orientation_index;
    return std::tie(left.position.z, left.position.y, left.position.x) <
        std::tie(right.position.z, right.position.y, right.position.x);
}

Cell
settle_candidate(const Raster &allowed, const SearchState &state,
        const OrientedRaster &orientation, const Cell &initial,
        const NestOptions &options)
{
    if (!options.gravity_enabled)
        return initial;

    double magnitude = std::max({std::fabs(options.gravity[X]),
        std::fabs(options.gravity[Y]), std::fabs(options.gravity[Z])});
    if (ZERO(magnitude))
        return initial;

    Cell settled = initial;
    std::array<int, 3> previous_delta = {{0, 0, 0}};
    int maximum_steps = allowed.dims[X] + allowed.dims[Y] + allowed.dims[Z];
    for (int step = 1; step <= maximum_steps; ++step) {
        std::array<int, 3> delta;
        for (int axis = 0; axis < 3; ++axis) {
            delta[axis] = static_cast<int>(std::lround(
                options.gravity[axis] * step / magnitude));
            if (options.dimension == Dimension::PLANAR && axis == options.axis)
                delta[axis] = 0;
        }
        if (delta == previous_delta)
            continue;
        previous_delta = delta;
        Cell candidate = {initial.x + delta[X], initial.y + delta[Y],
            initial.z + delta[Z]};
        if (!placement_feasible(allowed, state, orientation, candidate))
            break;
        settled = candidate;
    }
    return settled;
}

std::vector<std::size_t>
default_order(const std::vector<Item> &items)
{
    std::vector<std::size_t> order(items.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](std::size_t left,
            std::size_t right) {
        double left_density = items[left].value /
            items[left].geometry->source.count();
        double right_density = items[right].value /
            items[right].geometry->source.count();
        if (!NEAR_EQUAL(left_density, right_density, SMALL_FASTF))
            return left_density > right_density;
        if (items[left].geometry->source.count() !=
                items[right].geometry->source.count())
            return items[left].geometry->source.count() >
                items[right].geometry->source.count();
        return items[left].source_index < items[right].source_index;
    });
    return order;
}

SearchState
empty_state(const Raster &allowed)
{
    SearchState state;
    state.occupied = Raster(allowed.dims);
    state.occupied.model_min = allowed.model_min;
    state.occupied.model_max = allowed.model_max;
    return state;
}

SearchState
greedy_order(const Raster &allowed, const std::vector<Item> &items,
        const std::vector<std::size_t> &order, const NestOptions &options,
        SearchState state)
{
    for (std::size_t item_index : order) {
        std::vector<Placement> candidates = rank_placements(allowed, state,
            items[item_index], item_index, options, 1);
        if (candidates.empty())
            continue;
        apply_placement(state, items[item_index], candidates.front());
    }
    return state;
}

SearchState
beam_order(const Raster &allowed, const std::vector<Item> &items,
        const std::vector<std::size_t> &order, const NestOptions &options,
        std::size_t width)
{
    std::vector<SearchState> beam = {empty_state(allowed)};
    for (std::size_t item_index : order) {
        std::vector<SearchState> next;
        for (const SearchState &state : beam) {
            next.push_back(state);
            std::vector<Placement> candidates = rank_placements(allowed, state,
                items[item_index], item_index, options, 2);
            for (const Placement &candidate : candidates) {
                SearchState placed = state;
                apply_placement(placed, items[item_index], candidate);
                next.push_back(std::move(placed));
            }
        }
        std::stable_sort(next.begin(), next.end(), [](const SearchState &left,
                const SearchState &right) {
            return better_state(left, right);
        });
        if (next.size() > width)
            next.resize(width);
        beam = std::move(next);
    }
    return beam.front();
}

SearchState
rebuild_without(const Raster &allowed, const std::vector<Item> &items,
        const SearchState &source, const std::set<std::size_t> &removed)
{
    SearchState rebuilt = empty_state(allowed);
    for (const Placement &placement : source.placements) {
        if (!removed.count(placement.item_index))
            apply_placement(rebuilt, items[placement.item_index], placement);
    }
    return rebuilt;
}

std::vector<std::size_t>
available_items(const std::vector<Item> &items, const SearchState &state)
{
    std::vector<bool> placed(items.size(), false);
    for (const Placement &placement : state.placements)
        placed[placement.item_index] = true;
    std::vector<std::size_t> result;
    for (std::size_t i = 0; i < items.size(); ++i)
        if (!placed[i])
            result.push_back(i);
    return result;
}

std::vector<Placement>
rank_orientation(const Raster &allowed, const SearchState &state,
        const Item &item, std::size_t item_index, const NestOptions &options,
        std::size_t orientation_index, std::size_t maximum_results)
{
    const OrientedRaster &orientation =
        item.geometry->orientations[orientation_index];
    CandidateSet generated = generate_candidates(allowed, state, orientation,
        options.dimension, options.axis, candidate_budget(options.quality));
    std::vector<Placement> ranked;
    std::set<std::tuple<int, int, int>> evaluated;

    auto evaluate = [&](const Cell &position) {
        if (!placement_feasible(allowed, state, orientation, position))
            return;
        Cell settled = settle_candidate(allowed, state, orientation, position,
            options);
        auto key = std::make_tuple(settled.x, settled.y, settled.z);
        if (!evaluated.insert(key).second)
            return;
        Placement placement;
        placement.item_index = item_index;
        placement.orientation_index = orientation_index;
        placement.position = settled;
        placement.contact_faces = contact_faces(allowed, state, orientation,
            settled, options.dimension, options.axis);
        placement.envelope = envelope_size(state, orientation, settled,
            options.dimension, options.axis);
        ranked.push_back(placement);
    };

    for (const Cell &position : generated.positions)
        evaluate(position);
    std::stable_sort(ranked.begin(), ranked.end(), better_placement);

    if (generated.stride > 1 && !ranked.empty()) {
        std::size_t seeds = std::min<std::size_t>(12, ranked.size());
        int radius = std::min(3, generated.stride - 1);
        for (std::size_t seed = 0; seed < seeds; ++seed) {
            Cell base = ranked[seed].position;
            for (int dz = -radius; dz <= radius; ++dz) {
                if (options.dimension == Dimension::PLANAR && options.axis == Z && dz)
                    continue;
                for (int dy = -radius; dy <= radius; ++dy) {
                    if (options.dimension == Dimension::PLANAR && options.axis == Y && dy)
                        continue;
                    for (int dx = -radius; dx <= radius; ++dx) {
                        if (options.dimension == Dimension::PLANAR && options.axis == X && dx)
                            continue;
                        evaluate({base.x + dx, base.y + dy, base.z + dz});
                    }
                }
            }
        }
        std::stable_sort(ranked.begin(), ranked.end(), better_placement);
    }
    if (ranked.size() > maximum_results)
        ranked.resize(maximum_results);
    return ranked;
}

struct OrientationWork {
    const Raster *allowed = nullptr;
    const SearchState *state = nullptr;
    const Item *item = nullptr;
    const NestOptions *options = nullptr;
    std::size_t item_index = 0;
    std::size_t maximum_results = 0;
    std::size_t worker_count = 0;
    std::atomic<std::size_t> next_orientation{0};
    std::vector<std::vector<Placement>> results;
};

void
rank_orientation_worker(int UNUSED(cpu), void *data)
{
    auto *work = static_cast<OrientationWork *>(data);
    while (true) {
        std::size_t orientation_index = work->next_orientation.fetch_add(1,
            std::memory_order_relaxed);
        if (orientation_index >= work->item->geometry->orientations.size())
            break;
        work->results[orientation_index] = rank_orientation(*work->allowed,
            *work->state, *work->item, work->item_index, *work->options,
            orientation_index, work->maximum_results);
    }
}

} // namespace

bool
placement_feasible(const Raster &allowed, const SearchState &state,
        const OrientedRaster &orientation, const Cell &position)
{
    if (position.x < 0 || position.y < 0 || position.z < 0 ||
            position.x + orientation.collision.dims[X] > allowed.dims[X] ||
            position.y + orientation.collision.dims[Y] > allowed.dims[Y] ||
            position.z + orientation.collision.dims[Z] > allowed.dims[Z])
        return false;
    if (!contained_in_shifted_rows(allowed, orientation.collision, position))
        return false;
    return !intersects_shifted_rows(state.occupied, orientation.collision,
        position);
}

std::vector<Placement>
rank_placements(const Raster &allowed, const SearchState &state,
        const Item &item, std::size_t item_index, const NestOptions &options,
        std::size_t maximum_results)
{
    std::vector<Placement> ranked;
    OrientationWork work;
    work.allowed = &allowed;
    work.state = &state;
    work.item = &item;
    work.options = &options;
    work.item_index = item_index;
    work.maximum_results = maximum_results;
    work.worker_count = std::min<std::size_t>(item.geometry->orientations.size(),
        std::max<std::size_t>(1, bu_avail_cpus()));
    work.results.resize(item.geometry->orientations.size());

    if (work.worker_count > 1)
        bu_parallel(rank_orientation_worker, work.worker_count, &work);
    else
        rank_orientation_worker(0, &work);

    for (std::vector<Placement> &orientation_results : work.results)
        ranked.insert(ranked.end(), orientation_results.begin(),
            orientation_results.end());

    std::stable_sort(ranked.begin(), ranked.end(), better_placement);
    if (ranked.size() > maximum_results)
        ranked.resize(maximum_results);
    return ranked;
}

void
apply_placement(SearchState &state, const Item &item,
        const Placement &placement)
{
    const OrientedRaster &orientation =
        item.geometry->orientations[placement.orientation_index];
    for (const Cell &cell : orientation.shape.cells) {
        state.occupied.set(placement.position.x + orientation.pad[X] + cell.x,
            placement.position.y + orientation.pad[Y] + cell.y,
            placement.position.z + orientation.pad[Z] + cell.z);
    }
    state.value += item.value;
    state.occupied_cells += orientation.shape.count();
    state.contact_faces += placement.contact_faces;

    std::array<int, 3> position = coordinates(placement.position);
    for (int axis = 0; axis < 3; ++axis) {
        int minimum = position[axis] + orientation.pad[axis];
        int maximum = minimum + orientation.shape.dims[axis] - 1;
        if (state.envelope_max[axis] < state.envelope_min[axis]) {
            state.envelope_min[axis] = minimum;
            state.envelope_max[axis] = maximum;
        } else {
            state.envelope_min[axis] = std::min(state.envelope_min[axis], minimum);
            state.envelope_max[axis] = std::max(state.envelope_max[axis], maximum);
        }
    }
    state.placements.push_back(placement);
}

bool
better_state(const SearchState &left, const SearchState &right)
{
    if (!NEAR_EQUAL(left.value, right.value, SMALL_FASTF))
        return left.value > right.value;
    if (left.occupied_cells != right.occupied_cells)
        return left.occupied_cells > right.occupied_cells;
    if (left.contact_faces != right.contact_faces)
        return left.contact_faces > right.contact_faces;

    auto envelope = [](const SearchState &state) {
        std::size_t volume = 1;
        for (int axis = 0; axis < 3; ++axis) {
            if (state.envelope_max[axis] < state.envelope_min[axis])
                return std::size_t(0);
            volume *= static_cast<std::size_t>(state.envelope_max[axis] -
                state.envelope_min[axis] + 1);
        }
        return volume;
    };
    std::size_t left_envelope = envelope(left);
    std::size_t right_envelope = envelope(right);
    if (left_envelope != right_envelope)
        return left_envelope < right_envelope;
    if (left.placements.size() != right.placements.size())
        return left.placements.size() > right.placements.size();

    for (std::size_t i = 0; i < std::min(left.placements.size(),
            right.placements.size()); ++i) {
        const Placement &lp = left.placements[i];
        const Placement &rp = right.placements[i];
        auto left_key = std::make_tuple(lp.item_index, lp.orientation_index,
            lp.position.z, lp.position.y, lp.position.x);
        auto right_key = std::make_tuple(rp.item_index, rp.orientation_index,
            rp.position.z, rp.position.y, rp.position.x);
        if (left_key != right_key)
            return left_key < right_key;
    }
    return false;
}

SearchState
search_arrangement(const Raster &allowed, const std::vector<Item> &items,
        const NestOptions &options)
{
    std::vector<std::size_t> order = default_order(items);
    if (options.quality == Quality::FAST)
        return greedy_order(allowed, items, order, options, empty_state(allowed));

    std::size_t width = options.quality == Quality::BALANCED ?
        BALANCED_BEAM_WIDTH : THOROUGH_BEAM_WIDTH;
    SearchState best = beam_order(allowed, items, order, options, width);
    if (options.quality == Quality::BALANCED)
        return best;

    for (std::size_t restart = 1; restart < THOROUGH_RESTARTS; ++restart) {
        std::vector<std::size_t> alternate = order;
        std::rotate(alternate.begin(), alternate.begin() +
            (restart % alternate.size()), alternate.end());
        SearchState candidate = beam_order(allowed, items, alternate, options, width);
        if (better_state(candidate, best))
            best = std::move(candidate);
    }

    for (std::size_t step = 0; step < THOROUGH_LNS_STEPS &&
            !best.placements.empty(); ++step) {
        std::size_t pivot_index = step % best.placements.size();
        const Placement &pivot = best.placements[pivot_index];
        std::set<std::size_t> removed = {pivot.item_index};

        struct Neighbor {
            std::size_t item_index = 0;
            int distance = 0;
        };
        std::vector<Neighbor> neighbors;
        for (const Placement &placement : best.placements) {
            if (placement.item_index == pivot.item_index)
                continue;
            int distance = std::abs(placement.position.x - pivot.position.x) +
                std::abs(placement.position.y - pivot.position.y) +
                std::abs(placement.position.z - pivot.position.z);
            neighbors.push_back({placement.item_index, distance});
        }
        std::stable_sort(neighbors.begin(), neighbors.end(), [](const Neighbor &left,
                const Neighbor &right) {
            if (left.distance != right.distance)
                return left.distance < right.distance;
            return left.item_index < right.item_index;
        });
        for (std::size_t i = 0; i < std::min<std::size_t>(2, neighbors.size()); ++i)
            removed.insert(neighbors[i].item_index);

        SearchState candidate = rebuild_without(allowed, items, best, removed);
        std::vector<std::size_t> refill = available_items(items, candidate);
        if (!refill.empty()) {
            std::rotate(refill.begin(), refill.begin() + (step % refill.size()),
                refill.end());
            candidate = greedy_order(allowed, items, refill, options,
                std::move(candidate));
        }
        if (better_state(candidate, best))
            best = std::move(candidate);
    }
    return best;
}

} // namespace arrange
