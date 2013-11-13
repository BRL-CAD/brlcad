#!/bin/sh

# use after executing:
#   update-boost-tree.pl boost_1_55_0

FDIR=boost_1_55_0

BDIR=boost/preprocessor/facilities
mkdir -p $BDIR
cp $FDIR/$BDIR/overload.hpp $BDIR

BDIR=boost/preprocessor/variadic
mkdir -p $BDIR
cp $FDIR/$BDIR/size.hpp $BDIR
cp $FDIR/$BDIR/elem.hpp $BDIR

BDIR=boost/type_traits
mkdir -p $BDIR
cp $FDIR/$BDIR/has_operator.hpp $BDIR
cp $FDIR/$BDIR/has_bit_and.hpp $BDIR
cp $FDIR/$BDIR/has_bit_and_assign.hpp $BDIR
cp $FDIR/$BDIR/has_bit_or.hpp $BDIR
cp $FDIR/$BDIR/has_bit_or_assign.hpp $BDIR
cp $FDIR/$BDIR/has_bit_xor.hpp $BDIR
cp $FDIR/$BDIR/has_bit_xor_assign.hpp $BDIR
cp $FDIR/$BDIR/has_complement.hpp $BDIR
cp $FDIR/$BDIR/has_dereference.hpp $BDIR
cp $FDIR/$BDIR/has_divides.hpp $BDIR
cp $FDIR/$BDIR/has_divides_assign.hpp $BDIR
cp $FDIR/$BDIR/has_equal_to.hpp $BDIR
cp $FDIR/$BDIR/has_greater.hpp $BDIR
cp $FDIR/$BDIR/has_greater_equal.hpp $BDIR
cp $FDIR/$BDIR/has_left_shift.hpp $BDIR
cp $FDIR/$BDIR/has_left_shift_assign.hpp $BDIR
cp $FDIR/$BDIR/has_less.hpp $BDIR
cp $FDIR/$BDIR/has_less_equal.hpp $BDIR
cp $FDIR/$BDIR/has_logical_and.hpp $BDIR
cp $FDIR/$BDIR/has_logical_not.hpp $BDIR
cp $FDIR/$BDIR/has_logical_or.hpp $BDIR
cp $FDIR/$BDIR/has_minus.hpp $BDIR
cp $FDIR/$BDIR/has_minus_assign.hpp $BDIR
cp $FDIR/$BDIR/has_modulus.hpp $BDIR
cp $FDIR/$BDIR/has_modulus_assign.hpp $BDIR
cp $FDIR/$BDIR/has_multiplies.hpp $BDIR
cp $FDIR/$BDIR/has_multiplies_assign.hpp $BDIR
cp $FDIR/$BDIR/has_negate.hpp $BDIR
cp $FDIR/$BDIR/has_not_equal_to.hpp $BDIR
cp $FDIR/$BDIR/has_plus.hpp $BDIR
cp $FDIR/$BDIR/has_plus_assign.hpp $BDIR
cp $FDIR/$BDIR/has_post_decrement.hpp $BDIR
cp $FDIR/$BDIR/has_post_increment.hpp $BDIR
cp $FDIR/$BDIR/has_pre_decrement.hpp $BDIR
cp $FDIR/$BDIR/has_pre_increment.hpp $BDIR
cp $FDIR/$BDIR/has_right_shift.hpp $BDIR
cp $FDIR/$BDIR/has_right_shift_assign.hpp $BDIR
cp $FDIR/$BDIR/has_unary_minus.hpp $BDIR
cp $FDIR/$BDIR/has_unary_plus.hpp $BDIR
cp $FDIR/$BDIR/has_trivial_move_assign.hpp $BDIR
cp $FDIR/$BDIR/has_trivial_move_constructor.hpp $BDIR
cp $FDIR/$BDIR/is_copy_constructible.hpp $BDIR
cp $FDIR/$BDIR/is_nothrow_move_assignable.hpp $BDIR
cp $FDIR/$BDIR/is_nothrow_move_constructible.hpp $BDIR

