#!/bin/sh

# use after executing:
#   update-boost-tree.pl boost_1_55_0

makedir -p boost/smart_ptr/detail
cp boost_1_55_0/boost/smart_ptr/detail/sp_nullptr_t.hpp boost/smart_ptr/detail
makedir -p d boost/preprocessor/facilities
cp boost_1_55_0/boost/preprocessor/facilities/overload.hpp boost/preprocessor/facilities

