#ifndef BREP_EXCEPT_H
#define BREP_EXCEPT_H

#include "common.h"

#include <stdexcept>

class InvalidBooleanOperation : public std::invalid_argument
{
public:
    InvalidBooleanOperation(const std::string &msg = "") : std::invalid_argument(msg) {}
};

class InvalidGeometry : public std::invalid_argument
{
public:
    InvalidGeometry(const std::string &msg = "") : std::invalid_argument(msg) {}
};

class AlgorithmError : public std::runtime_error
{
public:
    AlgorithmError(const std::string &msg = "") : std::runtime_error(msg) {}
};

class GeometryGenerationError : public std::runtime_error
{
public:
    GeometryGenerationError(const std::string &msg = "") : std::runtime_error(msg) {}
};

class IntervalGenerationError : public std::runtime_error
{
public:
    IntervalGenerationError(const std::string &msg = "") : std::runtime_error(msg) {}
};

class InvalidInterval : public std::runtime_error
{
public:
    InvalidInterval(const std::string &msg = "") : std::runtime_error(msg) {}
};

#endif
