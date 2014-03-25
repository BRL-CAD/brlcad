# a local library for this directory
BRLCAD_ADDLIB(libbu_tests "test_funcs.c" libbu NO_INSTALL)

BRLCAD_ADDEXEC(tester_bu_bitv bu_bitv.c "libbu;libbu_tests" NO_INSTALL)

# The master to/from bitv test
# args: function number
add_test(bu_bitv_master tester_bu_bitv 0)

# args: function number, binary string, expected bitv value in hex
add_test(bu_binary_to_bitv_01 tester_bu_bitv  1 "0b0101"          0x5   )
add_test(bu_binary_to_bitv_02 tester_bu_bitv  1 "0B0101"          0x5   )
add_test(bu_binary_to_bitv_03 tester_bu_bitv  1 "0B010 1"         0x5   )
add_test(bu_binary_to_bitv_04 tester_bu_bitv  1 "0b0"             0x0   )
add_test(bu_binary_to_bitv_05 tester_bu_bitv  1 "0b100100"        0x24  )
add_test(bu_binary_to_bitv_06 tester_bu_bitv  1 "0b1011011101110" 0x16ee)
add_test(bu_binary_to_bitv_07 tester_bu_bitv  1 "01"              0x0   ) # XFAIL
add_test(bu_binary_to_bitv_08 tester_bu_bitv  1 "0F1"             0x0   ) # XFAIL
add_test(bu_binary_to_bitv_09 tester_bu_bitv  1 "0b"              0x0   )
add_test(bu_binary_to_bitv_10 tester_bu_bitv  1 "b1"              0x0   ) # XFAIL

# args: function number, input hex number for bitv, expected binary string
add_test(bu_bitv_to_binary_01 tester_bu_bitv  2 0x5    "0b00000101"        )
add_test(bu_bitv_to_binary_02 tester_bu_bitv  2 0x0    "0b00000000"        )
add_test(bu_bitv_to_binary_03 tester_bu_bitv  2 0x24   "0b00100100"        )
add_test(bu_bitv_to_binary_04 tester_bu_bitv  2 0x16ee "0b0001011011101110")

# args: function number, two input binary strings (and byte lengths) for comparison
add_test(bu_bitv_compare_equal_01 tester_bu_bitv    3 "0b00000101" 1 "0b00000101"  1)
add_test(bu_bitv_compare_equal_02 tester_bu_bitv    3 "0b00000101" 1 "0b000000101" 9) # XFAIL
add_test(bu_bitv_compare_equal_03 tester_bu_bitv    3 "0b00000100" 1 "0b000000101" 1) # XFAIL
add_test(bu_bitv_compare_equal_04 tester_bu_bitv    3 "0b00000101" 9 "0b000000101" 1) # XFAIL

# args: function number, two input binary strings (and byte lengths) for comparison
add_test(bu_bitv_compare_equal2_01 tester_bu_bitv   4 "0b00000101" 1 "0b00000101"  1)
add_test(bu_bitv_compare_equal2_02 tester_bu_bitv   4 "0b00000101" 1 "0b000000101" 9)
add_test(bu_bitv_compare_equal2_03 tester_bu_bitv   4 "0b00000100" 1 "0b000000101" 1) # XFAIL
add_test(bu_bitv_compare_equal2_04 tester_bu_bitv   4 "0b00000101" 9 "0b000000101" 1)

# args: function number, input binary string, num bytes desired, expected bit value in hex
add_test(bu_binary_to_bitv2_01 tester_bu_bitv  5 "0b0101"          1 0x5   )
add_test(bu_binary_to_bitv2_02 tester_bu_bitv  5 "0b0101"          9 0x5   )

# args: function number, input hex string 1, input hex string 2, expected hex result
add_test(bu_bitv_and_test1 tester_bu_bitv 6 "ffffffff" "00000000" "00000000")
add_test(bu_bitv_and_test2 tester_bu_bitv 6     "ab00"     "1200"     "0200")

