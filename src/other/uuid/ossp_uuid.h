/*
*   _        ___  ____ ____  ____                _     _
*  |_|_ _   / _ \/ ___/ ___||  _ \   _   _ _   _(_) __| |
*  _|_||_| | | | \___ \___ \| |_) | | | | | | | | |/ _` |
* |_||_|_| | |_| |___) |__) |  __/  | |_| | |_| | | (_| |
*  |_|_|_|  \___/|____/____/|_|      \__,_|\__,_|_|\__,_|
*
*  OSSP uuid - Universally Unique Identifier
*  Version 1.6.2 (04-Jul-2008)
*
*  ABSTRACT
*
*  OSSP uuid is a ISO-C:1999 application programming interface (API)
*  and corresponding command line interface (CLI) for the generation of
*  DCE 1.1, ISO/IEC 11578:1996 and IETF RFC-4122 compliant Universally
*  Unique Identifier (UUID). It supports DCE 1.1 variant UUIDs of version
*  1 (time and node based), version 3 (name based, MD5), version 4
*  (random number based) and version 5 (name based, SHA-1). Additional
*  API bindings are provided for the languages ISO-C++:1998, Perl:5 and
*  PHP:4/5. Optional backward compatibility exists for the ISO-C DCE-1.1
*  and Perl Data::UUID APIs.
*
*  UUIDs are 128 bit numbers which are intended to have a high likelihood
*  of uniqueness over space and time and are computationally difficult
*  to guess. They are globally unique identifiers which can be locally
*  generated without contacting a global registration authority. UUIDs
*  are intended as unique identifiers for both mass tagging objects
*  with an extremely short lifetime and to reliably identifying very
*  persistent objects across a network.
*
*  COPYRIGHT AND LICENSE
*
*  Copyright (c) 2004-2008 Ralf S. Engelschall <rse@engelschall.com>
*  Copyright (c) 2004-2008 The OSSP Project <http://www.ossp.org/>
*
*  This file is part of OSSP uuid, a library for the generation
*  of UUIDs which can found at http://www.ossp.org/pkg/lib/uuid/
*
*  Permission to use, copy, modify, and distribute this software for
*  any purpose with or without fee is hereby granted, provided that
*  the above copyright notice and this permission notice appear in all
*  copies.
*
*  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
*  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
*  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
*  IN NO EVENT SHALL THE AUTHORS AND COPYRIGHT HOLDERS AND THEIR
*  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
*  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
*  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
*  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
*  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
*  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
*  SUCH DAMAGE.
*
*  HOME AND DOCUMENTATION
*
*  The documentation and latest release can be found on
*
*  o http://www.ossp.org/pkg/lib/uuid/
*  o  ftp://ftp.ossp.org/pkg/lib/uuid/
*
*
*
*  OVERVIEW
*
*  A UUID consists of 128 bits (16-octets) which are split into 6
*  octet-bounded unsigned integer fields ("time_low", "time_mid",
*  "time_hi_and_version", "clk_seq_hi_res", "clk_seq_low" and "node") and
*  where two fields are multiplexed with a fixed size 4-bit "version" and
*  a variable sized 2-3 bit "variant" field.
*
*  The UUID octets are counted from left to right 15 to 0 and the bits
*  in each octet are counted from left to right 7 to 0 (most significant
*  bit first, least significant bit last). The unsigned integer fields
*  formed out of multiple octets are stored in "network byte order" (most
*  significant octet first, least significant octet last). A UUID is
*  stored and transmitted from left to right, i.e., in "network byte
*  order" with the most significant octet first and the least significant
*  octet last.
*
*  Illustration 1:
*  (single octet array, less compact, more annotations)
*
*
*  Bits:                                                   [4]           [2-3]
*  Field:                                                version        variant
*        MSO                                           -->|  |<--      -->| |<--                                                        LSO
*           \                                             |  |            | |                                                          /
*  Octet:    15      14      13      12      11      10   |  |9       8   | | 7       6       5       4       3       2       1       0
*         +------++------++------++------++------++------++------++------++------++------++------++------++------++------++------++------+
*  UUID:  |      ||      ||      ||      ||      ||      |####   ||      |##:    ||      ||      ||      ||      ||      ||      ||      |
*         +------++------++------++------++------++------++------++------++------++------++------++------++------++------++------++------+
*  Bit:   76543210765432107654321076543210765432107654321076543210765432107654321076543210765432107654321076543210765432107654321076543210
*        /|                              ||              ||              ||      ||      ||                                              |\
*     MSB |                              ||              ||              ||      ||      ||                                              | LSB
*         |<---------------------------->||<------------>||<------------>||<---->||<---->||<-------------------------------------------->|
*                                                           time_hi        clk_seq clk_seq
*  Field:           time_low                  time_mid      _and_version   _hi_res _low                         node
*  Bits:              [32]                      [16]           [16]        [5-6]     [8]                        [48]
*
*
*  Illustration 2:
*  (two octet arrays, more compact, less annotations)
*
*                                                        [4]
*                                                      version
*                                                    -->|  |<--
*                                                       |  |
*                                                       |  |  [16]
*                   [32]                      [16]      |  |time_hi
*                 time_low                  time_mid    | _and_version
*       |<---------------------------->||<------------>||<------------>|
*       |    MSO                       ||              ||  |           |
*       |   /                          ||              ||  |           |
*       |  15      14      13      12  ||  11      10  ||  |9       8  |
*       7654321076543210765432107654321076543210765432107654321076543210
*      /+------++------++------++------++------++------++------++------+~
*   MSB |      ||      ||      ||      ||      ||      |####   ||      |  ...
*       +------++------++------++------++------++------++------++------+~
*      ~+------++------++------++------++------++------++------++------+
*  ...  ##:    ||      ||      ||      ||      ||      ||      ||      | LSB
*      ~+------++------++------++------++------++------++------++------+/
*       7654321076543210765432107654321076543210765432107654321076543210
*       | | 7  ||   6  ||   5       4       3       2       1       0  |
*       | |    ||      ||                                          /   |
*       | |    ||      ||                                        LSO   |
*       |<---->||<---->||<-------------------------------------------->|
*       |clk_seq clk_seq                      node
*       |_hi_res _low                         [48]
*       |[5-6]    [8]
*       | |
*    -->| |<--
*     variant
*      [2-3]
*
*
*
*  AUTHORS
*
*  This is a list of authors who have written
*  or edited major parts of the OSSP uuid sources.
*
*  Ralf S. Engelschall   <rse@engelschall.com>
*
*
*
*  NEWS
*
*  This is a list of major changes to OSSP uuid. For more detailed
*  change descriptions, please have a look at the ChangeLog file.
*
*  Major changes between 1.4 and 1.5
*
*    o Many internal code cleanups and fixes.
*    o Improved and fixed PostgreSQL API.
*    o Improved and fixed PHP API.
*
*  Major changes between 1.3 and 1.4
*
*    o Added PostgreSQL API.
*
*  Major changes between 1.2 and 1.3
*
*    o Added Perl TIE-style API.
*    o Added Perl Data::UUID backward compatibility API.
*    o Added C++ API.
*    o Added PHP API.
*
*  Major changes between 1.1 and 1.2
*
*    o Added support for version 5 UUIDs (name-based, SHA-1)
*
*  Major changes between 1.0 and 1.1
*
*    o Added Perl API
*
*  Major changes between 0.9 and 1.0
*
*    o Initial functionality
*
*
*
*
*
*  HISTORY
*
*  During OSSP uuid we were totally puzzled by a subtle bug in the UUID
*  standards related to the generation of multi-cast MAC addresses.
*  This part of the history shows a very interesting technical bug,
*  the unusual way of having to fix a standard (which was multiple
*  times revised by different standard authorities, including the
*  IETF, the OpenGroup and ISO/IEC) afterwards plus the fixing of six
*  implementations into which the bug was inherited similarly. Below are
*  some snapshot of this part of history: the first implementation fix
*  (for FreeBSD) and the notification of the IETF standards authors.
*
*  ___________________________________________________________________________
*
*  Date: Fri, 13 Feb 2004 16:09:31 +0100
*  From: "Ralf S. Engelschall" <rse@en1.engelschall.com>
*  To: paulle@microsoft.com, michael@neonym.net, rsalz@datapower.com
*  Subject: [PATCH] draft-mealling-uuid-urn-02.txt
*  Message-ID: <20040213150931.GA7656@engelschall.com>
*
*  During implementation of OSSP uuid (a flexible CLI and C API for
*  generation and partial decoding of version 1, 3 and 4 UUIDs, see
*  http://www.ossp.org/pkg/lib/uuid/ for details), I discovered a nasty bug
*  in the generation of random multicast MAC addresses. It is present in
*  all standards and drafts (both expired ones and current ones) and was
*  also inherited (until I fixed it by submitting patches to the authors
*  recently) by all six freely available UUID implementations (Apache APR,
*  FreeBSD uuidgen(2), Java JUG, Linux's libuuid from e2fsutil, Perl's
*  Data::UUID and WINE's UUID generator)).
*
*  In case no real/physical IEEE 802 address is available, both the
*  expired "draft-leach-uuids-guids-01" (section "4. Node IDs when no IEEE
*  802 network card is available"), RFC 2518 (section "6.4.1 Node Field
*  Generation Without the IEEE 802 Address") and now even your current
*  "draft-mealling-uuid-urn-02.txt" (section "4.5 Node IDs that do not
*  identify the host") recommend:
*
*      "A better solution is to obtain a 47-bit cryptographic quality
*      random number, and use it as the low 47 bits of the node ID, with
*      the _most_ significant bit of the first octet of the node ID set to
*      one. This bit is the unicast/multicast bit, which will never be set
*      in IEEE 802 addresses obtained from network cards; hence, there can
*      never be a conflict between UUIDs generated by machines with and
*      without network cards."
*
*  Unfortunately, this incorrectly explains how to implement this and even
*  the example implementation (draft-mealling-uuid-urn-02.txt, "Appendix
*  A. Appendix A - Sample Implementation") inherited this. Correct is
*  "the _least_ significant bit of the first octet of the node ID" as the
*  multicast bit in a memory and hexadecimal string representation of a
*  48-bit IEEE 802 MAC address.
*
*  This standards bug arised from a false interpretation, as the multicast
*  bit is actually the _most_ significant bit in IEEE 802.3 (Ethernet)
*  _transmission order_ of an IEEE 802 MAC address. But you forgot that the
*  bitwise order of an _octet_ from a MAC address _memory_ and hexadecimal
*  string representation is still always from left (MSB, bit 7) to right
*  (LSB, bit 0). And the standard deals with memory representations only,
*  so the transmission order of a MAC doesnt' matter here.
*
*  As mentioned, OSSP uuid already implements this correctly. The FreeBSD
*  uuidgen(2) and Apache APR generators I've also fixed myself recently in
*  CVS. And for the remaining implementations I've submitted patches to the
*  authors and they all (except for WINE) responded that they took over the
*  patch. So the results of this long-standing bug we were able to fix --
*  at least for the free software world ;-). What is now remaining is that
*  you finally also should fix this in your standard so the bug does not
*  spread any longer into other implementations.
*
*  Here is the minimal required patch against your draft:
*
*  --- draft-mealling-uuid-urn-02.txt.orig	Mon Feb  2 21:50:35 2004
*  +++ draft-mealling-uuid-urn-02.txt	Fri Feb 13 15:41:49 2004
*  @@ -751,7 +751,7 @@
*      [6], and the cost was US$550.
*
*      A better solution is to obtain a 47-bit cryptographic quality random
*  -   number, and use it as the low 47 bits of the node ID, with the most
*  +   number, and use it as the low 47 bits of the node ID, with the least
*      significant bit of the first octet of the node ID set to one. This
*      bit is the unicast/multicast bit, which will never be set in IEEE 802
*      addresses obtained from network cards; hence, there can never be a
*  @@ -1369,7 +1369,7 @@
*              }
*              else {
*                  get_random_info(seed);
*  -               seed[0] |= 0x80;
*  +               seed[0] |= 0x01;
*                  memcpy(&saved_node, seed, sizeof saved_node);
*                  fp = fopen("nodeid", "wb");
*                  if (fp) {
*
*  But I recommend you to perhaps also add one or two sentences which
*  explain what I explained above (the difference between memory and
*  transmission order), just to make sure people are not confused in the
*  other direction and then think there is a bug (in the then fixed and
*  correct) standard, because they know about the transmission order of MAC
*  addresses.
*
*  Yours,
*                                         Ralf S. Engelschall
*                                         rse@engelschall.com
*                                         www.engelschall.com
*
*  Date: Fri, 13 Feb 2004 11:05:51 -0500
*  From: Rich Salz <rsalz@datapower.com>
*  To: rse@engelschall.com
*  Cc: paulle@microsoft.com, michael@neonym.net
*  Message-ID: <402CF5DF.4020601@datapower.com>
*  Subject: Re: [PATCH] draft-mealling-uuid-urn-02.txt
*  References: <20040213150931.GA7656@engelschall.com>
*  Content-Length: 431
*  Lines: 11
*
*  Thanks for writing, Ralf.
*
*  You're correct, and this has been noted by the IESG and will be fixed in
*  the next draft.  It's unfortunate we made it this far with the bug.
*
*      /r$
*  --
*  Rich Salz, Chief Security Architect
*  DataPower Technology                           http://www.datapower.com
*  XS40 XML Security Gateway   http://www.datapower.com/products/xs40.html
*  XML Security Overview  http://www.datapower.com/xmldev/xmlsecurity.html
*
*  Date: Thu, 22 Jan 2004 05:34:11 -0800 (PST)
*  From: "Ralf S. Engelschall" <rse@FreeBSD.org>
*  Message-Id: <200401221334.i0MDYB1K018137@repoman.freebsd.org>
*  To: src-committers@FreeBSD.org, cvs-src@FreeBSD.org, cvs-all@FreeBSD.org
*  Subject: cvs commit: src/sys/kern kern_uuid.c
*  X-FreeBSD-CVS-Branch: HEAD
*  X-Loop: FreeBSD.ORG
*  Content-Length: 1907
*  Lines: 42
*
*  rse         2004/01/22 05:34:11 PST
*
*    FreeBSD src repository
*
*    Modified files:
*      sys/kern             kern_uuid.c
*    Log:
*    Fix generation of random multicast MAC address.
*
*    In case no real/physical IEEE 802 address is available, both the expired
*    "draft-leach-uuids-guids-01" (section "4. Node IDs when no IEEE 802
*    network card is available") and RFC 2518 (section "6.4.1 Node Field
*    Generation Without the IEEE 802 Address") recommend (quoted from RFC
*    2518):
*
*      "The ideal solution is to obtain a 47 bit cryptographic quality random
*      number, and use it as the low 47 bits of the node ID, with the _most_
*      significant bit of the first octet of the node ID set to 1. This bit
*      is the unicast/multicast bit, which will never be set in IEEE 802
*      addresses obtained from network cards; hence, there can never be a
*      conflict between UUIDs generated by machines with and without network
*      cards."
*
*    Unfortunately, this incorrectly explains how to implement this and
*    the FreeBSD UUID generator code inherited this generation bug from
*    the broken reference code in the standards draft. They should instead
*    specify the "_least_ significant bit of the first octet of the node ID"
*    as the multicast bit in a memory and hexadecimal string representation
*    of a 48-bit IEEE 802 MAC address.
*
*    This standards bug arised from a false interpretation, as the multicast
*    bit is actually the _most_ significant bit in IEEE 802.3 (Ethernet)
*    _transmission order_ of an IEEE 802 MAC address. The standards authors
*    forgot that the bitwise order of an _octet_ from a MAC address _memory_
*    and hexadecimal string representation is still always from left (MSB,
*    bit 7) to right (LSB, bit 0).
*
*    Fortunately, this UUID generation bug could have occurred on systems
*    without any Ethernet NICs only.
*
*    Revision  Changes    Path
*    1.7       +1 -1      src/sys/kern/kern_uuid.c
*
*  Date: Thu, 22 Jan 2004 15:20:22 -0800
*  From: Marcel Moolenaar <marcel@xcllnt.net>
*  To: "Ralf S. Engelschall" <rse@FreeBSD.org>
*  Cc: src-committers@FreeBSD.org, cvs-src@FreeBSD.org, cvs-all@FreeBSD.org
*  Subject: Re: cvs commit: src/sys/kern kern_uuid.c
*  Message-ID: <20040122232022.GA77798@ns1.xcllnt.net>
*  References: <200401221334.i0MDYB1K018137@repoman.freebsd.org>
*  Content-Length: 380
*  Lines: 14
*
*  On Thu, Jan 22, 2004 at 05:34:11AM -0800, Ralf S. Engelschall wrote:
*  > rse         2004/01/22 05:34:11 PST
*  >
*  >   FreeBSD src repository
*  >
*  >   Modified files:
*  >     sys/kern             kern_uuid.c
*  >   Log:
*  >   Fix generation of random multicast MAC address.
*
*  An excellent catch and an outstanding commit log. Chapeau!
*
*  --
*   Marcel Moolenaar	  USPA: A-39004		 marcel@xcllnt.net
*
*  ___________________________________________________________________________
*
*  Index: ChangeLog
*  ===================================================================
*  RCS file: /e/ossp/cvs/ossp-pkg/uuid/ChangeLog,v
*  retrieving revision 1.42
*  diff -u -d -r1.42 ChangeLog
*  --- ChangeLog	13 Feb 2004 16:17:07 -0000	1.42
*  +++ ChangeLog	13 Feb 2004 21:01:07 -0000
*  @@ -13,6 +13,14 @@
*
*     Changes between 0.9.6 and 0.9.7 (11-Feb-2004 to 13-Feb-2004)
*
*  +   o remove --with-rfc2518 option and functionality because
*  +     even the IETF/IESG has finally approved our report about the broken
*  +     random multicast MAC address generation in the standard (and
*  +     will fix it in new versions of the draft-mealling-uuid-urn). So,
*  +     finally get rid of this broken-by-design backward compatibility
*  +     functionality.
*  +     [Ralf S. Engelschall]
*  +
*      o Add support to uuid(1) CLI for decoding from stdin for
*        both binary and string representations.
*        [Ralf S. Engelschall]
*  Index: uuid.ac
*  ===================================================================
*  RCS file: /e/ossp/cvs/ossp-pkg/uuid/uuid.ac,v
*  retrieving revision 1.10
*  diff -u -d -r1.10 uuid.ac
*  --- uuid.ac	11 Feb 2004 14:38:40 -0000	1.10
*  +++ uuid.ac	13 Feb 2004 19:20:32 -0000
*  @@ -71,12 +71,6 @@
*       AC_CHECK_SIZEOF(unsigned long long, 8)
*
*       dnl #   options
*  -    AC_ARG_WITH(rfc2518,
*  -        AC_HELP_STRING([--with-rfc2518], [use incorrect generation of IEEE 802 multicast addresses according to RFC2518]),
*  -        [ac_cv_with_rfc2518=$withval], [ac_cv_with_rfc2518=no])
*  -    if test ".$ac_cv_with_rfc2518" = ".yes"; then
*  -        AC_DEFINE(WITH_RFC2518, 1, [whether to use incorrect generation of IEEE 802 multicast addresses according to RFC2518])
*  -    fi
*       AC_ARG_WITH(dce,
*           AC_HELP_STRING([--with-dce], [build DCE 1.1 backward compatibility API]),
*           [ac_cv_with_dce=$withval], [ac_cv_with_dce=no])
*  Index: uuid.c
*  ===================================================================
*  RCS file: /e/ossp/cvs/ossp-pkg/uuid/uuid.c,v
*  retrieving revision 1.44
*  diff -u -d -r1.44 uuid.c
*  --- uuid.c	19 Jan 2004 14:56:35 -0000	1.44
*  +++ uuid.c	13 Feb 2004 19:22:01 -0000
*  @@ -61,69 +61,9 @@
*       Unix UTC base time is January  1, 1970)
*   #define UUID_TIMEOFFSET "01B21DD213814000"
*
*  -   IEEE 802 MAC address encoding/decoding bit fields
*  -
*  -   ATTENTION:
*  -
*  -   In case no real/physical IEEE 802 address is available, both
*  -   "draft-leach-uuids-guids-01" (section "4. Node IDs when no IEEE 802
*  -   network card is available") and RFC 2518 (section "6.4.1 Node Field
*  -   Generation Without the IEEE 802 Address") recommend (quoted from RFC
*  -   2518):
*  -
*  -     "The ideal solution is to obtain a 47 bit cryptographic quality
*  -     random number, and use it as the low 47 bits of the node ID, with
*  -     the most significant bit of the first octet of the node ID set to
*  -     1. This bit is the unicast/multicast bit, which will never be set
*  -     in IEEE 802 addresses obtained from network cards; hence, there can
*  -     never be a conflict between UUIDs generated by machines with and
*  -     without network cards."
*  -
*  -   This passage clearly explains the intention to use IEEE 802 multicast
*  -   addresses. Unfortunately, it incorrectly explains how to implement
*  -   this! It should instead specify the "*LEAST* significant bit of the
*  -   first octet of the node ID" as the multicast bit in a memory and
*  -   hexadecimal string representation of a 48-bit IEEE 802 MAC address.
*  -
*  -   Unfortunately, even the reference implementation included in the
*  -   expired IETF "draft-leach-uuids-guids-01" incorrectly set the
*  -   multicast bit with an OR bit operation and an incorrect mask of
*  -   0x80. Hence, several other UUID implementations found on the
*  -   Internet have inherited this bug.
*  -
*  -   Luckily, neither DCE 1.1 nor ISO/IEC 11578:1996 are affected by this
*  -   problem. They disregard the topic of missing IEEE 802 addresses
*  -   entirely, and thus avoid adopting this bug from the original draft
*  -   and code ;-)
*  -
*  -   It seems that this standards bug arises from a false interpretation,
*  -   as the multicast bit is actually the *MOST* significant bit in IEEE
*  -   802.3 (Ethernet) _transmission order_ of an IEEE 802 MAC address. The
*  -   authors were likely not aware that the bitwise order of an octet from
*  -   a MAC address memory and hexadecimal string representation is still
*  -   always from left (MSB, bit 7) to right (LSB, bit 0).
*  -
*  -   For more information, see "Understanding Physical Addresses" in
*  -   "Ethernet -- The Definitive Guide", p.43, and the section "ETHERNET
*  -   MULTICAST ADDRESSES" in http://www.iana.org/assignments/ethernet-numbers.
*  -
*  -   At OSSP, we do it the intended/correct way and generate a real
*  -   IEEE 802 multicast address. Those wanting to encode broken IEEE
*  -   802 MAC addresses (as specified) can nevertheless use a brain dead
*  -   compile-time option to switch off the correct behavior. When decoding
*  -   we always use the correct behavior of course.
*  -
*  - encoding
*  -#ifdef WITH_RFC2518
*  -#define IEEE_MAC_MCBIT_ENC BM_OCTET(1,0,0,0,0,0,0,0)
*  -#else
*  -#define IEEE_MAC_MCBIT_ENC BM_OCTET(0,0,0,0,0,0,0,1)
*  -#endif
*  -#define IEEE_MAC_LOBIT_ENC BM_OCTET(0,0,0,0,0,0,1,0)
*  -
*  - decoding
*  -#define IEEE_MAC_MCBIT_DEC BM_OCTET(0,0,0,0,0,0,0,1)
*  -#define IEEE_MAC_LOBIT_DEC BM_OCTET(0,0,0,0,0,0,1,0)
*  +   IEEE 802 MAC address encoding/decoding bit fields
*  +#define IEEE_MAC_MCBIT BM_OCTET(0,0,0,0,0,0,0,1)
*  +#define IEEE_MAC_LOBIT BM_OCTET(0,0,0,0,0,0,1,0)
*
*    IEEE 802 MAC address octet length
*   #define IEEE_MAC_OCTETS 6
*  @@ -622,8 +562,8 @@
*               (unsigned int)uuid->obj.node[3],
*               (unsigned int)uuid->obj.node[4],
*               (unsigned int)uuid->obj.node[5],
*  -            (uuid->obj.node[0] & IEEE_MAC_LOBIT_DEC ? "local" : "global"),
*  -            (uuid->obj.node[0] & IEEE_MAC_MCBIT_DEC ? "multicast" : "unicast"));
*  +            (uuid->obj.node[0] & IEEE_MAC_LOBIT ? "local" : "global"),
*  +            (uuid->obj.node[0] & IEEE_MAC_MCBIT ? "multicast" : "unicast"));
*       }
*       else {
*           decode anything else as hexadecimal byte-string only
*  @@ -843,8 +783,8 @@
*       if ((mode & UUID_MAKE_MC) || (uuid->mac[0] & BM_OCTET(1,0,0,0,0,0,0,0))) {
*              generate random IEEE 802 local multicast MAC address
*           prng_data(uuid->prng, (void *)&(uuid->obj.node), sizeof(uuid->obj.node));
*  -        uuid->obj.node[0] |= IEEE_MAC_MCBIT_ENC;
*  -        uuid->obj.node[0] |= IEEE_MAC_LOBIT_ENC;
*  +        uuid->obj.node[0] |= IEEE_MAC_MCBIT;
*  +        uuid->obj.node[0] |= IEEE_MAC_LOBIT;
*       }
*       else {
*              use real regular MAC address
*   _        ___  ____ ____  ____                _     _
*
*
*
*
*  THANKS
*
*  Credit has to be given to the following people who contributed ideas,
*  bugfixes, hints, gave platform feedback, etc. (in alphabetical order):
*
*    o  Matthias Andree             <matthias.andree@gmx.de>
*    o  Neil Caunt                  <retardis@gmail.com>
*    o  Neil Conway                 <neilc@samurai.com>
*    o  M. Daniel                   <mdaniel@scdi.com>
*    o  Simon "janus" Dassow        <janus@errornet.de>
*    o  Fuyuki                      <fuyuki@nigredo.org>
*    o  Thomas Lotterer             <thomas@lotterer.net>
*    o  Roman Neuhauser             <neuhauser@sigpipe.cz>
*    o  Hrvoje Niksic               <hniksic@xemacs.org>
*    o  Piotr Roszatycki            <dexter@debian.org>
*    o  Hiroshi Saito               <z-saito@guitar.ocn.ne.jp>
*    o  Michael Schloh              <michael@schloh.com>
*    o  Guerry Semones              <guerry@tsunamiresearch.com>
*    o  David Wheeler               <david@justatheory.com>
*    o  Wu Yongwei                  <wuyongwei@gmail.com>
*/


