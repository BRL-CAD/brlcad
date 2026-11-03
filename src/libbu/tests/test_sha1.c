/*                      T E S T _ S H A 1 . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "../sha1.h"

enum { SHA1_DIGEST_BYTES = 20, SHA1_HEX_BYTES = 2 * SHA1_DIGEST_BYTES + 1 };

static int
check_digest(const unsigned char *data, uint32_t length, uint32_t chunk, const char *expected)
{
    SHA1_CTX context;
    unsigned char digest[SHA1_DIGEST_BYTES];
    char hex[SHA1_HEX_BYTES];
    uint32_t offset = 0;
    size_t i;

    SHA1Init(&context);
    SHA1Update(&context, NULL, 0);
    while (offset < length) {
	uint32_t count = length - offset;
	if (count > chunk)
	    count = chunk;
	SHA1Update(&context, data + offset, count);
	offset += count;
    }
    SHA1Final(digest, &context);
    for (i = 0; i < sizeof(digest); i++)
	snprintf(hex + 2 * i, sizeof(hex) - 2 * i, "%02x", (unsigned int)digest[i]);
    if (!memcmp(hex, expected, sizeof(hex)))
	return 0;
    fprintf(stderr, "SHA-1 mismatch for %u bytes in chunks of %u: %s\n",
	    (unsigned int)length, (unsigned int)chunk, hex);
    return 1;
}

int
main(void)
{
    const struct {
	const char *message;
	const char *digest;
    } vectors[] = {
	{"", "da39a3ee5e6b4b0d3255bfef95601890afd80709"},
	{"abc", "a9993e364706816aba3e25717850c26c9cd0d89d"},
	{"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
	    "84983e441c3bd26ebaae4aa1f95129e5e54670f1"}
    };
    const struct {
	uint32_t length;
	const char *digest;
    } boundaries[] = {
    {55, "8ae2d46729cfe68ff927af5eec9c7d1b66d65ac2"},
    {56, "636e2ec698dac903498e648bd2f3af641d3c88cb"},
    {63, "6d942da0c4392b123528f2905c713a3ce28364bd"},
    {64, "c6138d514ffa2135bfce0ed0b8fac65669917ec7"},
    {65, "69bd728ad6e13cd76ff19751fde427b00e395746"},
    {119, "41c89d06001bab4ab78736b44efe7ce18ce6ae08"},
    {120, "d3dbd653bd8597b7475321b60a36891278e6a04a"},
    {127, "89d7312a903f65cd2b3e34a975e55dbea9033353"},
    {128, "e6434bc401f98603d7eda504790c98c67385d535"},
    {129, "3352e41cc30b40ae80108970492b21014049e625"},
    };
    unsigned char data[2 * SHA1_BLOCK_SIZE + 1];
    uint32_t chunk;
    size_t i;
    int failures = 0;

    for (i = 0; i < sizeof(data); i++)
	data[i] = (unsigned char)i;
    for (chunk = 1; chunk <= SHA1_BLOCK_SIZE + 1; chunk++) {
	for (i = 0; i < sizeof(vectors) / sizeof(vectors[0]); i++)
	    failures += check_digest((const unsigned char *)vectors[i].message,
		    (uint32_t)strlen(vectors[i].message), chunk, vectors[i].digest);
	for (i = 0; i < sizeof(boundaries) / sizeof(boundaries[0]); i++)
	    failures += check_digest(data, boundaries[i].length, chunk, boundaries[i].digest);
    }
    return failures ? 1 : 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