# args: function number, input hex string 1, input hex string 2, expected hex result
add_test(bu_bitv_or_test1 tester_bu_bitv 7 "ffffffff" "00000000" "ffffffff")
add_test(bu_bitv_or_test2 tester_bu_bitv 7     "ab00"     "1200"     "bb00")

# args: function number
add_test(bu_bitv_shift tester_bu_bitv 8)

# args: function number, input char string, expected bitv vls dump
add_test(bu_bitv_vls_test1 tester_bu_bitv 9 "00000000"   "()")
add_test(bu_bitv_vls_test2 tester_bu_bitv 9 "f0f0f0f0"   "(4, 5, 6, 7, 12, 13, 14, 15, 20, 21, 22, 23, 28, 29, 30, 31)")
add_test(bu_bitv_vls_test3 tester_bu_bitv 9 "f0f0f0f1"   "(0, 4, 5, 6, 7, 12, 13, 14, 15, 20, 21, 22, 23, 28, 29, 30, 31)")
add_test(bu_bitv_vls_test4 tester_bu_bitv 9 "01f0f0f0f0" "(4, 5, 6, 7, 12, 13, 14, 15, 20, 21, 22, 23, 28, 29, 30, 31, 32)")

# args: function number, input char string, expected hex string, expected bitv length
add_test(bu_bitv_to_hex_test1 tester_bu_bitv 10 "0123" "00323130" 32)
add_test(bu_bitv_to_hex_test2 tester_bu_bitv 10   "12"     "0031" 16)

# args: function number, input hex string, expected bitv vls dump
add_test(bu_hex_to_bitv_test1 tester_bu_bitv 11 "00323130" "(4, 5, 8, 12, 13, 17, 20, 21)")
add_test(bu_hex_to_bitv_test2 tester_bu_bitv 11       "30" "(4, 5)")
add_test(bu_hex_to_bitv_test3 tester_bu_bitv 11         "" "()") # XFAIL

# args: function number, input hex string, expected binary string
add_test(bu_hexstr_to_binstr_test1 tester_bu_bitv 12     "3"              "00000011")
add_test(bu_hexstr_to_binstr_test2 tester_bu_bitv 12  "0x03"            "0b00000011")
add_test(bu_hexstr_to_binstr_test3 tester_bu_bitv 12  "0x03|04" "0b0000001100000100")
add_test(bu_hexstr_to_binstr_test4 tester_bu_bitv 12  "0x03_04" "0b0000001100000100")
add_test(bu_hexstr_to_binstr_test5 tester_bu_bitv 12  "0x03 04" "0b0000001100000100")

# args: function number, input binary string, expected hex string
add_test(bu_binstr_to_hexstr_test1 tester_bu_bitv 13        " 0011"    "03")
add_test(bu_binstr_to_hexstr_test2 tester_bu_bitv 13   "0b0000011 "  "0x03")
add_test(bu_binstr_to_hexstr_test3 tester_bu_bitv 13   "0b000 0011"  "0x03")
add_test(bu_binstr_to_hexstr_test4 tester_bu_bitv 13             ""    "00")
add_test(bu_binstr_to_hexstr_test5 tester_bu_bitv 13           "0b"  "0x00")
add_test(bu_binstr_to_hexstr_test6 tester_bu_bitv 13   "0b000_0011"  "0x03")
add_test(bu_binstr_to_hexstr_test7 tester_bu_bitv 13   "0b000|0011"  "0x03")

# some tests are expected to fail:
set_tests_properties(
  bu_binary_to_bitv_07
  bu_binary_to_bitv_08
  bu_binary_to_bitv_10
  bu_bitv_compare_equal2_03
  bu_bitv_compare_equal_02
  bu_bitv_compare_equal_03
  bu_bitv_compare_equal_04
  bu_hex_to_bitv_test3

  PROPERTIES WILL_FAIL true
)