#include <string.h>
#include <stdarg.h>
#include <string.h>

#if defined(WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <time.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif


/*
**  uuid_md5.h: MD5 API definition
*/


#ifndef __MD5_H___
#define __MD5_H___

#define MD5_PREFIX uuid_

/* embedding support */
#ifdef MD5_PREFIX
#if defined(__STDC__) || defined(__cplusplus)
#define __MD5_CONCAT(x,y) x ## y
#define MD5_CONCAT(x,y) __MD5_CONCAT(x,y)
#else
#define __MD5_CONCAT(x) x
#define MD5_CONCAT(x,y) __MD5_CONCAT(x)y
#endif
#define md5_st      MD5_CONCAT(MD5_PREFIX,md5_st)
#define md5_t       MD5_CONCAT(MD5_PREFIX,md5_t)
#define md5_create  MD5_CONCAT(MD5_PREFIX,md5_create)
#define md5_init    MD5_CONCAT(MD5_PREFIX,md5_init)
#define md5_update  MD5_CONCAT(MD5_PREFIX,md5_update)
#define md5_store   MD5_CONCAT(MD5_PREFIX,md5_store)
#define md5_format  MD5_CONCAT(MD5_PREFIX,md5_format)
#define md5_destroy MD5_CONCAT(MD5_PREFIX,md5_destroy)
#endif

