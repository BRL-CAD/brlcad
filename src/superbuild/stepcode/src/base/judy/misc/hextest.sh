#!/bin/sh

#generates a file of hex keys and runs hexsort on it. No verification is done.
#hex data comes from /dev/urandom, so it isn't all that random.

hexdump -v -e '2/8 "%08x"' -e '"\n"' /dev/urandom |head -n 1000000 >ukeys
#a.out [in-file] [out-file] [keysize] [recordlen] [keyoffset] [mergerecs]
time bin/hexsort ukeys skeys