/*              A R T _ V O L U M E _ R E F E R E N C E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
#include "common.h"
#include "bu/app.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>

namespace {

constexpr double OuterRadius = 10;
constexpr double InnerRadius = 5;
constexpr double NegligibleWeight = 1e-12;
constexpr std::uint64_t DefaultSamples = 2000000;
constexpr std::uint64_t ReferenceSeed = 701283;
constexpr size_t MaximumEvents = 10000;
// Ignore roundoff-sized reentries after crossing the inner sphere.
constexpr double BoundaryTolerance = 64 * std::numeric_limits<double>::epsilon() * OuterRadius;
using Channels = std::array<double, 3>;
constexpr Channels OuterAbsorption = {0.04, 0.03, 0.12};
constexpr Channels OuterScattering = {0.16, 0.07, 0.18};
constexpr Channels InnerAbsorption = {0.02, 0.015, 0.06};
constexpr Channels InnerScattering = {0.08, 0.035, 0.09};

struct Vector {
    double x, y, z;
};

Vector operator+(const Vector a, const Vector b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vector operator*(const double a, const Vector b) { return {a * b.x, a * b.y, a * b.z}; }
double dot(const Vector a, const Vector b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vector cross(const Vector a, const Vector b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

Vector normalize(const Vector a)
{
    const double squared_length = dot(a, a);
    if (!std::isfinite(squared_length) || squared_length <= 0)
	throw std::runtime_error("Invalid reference direction");
    return (1 / std::sqrt(squared_length)) * a;
}

struct Medium {
    double absorption, scattering, phase;
};

struct Profile {
    bool overlap;
    double outer_phase, inner_phase;
};

struct Estimate {
    double mean = 0;
    double squared_deviations = 0;
    std::uint64_t samples = 0;
    std::uint64_t collisions = 0;
    std::uint64_t inner_crossings = 0;

    void add(const double value)
    {
	++samples;
	const double delta = value - mean;
	mean += delta / static_cast<double>(samples);
	squared_deviations += delta * (value - mean);
    }

    double standard_error() const
    {
	return std::sqrt(std::max(0.0, squared_deviations) /
	    (static_cast<double>(samples) * static_cast<double>(samples - 1)));
    }
};

class RandomWalk {
public:
    explicit RandomWalk(const std::uint64_t seed) : engine(seed) {}

    Estimate trace(const std::uint64_t samples, const bool inside,
	const bool overlap, const Medium outer, const Medium inner)
    {
	Estimate result;
	for (std::uint64_t sample = 0; sample < samples; ++sample) {
	    Vector point = {0, 0, inside ? 0 : OuterRadius};
	    Vector direction = {0, 0, -1};
	    bool active_inner = inside && overlap;
	    double weight = 1;
	    size_t events = 0;
	    for (; events < MaximumEvents && weight > NegligibleWeight; ++events) {
		double boundary = exit_distance(point, direction, active_inner ? InnerRadius : OuterRadius);
		bool crosses_inner = active_inner;
		if (overlap && !active_inner) {
		    const double projection = dot(point, direction);
		    const double discriminant = projection * projection + InnerRadius * InnerRadius - dot(point, point);
		    if (discriminant >= 0) {
			const double entry = -projection - std::sqrt(discriminant);
			if (entry > BoundaryTolerance && entry < boundary) {
			    boundary = entry;
			    crosses_inner = true;
			}
		    }
		}
		const Medium medium = active_inner ? inner : outer;
		const double extinction = medium.absorption + medium.scattering;
		const double free_path = -std::log1p(-uniform()) / extinction;
		if (free_path >= boundary) {
		    if (!crosses_inner)
			break;
		    point = point + boundary * direction;
		    active_inner = !active_inner;
		    ++result.inner_crossings;
		    continue;
		}
		point = point + free_path * direction;
		// Survival weighting integrates absorption without a separate
		// sampled termination decision.  Each channel is independent.
		weight *= medium.scattering / extinction;
		++result.collisions;
		if (weight > NegligibleWeight)
		    direction = scatter(direction, medium.phase);
	    }
	    if (events == MaximumEvents)
		throw std::runtime_error("Reference path exceeded its event limit");
	    // The boundary environment has unit radiance.  Discarding a
	    // smaller surviving weight bounds the absolute bias by 1e-12.
	    result.add(weight > NegligibleWeight ? weight : 0);
	}
	return result;
    }

private:
    std::mt19937_64 engine;

    double uniform() { return std::generate_canonical<double, std::numeric_limits<double>::digits>(engine); }

    static double exit_distance(const Vector point, const Vector direction, const double radius)
    {
	const double projection = dot(point, direction);
	return std::max(0.0, -projection + std::sqrt(std::max(0.0,
	    projection * projection + radius * radius - dot(point, point))));
    }

    Vector scatter(const Vector direction, const double phase)
    {
	const double u = uniform();
	double cosine = 2 * u - 1;
	if (std::fpclassify(phase) != FP_ZERO) {
	    const double ratio = (1 - phase * phase) / (1 - phase + 2 * phase * u);
	    cosine = std::clamp((1 + phase * phase - ratio * ratio) / (2 * phase), -1.0, 1.0);
	}
	const double sine = std::sqrt(std::max(0.0, 1 - cosine * cosine));
	const double azimuth = 2 * std::acos(-1.0) * uniform();
	const Vector seed = std::abs(direction.z) < std::abs(direction.y) ? Vector{0, 0, 1} : Vector{0, 1, 0};
	const Vector tangent = normalize(cross(seed, direction));
	const Vector bitangent = cross(direction, tangent);
	return normalize(cosine * direction + (sine * std::cos(azimuth)) * tangent + (sine * std::sin(azimuth)) * bitangent);
    }
};

void check_reference_controls(const std::uint64_t samples)
{
    constexpr double SigmaBound = 5;
    constexpr double RoundoffBound = 1e-10;
    for (const bool inside : {false, true}) {
	for (const double phase : {0.0, 0.4, -0.4}) {
	    RandomWalk absorption(ReferenceSeed);
	    const auto beer = absorption.trace(samples, inside, true, {0.2, 0, phase}, {0.1, 0, phase});
	    const double expected = std::exp(inside ? -1.5 : -3);
	    if (std::abs(beer.mean - expected) > SigmaBound * beer.standard_error() + RoundoffBound)
		throw std::runtime_error("Reference failed analytic Beer control");
	    RandomWalk scattering(ReferenceSeed);
	    const auto conserved = scattering.trace(samples, inside, true, {0, 0.2, phase}, {0, 0.1, -phase});
	    if (std::abs(conserved.mean - 1) > RoundoffBound)
		throw std::runtime_error("Reference failed conservative-field control");
	    RandomWalk sphere(ReferenceSeed), split_sphere(ReferenceSeed);
	    const Medium medium{OuterAbsorption[0], OuterScattering[0], phase};
	    const auto plain = sphere.trace(samples, inside, false, medium, medium);
	    const auto split = split_sphere.trace(samples, inside, true, medium, medium);
	    const double uncertainty = std::hypot(plain.standard_error(), split.standard_error());
	    if (std::abs(plain.mean - split.mean) > SigmaBound * uncertainty + RoundoffBound)
		throw std::runtime_error("Reference failed homogeneous-medium recovery");
	}
    }
}

void write_channels(const Channels &values)
{
    std::cout << '[' << values[0] << ',' << values[1] << ',' << values[2] << ']';
}

void write_estimates(const std::array<Estimate, 3> &estimates)
{
    std::cout << "{\"mean\":";
    write_channels({estimates[0].mean, estimates[1].mean, estimates[2].mean});
    std::cout << ",\"standard_error\":";
    write_channels({estimates[0].standard_error(), estimates[1].standard_error(), estimates[2].standard_error()});
    std::cout << ",\"mean_collisions\":";
    write_channels({static_cast<double>(estimates[0].collisions) / estimates[0].samples,
	static_cast<double>(estimates[1].collisions) / estimates[1].samples,
	static_cast<double>(estimates[2].collisions) / estimates[2].samples});
    std::cout << ",\"mean_inner_crossings\":";
    write_channels({static_cast<double>(estimates[0].inner_crossings) / estimates[0].samples,
	static_cast<double>(estimates[1].inner_crossings) / estimates[1].samples,
	static_cast<double>(estimates[2].inner_crossings) / estimates[2].samples});
    std::cout << '}';
}

} // namespace

int main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    try {
	if (argc > 2)
	    throw std::runtime_error("Usage: art_volume_reference [samples]");
	std::uint64_t samples = DefaultSamples;
	if (argc == 2) {
	    const std::string argument(argv[1]);
	    if (argument.empty() || argument.find_first_not_of("0123456789") != std::string::npos)
		throw std::runtime_error("Sample count must be a positive integer");
	    samples = std::stoull(argument);
	}
	if (samples < 2)
	    throw std::runtime_error("At least two reference samples are required");
	check_reference_controls(samples);
	std::cout << std::setprecision(17) << "{\"samples\":" << samples << ",\"seed\":" << ReferenceSeed
	    << ",\"controls_pass\":true,\"outer_radius\":" << OuterRadius << ",\"inner_radius\":" << InnerRadius
	    << ",\"outer_absorption\":";
	write_channels(OuterAbsorption);
	std::cout << ",\"outer_scattering\":";
	write_channels(OuterScattering);
	std::cout << ",\"inner_absorption\":";
	write_channels(InnerAbsorption);
	std::cout << ",\"inner_scattering\":";
	write_channels(InnerScattering);
	std::cout << ",\"profiles\":[";
	const Profile profiles[] = {{false, 0, 0}, {false, 0.4, 0}, {false, -0.4, 0},
	    {true, 0, 0}, {true, 0.4, 0.4}, {true, -0.4, -0.4}, {true, 0.4, -0.4}, {true, -0.4, 0.4}};
	bool first = true;
	for (const auto &profile : profiles) {
	    if (!first)
		std::cout << ',';
	    first = false;
	    std::cout << "{\"overlap\":" << (profile.overlap ? "true" : "false")
		<< ",\"outer_phase\":" << profile.outer_phase << ",\"inner_phase\":" << profile.inner_phase;
	    for (const bool inside : {false, true}) {
		std::array<Estimate, 3> estimates;
		for (size_t channel = 0; channel < estimates.size(); ++channel) {
		    RandomWalk walk(ReferenceSeed);
		    estimates[channel] = walk.trace(samples, inside, profile.overlap,
			{OuterAbsorption[channel], OuterScattering[channel], profile.outer_phase},
			{InnerAbsorption[channel], InnerScattering[channel], profile.inner_phase});
		}
		std::cout << (inside ? ",\"inside\":" : ",\"outside\":");
		write_estimates(estimates);
	    }
	    std::cout << '}';
	}
	std::cout << "]}\n";
	return 0;
    } catch (const std::exception &error) {
	std::cerr << error.what() << '\n';
	return 1;
    }
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