struct md5_st;
typedef struct md5_st md5_t;

#define MD5_LEN_BIN 16
#define MD5_LEN_STR 32

typedef enum {
    MD5_RC_OK  = 0,
    MD5_RC_ARG = 1,
    MD5_RC_MEM = 2
} md5_rc_t;

extern md5_rc_t md5_create  (md5_t **md5);
extern md5_rc_t md5_init    (md5_t  *md5);
extern md5_rc_t md5_update  (md5_t  *md5, const void  *data_ptr, size_t  data_len);
extern md5_rc_t md5_store   (md5_t  *md5,       void **data_ptr, size_t *data_len);
extern md5_rc_t md5_format  (md5_t  *md5,       char **data_ptr, size_t *data_len);
extern md5_rc_t md5_destroy (md5_t  *md5);

#endif /* __MD5_H___ */

/*
**  uuid_sha1.h: SHA-1 API definition
*/

#ifndef __SHA1_H___
#define __SHA1_H___

#define SHA1_PREFIX uuid_

/* embedding support */
#ifdef SHA1_PREFIX
#if defined(__STDC__) || defined(__cplusplus)
#define __SHA1_CONCAT(x,y) x ## y
#define SHA1_CONCAT(x,y) __SHA1_CONCAT(x,y)
#else
#define __SHA1_CONCAT(x) x
#define SHA1_CONCAT(x,y) __SHA1_CONCAT(x)y
#endif
#define sha1_st      SHA1_CONCAT(SHA1_PREFIX,sha1_st)
#define sha1_t       SHA1_CONCAT(SHA1_PREFIX,sha1_t)
#define sha1_create  SHA1_CONCAT(SHA1_PREFIX,sha1_create)
#define sha1_init    SHA1_CONCAT(SHA1_PREFIX,sha1_init)
#define sha1_update  SHA1_CONCAT(SHA1_PREFIX,sha1_update)
#define sha1_store   SHA1_CONCAT(SHA1_PREFIX,sha1_store)
#define sha1_format  SHA1_CONCAT(SHA1_PREFIX,sha1_format)
#define sha1_destroy SHA1_CONCAT(SHA1_PREFIX,sha1_destroy)
#endif