BDIR=boost/type_traits/detail
mkdir -p $BDIR
cp $FDIR/$BDIR/has_binary_operator.hpp $BDIR
cp $FDIR/$BDIR/has_prefix_operator.hpp $BDIR
cp $FDIR/$BDIR/has_postfix_operator.hpp $BDIR

BDIR=boost/move
mkdir -p $BDIR
cp $FDIR/$BDIR/move.hpp $BDIR
cp $FDIR/$BDIR/utility.hpp $BDIR
cp $FDIR/$BDIR/core.hpp $BDIR
cp $FDIR/$BDIR/iterator.hpp $BDIR
cp $FDIR/$BDIR/traits.hpp $BDIR
cp $FDIR/$BDIR/algorithm.hpp $BDIR

BDIR=boost/move/detail
mkdir -p $BDIR
cp $FDIR/$BDIR/config_begin.hpp $BDIR
cp $FDIR/$BDIR/meta_utils.hpp $BDIR
cp $FDIR/$BDIR/config_end.hpp $BDIR

BDIR=boost/unordered/detail
mkdir -p $BDIR
cp $FDIR/$BDIR/allocate.hpp $BDIR

BDIR=boost/multi_index/detail
mkdir -p $BDIR
cp $FDIR/$BDIR/do_not_copy_elements_tag.hpp $BDIR
cp $FDIR/$BDIR/vartempl_support.hpp $BDIR

BDIR=boost/numeric/conversion
mkdir -p $BDIR
cp $FDIR/$BDIR/numeric_cast_traits.hpp $BDIR

BDIR=boost/numeric/conversion/detail
mkdir -p $BDIR
cp $FDIR/$BDIR/numeric_cast_traits.hpp $BDIR

BDIR=boost/numeric/conversion/detail/preprocessed
mkdir -p $BDIR
cp $FDIR/$BDIR/numeric_cast_traits_common.hpp $BDIR
cp $FDIR/$BDIR/numeric_cast_traits_long_long.hpp $BDIR

BDIR=boost/predef/detail
mkdir -p $BDIR
cp $FDIR/$BDIR/endian_compat.h $BDIR
cp $FDIR/$BDIR/test.h $BDIR
cp $FDIR/$BDIR/_cassert.h $BDIR

BDIR=boost/predef/other
mkdir -p $BDIR
cp $FDIR/$BDIR/endian.h $BDIR

BDIR=boost/predef
mkdir -p $BDIR
cp $FDIR/$BDIR/version_number.h $BDIR
cp $FDIR/$BDIR/make.h $BDIR

BDIR=boost/predef/other
mkdir -p $BDIR
cp $FDIR/$BDIR/endian.h $BDIR

BDIR=boost/predef/library/c
mkdir -p $BDIR
cp $FDIR/$BDIR/gnu.h $BDIR
cp $FDIR/$BDIR/_prefix.h $BDIR

BDIR=boost/predef/os
mkdir -p $BDIR
cp $FDIR/$BDIR/macos.h $BDIR
cp $FDIR/$BDIR/bsd.h $BDIR

BDIR=boost/predef/os/bsd
mkdir -p $BDIR
cp $FDIR/$BDIR/bsdi.h $BDIR
cp $FDIR/$BDIR/dragonfly.h $BDIR
cp $FDIR/$BDIR/free.h $BDIR
cp $FDIR/$BDIR/open.h $BDIR
cp $FDIR/$BDIR/net.h $BDIR

BDIR=boost/container
mkdir -p $BDIR
cp $FDIR/$BDIR/container_fwd.hpp $BDIR

BDIR=boost/smart_ptr/detail
mkdir -p $BDIR
cp $FDIR/$BDIR/sp_nullptr_t.hpp $BDIR
cp $FDIR/$BDIR/sp_forward.hpp $BDIR

BDIR=boost/smart_ptr
mkdir -p $BDIR
cp $FDIR/$BDIR/make_shared_object.hpp $BDIR
cp $FDIR/$BDIR/make_shared_array.hpp $BDIR


#=======================
exit
cp $FDIR/$BDIR/.hpp $BDIR