struct sha1_st;
typedef struct sha1_st sha1_t;

#define SHA1_LEN_BIN 20
#define SHA1_LEN_STR 40

typedef enum {
    SHA1_RC_OK  = 0,
    SHA1_RC_ARG = 1,
    SHA1_RC_MEM = 2,
    SHA1_RC_INT = 3
} sha1_rc_t;

extern sha1_rc_t sha1_create  (sha1_t **sha1);
extern sha1_rc_t sha1_init    (sha1_t  *sha1);
extern sha1_rc_t sha1_update  (sha1_t  *sha1, const void  *data_ptr, size_t  data_len);
extern sha1_rc_t sha1_store   (sha1_t  *sha1,       void **data_ptr, size_t *data_len);
extern sha1_rc_t sha1_format  (sha1_t  *sha1,       char **data_ptr, size_t *data_len);
extern sha1_rc_t sha1_destroy (sha1_t  *sha1);

#endif /* __SHA1_H___ */

/*
**  uuid_prng.h: PRNG API definition
*/

#ifndef __PRNG_H___
#define __PRNG_H___


#define PRNG_PREFIX uuid_

/* embedding support */
#ifdef PRNG_PREFIX
#if defined(__STDC__) || defined(__cplusplus)
#define __PRNG_CONCAT(x,y) x ## y
#define PRNG_CONCAT(x,y) __PRNG_CONCAT(x,y)
#else
#define __PRNG_CONCAT(x) x
#define PRNG_CONCAT(x,y) __PRNG_CONCAT(x)y
#endif
#define prng_st      PRNG_CONCAT(PRNG_PREFIX,prng_st)
#define prng_t       PRNG_CONCAT(PRNG_PREFIX,prng_t)
#define prng_create  PRNG_CONCAT(PRNG_PREFIX,prng_create)
#define prng_data    PRNG_CONCAT(PRNG_PREFIX,prng_data)
#define prng_destroy PRNG_CONCAT(PRNG_PREFIX,prng_destroy)
#endif

struct prng_st;
typedef struct prng_st prng_t;

typedef enum {
    PRNG_RC_OK  = 0,
    PRNG_RC_ARG = 1,
    PRNG_RC_MEM = 2,
    PRNG_RC_INT = 3
} prng_rc_t;

extern prng_rc_t prng_create  (prng_t **prng);
extern prng_rc_t prng_data    (prng_t  *prng, void *data_ptr, size_t data_len);
extern prng_rc_t prng_destroy (prng_t  *prng);

#endif /* __PRNG_H___ */

/*
**  uuid_mac.h: Media Access Control (MAC) resolver API definition
*/

#ifndef __UUID_MAC_H__
#define __UUID_MAC_H__


#define MAC_PREFIX uuid_

/* embedding support */
#ifdef MAC_PREFIX
#if defined(__STDC__) || defined(__cplusplus)
#define __MAC_CONCAT(x,y) x ## y
#define MAC_CONCAT(x,y) __MAC_CONCAT(x,y)
#else
#define __MAC_CONCAT(x) x
#define MAC_CONCAT(x,y) __MAC_CONCAT(x)y
#endif
#define mac_address MAC_CONCAT(MAC_PREFIX,mac_address)
#endif

#define MAC_LEN 6

extern int mac_address(unsigned char *_data_ptr, size_t _data_len);

#endif /* __UUID_MAC_H__ */

/*
**  uuid_time.h: Time Management API
*/

#ifndef __UUID_TIME_H__
#define __UUID_TIME_H__

#define TIME_PREFIX uuid_

/* embedding support */
#ifdef TIME_PREFIX
#if defined(__STDC__) || defined(__cplusplus)
#define __TIME_CONCAT(x,y) x ## y
#define TIME_CONCAT(x,y) __TIME_CONCAT(x,y)
#else
#define __TIME_CONCAT(x) x
#define TIME_CONCAT(x,y) __TIME_CONCAT(x)y
#endif
#define time_gettimeofday TIME_CONCAT(TIME_PREFIX,time_gettimeofday)
#define time_usleep       TIME_CONCAT(TIME_PREFIX,time_usleep)
#endif

/* minimum C++ support */
#ifdef __cplusplus
#define DECLARATION_BEGIN extern "C" {
#define DECLARATION_END   }
#else
#define DECLARATION_BEGIN
#define DECLARATION_END
#endif

DECLARATION_BEGIN

#ifndef HAVE_STRUCT_TIMEVAL
struct timeval { long tv_sec; long tv_usec; };
#endif

extern int time_gettimeofday(struct timeval *);
extern int time_usleep(long usec);

DECLARATION_END

#endif /* __UUID_TIME_H__ */

/*
**  ui64.h: API declaration
*/

#ifndef __UI64_H__
#define __UI64_H__

#define UI64_PREFIX uuid_

/* embedding support */
#ifdef UI64_PREFIX
#if defined(__STDC__) || defined(__cplusplus)
#define __UI64_CONCAT(x,y) x ## y
#define UI64_CONCAT(x,y) __UI64_CONCAT(x,y)
#else
#define __UI64_CONCAT(x) x
#define UI64_CONCAT(x,y) __UI64_CONCAT(x)y
#endif
#define ui64_t     UI64_CONCAT(UI64_PREFIX,ui64_t)
#define ui64_zero  UI64_CONCAT(UI64_PREFIX,ui64_zero)
#define ui64_max   UI64_CONCAT(UI64_PREFIX,ui64_max)
#define ui64_n2i   UI64_CONCAT(UI64_PREFIX,ui64_n2i)
#define ui64_i2n   UI64_CONCAT(UI64_PREFIX,ui64_i2n)
#define ui64_s2i   UI64_CONCAT(UI64_PREFIX,ui64_s2i)
#define ui64_i2s   UI64_CONCAT(UI64_PREFIX,ui64_i2s)
#define ui64_add   UI64_CONCAT(UI64_PREFIX,ui64_add)
#define ui64_addn  UI64_CONCAT(UI64_PREFIX,ui64_addn)
#define ui64_sub   UI64_CONCAT(UI64_PREFIX,ui64_sub)
#define ui64_subn  UI64_CONCAT(UI64_PREFIX,ui64_subn)
#define ui64_mul   UI64_CONCAT(UI64_PREFIX,ui64_mul)
#define ui64_muln  UI64_CONCAT(UI64_PREFIX,ui64_muln)
#define ui64_div   UI64_CONCAT(UI64_PREFIX,ui64_div)
#define ui64_divn  UI64_CONCAT(UI64_PREFIX,ui64_divn)
#define ui64_and   UI64_CONCAT(UI64_PREFIX,ui64_and)
#define ui64_or    UI64_CONCAT(UI64_PREFIX,ui64_or)
#define ui64_xor   UI64_CONCAT(UI64_PREFIX,ui64_xor)
#define ui64_not   UI64_CONCAT(UI64_PREFIX,ui64_not)
#define ui64_rol   UI64_CONCAT(UI64_PREFIX,ui64_rol)
#define ui64_ror   UI64_CONCAT(UI64_PREFIX,ui64_ror)
#define ui64_len   UI64_CONCAT(UI64_PREFIX,ui64_len)
#define ui64_cmp   UI64_CONCAT(UI64_PREFIX,ui64_cmp)
#endif

typedef struct {
    unsigned char x[8]; /* x_0, ..., x_7 */
} ui64_t;

#define ui64_cons(x7,x6,x5,x4,x3,x2,x1,x0) \
    { { 0x##x0, 0x##x1, 0x##x2, 0x##x3, 0x##x4, 0x##x5, 0x##x6, 0x##x7 } }

/* particular values */
extern ui64_t        ui64_zero (void);
extern ui64_t        ui64_max  (void);

/* import and export via ISO-C "unsigned long" */
extern ui64_t        ui64_n2i  (unsigned long n);
extern unsigned long ui64_i2n  (ui64_t x);

/* import and export via ISO-C string of arbitrary base */
extern ui64_t        ui64_s2i  (const char *str, char **end, int base);
extern char *        ui64_i2s  (ui64_t x, char *str, size_t len, int base);

/* arithmetical operations */
extern ui64_t        ui64_add  (ui64_t x, ui64_t y, ui64_t *ov);
extern ui64_t        ui64_addn (ui64_t x, int    y, int    *ov);
extern ui64_t        ui64_sub  (ui64_t x, ui64_t y, ui64_t *ov);
extern ui64_t        ui64_subn (ui64_t x, int    y, int    *ov);
extern ui64_t        ui64_mul  (ui64_t x, ui64_t y, ui64_t *ov);
extern ui64_t        ui64_muln (ui64_t x, int    y, int    *ov);
extern ui64_t        ui64_div  (ui64_t x, ui64_t y, ui64_t *ov);
extern ui64_t        ui64_divn (ui64_t x, int    y, int    *ov);

/* bit operations */
extern ui64_t        ui64_and  (ui64_t x, ui64_t y);
extern ui64_t        ui64_or   (ui64_t x, ui64_t y);
extern ui64_t        ui64_xor  (ui64_t x, ui64_t y);
extern ui64_t        ui64_not  (ui64_t x);
extern ui64_t        ui64_rol  (ui64_t x, int s, ui64_t *ov);
extern ui64_t        ui64_ror  (ui64_t x, int s, ui64_t *ov);

/* other operations */
extern int           ui64_len  (ui64_t x);
extern int           ui64_cmp  (ui64_t x, ui64_t y);

#endif /* __UI64_H__ */

/*
**  ui128.h: API declaration
*/

#ifndef __UI128_H__
#define __UI128_H__

#define UI128_PREFIX uuid_

/* embedding support */
#ifdef UI128_PREFIX
#if defined(__STDC__) || defined(__cplusplus)
#define __UI128_CONCAT(x,y) x ## y
#define UI128_CONCAT(x,y) __UI128_CONCAT(x,y)
#else
#define __UI128_CONCAT(x) x
#define UI128_CONCAT(x,y) __UI128_CONCAT(x)y
#endif
#define ui128_t     UI128_CONCAT(UI128_PREFIX,ui128_t)
#define ui128_zero  UI128_CONCAT(UI128_PREFIX,ui128_zero)
#define ui128_max   UI128_CONCAT(UI128_PREFIX,ui128_max)
#define ui128_n2i   UI128_CONCAT(UI128_PREFIX,ui128_n2i)
#define ui128_i2n   UI128_CONCAT(UI128_PREFIX,ui128_i2n)
#define ui128_s2i   UI128_CONCAT(UI128_PREFIX,ui128_s2i)
#define ui128_i2s   UI128_CONCAT(UI128_PREFIX,ui128_i2s)
#define ui128_add   UI128_CONCAT(UI128_PREFIX,ui128_add)
#define ui128_addn  UI128_CONCAT(UI128_PREFIX,ui128_addn)
#define ui128_sub   UI128_CONCAT(UI128_PREFIX,ui128_sub)
#define ui128_subn  UI128_CONCAT(UI128_PREFIX,ui128_subn)
#define ui128_mul   UI128_CONCAT(UI128_PREFIX,ui128_mul)
#define ui128_muln  UI128_CONCAT(UI128_PREFIX,ui128_muln)
#define ui128_div   UI128_CONCAT(UI128_PREFIX,ui128_div)
#define ui128_divn  UI128_CONCAT(UI128_PREFIX,ui128_divn)
#define ui128_and   UI128_CONCAT(UI128_PREFIX,ui128_and)
#define ui128_or    UI128_CONCAT(UI128_PREFIX,ui128_or)
#define ui128_xor   UI128_CONCAT(UI128_PREFIX,ui128_xor)
#define ui128_not   UI128_CONCAT(UI128_PREFIX,ui128_not)
#define ui128_rol   UI128_CONCAT(UI128_PREFIX,ui128_rol)
#define ui128_ror   UI128_CONCAT(UI128_PREFIX,ui128_ror)
#define ui128_len   UI128_CONCAT(UI128_PREFIX,ui128_len)
#define ui128_cmp   UI128_CONCAT(UI128_PREFIX,ui128_cmp)
#endif

typedef struct {
    unsigned char x[16]; /* x_0, ..., x_15 */
} ui128_t;

#define ui128_cons(x15,x14,x13,x12,x11,x10,x9,x8,x7,x6,x5,x4,x3,x2,x1,x0) \
    { { 0x##x0, 0x##x1, 0x##x2,  0x##x3,  0x##x4,  0x##x5,  0x##x6,  0x##x7, \
    { { 0x##x8, 0x##x9, 0x##x10, 0x##x11, 0x##x12, 0x##x13, 0x##x14, 0x##x15 } }

/* particular values */
extern ui128_t        ui128_zero (void);
extern ui128_t        ui128_max  (void);

/* import and export via ISO-C "unsigned long" */
extern ui128_t        ui128_n2i  (unsigned long n);
extern unsigned long  ui128_i2n  (ui128_t x);

/* import and export via ISO-C string of arbitrary base */
extern ui128_t        ui128_s2i  (const char *str, char **end, int base);
extern char *         ui128_i2s  (ui128_t x, char *str, size_t len, int base);

/* arithmetical operations */
extern ui128_t        ui128_add  (ui128_t x, ui128_t y, ui128_t *ov);
extern ui128_t        ui128_addn (ui128_t x, int     y, int     *ov);
extern ui128_t        ui128_sub  (ui128_t x, ui128_t y, ui128_t *ov);
extern ui128_t        ui128_subn (ui128_t x, int     y, int     *ov);
extern ui128_t        ui128_mul  (ui128_t x, ui128_t y, ui128_t *ov);
extern ui128_t        ui128_muln (ui128_t x, int     y, int     *ov);
extern ui128_t        ui128_div  (ui128_t x, ui128_t y, ui128_t *ov);
extern ui128_t        ui128_divn (ui128_t x, int     y, int     *ov);

/* bit operations */
extern ui128_t        ui128_and  (ui128_t x, ui128_t y);
extern ui128_t        ui128_or   (ui128_t x, ui128_t y);
extern ui128_t        ui128_xor  (ui128_t x, ui128_t y);
extern ui128_t        ui128_not  (ui128_t x);
extern ui128_t        ui128_rol  (ui128_t x, int s, ui128_t *ov);
extern ui128_t        ui128_ror  (ui128_t x, int s, ui128_t *ov);

/* other operations */
extern int            ui128_len  (ui128_t x);
extern int            ui128_cmp  (ui128_t x, ui128_t y);

#endif /* __UI128_H__ */

/*
**  uuid_str.h: string formatting functions
*/

#ifndef __UUID_STR_H__
#define __UUID_STR_H__

#define STR_PREFIX uuid_

/* embedding support */
#ifdef STR_PREFIX
#if defined(__STDC__) || defined(__cplusplus)
#define __STR_CONCAT(x,y) x ## y
#define STR_CONCAT(x,y) __STR_CONCAT(x,y)
#else
#define __STR_CONCAT(x) x
#define STR_CONCAT(x,y) __STR_CONCAT(x)y
#endif
#define str_vsnprintf  STR_CONCAT(STR_PREFIX,str_vsnprintf)
#define str_snprintf   STR_CONCAT(STR_PREFIX,str_snprintf)
#define str_vrsprintf  STR_CONCAT(STR_PREFIX,str_vrsprintf)
#define str_rsprintf   STR_CONCAT(STR_PREFIX,str_rsprintf)
#define str_vasprintf  STR_CONCAT(STR_PREFIX,str_vasprintf)
#define str_asprintf   STR_CONCAT(STR_PREFIX,str_asprintf)
#endif

extern int   str_vsnprintf (char  *, size_t, const char *, va_list);
extern int   str_snprintf  (char  *, size_t, const char *, ...);
extern int   str_vrsprintf (char **,         const char *, va_list);
extern int   str_rsprintf  (char **,         const char *, ...);
extern char *str_vasprintf (                 const char *, va_list);
extern char *str_asprintf  (                 const char *, ...);

#endif /* __UUID_STR_H__ */

