/*
**  OSSP uuid - Universally Unique Identifier
**  Copyright (c) 2004-2008 Ralf S. Engelschall <rse@engelschall.com>
**  Copyright (c) 2004-2008 The OSSP Project <http://www.ossp.org/>
**
**  OSSP uuid is a library for the generation
**  of UUIDs which can found at http://www.ossp.org/pkg/lib/uuid/
**
**  Permission to use, copy, modify, and distribute this software for
**  any purpose with or without fee is hereby granted, provided that
**  the above copyright notice and this permission notice appear in all
**  copies.
**
**  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
**  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
**  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
**  IN NO EVENT SHALL THE AUTHORS AND COPYRIGHT HOLDERS AND THEIR
**  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
**  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
**  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
**  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
**  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
**  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
**  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
**  SUCH DAMAGE.
*/

/* uuid.c: library API implementation */

/* system headers */
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif
#ifdef HAVE_NET_IF_DL_H
#include <net/if_dl.h>
#endif
#ifdef HAVE_NET_IF_ARP_H
#include <net/if_arp.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif


#if defined(WIN32)
#define WINVER 0x0500
#include <windows.h>
#include <wincrypt.h>
#endif


/* own headers (part 2/2) */
#include "uuid_all.h"

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE (/*lint -save -e506*/ !FALSE /*lint -restore*/)
#endif


/* maximum number of 100ns ticks of the actual resolution of system clock
   (which in our case is 1us (= 1000ns) because we use gettimeofday(2) */
#define UUIDS_PER_TICK 10

/* time offset between UUID and Unix Epoch time according to standards.
   (UUID UTC base time is October 15, 1582
    Unix UTC base time is January  1, 1970) */
#define UUID_TIMEOFFSET "01B21DD213814000"

/* IEEE 802 MAC address encoding/decoding bit fields */
#define IEEE_MAC_MCBIT BM_OCTET(0,0,0,0,0,0,0,1)
#define IEEE_MAC_LOBIT BM_OCTET(0,0,0,0,0,0,1,0)

/* IEEE 802 MAC address octet length */
#define IEEE_MAC_OCTETS 6

/* UUID binary representation according to UUID standards */
typedef struct {
    uuid_uint32_t  time_low;                  /* bits  0-31 of time field */
    uuid_uint16_t  time_mid;                  /* bits 32-47 of time field */
    uuid_uint16_t  time_hi_and_version;       /* bits 48-59 of time field plus 4 bit version */
    uuid_uint8_t   clock_seq_hi_and_reserved; /* bits  8-13 of clock sequence field plus 2 bit variant */
    uuid_uint8_t   clock_seq_low;             /* bits  0-7  of clock sequence field */
    uuid_uint8_t   node[IEEE_MAC_OCTETS];     /* bits  0-47 of node MAC address */
} uuid_obj_t;

/* abstract data type (ADT) of API */
struct uuid_st {
    uuid_obj_t     obj;                       /* inlined UUID object */
    prng_t        *prng;                      /* RPNG sub-object */
    md5_t         *md5;                       /* MD5 sub-object */
    sha1_t        *sha1;                      /* SHA-1 sub-object */
    uuid_uint8_t   mac[IEEE_MAC_OCTETS];      /* pre-determined MAC address */
    struct timeval time_last;                 /* last retrieved timestamp */
    unsigned long  time_seq;                  /* last timestamp sequence counter */
};

/* create UUID object */
uuid_rc_t uuid_create(uuid_t **uuid)
{
    uuid_t *obj;

    /* argument sanity check */
    if (uuid == NULL)
        return UUID_RC_ARG;

    /* allocate UUID object */
    if ((obj = (uuid_t *)malloc(sizeof(uuid_t))) == NULL)
        return UUID_RC_MEM;

    /* create PRNG, MD5 and SHA1 sub-objects */
    if (prng_create(&obj->prng) != PRNG_RC_OK) {
        free(obj);
        return UUID_RC_INT;
    }
    if (md5_create(&obj->md5) != MD5_RC_OK) {
        (void)prng_destroy(obj->prng);
        free(obj);
        return UUID_RC_INT;
    }
    if (sha1_create(&obj->sha1) != SHA1_RC_OK) {
        (void)md5_destroy(obj->md5);
        (void)prng_destroy(obj->prng);
        free(obj);
        return UUID_RC_INT;
    }

    /* set UUID object initially to "Nil UUID" */
    if (uuid_load(obj, "nil") != UUID_RC_OK) {
        (void)sha1_destroy(obj->sha1);
        (void)md5_destroy(obj->md5);
        (void)prng_destroy(obj->prng);
        free(obj);
        return UUID_RC_INT;
    }

    /* resolve MAC address for insertion into node field of UUIDs */
    if (!mac_address((unsigned char *)(obj->mac), sizeof(obj->mac))) {
        memset(obj->mac, 0, sizeof(obj->mac));
        obj->mac[0] = BM_OCTET(1,0,0,0,0,0,0,0);
    }

    /* initialize time attributes */
    obj->time_last.tv_sec  = 0;
    obj->time_last.tv_usec = 0;
    obj->time_seq = 0;

    /* store result object */
    *uuid = obj;

    return UUID_RC_OK;
}

/* destroy UUID object */
uuid_rc_t uuid_destroy(uuid_t *uuid)
{
    /* argument sanity check */
    if (uuid == NULL)
        return UUID_RC_ARG;

    /* destroy PRNG, MD5 and SHA-1 sub-objects */
    (void)prng_destroy(uuid->prng);
    (void)md5_destroy(uuid->md5);
    (void)sha1_destroy(uuid->sha1);

    /* free UUID object */
    free(uuid);

    return UUID_RC_OK;
}

/* clone UUID object */
uuid_rc_t uuid_clone(const uuid_t *uuid, uuid_t **clone)
{
    uuid_t *obj;

    /* argument sanity check */
    if (uuid == NULL || uuid_clone == NULL)
        return UUID_RC_ARG;

    /* allocate UUID object */
    if ((obj = (uuid_t *)malloc(sizeof(uuid_t))) == NULL)
        return UUID_RC_MEM;

    /* clone entire internal state */
    memcpy(obj, uuid, sizeof(uuid_t));

    /* re-initialize with new PRNG, MD5 and SHA1 sub-objects */
    if (prng_create(&obj->prng) != PRNG_RC_OK) {
        free(obj);
        return UUID_RC_INT;
    }
    if (md5_create(&obj->md5) != MD5_RC_OK) {
        (void)prng_destroy(obj->prng);
        free(obj);
        return UUID_RC_INT;
    }
    if (sha1_create(&obj->sha1) != SHA1_RC_OK) {
        (void)md5_destroy(obj->md5);
        (void)prng_destroy(obj->prng);
        free(obj);
        return UUID_RC_INT;
    }

    /* store result object */
    *clone = obj;

    return UUID_RC_OK;
}

/* check whether UUID object represents "Nil UUID" */
uuid_rc_t uuid_isnil(const uuid_t *uuid, int *result)
{
    const unsigned char *ucp;
    int i;

    /* sanity check argument(s) */
    if (uuid == NULL || result == NULL)
        return UUID_RC_ARG;

    /* a "Nil UUID" is defined as all octets zero, so check for this case */
    *result = UUID_TRUE;
    for (i = 0, ucp = (unsigned char *)&(uuid->obj); i < UUID_LEN_BIN; i++) {
        if (*ucp++ != (unsigned char)'\0') {
            *result = UUID_FALSE;
            break;
        }
    }

    return UUID_RC_OK;
}

/* compare UUID objects */
uuid_rc_t uuid_compare(const uuid_t *uuid1, const uuid_t *uuid2, int *result)
{
    int r;

    /* argument sanity check */
    if (result == NULL)
        return UUID_RC_ARG;

    /* convenience macro for setting result */
#define RESULT(r) \
    /*lint -save -e801 -e717*/ \
    do { \
        *result = (r); \
        goto result_exit; \
    } while (0) \
    /*lint -restore*/

    /* special cases: NULL or equal UUIDs */
    if (uuid1 == uuid2)
        RESULT(0);
    if (uuid1 == NULL && uuid2 == NULL)
        RESULT(0);
    if (uuid1 == NULL)
        RESULT((uuid_isnil(uuid2, &r) == UUID_RC_OK ? r : 0) ? 0 : -1);
    if (uuid2 == NULL)
        RESULT((uuid_isnil(uuid1, &r) == UUID_RC_OK ? r : 0) ? 0 : 1);

    /* standard cases: regular different UUIDs */
    if (uuid1->obj.time_low != uuid2->obj.time_low)
        RESULT((uuid1->obj.time_low < uuid2->obj.time_low) ? -1 : 1);
    if ((r = (int)uuid1->obj.time_mid
           - (int)uuid2->obj.time_mid) != 0)
        RESULT((r < 0) ? -1 : 1);
    if ((r = (int)uuid1->obj.time_hi_and_version
           - (int)uuid2->obj.time_hi_and_version) != 0)
        RESULT((r < 0) ? -1 : 1);
    if ((r = (int)uuid1->obj.clock_seq_hi_and_reserved
           - (int)uuid2->obj.clock_seq_hi_and_reserved) != 0)
        RESULT((r < 0) ? -1 : 1);
    if ((r = (int)uuid1->obj.clock_seq_low
           - (int)uuid2->obj.clock_seq_low) != 0)
        RESULT((r < 0) ? -1 : 1);
    if ((r = memcmp(uuid1->obj.node, uuid2->obj.node, sizeof(uuid1->obj.node))) != 0)
        RESULT((r < 0) ? -1 : 1);

    /* default case: the keys are equal */
    *result = 0;

    result_exit:
    return UUID_RC_OK;
}

/* INTERNAL: unpack UUID binary presentation into UUID object
   (allows in-place operation for internal efficiency!) */
static uuid_rc_t uuid_import_bin(uuid_t *uuid, const void *data_ptr, size_t data_len)
{
    const uuid_uint8_t *in;
    uuid_uint32_t tmp32;
    uuid_uint16_t tmp16;
    unsigned int i;

    /* sanity check argument(s) */
    if (uuid == NULL || data_ptr == NULL || data_len < UUID_LEN_BIN)
        return UUID_RC_ARG;

    /* treat input data buffer as octet stream */
    in = (const uuid_uint8_t *)data_ptr;

    /* unpack "time_low" field */
    tmp32 = (uuid_uint32_t)(*in++);
    tmp32 = (tmp32 << 8) | (uuid_uint32_t)(*in++);
    tmp32 = (tmp32 << 8) | (uuid_uint32_t)(*in++);
    tmp32 = (tmp32 << 8) | (uuid_uint32_t)(*in++);
    uuid->obj.time_low = tmp32;

    /* unpack "time_mid" field */
    tmp16 = (uuid_uint16_t)(*in++);
    tmp16 = (uuid_uint16_t)(tmp16 << 8) | (uuid_uint16_t)(*in++);
    uuid->obj.time_mid = tmp16;

    /* unpack "time_hi_and_version" field */
    tmp16 = (uuid_uint16_t)*in++;
    tmp16 = (uuid_uint16_t)(tmp16 << 8) | (uuid_uint16_t)(*in++);
    uuid->obj.time_hi_and_version = tmp16;

    /* unpack "clock_seq_hi_and_reserved" field */
    uuid->obj.clock_seq_hi_and_reserved = *in++;

    /* unpack "clock_seq_low" field */
    uuid->obj.clock_seq_low = *in++;

    /* unpack "node" field */
    for (i = 0; i < (unsigned int)sizeof(uuid->obj.node); i++)
        uuid->obj.node[i] = *in++;

    return UUID_RC_OK;
}

/* INTERNAL: pack UUID object into binary representation
   (allows in-place operation for internal efficiency!) */
static uuid_rc_t uuid_export_bin(const uuid_t *uuid, void *_data_ptr, size_t *data_len)
{
    uuid_uint8_t **data_ptr;
    uuid_uint8_t *out;
    uuid_uint32_t tmp32;
    uuid_uint16_t tmp16;
    unsigned int i;

    /* cast generic data pointer to particular pointer to pointer type */
    data_ptr = (uuid_uint8_t **)_data_ptr;

    /* sanity check argument(s) */
    if (uuid == NULL || data_ptr == NULL)
        return UUID_RC_ARG;

    /* optionally allocate octet data buffer */
    if (*data_ptr == NULL) {
        if ((*data_ptr = (uuid_uint8_t *)malloc(sizeof(uuid_t))) == NULL)
            return UUID_RC_MEM;
        if (data_len != NULL)
            *data_len = UUID_LEN_BIN;
    }
    else {
        if (data_len == NULL)
            return UUID_RC_ARG;
        if (*data_len < UUID_LEN_BIN)
            return UUID_RC_MEM;
        *data_len = UUID_LEN_BIN;
    }

    /* treat output data buffer as octet stream */
    out = *data_ptr;

    /* pack "time_low" field */
    tmp32 = uuid->obj.time_low;
    out[3] = (uuid_uint8_t)(tmp32 & 0xff); tmp32 >>= 8;
    out[2] = (uuid_uint8_t)(tmp32 & 0xff); tmp32 >>= 8;
    out[1] = (uuid_uint8_t)(tmp32 & 0xff); tmp32 >>= 8;
    out[0] = (uuid_uint8_t)(tmp32 & 0xff);

    /* pack "time_mid" field */
    tmp16 = uuid->obj.time_mid;
    out[5] = (uuid_uint8_t)(tmp16 & 0xff); tmp16 >>= 8;
    out[4] = (uuid_uint8_t)(tmp16 & 0xff);

    /* pack "time_hi_and_version" field */
    tmp16 = uuid->obj.time_hi_and_version;
    out[7] = (uuid_uint8_t)(tmp16 & 0xff); tmp16 >>= 8;
    out[6] = (uuid_uint8_t)(tmp16 & 0xff);

    /* pack "clock_seq_hi_and_reserved" field */
    out[8] = uuid->obj.clock_seq_hi_and_reserved;

    /* pack "clock_seq_low" field */
    out[9] = uuid->obj.clock_seq_low;

    /* pack "node" field */
    for (i = 0; i < (unsigned int)sizeof(uuid->obj.node); i++)
        out[10+i] = uuid->obj.node[i];

    return UUID_RC_OK;
}

/* INTERNAL: check for valid UUID string representation syntax */
static int uuid_isstr(const char *str, size_t str_len)
{
    int i;
    const char *cp;

    /* example reference:
       f81d4fae-7dec-11d0-a765-00a0c91e6bf6
       012345678901234567890123456789012345
       0         1         2         3       */
    if (str == NULL)
        return UUID_FALSE;
    if (str_len == 0)
        str_len = strlen(str);
    if (str_len < UUID_LEN_STR)
        return UUID_FALSE;
    for (i = 0, cp = str; i < UUID_LEN_STR; i++, cp++) {
        if ((i == 8) || (i == 13) || (i == 18) || (i == 23)) {
            if (*cp == '-')
                continue;
            else
                return UUID_FALSE;
        }
        if (!isxdigit((int)(*cp)))
            return UUID_FALSE;
    }
    return UUID_TRUE;
}

/* INTERNAL: import UUID object from string representation */
static uuid_rc_t uuid_import_str(uuid_t *uuid, const void *data_ptr, size_t data_len)
{
    uuid_uint16_t tmp16;
    const char *cp;
    char hexbuf[3];
    const char *str;
    unsigned int i;

    /* sanity check argument(s) */
    if (uuid == NULL || data_ptr == NULL || data_len < UUID_LEN_STR)
        return UUID_RC_ARG;

    /* check for correct UUID string representation syntax */
    str = (const char *)data_ptr;
    if (!uuid_isstr(str, 0))
        return UUID_RC_ARG;

    /* parse hex values of "time" parts */
    uuid->obj.time_low            = (uuid_uint32_t)strtoul(str,    NULL, 16);
    uuid->obj.time_mid            = (uuid_uint16_t)strtoul(str+9,  NULL, 16);
    uuid->obj.time_hi_and_version = (uuid_uint16_t)strtoul(str+14, NULL, 16);

    /* parse hex values of "clock" parts */
    tmp16 = (uuid_uint16_t)strtoul(str+19, NULL, 16);
    uuid->obj.clock_seq_low             = (uuid_uint8_t)(tmp16 & 0xff); tmp16 >>= 8;
    uuid->obj.clock_seq_hi_and_reserved = (uuid_uint8_t)(tmp16 & 0xff);

    /* parse hex values of "node" part */
    cp = str+24;
    hexbuf[2] = '\0';
    for (i = 0; i < (unsigned int)sizeof(uuid->obj.node); i++) {
        hexbuf[0] = *cp++;
        hexbuf[1] = *cp++;
        uuid->obj.node[i] = (uuid_uint8_t)strtoul(hexbuf, NULL, 16);
    }

    return UUID_RC_OK;
}

/* INTERNAL: import UUID object from single integer value representation */
static uuid_rc_t uuid_import_siv(uuid_t *uuid, const void *data_ptr, size_t data_len)
{
    const char *str;
    uuid_uint8_t tmp_bin[UUID_LEN_BIN];
    ui128_t ui, ui2;
    uuid_rc_t rc;
    int i;

    /* sanity check argument(s) */
    if (uuid == NULL || data_ptr == NULL || data_len < 1)
        return UUID_RC_ARG;

    /* check for correct UUID single integer value syntax */
    str = (const char *)data_ptr;
    for (i = 0; i < (int)data_len; i++)
        if (!isdigit((int)str[i]))
            return UUID_RC_ARG;

    /* parse single integer value representation (SIV) */
    ui = ui128_s2i(str, NULL, 10);

    /* import octets into UUID binary representation */
    for (i = 0; i < UUID_LEN_BIN; i++) {
        ui = ui128_rol(ui, 8, &ui2);
        tmp_bin[i] = (uuid_uint8_t)(ui128_i2n(ui2) & 0xff);
    }

    /* import into internal UUID representation */
    if ((rc = uuid_import(uuid, UUID_FMT_BIN, (void *)&tmp_bin, UUID_LEN_BIN)) != UUID_RC_OK)
        return rc;

    return UUID_RC_OK;
}

/* INTERNAL: export UUID object to string representation */
static uuid_rc_t uuid_export_str(const uuid_t *uuid, void *_data_ptr, size_t *data_len)
{
    char **data_ptr;
    char *data_buf;

    /* cast generic data pointer to particular pointer to pointer type */
    data_ptr = (char **)_data_ptr;

    /* sanity check argument(s) */
    if (uuid == NULL || data_ptr == NULL)
        return UUID_RC_ARG;

    /* determine output buffer */
    if (*data_ptr == NULL) {
        if ((data_buf = (char *)malloc(UUID_LEN_STR+1)) == NULL)
            return UUID_RC_MEM;
        if (data_len != NULL)
            *data_len = UUID_LEN_STR+1;
    }
    else {
        data_buf = (char *)(*data_ptr);
        if (data_len == NULL)
            return UUID_RC_ARG;
        if (*data_len < UUID_LEN_STR+1)
            return UUID_RC_MEM;
        *data_len = UUID_LEN_STR+1;
    }

    /* format UUID into string representation */
    if (str_snprintf(data_buf, UUID_LEN_STR+1,
        "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        (unsigned long)uuid->obj.time_low,
        (unsigned int)uuid->obj.time_mid,
        (unsigned int)uuid->obj.time_hi_and_version,
        (unsigned int)uuid->obj.clock_seq_hi_and_reserved,
        (unsigned int)uuid->obj.clock_seq_low,
        (unsigned int)uuid->obj.node[0],
        (unsigned int)uuid->obj.node[1],
        (unsigned int)uuid->obj.node[2],
        (unsigned int)uuid->obj.node[3],
        (unsigned int)uuid->obj.node[4],
        (unsigned int)uuid->obj.node[5]) != UUID_LEN_STR) {
        if (*data_ptr == NULL)
            free(data_buf);
        return UUID_RC_INT;
    }

    /* pass back new buffer if locally allocated */
    if (*data_ptr == NULL)
        *data_ptr = data_buf;

    return UUID_RC_OK;
}

/* INTERNAL: export UUID object to single integer value representation */
static uuid_rc_t uuid_export_siv(const uuid_t *uuid, void *_data_ptr, size_t *data_len)
{
    char **data_ptr;
    char *data_buf;
    void *tmp_ptr;
    size_t tmp_len;
    uuid_uint8_t tmp_bin[UUID_LEN_BIN];
    ui128_t ui, ui2;
    uuid_rc_t rc;
    int i;

    /* cast generic data pointer to particular pointer to pointer type */
    data_ptr = (char **)_data_ptr;

    /* sanity check argument(s) */
    if (uuid == NULL || data_ptr == NULL)
        return UUID_RC_ARG;

    /* determine output buffer */
    if (*data_ptr == NULL) {
        if ((data_buf = (char *)malloc(UUID_LEN_SIV+1)) == NULL)
            return UUID_RC_MEM;
        if (data_len != NULL)
            *data_len = UUID_LEN_SIV+1;
    }
    else {
        data_buf = (char *)(*data_ptr);
        if (data_len == NULL)
            return UUID_RC_ARG;
        if (*data_len < UUID_LEN_SIV+1)
            return UUID_RC_MEM;
        *data_len = UUID_LEN_SIV+1;
    }

    /* export into UUID binary representation */
    tmp_ptr = (void *)&tmp_bin;
    tmp_len = sizeof(tmp_bin);
    if ((rc = uuid_export(uuid, UUID_FMT_BIN, &tmp_ptr, &tmp_len)) != UUID_RC_OK) {
        if (*data_ptr == NULL)
            free(data_buf);
        return rc;
    }

    /* import from UUID binary representation */
    ui = ui128_zero();
    for (i = 0; i < UUID_LEN_BIN; i++) {
        ui2 = ui128_n2i((unsigned long)tmp_bin[i]);
        ui = ui128_rol(ui, 8, NULL);
        ui = ui128_or(ui, ui2);
    }

    /* format into single integer value representation */
    (void)ui128_i2s(ui, data_buf, UUID_LEN_SIV+1, 10);

    /* pass back new buffer if locally allocated */
    if (*data_ptr == NULL)
        *data_ptr = data_buf;

    return UUID_RC_OK;
}

/* decoding tables */
static struct {
    uuid_uint8_t num;
    const char *desc;
} uuid_dectab_variant[] = {
    { (uuid_uint8_t)BM_OCTET(0,0,0,0,0,0,0,0), "reserved (NCS backward compatible)" },
    { (uuid_uint8_t)BM_OCTET(1,0,0,0,0,0,0,0), "DCE 1.1, ISO/IEC 11578:1996" },
    { (uuid_uint8_t)BM_OCTET(1,1,0,0,0,0,0,0), "reserved (Microsoft GUID)" },
    { (uuid_uint8_t)BM_OCTET(1,1,1,0,0,0,0,0), "reserved (future use)" }
};
static struct {
    int num;
    const char *desc;
} uuid_dectab_version[] = {
    { 1, "time and node based" },
    { 3, "name based, MD5" },
    { 4, "random data based" },
    { 5, "name based, SHA-1" }
};

/* INTERNAL: dump UUID object as descriptive text */
static uuid_rc_t uuid_export_txt(const uuid_t *uuid, void *_data_ptr, size_t *data_len)
{
    char **data_ptr;
    uuid_rc_t rc;
    char **out;
    char *out_ptr;
    size_t out_len;
    const char *version;
    const char *variant;
    char *content;
    int isnil;
    uuid_uint8_t tmp8;
    uuid_uint16_t tmp16;
    uuid_uint32_t tmp32;
    uuid_uint8_t tmp_bin[UUID_LEN_BIN];
    char tmp_str[UUID_LEN_STR+1];
    char tmp_siv[UUID_LEN_SIV+1];
    void *tmp_ptr;
    size_t tmp_len;
    ui64_t t;
    ui64_t t_offset;
    int t_nsec;
    int t_usec;
    time_t t_sec;
    char t_buf[19+1]; /* YYYY-MM-DD HH:MM:SS */
    struct tm *tm;
    int i;

    /* cast generic data pointer to particular pointer to pointer type */
    data_ptr = (char **)_data_ptr;

    /* sanity check argument(s) */
    if (uuid == NULL || data_ptr == NULL)
        return UUID_RC_ARG;

    /* initialize output buffer */
    out_ptr = NULL;
    out = &out_ptr;

    /* check for special case of "Nil UUID" */
    if ((rc = uuid_isnil(uuid, &isnil)) != UUID_RC_OK)
        return rc;

    /* decode into various representations */
    tmp_ptr = (void *)&tmp_str;
    tmp_len = sizeof(tmp_str);
    if ((rc = uuid_export(uuid, UUID_FMT_STR, &tmp_ptr, &tmp_len)) != UUID_RC_OK)
        return rc;
    tmp_ptr = (void *)&tmp_siv;
    tmp_len = sizeof(tmp_siv);
    if ((rc = uuid_export(uuid, UUID_FMT_SIV, &tmp_ptr, &tmp_len)) != UUID_RC_OK)
        return rc;
    (void)str_rsprintf(out, "encode: STR:     %s\n", tmp_str);
    (void)str_rsprintf(out, "        SIV:     %s\n", tmp_siv);

    /* decode UUID variant */
    tmp8 = uuid->obj.clock_seq_hi_and_reserved;
    if (isnil)
        variant = "n.a.";
    else {
        variant = "unknown";
        for (i = 7; i >= 0; i--) {
            if ((tmp8 & (uuid_uint8_t)BM_BIT(i,1)) == 0) {
                tmp8 &= ~(uuid_uint8_t)BM_MASK(i,0);
                break;
            }
        }
        for (i = 0; i < (int)(sizeof(uuid_dectab_variant)/sizeof(uuid_dectab_variant[0])); i++) {
            if (uuid_dectab_variant[i].num == tmp8) {
                variant = uuid_dectab_variant[i].desc;
                break;
            }
        }
    }
    (void)str_rsprintf(out, "decode: variant: %s\n", variant);

    /* decode UUID version */
    tmp16 = (BM_SHR(uuid->obj.time_hi_and_version, 12) & (uuid_uint16_t)BM_MASK(3,0));
    if (isnil)
        version = "n.a.";
    else {
        version = "unknown";
        for (i = 0; i < (int)(sizeof(uuid_dectab_version)/sizeof(uuid_dectab_version[0])); i++) {
            if (uuid_dectab_version[i].num == (int)tmp16) {
                version = uuid_dectab_version[i].desc;
                break;
            }
        }
    }
    str_rsprintf(out, "        version: %d (%s)\n", (int)tmp16, version);

    /*
     * decode UUID content
     */

    if (tmp8 == BM_OCTET(1,0,0,0,0,0,0,0) && tmp16 == 1) {
        /* decode DCE 1.1 version 1 UUID */

        /* decode system time */
        t = ui64_rol(ui64_n2i((unsigned long)(uuid->obj.time_hi_and_version & BM_MASK(11,0))), 48, NULL),
        t = ui64_or(t, ui64_rol(ui64_n2i((unsigned long)(uuid->obj.time_mid)), 32, NULL));
        t = ui64_or(t, ui64_n2i((unsigned long)(uuid->obj.time_low)));
        t_offset = ui64_s2i(UUID_TIMEOFFSET, NULL, 16);
        t = ui64_sub(t, t_offset, NULL);
        t = ui64_divn(t, 10, &t_nsec);
        t = ui64_divn(t, 1000000, &t_usec);
        t_sec = (time_t)ui64_i2n(t);
        tm = gmtime(&t_sec);
        (void)strftime(t_buf, sizeof(t_buf), "%Y-%m-%d %H:%M:%S", tm);
        (void)str_rsprintf(out, "        content: time:  %s.%06d.%d UTC\n", t_buf, t_usec, t_nsec);

        /* decode clock sequence */
        tmp32 = ((uuid->obj.clock_seq_hi_and_reserved & BM_MASK(5,0)) << 8)
                + uuid->obj.clock_seq_low;
        (void)str_rsprintf(out, "                 clock: %ld (usually random)\n", (long)tmp32);

        /* decode node MAC address */
        (void)str_rsprintf(out, "                 node:  %02x:%02x:%02x:%02x:%02x:%02x (%s %s)\n",
            (unsigned int)uuid->obj.node[0],
            (unsigned int)uuid->obj.node[1],
            (unsigned int)uuid->obj.node[2],
            (unsigned int)uuid->obj.node[3],
            (unsigned int)uuid->obj.node[4],
            (unsigned int)uuid->obj.node[5],
            (uuid->obj.node[0] & IEEE_MAC_LOBIT ? "local" : "global"),
            (uuid->obj.node[0] & IEEE_MAC_MCBIT ? "multicast" : "unicast"));
    }
    else {
        /* decode anything else as hexadecimal byte-string only */

        /* determine annotational hint */
        content = "not decipherable: unknown UUID version";
        if (isnil)
            content = "special case: DCE 1.1 Nil UUID";
        else if (tmp16 == 3)
            content = "not decipherable: MD5 message digest only";
        else if (tmp16 == 4)
            content = "no semantics: random data only";
        else if (tmp16 == 5)
            content = "not decipherable: truncated SHA-1 message digest only";

        /* pack UUID into binary representation */
        tmp_ptr = (void *)&tmp_bin;
        tmp_len = sizeof(tmp_bin);
        if ((rc = uuid_export(uuid, UUID_FMT_BIN, &tmp_ptr, &tmp_len)) != UUID_RC_OK)
            return rc;

        /* mask out version and variant parts */
        tmp_bin[6] &= BM_MASK(3,0);
        tmp_bin[8] &= BM_MASK(5,0);

        /* dump as colon-seperated hexadecimal byte-string */
        (void)str_rsprintf(out,
            "        content: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n"
            "                 (%s)\n",
            (unsigned int)tmp_bin[0],  (unsigned int)tmp_bin[1],  (unsigned int)tmp_bin[2],
            (unsigned int)tmp_bin[3],  (unsigned int)tmp_bin[4],  (unsigned int)tmp_bin[5],
            (unsigned int)tmp_bin[6],  (unsigned int)tmp_bin[7],  (unsigned int)tmp_bin[8],
            (unsigned int)tmp_bin[9],  (unsigned int)tmp_bin[10], (unsigned int)tmp_bin[11],
            (unsigned int)tmp_bin[12], (unsigned int)tmp_bin[13], (unsigned int)tmp_bin[14],
            (unsigned int)tmp_bin[15], content);
    }

    /* provide result */
    out_len = strlen(out_ptr)+1;
    if (*data_ptr == NULL) {
        *data_ptr = (void *)out_ptr;
        if (data_len != NULL)
            *data_len = out_len;
    }
    else {
        if (data_len == NULL)
            return UUID_RC_ARG;
        if (*data_len < out_len)
            return UUID_RC_MEM;
        memcpy(*data_ptr, out_ptr, out_len);
    }

    return UUID_RC_OK;
}

/* UUID importing */
uuid_rc_t uuid_import(uuid_t *uuid, uuid_fmt_t fmt, const void *data_ptr, size_t data_len)
{
    uuid_rc_t rc;

    /* sanity check argument(s) */
    if (uuid == NULL || data_ptr == NULL)
        return UUID_RC_ARG;

    /* dispatch into format-specific functions */
    switch (fmt) {
        case UUID_FMT_BIN: rc = uuid_import_bin(uuid, data_ptr, data_len); break;
        case UUID_FMT_STR: rc = uuid_import_str(uuid, data_ptr, data_len); break;
        case UUID_FMT_SIV: rc = uuid_import_siv(uuid, data_ptr, data_len); break;
        case UUID_FMT_TXT: rc = UUID_RC_IMP; /* not implemented */ break;
        default:           rc = UUID_RC_ARG;
    }

    return rc;
}

/* UUID exporting */
uuid_rc_t uuid_export(const uuid_t *uuid, uuid_fmt_t fmt, void *data_ptr, size_t *data_len)
{
    uuid_rc_t rc;

    /* sanity check argument(s) */
    if (uuid == NULL || data_ptr == NULL)
        return UUID_RC_ARG;

    /* dispatch into format-specific functions */
    switch (fmt) {
        case UUID_FMT_BIN: rc = uuid_export_bin(uuid, data_ptr, data_len); break;
        case UUID_FMT_STR: rc = uuid_export_str(uuid, data_ptr, data_len); break;
        case UUID_FMT_SIV: rc = uuid_export_siv(uuid, data_ptr, data_len); break;
        case UUID_FMT_TXT: rc = uuid_export_txt(uuid, data_ptr, data_len); break;
        default:           rc = UUID_RC_ARG;
    }

    return rc;
}

/* INTERNAL: brand UUID with version and variant */
static void uuid_brand(uuid_t *uuid, unsigned int version)
{
    /* set version (as given) */
    uuid->obj.time_hi_and_version &= BM_MASK(11,0);
    uuid->obj.time_hi_and_version |= (uuid_uint16_t)BM_SHL(version, 12);

    /* set variant (always DCE 1.1 only) */
    uuid->obj.clock_seq_hi_and_reserved &= BM_MASK(5,0);
    uuid->obj.clock_seq_hi_and_reserved |= BM_SHL(0x02, 6);
    return;
}

/* INTERNAL: generate UUID version 1: time, clock and node based */
static uuid_rc_t uuid_make_v1(uuid_t *uuid, unsigned int mode, va_list ap)
{
    struct timeval time_now;
    ui64_t t;
    ui64_t offset;
    ui64_t ov;
    uuid_uint16_t clck;

    /*
     *  GENERATE TIME
     */

    /* determine current system time and sequence counter */
    for (;;) {
        /* determine current system time */
        if (time_gettimeofday(&time_now) == -1)
            return UUID_RC_SYS;

        /* check whether system time changed since last retrieve */
        if (!(   time_now.tv_sec  == uuid->time_last.tv_sec
              && time_now.tv_usec == uuid->time_last.tv_usec)) {
            /* reset time sequence counter and continue */
            uuid->time_seq = 0;
            break;
        }

        /* until we are out of UUIDs per tick, increment
           the time/tick sequence counter and continue */
        if (uuid->time_seq < UUIDS_PER_TICK) {
            uuid->time_seq++;
            break;
        }

        /* stall the UUID generation until the system clock (which
           has a gettimeofday(2) resolution of 1us) catches up */
        time_usleep(1);
    }

    /* convert from timeval (sec,usec) to OSSP ui64 (100*nsec) format */
    t = ui64_n2i((unsigned long)time_now.tv_sec);
    t = ui64_muln(t, 1000000, NULL);
    t = ui64_addn(t, (int)time_now.tv_usec, NULL);
    t = ui64_muln(t, 10, NULL);

    /* adjust for offset between UUID and Unix Epoch time */
    offset = ui64_s2i(UUID_TIMEOFFSET, NULL, 16);
    t = ui64_add(t, offset, NULL);

    /* compensate for low resolution system clock by adding
       the time/tick sequence counter */
    if (uuid->time_seq > 0)
        t = ui64_addn(t, (int)uuid->time_seq, NULL);

    /* store the 60 LSB of the time in the UUID */
    t = ui64_rol(t, 16, &ov);
    uuid->obj.time_hi_and_version =
        (uuid_uint16_t)(ui64_i2n(ov) & 0x00000fff); /* 12 of 16 bit only! */
    t = ui64_rol(t, 16, &ov);
    uuid->obj.time_mid =
        (uuid_uint16_t)(ui64_i2n(ov) & 0x0000ffff); /* all 16 bit */
    t = ui64_rol(t, 32, &ov);
    uuid->obj.time_low =
        (uuid_uint32_t)(ui64_i2n(ov) & 0xffffffff); /* all 32 bit */

    /*
     *  GENERATE CLOCK
     */

    /* retrieve current clock sequence */
    clck = ((uuid->obj.clock_seq_hi_and_reserved & BM_MASK(5,0)) << 8)
           + uuid->obj.clock_seq_low;

    /* generate new random clock sequence (initially or if the
       time has stepped backwards) or else just increase it */
    if (   clck == 0
        || (   time_now.tv_sec < uuid->time_last.tv_sec
            || (   time_now.tv_sec == uuid->time_last.tv_sec
                && time_now.tv_usec < uuid->time_last.tv_usec))) {
        if (prng_data(uuid->prng, (void *)&clck, sizeof(clck)) != PRNG_RC_OK)
            return UUID_RC_INT;
    }
    else
        clck++;
    clck %= BM_POW2(14);

    /* store back new clock sequence */
    uuid->obj.clock_seq_hi_and_reserved =
        (uuid->obj.clock_seq_hi_and_reserved & BM_MASK(7,6))
        | (uuid_uint8_t)((clck >> 8) & 0xff);
    uuid->obj.clock_seq_low =
        (uuid_uint8_t)(clck & 0xff);

    /*
     *  GENERATE NODE
     */

    if ((mode & UUID_MAKE_MC) || (uuid->mac[0] & BM_OCTET(1,0,0,0,0,0,0,0))) {
        /* generate random IEEE 802 local multicast MAC address */
        if (prng_data(uuid->prng, (void *)&(uuid->obj.node), sizeof(uuid->obj.node)) != PRNG_RC_OK)
            return UUID_RC_INT;
        uuid->obj.node[0] |= IEEE_MAC_MCBIT;
        uuid->obj.node[0] |= IEEE_MAC_LOBIT;
    }
    else {
        /* use real regular MAC address */
        memcpy(uuid->obj.node, uuid->mac, sizeof(uuid->mac));
    }

    /*
     *  FINISH
     */

    /* remember current system time for next iteration */
    uuid->time_last.tv_sec  = time_now.tv_sec;
    uuid->time_last.tv_usec = time_now.tv_usec;

    /* brand with version and variant */
    uuid_brand(uuid, 1);

    return UUID_RC_OK;
}

/* INTERNAL: pre-defined UUID values.
   (defined as network byte ordered octet stream) */
#define UUID_MAKE(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16) \
    { (uuid_uint8_t)(a1),  (uuid_uint8_t)(a2),  (uuid_uint8_t)(a3),  (uuid_uint8_t)(a4),  \
      (uuid_uint8_t)(a5),  (uuid_uint8_t)(a6),  (uuid_uint8_t)(a7),  (uuid_uint8_t)(a8),  \
      (uuid_uint8_t)(a9),  (uuid_uint8_t)(a10), (uuid_uint8_t)(a11), (uuid_uint8_t)(a12), \
      (uuid_uint8_t)(a13), (uuid_uint8_t)(a14), (uuid_uint8_t)(a15), (uuid_uint8_t)(a16) }
static struct {
    char *name;
    uuid_uint8_t uuid[UUID_LEN_BIN];
} uuid_value_table[] = {
    { "nil",     /* 00000000-0000-0000-0000-000000000000 ("Nil UUID") */
      UUID_MAKE(0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00) },
    { "ns:DNS",  /* 6ba7b810-9dad-11d1-80b4-00c04fd430c8 (see RFC 4122) */
      UUID_MAKE(0x6b,0xa7,0xb8,0x10,0x9d,0xad,0x11,0xd1,0x80,0xb4,0x00,0xc0,0x4f,0xd4,0x30,0xc8) },
    { "ns:URL",  /* 6ba7b811-9dad-11d1-80b4-00c04fd430c8 (see RFC 4122) */
      UUID_MAKE(0x6b,0xa7,0xb8,0x11,0x9d,0xad,0x11,0xd1,0x80,0xb4,0x00,0xc0,0x4f,0xd4,0x30,0xc8) },
    { "ns:OID",  /* 6ba7b812-9dad-11d1-80b4-00c04fd430c8 (see RFC 4122) */
      UUID_MAKE(0x6b,0xa7,0xb8,0x12,0x9d,0xad,0x11,0xd1,0x80,0xb4,0x00,0xc0,0x4f,0xd4,0x30,0xc8) },
    { "ns:X500", /* 6ba7b814-9dad-11d1-80b4-00c04fd430c8 (see RFC 4122) */
      UUID_MAKE(0x6b,0xa7,0xb8,0x14,0x9d,0xad,0x11,0xd1,0x80,0xb4,0x00,0xc0,0x4f,0xd4,0x30,0xc8) }
};

/* load UUID object with pre-defined value */
uuid_rc_t uuid_load(uuid_t *uuid, const char *name)
{
    uuid_uint8_t *uuid_octets;
    uuid_rc_t rc;
    unsigned int i;

    /* sanity check argument(s) */
    if (uuid == NULL || name == NULL)
        return UUID_RC_ARG;

    /* search for UUID in table */
    uuid_octets = NULL;
    for (i = 0; i < (unsigned int)sizeof(uuid_value_table)/sizeof(uuid_value_table[0]); i++) {
         if (strcmp(uuid_value_table[i].name, name) == 0) {
             uuid_octets = uuid_value_table[i].uuid;
             break;
         }
    }
    if (uuid_octets == NULL)
        return UUID_RC_ARG;

    /* import value into UUID object */
    if ((rc = uuid_import(uuid, UUID_FMT_BIN, uuid_octets, UUID_LEN_BIN)) != UUID_RC_OK)
        return rc;

    return UUID_RC_OK;
}

/* INTERNAL: generate UUID version 3: name based with MD5 */
static uuid_rc_t uuid_make_v3(uuid_t *uuid, unsigned int mode, va_list ap)
{
    char *str;
    uuid_t *uuid_ns;
    uuid_uint8_t uuid_buf[UUID_LEN_BIN];
    void *uuid_ptr;
    size_t uuid_len;
    uuid_rc_t rc;

    /* determine namespace UUID and name string arguments */
    if ((uuid_ns = (uuid_t *)va_arg(ap, void *)) == NULL)
        return UUID_RC_ARG;
    if ((str = (char *)va_arg(ap, char *)) == NULL)
        return UUID_RC_ARG;

    /* initialize MD5 context */
    if (md5_init(uuid->md5) != MD5_RC_OK)
        return UUID_RC_MEM;

    /* load the namespace UUID into MD5 context */
    uuid_ptr = (void *)&uuid_buf;
    uuid_len = sizeof(uuid_buf);
    if ((rc = uuid_export(uuid_ns, UUID_FMT_BIN, &uuid_ptr, &uuid_len)) != UUID_RC_OK)
        return rc;
    if (md5_update(uuid->md5, uuid_buf, uuid_len) != MD5_RC_OK)
        return UUID_RC_INT;

    /* load the argument name string into MD5 context */
    if (md5_update(uuid->md5, str, strlen(str)) != MD5_RC_OK)
        return UUID_RC_INT;

    /* store MD5 result into UUID
       (requires MD5_LEN_BIN space, UUID_LEN_BIN space is available,
       and both are equal in size, so we are safe!) */
    uuid_ptr = (void *)&(uuid->obj);
    if (md5_store(uuid->md5, &uuid_ptr, NULL) != MD5_RC_OK)
        return UUID_RC_INT;

    /* fulfill requirement of standard and convert UUID data into
       local/host byte order (this uses fact that uuid_import_bin() is
       able to operate in-place!) */
    if ((rc = uuid_import(uuid, UUID_FMT_BIN, (void *)&(uuid->obj), UUID_LEN_BIN)) != UUID_RC_OK)
        return rc;

    /* brand UUID with version and variant */
    uuid_brand(uuid, 3);

    return UUID_RC_OK;
}

/* INTERNAL: generate UUID version 4: random number based */
static uuid_rc_t uuid_make_v4(uuid_t *uuid, unsigned int mode, va_list ap)
{
    /* fill UUID with random data */
    if (prng_data(uuid->prng, (void *)&(uuid->obj), sizeof(uuid->obj)) != PRNG_RC_OK)
        return UUID_RC_INT;

    /* brand UUID with version and variant */
    uuid_brand(uuid, 4);

    return UUID_RC_OK;
}

/* INTERNAL: generate UUID version 5: name based with SHA-1 */
static uuid_rc_t uuid_make_v5(uuid_t *uuid, unsigned int mode, va_list ap)
{
    char *str;
    uuid_t *uuid_ns;
    uuid_uint8_t uuid_buf[UUID_LEN_BIN];
    void *uuid_ptr;
    size_t uuid_len;
    uuid_uint8_t sha1_buf[SHA1_LEN_BIN];
    void *sha1_ptr;
    uuid_rc_t rc;

    /* determine namespace UUID and name string arguments */
    if ((uuid_ns = (uuid_t *)va_arg(ap, void *)) == NULL)
        return UUID_RC_ARG;
    if ((str = (char *)va_arg(ap, char *)) == NULL)
        return UUID_RC_ARG;

    /* initialize SHA-1 context */
    if (sha1_init(uuid->sha1) != SHA1_RC_OK)
        return UUID_RC_INT;

    /* load the namespace UUID into SHA-1 context */
    uuid_ptr = (void *)&uuid_buf;
    uuid_len = sizeof(uuid_buf);
    if ((rc = uuid_export(uuid_ns, UUID_FMT_BIN, &uuid_ptr, &uuid_len)) != UUID_RC_OK)
        return rc;
    if (sha1_update(uuid->sha1, uuid_buf, uuid_len) != SHA1_RC_OK)
        return UUID_RC_INT;

    /* load the argument name string into SHA-1 context */
    if (sha1_update(uuid->sha1, str, strlen(str)) != SHA1_RC_OK)
        return UUID_RC_INT;

    /* store SHA-1 result into UUID
       (requires SHA1_LEN_BIN space, but UUID_LEN_BIN space is available
       only, so use a temporary buffer to store SHA-1 results and then
       use lower part only according to standard */
    sha1_ptr = (void *)sha1_buf;
    if (sha1_store(uuid->sha1, &sha1_ptr, NULL) != SHA1_RC_OK)
        return UUID_RC_INT;
    uuid_ptr = (void *)&(uuid->obj);
    memcpy(uuid_ptr, sha1_ptr, UUID_LEN_BIN);

    /* fulfill requirement of standard and convert UUID data into
       local/host byte order (this uses fact that uuid_import_bin() is
       able to operate in-place!) */
    if ((rc = uuid_import(uuid, UUID_FMT_BIN, (void *)&(uuid->obj), UUID_LEN_BIN)) != UUID_RC_OK)
        return rc;

    /* brand UUID with version and variant */
    uuid_brand(uuid, 5);

    return UUID_RC_OK;
}

/* generate UUID */
uuid_rc_t uuid_make(uuid_t *uuid, unsigned int mode, ...)
{
    va_list ap;
    uuid_rc_t rc;

    /* sanity check argument(s) */
    if (uuid == NULL)
        return UUID_RC_ARG;

    /* dispatch into version dependent generation functions */
    va_start(ap, mode);
    if (mode & UUID_MAKE_V1)
        rc = uuid_make_v1(uuid, mode, ap);
    else if (mode & UUID_MAKE_V3)
        rc = uuid_make_v3(uuid, mode, ap);
    else if (mode & UUID_MAKE_V4)
        rc = uuid_make_v4(uuid, mode, ap);
    else if (mode & UUID_MAKE_V5)
        rc = uuid_make_v5(uuid, mode, ap);
    else
        rc = UUID_RC_ARG;
    va_end(ap);

    return rc;
}

/* translate UUID API error code into corresponding error string */
char *uuid_error(uuid_rc_t rc)
{
    char *str;

    switch (rc) {
        case UUID_RC_OK:  str = "everything ok";    break;
        case UUID_RC_ARG: str = "invalid argument"; break;
        case UUID_RC_MEM: str = "out of memory";    break;
        case UUID_RC_SYS: str = "system error";     break;
        case UUID_RC_INT: str = "internal error";   break;
        case UUID_RC_IMP: str = "not implemented";  break;
        default:          str = NULL;               break;
    }
    return str;
}

/* OSSP uuid version (link-time information) */
unsigned long uuid_version(void)
{
    return (unsigned long)(_UUID_VERSION);
}

/*
**  uuid_md5.c: MD5 API implementation
*/

/*
 * This is a RFC 1321 compliant Message Digest 5 (MD5) algorithm
 * implementation. It is directly derived from the RSA code published in
 * RFC 1321 with just the following functionality preserving changes:
 * - converted function definitions from K&R to ANSI C
 * - included contents of the "global.h" and "md5.h" headers
 * - moved the SXX defines into the MD5Transform function
 * - replaced MD5_memcpy() with memcpy(3) and MD5_memset() with memset(3)
 * - renamed "index" variables to "idx" to avoid namespace conflicts
 * - reformatted C style to conform with OSSP C style
 * - added own OSSP style frontend API
 */

/*
** ==== BEGIN RFC 1321 CODE ====
*/

/*
 * RSA Data Security, Inc., MD5 message-digest algorithm
 * Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991.
 * All rights reserved.
 *
 * License to copy and use this software is granted provided that it
 * is identified as the "RSA Data Security, Inc. MD5 Message-Digest
 * Algorithm" in all material mentioning or referencing this software
 * or this function.
 *
 * License is also granted to make and use derivative works provided
 * that such works are identified as "derived from the RSA Data
 * Security, Inc. MD5 Message-Digest Algorithm" in all material
 * mentioning or referencing the derived work.
 *
 * RSA Data Security, Inc. makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 */

/* POINTER defines a generic pointer type */
typedef unsigned char *POINTER;

/* UINT4 defines a four byte word */
#if SIZEOF_UNSIGNED_SHORT       == 4
typedef unsigned short int     UINT4;
#elif SIZEOF_UNSIGNED_INT       == 4
typedef unsigned int           UINT4;
#elif SIZEOF_UNSIGNED_LONG      == 4
typedef unsigned long int      UINT4;
#elif SIZEOF_UNSIGNED_LONG_LONG == 4
typedef unsigned long long int UINT4;
#else
#error ERROR: unable to determine UINT4 type (four byte word)
#endif

/* MD5 context. */
typedef struct {
  UINT4 state[4];                                   /* state (ABCD) */
  UINT4 count[2];        /* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];                         /* input buffer */
} MD5_CTX;

/* prototypes for internal functions */
static void MD5Init      (MD5_CTX *_ctx);
static void MD5Update    (MD5_CTX *_ctx, unsigned char *, unsigned int);
static void MD5Final     (unsigned char [], MD5_CTX *);
static void MD5Transform (UINT4 [], unsigned char []);
static void Encode       (unsigned char *, UINT4 *, unsigned int);
static void Decode       (UINT4 *, unsigned char *, unsigned int);

/* finalization padding */
static unsigned char PADDING[64] = {
  0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* F, G, H and I are basic MD5 functions. */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits. */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
   Rotation is separate from addition to prevent recomputation. */
#define FF(a, b, c, d, x, s, ac) { \
 (a) += F ((b), (c), (d)) + (x) + (UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
}
#define GG(a, b, c, d, x, s, ac) { \
 (a) += G ((b), (c), (d)) + (x) + (UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
}
#define HH(a, b, c, d, x, s, ac) { \
 (a) += H ((b), (c), (d)) + (x) + (UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
}
#define II(a, b, c, d, x, s, ac) { \
 (a) += I ((b), (c), (d)) + (x) + (UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
}

/* MD5 initialization. Begins an MD5 operation, writing a new context. */
static void MD5Init(
    MD5_CTX *context)
{
    context->count[0] = context->count[1] = 0;

    /* Load magic initialization constants. */
    context->state[0] = 0x67452301;
    context->state[1] = 0xefcdab89;
    context->state[2] = 0x98badcfe;
    context->state[3] = 0x10325476;
    return;
}

/* MD5 block update operation. Continues an MD5 message-digest
   operation, processing another message block, and updating the
   context. */
static void MD5Update(
    MD5_CTX *context,                                        /* context */
    unsigned char *input,                                /* input block */
    unsigned int inputLen)                     /* length of input block */
{
    unsigned int i, idx, partLen;

    /* Compute number of bytes mod 64 */
    idx = (unsigned int)((context->count[0] >> 3) & 0x3F);

    /* Update number of bits */
    if ((context->count[0] += ((UINT4)inputLen << 3)) < ((UINT4)inputLen << 3))
        context->count[1]++;
    context->count[1] += ((UINT4)inputLen >> 29);

    partLen = (unsigned int)64 - idx;

    /* Transform as many times as possible.  */
    if (inputLen >= partLen) {
        memcpy((POINTER)&context->buffer[idx], (POINTER)input, (size_t)partLen);
        MD5Transform(context->state, context->buffer);
        for (i = partLen; i + 63 < inputLen; i += 64)
            MD5Transform(context->state, &input[i]);
        idx = 0;
    }
    else
        i = 0;

    /* Buffer remaining input */
    memcpy((POINTER)&context->buffer[idx], (POINTER)&input[i], (size_t)(inputLen - i));
}

/* MD5 finalization. Ends an MD5 message-digest operation, writing the
   the message digest and zeroizing the context. */
static void MD5Final(
    unsigned char digest[],                                 /* message digest */
    MD5_CTX *context)                                       /* context */
{
    unsigned char bits[8];
    unsigned int idx, padLen;

    /* Save number of bits */
    Encode(bits, context->count, 8);

    /* Pad out to 56 mod 64. */
    idx = (unsigned int)((context->count[0] >> 3) & 0x3f);
    padLen = (idx < 56) ? ((unsigned int)56 - idx) : ((unsigned int)120 - idx);
    MD5Update(context, PADDING, padLen);

    /* Append length (before padding) */
    MD5Update(context, bits, 8);

    /* Store state in digest */
    Encode(digest, context->state, 16);

    /* Zeroize sensitive information. */
    memset((POINTER)context, 0, sizeof(*context));
}

/* MD5 basic transformation. Transforms state based on block. */
static void MD5Transform(
    UINT4 state[],
    unsigned char block[])
{
    UINT4 a = state[0], b = state[1], c = state[2], d = state[3], x[16];

    Decode(x, block, 64);

    /* Round 1 */
#define S11 7
#define S12 12
#define S13 17
#define S14 22
    FF (a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
    FF (d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
    FF (c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
    FF (b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
    FF (a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
    FF (d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
    FF (c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
    FF (b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
    FF (a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
    FF (d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
    FF (c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
    FF (b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
    FF (a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
    FF (d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
    FF (c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
    FF (b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

   /* Round 2 */
#define S21 5
#define S22 9
#define S23 14
#define S24 20
    GG (a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
    GG (d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
    GG (c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
    GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
    GG (a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
    GG (d, a, b, c, x[10], S22,  0x2441453); /* 22 */
    GG (c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
    GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
    GG (a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
    GG (d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
    GG (c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
    GG (b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
    GG (a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
    GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
    GG (c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
    GG (b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
#define S31 4
#define S32 11
#define S33 16
#define S34 23
    HH (a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
    HH (d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
    HH (c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
    HH (b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
    HH (a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
    HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
    HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
    HH (b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
    HH (a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
    HH (d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
    HH (c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
    HH (b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
    HH (a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
    HH (d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
    HH (c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
    HH (b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */

    /* Round 4 */
#define S41 6
#define S42 10
#define S43 15
#define S44 21
    II (a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
    II (d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
    II (c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
    II (b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
    II (a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
    II (d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
    II (c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
    II (b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
    II (a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
    II (d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
    II (c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
    II (b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
    II (a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
    II (d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
    II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
    II (b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

    /* Zeroize sensitive information. */
    memset((POINTER)x, 0, sizeof(x));
}

/* Encodes input (UINT4) into output (unsigned char).
   Assumes len is a multiple of 4. */
static void Encode(
    unsigned char *output,
    UINT4 *input,
    unsigned int len)
{
    unsigned int i, j;

    for (i = 0, j = 0; j < len; i++, j += 4) {
        output[j]   = (unsigned char)( input[i]        & 0xff);
        output[j+1] = (unsigned char)((input[i] >> 8)  & 0xff);
        output[j+2] = (unsigned char)((input[i] >> 16) & 0xff);
        output[j+3] = (unsigned char)((input[i] >> 24) & 0xff);
    }
    return;
}

/* Decodes input (unsigned char) into output (UINT4).
   Assumes len is a multiple of 4. */
static void Decode(
    UINT4 *output,
    unsigned char *input,
    unsigned int len)
{
    unsigned int i, j;

    for (i = 0, j = 0; j < len; i++, j += 4)
        output[i] =   ( (UINT4)input[j])
                    | (((UINT4)input[j+1]) << 8 )
                    | (((UINT4)input[j+2]) << 16)
                    | (((UINT4)input[j+3]) << 24);
    return;
}

/*
** ==== END RFC 1321 CODE ====
*/

struct md5_st {
    MD5_CTX ctx;
};

md5_rc_t md5_create(md5_t **md5)
{
    if (md5 == NULL)
        return MD5_RC_ARG;
    if ((*md5 = (md5_t *)malloc(sizeof(md5_t))) == NULL)
        return MD5_RC_MEM;
    MD5Init(&((*md5)->ctx));
    return MD5_RC_OK;
}

md5_rc_t md5_init(md5_t *md5)
{
    if (md5 == NULL)
        return MD5_RC_ARG;
    MD5Init(&(md5->ctx));
    return MD5_RC_OK;
}

md5_rc_t md5_update(md5_t *md5, const void *data_ptr, size_t data_len)
{
    if (md5 == NULL)
        return MD5_RC_ARG;
    MD5Update(&(md5->ctx), (unsigned char *)data_ptr, (unsigned int)data_len);
    return MD5_RC_OK;
}

md5_rc_t md5_store(md5_t *md5, void **data_ptr, size_t *data_len)
{
    MD5_CTX ctx;

    if (md5 == NULL || data_ptr == NULL)
        return MD5_RC_ARG;
    if (*data_ptr == NULL) {
        if ((*data_ptr = malloc(MD5_LEN_BIN)) == NULL)
            return MD5_RC_MEM;
        if (data_len != NULL)
            *data_len = MD5_LEN_BIN;
    }
    else {
        if (data_len != NULL) {
            if (*data_len < MD5_LEN_BIN)
                return MD5_RC_MEM;
            *data_len = MD5_LEN_BIN;
        }
    }
    memcpy((void *)(&ctx), (void *)(&(md5->ctx)), sizeof(MD5_CTX));
    MD5Final((unsigned char *)(*data_ptr), &(ctx));
    return MD5_RC_OK;
}

md5_rc_t md5_format(md5_t *md5, char **data_ptr, size_t *data_len)
{
    static const char hex[] = "0123456789abcdef";
    unsigned char buf[MD5_LEN_BIN];
    unsigned char *bufptr;
    size_t buflen;
    md5_rc_t rc;
    int i;

    if (md5 == NULL || data_ptr == NULL)
        return MD5_RC_ARG;
    if (*data_ptr == NULL) {
        if ((*data_ptr = (char *)malloc(MD5_LEN_STR+1)) == NULL)
            return MD5_RC_MEM;
        if (data_len != NULL)
            *data_len = MD5_LEN_STR+1;
    }
    else {
        if (data_len != NULL) {
            if (*data_len < MD5_LEN_STR+1)
                return MD5_RC_MEM;
            *data_len = MD5_LEN_STR+1;
        }
    }

    bufptr = buf;
    buflen = sizeof(buf);
    if ((rc = md5_store(md5, (void **)((void *)&bufptr), &buflen)) != MD5_RC_OK)
        return rc;

    for (i = 0; i < (int)buflen; i++) {
	    (*data_ptr)[(i*2)+0] = hex[(int)(bufptr[i] >> 4)];
	    (*data_ptr)[(i*2)+1] = hex[(int)(bufptr[i] & 0x0f)];
    }
    (*data_ptr)[(i*2)] = '\0';
    return MD5_RC_OK;
}

md5_rc_t md5_destroy(md5_t *md5)
{
    if (md5 == NULL)
        return MD5_RC_ARG;
    free(md5);
    return MD5_RC_OK;
}

/*
**  uuid_sha1.c: SHA-1 API implementation
*/

/*
 *  This is a RFC 3174 compliant Secure Hash Function (SHA-1) algorithm
 *  implementation. It is directly derived from the SHA-1 reference
 *  code published in RFC 3174 with just the following functionality
 *  preserving changes:
 *  - reformatted C style to conform with OSSP C style
 *  - added own OSSP style frontend API
 *  - added Autoconf based determination of sha1_uintX_t types
 */

/*
** ==== BEGIN RFC 3174 CODE ====
*/

/*
 *  This implements the Secure Hashing Algorithm 1 as defined in
 *  FIPS PUB 180-1 published April 17, 1995.
 *
 *  The SHA-1, produces a 160-bit message digest for a given data
 *  stream. It should take about 2**n steps to find a message with the
 *  same digest as a given message and 2**(n/2) to find any two messages
 *  with the same digest, when n is the digest size in bits. Therefore,
 *  this algorithm can serve as a means of providing a "fingerprint" for
 *  a message.
 *
 *  Caveats: SHA-1 is designed to work with messages less than 2^64 bits
 *  long. Although SHA-1 allows a message digest to be generated for
 *  messages of any number of bits less than 2^64, this implementation
 *  only works with messages with a length that is a multiple of the
 *  size of an 8-bit character.
 */

typedef unsigned char sha1_uint8_t;

#if SIZEOF_SHORT  > 2
typedef short int sha1_int16plus_t;
#elif SIZEOF_INT  > 2
typedef int       sha1_int16plus_t;
#elif SIZEOF_LONG > 2
typedef long int  sha1_int16plus_t;
#else
#error ERROR: unable to determine sha1_int16plus_t type (at least two byte word)
#endif

#if SIZEOF_UNSIGNED_SHORT       == 4
typedef unsigned short int     sha1_uint32_t;
#elif SIZEOF_UNSIGNED_INT       == 4
typedef unsigned int           sha1_uint32_t;
#elif SIZEOF_UNSIGNED_LONG      == 4
typedef unsigned long int      sha1_uint32_t;
#elif SIZEOF_UNSIGNED_LONG_LONG == 4
typedef unsigned long long int sha1_uint32_t;
#else
#error ERROR: unable to determine sha1_uint32_t type (four byte word)
#endif

enum {
    shaSuccess = 0,
    shaNull,            /* null pointer parameter */
    shaStateError       /* called Input after Result */
};

#define SHA1HashSize 20

/* This structure will hold context information for the SHA-1 hashing operation */
typedef struct SHA1Context {
    sha1_uint32_t Intermediate_Hash[SHA1HashSize/4]; /* Message Digest */
    sha1_uint32_t Length_Low;                        /* Message length in bits */
    sha1_uint32_t Length_High;                       /* Message length in bits */
    sha1_int16plus_t Message_Block_Index;            /* Index into message block array */
    sha1_uint8_t Message_Block[64];                  /* 512-bit message blocks */
    int Computed;                                    /* Is the digest computed? */
    int Corrupted;                                   /* Is the message digest corrupted? */
} SHA1Context;

/* Function Prototypes */
static int SHA1Reset  (SHA1Context *);
static int SHA1Input  (SHA1Context *, const sha1_uint8_t *, unsigned int);
static int SHA1Result (SHA1Context *, sha1_uint8_t Message_Digest[]);

/* Local Function Prototyptes */
static void SHA1PadMessage         (SHA1Context *);
static void SHA1ProcessMessageBlock(SHA1Context *);

/* Define the SHA1 circular left shift macro */
#define SHA1CircularShift(bits,word) \
    (((word) << (bits)) | ((word) >> (32-(bits))))

/*
 *  This function will initialize the SHA1Context in preparation for
 *  computing a new SHA1 message digest.
 */
static int SHA1Reset(SHA1Context *context)
{
    if (context == NULL)
        return shaNull;

    context->Length_Low             = 0;
    context->Length_High            = 0;
    context->Message_Block_Index    = 0;

    context->Intermediate_Hash[0]   = 0x67452301;
    context->Intermediate_Hash[1]   = 0xEFCDAB89;
    context->Intermediate_Hash[2]   = 0x98BADCFE;
    context->Intermediate_Hash[3]   = 0x10325476;
    context->Intermediate_Hash[4]   = 0xC3D2E1F0;

    context->Computed   = 0;
    context->Corrupted  = 0;

    return shaSuccess;
}

/*
 *  This function will return the 160-bit message digest into the
 *  Message_Digest array provided by the caller. NOTE: The first octet
 *  of hash is stored in the 0th element, the last octet of hash in the
 *  19th element.
 */
static int SHA1Result(SHA1Context *context, sha1_uint8_t Message_Digest[])
{
    int i;

    if (context == NULL || Message_Digest == NULL)
        return shaNull;
    if (context->Corrupted)
        return context->Corrupted;

    if (!context->Computed) {
        SHA1PadMessage(context);
        for (i = 0; i < 64; i++) {
            /* message may be sensitive, clear it out */
            context->Message_Block[i] = (sha1_uint8_t)0;
        }
        context->Length_Low  = 0; /* and clear length */
        context->Length_High = 0;
        context->Computed    = 1;
    }
    for (i = 0; i < SHA1HashSize; i++)
        Message_Digest[i] = (sha1_uint8_t)(context->Intermediate_Hash[i>>2] >> (8 * (3 - (i & 0x03))));

    return shaSuccess;
}

/*
 *  This function accepts an array of octets as the next portion of the
 *  message.
 */
static int SHA1Input(SHA1Context *context, const sha1_uint8_t *message_array, unsigned int length)
{
    if (length == 0)
        return shaSuccess;
    if (context == NULL || message_array == NULL)
        return shaNull;

    if (context->Computed) {
        context->Corrupted = shaStateError;
        return shaStateError;
    }
    if (context->Corrupted)
        return context->Corrupted;
    while (length-- && !context->Corrupted) {
        context->Message_Block[context->Message_Block_Index++] = (*message_array & 0xFF);
        context->Length_Low += 8;
        if (context->Length_Low == 0) {
            context->Length_High++;
            if (context->Length_High == 0)
                context->Corrupted = 1; /* Message is too long */
        }
        if (context->Message_Block_Index == 64)
            SHA1ProcessMessageBlock(context);
        message_array++;
    }

    return shaSuccess;
}

/*
 *  This function will process the next 512 bits of the message stored
 *  in the Message_Block array. NOTICE: Many of the variable names in
 *  this code, especially the single character names, were used because
 *  those were the names used in the publication.
 */
static void SHA1ProcessMessageBlock(SHA1Context *context)
{
    const sha1_uint32_t K[] = {   /* Constants defined in SHA-1   */
        0x5A827999,
        0x6ED9EBA1,
        0x8F1BBCDC,
        0xCA62C1D6
    };
    int            t;             /* Loop counter                */
    sha1_uint32_t  temp;          /* Temporary word value        */
    sha1_uint32_t  W[80];         /* Word sequence               */
    sha1_uint32_t  A, B, C, D, E; /* Word buffers                */

    /* Initialize the first 16 words in the array W */
    for (t = 0; t < 16; t++) {
        W[t]  = (sha1_uint32_t)(context->Message_Block[t * 4    ] << 24);
        W[t] |= (sha1_uint32_t)(context->Message_Block[t * 4 + 1] << 16);
        W[t] |= (sha1_uint32_t)(context->Message_Block[t * 4 + 2] <<  8);
        W[t] |= (sha1_uint32_t)(context->Message_Block[t * 4 + 3]      );
    }

    for (t = 16; t < 80; t++)
       W[t] = SHA1CircularShift(1, W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);

    A = context->Intermediate_Hash[0];
    B = context->Intermediate_Hash[1];
    C = context->Intermediate_Hash[2];
    D = context->Intermediate_Hash[3];
    E = context->Intermediate_Hash[4];

    for (t = 0; t < 20; t++) {
        temp =  SHA1CircularShift(5, A) + ((B & C) | ((~B) & D)) + E + W[t] + K[0];
        E = D;
        D = C;
        C = SHA1CircularShift(30, B);
        B = A;
        A = temp;
    }

    for (t = 20; t < 40; t++) {
        temp = SHA1CircularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[1];
        E = D;
        D = C;
        C = SHA1CircularShift(30, B);
        B = A;
        A = temp;
    }

    for (t = 40; t < 60; t++) {
        temp = SHA1CircularShift(5, A) + ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
        E = D;
        D = C;
        C = SHA1CircularShift(30, B);
        B = A;
        A = temp;
    }

    for (t = 60; t < 80; t++) {
        temp = SHA1CircularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[3];
        E = D;
        D = C;
        C = SHA1CircularShift(30, B);
        B = A;
        A = temp;
    }

    context->Intermediate_Hash[0] += A;
    context->Intermediate_Hash[1] += B;
    context->Intermediate_Hash[2] += C;
    context->Intermediate_Hash[3] += D;
    context->Intermediate_Hash[4] += E;

    context->Message_Block_Index = 0;

    return;
}

/*
 *  According to the standard, the message must be padded to an even
 *  512 bits. The first padding bit must be a '1'. The last 64 bits
 *  represent the length of the original message. All bits in between
 *  should be 0. This function will pad the message according to those
 *  rules by filling the Message_Block array accordingly. It will also
 *  call the ProcessMessageBlock function provided appropriately. When
 *  it returns, it can be assumed that the message digest has been
 *  computed.
 */
static void SHA1PadMessage(SHA1Context *context)
{
    /* Check to see if the current message block is too small to hold
       the initial padding bits and length. If so, we will pad the block,
       process it, and then continue padding into a second block. */
    if (context->Message_Block_Index > 55) {
        context->Message_Block[context->Message_Block_Index++] = (sha1_uint8_t)0x80;
        while (context->Message_Block_Index < 64)
            context->Message_Block[context->Message_Block_Index++] = (sha1_uint8_t)0;
        SHA1ProcessMessageBlock(context);
        while(context->Message_Block_Index < 56)
            context->Message_Block[context->Message_Block_Index++] = (sha1_uint8_t)0;
    }
    else {
        context->Message_Block[context->Message_Block_Index++] = (sha1_uint8_t)0x80;
        while(context->Message_Block_Index < 56)
            context->Message_Block[context->Message_Block_Index++] = (sha1_uint8_t)0;
    }

    /* Store the message length as the last 8 octets */
    context->Message_Block[56] = (sha1_uint8_t)(context->Length_High >> 24);
    context->Message_Block[57] = (sha1_uint8_t)(context->Length_High >> 16);
    context->Message_Block[58] = (sha1_uint8_t)(context->Length_High >>  8);
    context->Message_Block[59] = (sha1_uint8_t)(context->Length_High      );
    context->Message_Block[60] = (sha1_uint8_t)(context->Length_Low  >> 24);
    context->Message_Block[61] = (sha1_uint8_t)(context->Length_Low  >> 16);
    context->Message_Block[62] = (sha1_uint8_t)(context->Length_Low  >>  8);
    context->Message_Block[63] = (sha1_uint8_t)(context->Length_Low       );

    SHA1ProcessMessageBlock(context);
    return;
}

/*
** ==== END RFC 3174 CODE ====
*/

struct sha1_st {
    SHA1Context ctx;
};

sha1_rc_t sha1_create(sha1_t **sha1)
{
    if (sha1 == NULL)
        return SHA1_RC_ARG;
    if ((*sha1 = (sha1_t *)malloc(sizeof(sha1_t))) == NULL)
        return SHA1_RC_MEM;
    if (SHA1Reset(&((*sha1)->ctx)) != shaSuccess)
        return SHA1_RC_INT;
    return SHA1_RC_OK;
}

sha1_rc_t sha1_init(sha1_t *sha1)
{
    if (sha1 == NULL)
        return SHA1_RC_ARG;
    if (SHA1Reset(&(sha1->ctx)) != shaSuccess)
        return SHA1_RC_INT;
    return SHA1_RC_OK;
}

sha1_rc_t sha1_update(sha1_t *sha1, const void *data_ptr, size_t data_len)
{
    if (sha1 == NULL)
        return SHA1_RC_ARG;
    if (SHA1Input(&(sha1->ctx), (unsigned char *)data_ptr, (unsigned int)data_len) != shaSuccess)
        return SHA1_RC_INT;
    return SHA1_RC_OK;
}

sha1_rc_t sha1_store(sha1_t *sha1, void **data_ptr, size_t *data_len)
{
    SHA1Context ctx;

    if (sha1 == NULL || data_ptr == NULL)
        return SHA1_RC_ARG;
    if (*data_ptr == NULL) {
        if ((*data_ptr = malloc(SHA1_LEN_BIN)) == NULL)
            return SHA1_RC_MEM;
        if (data_len != NULL)
            *data_len = SHA1_LEN_BIN;
    }
    else {
        if (data_len != NULL) {
            if (*data_len < SHA1_LEN_BIN)
                return SHA1_RC_MEM;
            *data_len = SHA1_LEN_BIN;
        }
    }
    memcpy((void *)(&ctx), (void *)(&(sha1->ctx)), sizeof(SHA1Context));
    if (SHA1Result(&(ctx), (unsigned char *)(*data_ptr)) != shaSuccess)
        return SHA1_RC_INT;
    return SHA1_RC_OK;
}

sha1_rc_t sha1_format(sha1_t *sha1, char **data_ptr, size_t *data_len)
{
    static const char hex[] = "0123456789abcdef";
    unsigned char buf[SHA1_LEN_BIN];
    unsigned char *bufptr;
    size_t buflen;
    sha1_rc_t rc;
    int i;

    if (sha1 == NULL || data_ptr == NULL)
        return SHA1_RC_ARG;
    if (*data_ptr == NULL) {
        if ((*data_ptr = (char *)malloc(SHA1_LEN_STR+1)) == NULL)
            return SHA1_RC_MEM;
        if (data_len != NULL)
            *data_len = SHA1_LEN_STR+1;
    }
    else {
        if (data_len != NULL) {
            if (*data_len < SHA1_LEN_STR+1)
                return SHA1_RC_MEM;
            *data_len = SHA1_LEN_STR+1;
        }
    }

    bufptr = buf;
    buflen = sizeof(buf);
    if ((rc = sha1_store(sha1, (void **)((void *)&bufptr), &buflen)) != SHA1_RC_OK)
        return rc;

    for (i = 0; i < (int)buflen; i++) {
	    (*data_ptr)[(i*2)+0] = hex[(int)(bufptr[i] >> 4)];
	    (*data_ptr)[(i*2)+1] = hex[(int)(bufptr[i] & 0x0f)];
    }
    (*data_ptr)[(i*2)] = '\0';
    return SHA1_RC_OK;
}

sha1_rc_t sha1_destroy(sha1_t *sha1)
{
    if (sha1 == NULL)
        return SHA1_RC_ARG;
    free(sha1);
    return SHA1_RC_OK;
}

/*
**  uuid_prng.c: PRNG API implementation
*/

struct prng_st {
    int    dev; /* system PRNG device */
    md5_t *md5; /* local MD5 PRNG engine */
    long   cnt; /* time resolution compensation counter */
};

prng_rc_t prng_create(prng_t **prng)
{
#if !defined(WIN32)
    int fd = -1;
#endif
    struct timeval tv;
    pid_t pid;
    unsigned int i;

    /* sanity check argument(s) */
    if (prng == NULL)
        return PRNG_RC_ARG;

    /* allocate object */
    if ((*prng = (prng_t *)malloc(sizeof(prng_t))) == NULL)
        return PRNG_RC_MEM;

    /* try to open the system PRNG device */
    (*prng)->dev = -1;
#if !defined(WIN32)
    if ((fd = open("/dev/urandom", O_RDONLY)) == -1)
        fd = open("/dev/random", O_RDONLY|O_NONBLOCK);
    if (fd != -1) {
        (void)fcntl(fd, F_SETFD, FD_CLOEXEC);
        (*prng)->dev = fd;
    }
#endif

    /* initialize MD5 engine */
    if (md5_create(&((*prng)->md5)) != MD5_RC_OK) {
        free(*prng);
        return PRNG_RC_INT;
    }

    /* initialize time resolution compensation counter */
    (*prng)->cnt = 0;

    /* seed the C library PRNG once */
    (void)time_gettimeofday(&tv);
    pid = getpid();
    srand((unsigned int)(
        ((unsigned int)pid << 16)
        ^ (unsigned int)pid
        ^ (unsigned int)tv.tv_sec
        ^ (unsigned int)tv.tv_usec));
    for (i = (unsigned int)((tv.tv_sec ^ tv.tv_usec) & 0x1F); i > 0; i--)
        (void)rand();

    return PRNG_RC_OK;
}

prng_rc_t prng_data(prng_t *prng, void *data_ptr, size_t data_len)
{
    size_t n;
    unsigned char *p;
    struct {
        struct timeval tv;
        long cnt;
        int rnd;
    } entropy;
    unsigned char md5_buf[MD5_LEN_BIN];
    unsigned char *md5_ptr;
    size_t md5_len;
    int retries;
    int i;
#if defined(WIN32)
    HCRYPTPROV hProv;
#endif

    /* sanity check argument(s) */
    if (prng == NULL || data_len == 0)
        return PRNG_RC_ARG;

    /* prepare for generation */
    p = (unsigned char *)data_ptr;
    n = data_len;

    /* approach 1: try to gather data via stronger system PRNG device */
    if (prng->dev != -1) {
        retries = 0;
        while (n > 0) {
            i = (int)read(prng->dev, (void *)p, n);
            if (i <= 0) {
                if (retries++ > 16)
                    break;
                continue;
            }
            retries = 0;
            n -= (unsigned int)i;
            p += (unsigned int)i;
        }
    }
#if defined(WIN32)
    else {
        if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, 0))
            CryptGenRandom(hProv, n, p);
    }
#endif

    /* approach 2: try to gather data via weaker libc PRNG API. */
    while (n > 0) {
        /* gather new entropy */
        (void)time_gettimeofday(&(entropy.tv));  /* source: libc time */
        entropy.rnd = rand();                    /* source: libc PRNG */
        entropy.cnt = prng->cnt++;               /* source: local counter */

        /* pass entropy into MD5 engine */
        if (md5_update(prng->md5, (void *)&entropy, sizeof(entropy)) != MD5_RC_OK)
            return PRNG_RC_INT;

        /* store MD5 engine state as PRN output */
        md5_ptr = md5_buf;
        md5_len = sizeof(md5_buf);
        if (md5_store(prng->md5, (void **)(void *)&md5_ptr, &md5_len) != MD5_RC_OK)
            return PRNG_RC_INT;
        for (i = 0; i < MD5_LEN_BIN && n > 0; i++, n--)
            *p++ ^= md5_buf[i]; /* intentionally no assignment because arbitrary
                                   caller buffer content is leveraged, too */
    }

    return PRNG_RC_OK;
}

prng_rc_t prng_destroy(prng_t *prng)
{
    /* sanity check argument(s) */
    if (prng == NULL)
        return PRNG_RC_ARG;

    /* close PRNG device */
    if (prng->dev != -1)
        (void)close(prng->dev);

    /* destroy MD5 engine */
    (void)md5_destroy(prng->md5);

    /* free object */
    free(prng);

    return PRNG_RC_OK;
}

/*
**  uuid_mac.c: Media Access Control (MAC) resolver implementation
*/

/* return the Media Access Control (MAC) address of
   the FIRST network interface card (NIC) */
int mac_address(unsigned char *data_ptr, size_t data_len)
{
    /* sanity check arguments */
    if (data_ptr == NULL || data_len < MAC_LEN)
        return FALSE;

#if defined(HAVE_IFADDRS_H) && defined(HAVE_NET_IF_DL_H) && defined(HAVE_GETIFADDRS)
    /* use getifaddrs(3) on BSD class platforms (xxxBSD, MacOS X, etc) */
    {
        struct ifaddrs *ifap;
        struct ifaddrs *ifap_head;
        const struct sockaddr_dl *sdl;
        unsigned char *ucp;
        int i;

        if (getifaddrs(&ifap_head) < 0)
            return FALSE;
        for (ifap = ifap_head; ifap != NULL; ifap = ifap->ifa_next) {
            if (ifap->ifa_addr != NULL && ifap->ifa_addr->sa_family == AF_LINK) {
                sdl = (const struct sockaddr_dl *)(void *)ifap->ifa_addr;
                ucp = (unsigned char *)(sdl->sdl_data + sdl->sdl_nlen);
                if (sdl->sdl_alen > 0) {
                    for (i = 0; i < MAC_LEN && i < sdl->sdl_alen; i++, ucp++)
                        data_ptr[i] = (unsigned char)(*ucp & 0xff);
                    freeifaddrs(ifap_head);
                    return TRUE;
                }
            }
        }
        freeifaddrs(ifap_head);
    }
#endif

#if defined(HAVE_NET_IF_H) && defined(SIOCGIFHWADDR)
    /* use SIOCGIFHWADDR ioctl(2) on Linux class platforms */
    {
        struct ifreq ifr;
        struct sockaddr *sa;
        int s;
        int i;

        if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
            return FALSE;
        sprintf(ifr.ifr_name, "eth0");
        if (ioctl(s, SIOCGIFHWADDR, &ifr) < 0) {
            close(s);
            return FALSE;
        }
        sa = (struct sockaddr *)&ifr.ifr_addr;
        for (i = 0; i < MAC_LEN; i++)
            data_ptr[i] = (unsigned char)(sa->sa_data[i] & 0xff);
        close(s);
        return TRUE;
    }
#endif

#if defined(SIOCGARP)
    /* use SIOCGARP ioctl(2) on SVR4 class platforms (Solaris, etc) */
    {
        char hostname[MAXHOSTNAMELEN];
        struct hostent *he;
        struct arpreq ar;
        struct sockaddr_in *sa;
        int s;
        int i;

        if (gethostname(hostname, sizeof(hostname)) < 0)
            return FALSE;
        if ((he = gethostbyname(hostname)) == NULL)
            return FALSE;
        if ((s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
            return FALSE;
        memset(&ar, 0, sizeof(ar));
        sa = (struct sockaddr_in *)((void *)&(ar.arp_pa));
        sa->sin_family = AF_INET;
        memcpy(&(sa->sin_addr), *(he->h_addr_list), sizeof(struct in_addr));
        if (ioctl(s, SIOCGARP, &ar) < 0) {
            close(s);
            return FALSE;
        }
        close(s);
        if (!(ar.arp_flags & ATF_COM))
            return FALSE;
        for (i = 0; i < MAC_LEN; i++)
            data_ptr[i] = (unsigned char)(ar.arp_ha.sa_data[i] & 0xff);
        return TRUE;
    }
#endif

    return FALSE;
}

/*
**  uuid_time.c: Time Management
*/

/* POSIX gettimeofday(2) abstraction (without timezone) */
int time_gettimeofday(struct timeval *tv)
{
#if defined(HAVE_GETTIMEOFDAY)
    /* Unix/POSIX pass-through */
    return gettimeofday(tv, NULL);
#elif defined(HAVE_CLOCK_GETTIME)
    /* POSIX emulation */
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
        return -1;
    if (tv != NULL) {
        tv->tv_sec = (long)ts.tv_sec;
        tv->tv_usec = (long)ts.tv_nsec / 1000;
    }
    return 0;
#elif defined(WIN32)
    /* Windows emulation */
    FILETIME ft;
    LARGE_INTEGER li;
    __int64 t;
    static int tzflag;
#if !defined(__GNUC__)
#define EPOCHFILETIME 116444736000000000i64
#else
#define EPOCHFILETIME 116444736000000000LL
#endif
    if (tv != NULL) {
        GetSystemTimeAsFileTime(&ft);
        li.LowPart  = ft.dwLowDateTime;
        li.HighPart = ft.dwHighDateTime;
        t  = li.QuadPart;
        t -= EPOCHFILETIME;
        t /= 10;
        tv->tv_sec  = (long)(t / 1000000);
        tv->tv_usec = (long)(t % 1000000);
    }
    return 0;
#else
#error neither Win32 GetSystemTimeAsFileTime() nor Unix gettimeofday() nor POSIX clock_gettime() available!
#endif
}

/* BSD usleep(3) abstraction */
int time_usleep(long usec)
{
#if defined(WIN32) && defined(HAVE_SLEEP)
    /* Win32 newer Sleep(3) variant */
    Sleep(usec / 1000);
#elif defined(WIN32)
    /* Win32 older _sleep(3) variant */
    _sleep(usec / 1000);
#elif defined(HAVE_NANOSLEEP)
    /* POSIX newer nanosleep(3) variant */
    struct timespec ts;
    ts.tv_sec  = 0;
    ts.tv_nsec = 1000 * usec;
    nanosleep(&ts, NULL);
#else
    /* POSIX older select(2) variant */
    struct timeval tv;
    tv.tv_sec  = 0;
    tv.tv_usec = usec;
    select(0, NULL, NULL, NULL, &tv);
#endif
    return 0;
}

/*
**  ui64.c: implementation of 64-bit unsigned integer arithmetic
*/

#define UI64_BASE   256 /* 2^8 */
#define UI64_DIGITS 8   /* 8*8 = 64 bit */
#define UIXX_T(n) struct { unsigned char x[n]; }

/* fill an ui64_t with a sequence of a particular digit */
#define ui64_fill(__x, __n) \
    /*lint -save -e717*/ \
    do { int __i; \
      for (__i = 0; __i < UI64_DIGITS; __i++) \
          (__x).x[__i] = (__n); \
    } while (0) \
    /*lint -restore*/

/* the value zero */
ui64_t ui64_zero(void)
{
    ui64_t z;

    ui64_fill(z, 0);
    return z;
}

/* the maximum value */
ui64_t ui64_max(void)
{
    ui64_t z;

    ui64_fill(z, UI64_BASE-1);
    return z;
}

/* convert ISO-C "unsigned long" into internal format */
ui64_t ui64_n2i(unsigned long n)
{
    ui64_t z;
    int i;

    i = 0;
    do {
        z.x[i++] = (n % UI64_BASE);
    } while ((n /= UI64_BASE) > 0 && i < UI64_DIGITS);
    for ( ; i < UI64_DIGITS; i++)
        z.x[i] = 0;
    return z;
}

/* convert internal format into ISO-C "unsigned long";
   truncates if sizeof(unsigned long) is less than UI64_DIGITS! */
unsigned long ui64_i2n(ui64_t x)
{
    unsigned long n;
    int i;

    n = 0;
    i = (int)sizeof(n);
    /*lint -save -e774*/
    if (i > UI64_DIGITS)
        i = UI64_DIGITS;
    /*lint -restore*/
    while (--i >= 0) {
        n = (n * UI64_BASE) + x.x[i];
    }
    return n;
}

/* convert string representation of arbitrary base into internal format */
ui64_t ui64_s2i(const char *str, char **end, int base)
{
    ui64_t z;
    const char *cp;
    int carry;
    static char map[] = {
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9,             /* 0...9 */
        36, 36, 36, 36, 36, 36, 36,                         /* illegal chars */
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, /* A...M */
        23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, /* N...Z */
        36, 36, 36, 36, 36, 36,                             /* illegal chars */
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, /* a...m */
        23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35  /* m...z */
    };

    ui64_fill(z, 0);
    if (str == NULL || (base < 2 || base > 36))
        return z;
    cp = str;
    while (*cp != '\0' && isspace((int)(*cp)))
        cp++;
    while (   *cp != '\0'
           && isalnum((int)(*cp))
           && map[(int)(*cp)-'0'] < base) {
        z = ui64_muln(z, base, &carry);
        if (carry)
            break;
        z = ui64_addn(z, map[(int)(*cp)-'0'], &carry);
        if (carry)
            break;
        cp++;
    }
    if (end != NULL)
        *end = (char *)cp;
    return z;
}

/* convert internal format into string representation of arbitrary base */
char *ui64_i2s(ui64_t x, char *str, size_t len, int base)
{
    static char map[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char c;
    int r;
    int n;
    int i, j;

    if (str == NULL || len < 2 || (base < 2 || base > 36))
        return NULL;
    n = ui64_len(x);
    i = 0;
    do {
        x = ui64_divn(x, base, &r);
        str[i++] = map[r];
        while (n > 1 && x.x[n-1] == 0)
            n--;
    } while (i < ((int)len-1) && (n > 1 || x.x[0] != 0));
    str[i] = '\0';
    for (j = 0; j < --i; j++) {
        c = str[j];
        str[j] = str[i];
        str[i] = c;
    }
    return str;
}

/* addition of two ui64_t */
ui64_t ui64_add(ui64_t x, ui64_t y, ui64_t *ov)
{
    ui64_t z;
    int carry;
    int i;

    carry = 0;
    for (i = 0; i < UI64_DIGITS; i++) {
        carry += (x.x[i] + y.x[i]);
        z.x[i] = (carry % UI64_BASE);
        carry /= UI64_BASE;
    }
    if (ov != NULL)
        *ov = ui64_n2i((unsigned long)carry);
    return z;
}

/* addition of an ui64_t and a single digit */
ui64_t ui64_addn(ui64_t x, int y, int *ov)
{
    ui64_t z;
    int i;

    for (i = 0; i < UI64_DIGITS; i++) {
        y += x.x[i];
        z.x[i] = (y % UI64_BASE);
        y /= UI64_BASE;
    }
    if (ov != NULL)
        *ov = y;
    return z;
}

/* subtraction of two ui64_t */
ui64_t ui64_sub(ui64_t x, ui64_t y, ui64_t *ov)
{
    ui64_t z;
    int borrow;
    int i;
    int d;

    borrow = 0;
    for (i = 0; i < UI64_DIGITS; i++) {
        d = ((x.x[i] + UI64_BASE) - borrow - y.x[i]);
        z.x[i] = (d % UI64_BASE);
        borrow = (1 - (d/UI64_BASE));
    }
    if (ov != NULL)
        *ov = ui64_n2i((unsigned long)borrow);
    return z;
}

/* subtraction of an ui64_t and a single digit */
ui64_t ui64_subn(ui64_t x, int y, int *ov)
{
    ui64_t z;
    int i;
    int d;

    for (i = 0; i < UI64_DIGITS; i++) {
        d = (x.x[i] + UI64_BASE) - y;
        z.x[i] = (d % UI64_BASE);
        y = (1 - (d/UI64_BASE));
    }
    if (ov != NULL)
        *ov = y;
    return z;
}

/*
             7 3 2
         * 9 4 2 8
         ---------
           5 8 5 6
   +     1 4 6 4
   +   2 9 2 8
   + 6 5 8 8
   ---------------
   = 6 9 0 1 2 9 6
*/

ui64_t ui64_mul(ui64_t x, ui64_t y, ui64_t *ov)
{
    UIXX_T(UI64_DIGITS+UI64_DIGITS) zx;
    ui64_t z;
    int carry;
    int i, j;

    /* clear temporary result buffer */
    for (i = 0; i < (UI64_DIGITS+UI64_DIGITS); i++)
        zx.x[i] = 0;

    /* perform multiplication operation */
    for (i = 0; i < UI64_DIGITS; i++) {
        /* calculate partial product and immediately add to z */
        carry = 0;
        for (j = 0; j < UI64_DIGITS; j++) {
            carry += (x.x[i] * y.x[j]) + zx.x[i+j];
            zx.x[i+j] = (carry % UI64_BASE);
            carry /= UI64_BASE;
        }
        /* add carry to remaining digits in z */
        for ( ; j < UI64_DIGITS + UI64_DIGITS - i; j++) {
            carry += zx.x[i+j];
            zx.x[i+j] = (carry % UI64_BASE);
            carry /= UI64_BASE;
        }
    }

    /* provide result by splitting zx into z and ov */
    memcpy(z.x, zx.x, UI64_DIGITS);
    if (ov != NULL)
        memcpy((*ov).x, &zx.x[UI64_DIGITS], UI64_DIGITS);

    return z;
}

ui64_t ui64_muln(ui64_t x, int y, int *ov)
{
    ui64_t z;
    int carry;
    int i;

    carry = 0;
    for (i = 0; i < UI64_DIGITS; i++) {
        carry += (x.x[i] * y);
        z.x[i] = (carry % UI64_BASE);
        carry /= UI64_BASE;
    }
    if (ov != NULL)
        *ov = carry;
    return z;
}

/*
  =   2078 [q]
   0615367 [x] : 296 [y]
  -0592    [dq]
  -----
  = 0233
   -0000   [dq]
   -----
   = 2336
    -2072  [dq]
    -----
    = 2647
     -2308 [dq]
     -----
     = 279 [r]
 */
ui64_t ui64_div(ui64_t x, ui64_t y, ui64_t *ov)
{
    ui64_t q;
    ui64_t r;
    int i;
    int n, m;
    int ovn;

    /* determine actual number of involved digits */
    n = ui64_len(x);
    m = ui64_len(y);

    if (m == 1) {
        /* simple case #1: reduceable to ui64_divn() */
        if (y.x[0] == 0) {
            /* error case: division by zero! */
            ui64_fill(q, 0);
            ui64_fill(r, 0);
        }
        else {
            q = ui64_divn(x, y.x[0], &ovn);
            ui64_fill(r, 0);
            r.x[0] = (unsigned char)ovn;
        }

    } else if (n < m) {
        /* simple case #2: everything is in the remainder */
        ui64_fill(q, 0);
        r = x;

    } else { /* n >= m, m > 1 */
        /* standard case: x[0..n] / y[0..m] */
        UIXX_T(UI64_DIGITS+1) rx;
        UIXX_T(UI64_DIGITS+1) dq;
        ui64_t t;
        int km;
        int k;
        int qk;
        unsigned long y2;
        unsigned long r3;
        int borrow;
        int d;

        /* rx is x with a leading zero in order to make
           sure that n > m and not just n >= m */
        memcpy(rx.x, x.x, UI64_DIGITS);
        rx.x[UI64_DIGITS] = 0;

        for (k = n - m; k >= 0; k--) {
            /* efficiently compute qk by guessing
               qk := rx[k+m-2...k+m]/y[m-2...m-1] */
            km = k + m;
            y2 = (y.x[m-1]*UI64_BASE) + y.x[m-2];
            r3 = (rx.x[km]*(UI64_BASE*UI64_BASE)) +
                 (rx.x[km-1]*UI64_BASE) + rx.x[km-2];
            qk = r3 / y2;
            if (qk >= UI64_BASE)
                qk = UI64_BASE - 1;

            /* dq := y*qk (post-adjust qk if guessed incorrectly) */
            t = ui64_muln(y, qk, &ovn);
            memcpy(dq.x, t.x, UI64_DIGITS);
            dq.x[m] = (unsigned char)ovn;
            for (i = m; i > 0; i--)
                if (rx.x[i+k] != dq.x[i])
                    break;
            if (rx.x[i+k] < dq.x[i]) {
                t = ui64_muln(y, --qk, &ovn);
                memcpy(dq.x, t.x, UI64_DIGITS);
                dq.x[m] = (unsigned char)ovn;
            }

            /* store qk */
            q.x[k] = (unsigned char)qk;

            /* rx := rx - dq*(b^k) */
            borrow = 0;
            for (i = 0; i < m+1; i++) {
                d = ((rx.x[k+i] + UI64_BASE) - borrow - dq.x[i]);
                rx.x[k+i] = (d % UI64_BASE);
                borrow = (1 - (d/UI64_BASE));
            }
        }
        memcpy(r.x, rx.x, m);

        /* fill out results with leading zeros */
        for (i = n-m+1; i < UI64_DIGITS; i++)
            q.x[i] = 0;
        for (i = m; i < UI64_DIGITS; i++)
            r.x[i] = 0;
    }

    /* provide results */
    if (ov != NULL)
        *ov = r;
    return q;
}

ui64_t ui64_divn(ui64_t x, int y, int *ov)
{
    ui64_t z;
    unsigned int carry;
    int i;

    carry = 0;
    for (i = (UI64_DIGITS - 1); i >= 0; i--) {
        carry = (carry * UI64_BASE) + x.x[i];
        z.x[i] = (carry / y);
        carry %= y;
    }
    if (ov != NULL)
        *ov = carry;
    return z;
}

ui64_t ui64_and(ui64_t x, ui64_t y)
{
    ui64_t z;
    int i;

    for (i = 0; i < UI64_DIGITS; i++)
        z.x[i] = (x.x[i] & y.x[i]);
    return z;
}

ui64_t ui64_or(ui64_t x, ui64_t y)
{
    ui64_t z;
    int i;

    for (i = 0; i < UI64_DIGITS; i++)
        z.x[i] = (x.x[i] | y.x[i]);
    return z;
}

ui64_t ui64_xor(ui64_t x, ui64_t y)
{
    ui64_t z;
    int i;

    for (i = 0; i < UI64_DIGITS; i++)
        z.x[i] = ((x.x[i] & ~(y.x[i])) | (~(x.x[i]) & (y.x[i])));
    return z;
}

ui64_t ui64_not(ui64_t x)
{
    ui64_t z;
    int i;

    for (i = 0; i < UI64_DIGITS; i++)
        z.x[i] = ~(x.x[i]);
    return z;
}

ui64_t ui64_rol(ui64_t x, int s, ui64_t *ov)
{
    UIXX_T(UI64_DIGITS+UI64_DIGITS) zx;
    ui64_t z;
    int i;
    int carry;

    if (s <= 0) {
        /* no shift at all */
        if (ov != NULL)
            *ov = ui64_zero();
        return x;
    }
    else if (s > 64) {
        /* too large shift */
        if (ov != NULL)
            *ov = ui64_zero();
        return ui64_zero();
    }
    else if (s == 64) {
        /* maximum shift */
        if (ov != NULL)
            *ov = x;
        return ui64_zero();
    }
    else { /* regular shift */
        /* shift (logically) left by s/8 bytes */
        for (i = 0; i < UI64_DIGITS+UI64_DIGITS; i++)
            zx.x[i] = 0;
        for (i = 0; i < UI64_DIGITS; i++)
            zx.x[i+(s/8)] = x.x[i];
        /* shift (logically) left by remaining s%8 bits */
        s %= 8;
        if (s > 0) {
            carry = 0;
            for (i = 0; i < UI64_DIGITS+UI64_DIGITS; i++) {
                carry += (zx.x[i] * (1 << s));
                zx.x[i] = (carry % UI64_BASE);
                carry /= UI64_BASE;
            }
        }
        memcpy(z.x, zx.x, UI64_DIGITS);
        if (ov != NULL)
            memcpy((*ov).x, &zx.x[UI64_DIGITS], UI64_DIGITS);
    }
    return z;
}

ui64_t ui64_ror(ui64_t x, int s, ui64_t *ov)
{
    UIXX_T(UI64_DIGITS+UI64_DIGITS) zx;
    ui64_t z;
    int i;
    int carry;

    if (s <= 0) {
        /* no shift at all */
        if (ov != NULL)
            *ov = ui64_zero();
        return x;
    }
    else if (s > 64) {
        /* too large shift */
        if (ov != NULL)
            *ov = ui64_zero();
        return ui64_zero();
    }
    else if (s == 64) {
        /* maximum shift */
        if (ov != NULL)
            *ov = x;
        return ui64_zero();
    }
    else { /* regular shift */
        /* shift (logically) right by s/8 bytes */
        for (i = 0; i < UI64_DIGITS+UI64_DIGITS; i++)
            zx.x[i] = 0;
        for (i = 0; i < UI64_DIGITS; i++)
            zx.x[UI64_DIGITS+i-(s/8)] = x.x[i];
        /* shift (logically) right by remaining s%8 bits */
        s %= 8;
        if (s > 0) {
            carry = 0;
            for (i = (UI64_DIGITS+UI64_DIGITS - 1); i >= 0; i--) {
                carry = (carry * UI64_BASE) + zx.x[i];
                zx.x[i] = (carry / (1 << s));
                carry %= (1 << s);
            }
        }
        memcpy(z.x, &zx.x[UI64_DIGITS], UI64_DIGITS);
        if (ov != NULL)
            memcpy((*ov).x, zx.x, UI64_DIGITS);
    }
    return z;
}

int ui64_cmp(ui64_t x, ui64_t y)
{
    int i;

    i = UI64_DIGITS - 1;
    while (i > 0 && x.x[i] == y.x[i])
        i--;
    return (x.x[i] - y.x[i]);
}

int ui64_len(ui64_t x)
{
    int i;

    for (i = UI64_DIGITS; i > 1 && x.x[i-1] == 0; i--)
        ;
    return i;
}

/*
**  ui128.c: implementation of 128-bit unsigned integer arithmetic
*/

#define UI128_BASE   256 /* 2^8 */
#define UI128_DIGITS 16  /* 8*16 = 128 bit */
#define UIXX_T(n) struct { unsigned char x[n]; }

/* fill an ui128_t with a sequence of a particular digit */
#define ui128_fill(__x, __n) \
    /*lint -save -e717*/ \
    do { int __i; \
      for (__i = 0; __i < UI128_DIGITS; __i++) \
          (__x).x[__i] = (__n); \
    } while (0) \
    /*lint -restore*/

/* the value zero */
ui128_t ui128_zero(void)
{
    ui128_t z;

    ui128_fill(z, 0);
    return z;
}

/* the maximum value */
ui128_t ui128_max(void)
{
    ui128_t z;

    ui128_fill(z, UI128_BASE-1);
    return z;
}

/* convert ISO-C "unsigned long" into internal format */
ui128_t ui128_n2i(unsigned long n)
{
    ui128_t z;
    int i;

    i = 0;
    do {
        z.x[i++] = (n % UI128_BASE);
    } while ((n /= UI128_BASE) > 0 && i < UI128_DIGITS);
    for ( ; i < UI128_DIGITS; i++)
        z.x[i] = 0;
    return z;
}

/* convert internal format into ISO-C "unsigned long";
   truncates if sizeof(unsigned long) is less than UI128_DIGITS! */
unsigned long ui128_i2n(ui128_t x)
{
    unsigned long n;
    int i;

    n = 0;
    i = (int)sizeof(n);
    /*lint -save -e774*/
    if (i > UI128_DIGITS)
        i = UI128_DIGITS;
    /*lint -restore*/
    while (--i >= 0) {
        n = (n * UI128_BASE) + x.x[i];
    }
    return n;
}

/* convert string representation of arbitrary base into internal format */
ui128_t ui128_s2i(const char *str, char **end, int base)
{
    ui128_t z;
    const char *cp;
    int carry;
    static char map[] = {
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9,             /* 0...9 */
        36, 36, 36, 36, 36, 36, 36,                         /* illegal chars */
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, /* A...M */
        23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, /* N...Z */
        36, 36, 36, 36, 36, 36,                             /* illegal chars */
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, /* a...m */
        23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35  /* m...z */
    };

    ui128_fill(z, 0);
    if (str == NULL || (base < 2 || base > 36))
        return z;
    cp = str;
    while (*cp != '\0' && isspace((int)(*cp)))
        cp++;
    while (   *cp != '\0'
           && isalnum((int)(*cp))
           && map[(int)(*cp)-'0'] < base) {
        z = ui128_muln(z, base, &carry);
        if (carry)
            break;
        z = ui128_addn(z, map[(int)(*cp)-'0'], &carry);
        if (carry)
            break;
        cp++;
    }
    if (end != NULL)
        *end = (char *)cp;
    return z;
}

/* convert internal format into string representation of arbitrary base */
char *ui128_i2s(ui128_t x, char *str, size_t len, int base)
{
    static char map[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char c;
    int r;
    int n;
    int i, j;

    if (str == NULL || len < 2 || (base < 2 || base > 36))
        return NULL;
    n = ui128_len(x);
    i = 0;
    do {
        x = ui128_divn(x, base, &r);
        str[i++] = map[r];
        while (n > 1 && x.x[n-1] == 0)
            n--;
    } while (i < ((int)len-1) && (n > 1 || x.x[0] != 0));
    str[i] = '\0';
    for (j = 0; j < --i; j++) {
        c = str[j];
        str[j] = str[i];
        str[i] = c;
    }
    return str;
}

/* addition of two ui128_t */
ui128_t ui128_add(ui128_t x, ui128_t y, ui128_t *ov)
{
    ui128_t z;
    int carry;
    int i;

    carry = 0;
    for (i = 0; i < UI128_DIGITS; i++) {
        carry += (x.x[i] + y.x[i]);
        z.x[i] = (carry % UI128_BASE);
        carry /= UI128_BASE;
    }
    if (ov != NULL)
        *ov = ui128_n2i((unsigned long)carry);
    return z;
}

/* addition of an ui128_t and a single digit */
ui128_t ui128_addn(ui128_t x, int y, int *ov)
{
    ui128_t z;
    int i;

    for (i = 0; i < UI128_DIGITS; i++) {
        y += x.x[i];
        z.x[i] = (y % UI128_BASE);
        y /= UI128_BASE;
    }
    if (ov != NULL)
        *ov = y;
    return z;
}

/* subtraction of two ui128_t */
ui128_t ui128_sub(ui128_t x, ui128_t y, ui128_t *ov)
{
    ui128_t z;
    int borrow;
    int i;
    int d;

    borrow = 0;
    for (i = 0; i < UI128_DIGITS; i++) {
        d = ((x.x[i] + UI128_BASE) - borrow - y.x[i]);
        z.x[i] = (d % UI128_BASE);
        borrow = (1 - (d/UI128_BASE));
    }
    if (ov != NULL)
        *ov = ui128_n2i((unsigned long)borrow);
    return z;
}

/* subtraction of an ui128_t and a single digit */
ui128_t ui128_subn(ui128_t x, int y, int *ov)
{
    ui128_t z;
    int i;
    int d;

    for (i = 0; i < UI128_DIGITS; i++) {
        d = (x.x[i] + UI128_BASE) - y;
        z.x[i] = (d % UI128_BASE);
        y = (1 - (d/UI128_BASE));
    }
    if (ov != NULL)
        *ov = y;
    return z;
}

/*
             7 3 2
         * 9 4 2 8
         ---------
           5 8 5 6
   +     1 4 6 4
   +   2 9 2 8
   + 6 5 8 8
   ---------------
   = 6 9 0 1 2 9 6
*/

ui128_t ui128_mul(ui128_t x, ui128_t y, ui128_t *ov)
{
    UIXX_T(UI128_DIGITS+UI128_DIGITS) zx;
    ui128_t z;
    int carry;
    int i, j;

    /* clear temporary result buffer */
    for (i = 0; i < (UI128_DIGITS+UI128_DIGITS); i++)
        zx.x[i] = 0;

    /* perform multiplication operation */
    for (i = 0; i < UI128_DIGITS; i++) {
        /* calculate partial product and immediately add to z */
        carry = 0;
        for (j = 0; j < UI128_DIGITS; j++) {
            carry += (x.x[i] * y.x[j]) + zx.x[i+j];
            zx.x[i+j] = (carry % UI128_BASE);
            carry /= UI128_BASE;
        }
        /* add carry to remaining digits in z */
        for ( ; j < UI128_DIGITS + UI128_DIGITS - i; j++) {
            carry += zx.x[i+j];
            zx.x[i+j] = (carry % UI128_BASE);
            carry /= UI128_BASE;
        }
    }

    /* provide result by splitting zx into z and ov */
    memcpy(z.x, zx.x, UI128_DIGITS);
    if (ov != NULL)
        memcpy((*ov).x, &zx.x[UI128_DIGITS], UI128_DIGITS);

    return z;
}

ui128_t ui128_muln(ui128_t x, int y, int *ov)
{
    ui128_t z;
    int carry;
    int i;

    carry = 0;
    for (i = 0; i < UI128_DIGITS; i++) {
        carry += (x.x[i] * y);
        z.x[i] = (carry % UI128_BASE);
        carry /= UI128_BASE;
    }
    if (ov != NULL)
        *ov = carry;
    return z;
}

/*
  =   2078 [q]
   0615367 [x] : 296 [y]
  -0592    [dq]
  -----
  = 0233
   -0000   [dq]
   -----
   = 2336
    -2072  [dq]
    -----
    = 2647
     -2308 [dq]
     -----
     = 279 [r]
 */
ui128_t ui128_div(ui128_t x, ui128_t y, ui128_t *ov)
{
    ui128_t q;
    ui128_t r;
    int i;
    int n, m;
    int ovn;

    /* determine actual number of involved digits */
    n = ui128_len(x);
    m = ui128_len(y);

    if (m == 1) {
        /* simple case #1: reduceable to ui128_divn() */
        if (y.x[0] == 0) {
            /* error case: division by zero! */
            ui128_fill(q, 0);
            ui128_fill(r, 0);
        }
        else {
            q = ui128_divn(x, y.x[0], &ovn);
            ui128_fill(r, 0);
            r.x[0] = (unsigned char)ovn;
        }

    } else if (n < m) {
        /* simple case #2: everything is in the remainder */
        ui128_fill(q, 0);
        r = x;

    } else { /* n >= m, m > 1 */
        /* standard case: x[0..n] / y[0..m] */
        UIXX_T(UI128_DIGITS+1) rx;
        UIXX_T(UI128_DIGITS+1) dq;
        ui128_t t;
        int km;
        int k;
        int qk;
        unsigned long y2;
        unsigned long r3;
        int borrow;
        int d;

        /* rx is x with a leading zero in order to make
           sure that n > m and not just n >= m */
        memcpy(rx.x, x.x, UI128_DIGITS);
        rx.x[UI128_DIGITS] = 0;

        for (k = n - m; k >= 0; k--) {
            /* efficiently compute qk by guessing
               qk := rx[k+m-2...k+m]/y[m-2...m-1] */
            km = k + m;
            y2 = (y.x[m-1]*UI128_BASE) + y.x[m-2];
            r3 = (rx.x[km]*(UI128_BASE*UI128_BASE)) +
                 (rx.x[km-1]*UI128_BASE) + rx.x[km-2];
            qk = r3 / y2;
            if (qk >= UI128_BASE)
                qk = UI128_BASE - 1;

            /* dq := y*qk (post-adjust qk if guessed incorrectly) */
            t = ui128_muln(y, qk, &ovn);
            memcpy(dq.x, t.x, UI128_DIGITS);
            dq.x[m] = (unsigned char)ovn;
            for (i = m; i > 0; i--)
                if (rx.x[i+k] != dq.x[i])
                    break;
            if (rx.x[i+k] < dq.x[i]) {
                t = ui128_muln(y, --qk, &ovn);
                memcpy(dq.x, t.x, UI128_DIGITS);
                dq.x[m] = (unsigned char)ovn;
            }

            /* store qk */
            q.x[k] = (unsigned char)qk;

            /* rx := rx - dq*(b^k) */
            borrow = 0;
            for (i = 0; i < m+1; i++) {
                d = ((rx.x[k+i] + UI128_BASE) - borrow - dq.x[i]);
                rx.x[k+i] = (d % UI128_BASE);
                borrow = (1 - (d/UI128_BASE));
            }
        }
        memcpy(r.x, rx.x, m);

        /* fill out results with leading zeros */
        for (i = n-m+1; i < UI128_DIGITS; i++)
            q.x[i] = 0;
        for (i = m; i < UI128_DIGITS; i++)
            r.x[i] = 0;
    }

    /* provide results */
    if (ov != NULL)
        *ov = r;
    return q;
}

ui128_t ui128_divn(ui128_t x, int y, int *ov)
{
    ui128_t z;
    unsigned int carry;
    int i;

    carry = 0;
    for (i = (UI128_DIGITS - 1); i >= 0; i--) {
        carry = (carry * UI128_BASE) + x.x[i];
        z.x[i] = (carry / y);
        carry %= y;
    }
    if (ov != NULL)
        *ov = carry;
    return z;
}

ui128_t ui128_and(ui128_t x, ui128_t y)
{
    ui128_t z;
    int i;

    for (i = 0; i < UI128_DIGITS; i++)
        z.x[i] = (x.x[i] & y.x[i]);
    return z;
}

ui128_t ui128_or(ui128_t x, ui128_t y)
{
    ui128_t z;
    int i;

    for (i = 0; i < UI128_DIGITS; i++)
        z.x[i] = (x.x[i] | y.x[i]);
    return z;
}

ui128_t ui128_xor(ui128_t x, ui128_t y)
{
    ui128_t z;
    int i;

    for (i = 0; i < UI128_DIGITS; i++)
        z.x[i] = ((x.x[i] & ~(y.x[i])) | (~(x.x[i]) & (y.x[i])));
    return z;
}

ui128_t ui128_not(ui128_t x)
{
    ui128_t z;
    int i;

    for (i = 0; i < UI128_DIGITS; i++)
        z.x[i] = ~(x.x[i]);
    return z;
}

ui128_t ui128_rol(ui128_t x, int s, ui128_t *ov)
{
    UIXX_T(UI128_DIGITS+UI128_DIGITS) zx;
    ui128_t z;
    int i;
    int carry;

    if (s <= 0) {
        /* no shift at all */
        if (ov != NULL)
            *ov = ui128_zero();
        return x;
    }
    else if (s > 128) {
        /* too large shift */
        if (ov != NULL)
            *ov = ui128_zero();
        return ui128_zero();
    }
    else if (s == 128) {
        /* maximum shift */
        if (ov != NULL)
            *ov = x;
        return ui128_zero();
    }
    else { /* regular shift */
        /* shift (logically) left by s/8 bytes */
        for (i = 0; i < UI128_DIGITS+UI128_DIGITS; i++)
            zx.x[i] = 0;
        for (i = 0; i < UI128_DIGITS; i++)
            zx.x[i+(s/8)] = x.x[i];
        /* shift (logically) left by remaining s%8 bits */
        s %= 8;
        if (s > 0) {
            carry = 0;
            for (i = 0; i < UI128_DIGITS+UI128_DIGITS; i++) {
                carry += (zx.x[i] * (1 << s));
                zx.x[i] = (carry % UI128_BASE);
                carry /= UI128_BASE;
            }
        }
        memcpy(z.x, zx.x, UI128_DIGITS);
        if (ov != NULL)
            memcpy((*ov).x, &zx.x[UI128_DIGITS], UI128_DIGITS);
    }
    return z;
}

ui128_t ui128_ror(ui128_t x, int s, ui128_t *ov)
{
    UIXX_T(UI128_DIGITS+UI128_DIGITS) zx;
    ui128_t z;
    int i;
    int carry;

    if (s <= 0) {
        /* no shift at all */
        if (ov != NULL)
            *ov = ui128_zero();
        return x;
    }
    else if (s > 128) {
        /* too large shift */
        if (ov != NULL)
            *ov = ui128_zero();
        return ui128_zero();
    }
    else if (s == 128) {
        /* maximum shift */
        if (ov != NULL)
            *ov = x;
        return ui128_zero();
    }
    else { /* regular shift */
        /* shift (logically) right by s/8 bytes */
        for (i = 0; i < UI128_DIGITS+UI128_DIGITS; i++)
            zx.x[i] = 0;
        for (i = 0; i < UI128_DIGITS; i++)
            zx.x[UI128_DIGITS+i-(s/8)] = x.x[i];
        /* shift (logically) right by remaining s%8 bits */
        s %= 8;
        if (s > 0) {
            carry = 0;
            for (i = (UI128_DIGITS+UI128_DIGITS - 1); i >= 0; i--) {
                carry = (carry * UI128_BASE) + zx.x[i];
                zx.x[i] = (carry / (1 << s));
                carry %= (1 << s);
            }
        }
        memcpy(z.x, &zx.x[UI128_DIGITS], UI128_DIGITS);
        if (ov != NULL)
            memcpy((*ov).x, zx.x, UI128_DIGITS);
    }
    return z;
}

int ui128_cmp(ui128_t x, ui128_t y)
{
    int i;

    i = UI128_DIGITS - 1;
    while (i > 0 && x.x[i] == y.x[i])
        i--;
    return (x.x[i] - y.x[i]);
}

int ui128_len(ui128_t x)
{
    int i;

    for (i = UI128_DIGITS; i > 1 && x.x[i-1] == 0; i--)
        ;
    return i;
}

/*
**  uuid_str.c: string formatting functions
*/

/*
 * Copyright Patrick Powell 1995
 * This code is based on code written by Patrick Powell <papowell@astart.com>
 * It may be used for any purpose as long as this notice remains intact
 * on all source code distributions.
 */

/*
 * This code contains numerious changes and enhancements which were
 * made by lots of contributors over the last years to Patrick Powell's
 * original code:
 *
 * o Patrick Powell <papowell@astart.com>      (1995)
 * o Brandon Long <blong@fiction.net>          (1996, for Mutt)
 * o Thomas Roessler <roessler@guug.de>        (1998, for Mutt)
 * o Michael Elkins <me@cs.hmc.edu>            (1998, for Mutt)
 * o Andrew Tridgell <tridge@samba.org>        (1998, for Samba)
 * o Luke Mewburn <lukem@netbsd.org>           (1999, for LukemFTP)
 * o Ralf S. Engelschall <rse@engelschall.com> (1999, for OSSP)
 */

#if HAVE_LONG_LONG
#define LLONG long long
#else
#define LLONG long
#endif

#if HAVE_LONG_DOUBLE
#define LDOUBLE long double
#else
#define LDOUBLE double
#endif

static void fmtstr     (char *, size_t *, size_t, char *, int, int, int);
static void fmtint     (char *, size_t *, size_t, LLONG, int, int, int, int);
static void fmtfp      (char *, size_t *, size_t, LDOUBLE, int, int, int);
static void dopr_outch (char *, size_t *, size_t, int);

/* format read states */
#define DP_S_DEFAULT    0
#define DP_S_FLAGS      1
#define DP_S_MIN        2
#define DP_S_DOT        3
#define DP_S_MAX        4
#define DP_S_MOD        5
#define DP_S_CONV       6
#define DP_S_DONE       7

/* format flags - Bits */
#define DP_F_MINUS      (1 << 0)
#define DP_F_PLUS       (1 << 1)
#define DP_F_SPACE      (1 << 2)
#define DP_F_NUM        (1 << 3)
#define DP_F_ZERO       (1 << 4)
#define DP_F_UP         (1 << 5)
#define DP_F_UNSIGNED   (1 << 6)

/* conversion flags */
#define DP_C_SHORT      1
#define DP_C_LONG       2
#define DP_C_LDOUBLE    3
#define DP_C_LLONG      4

/* some handy macros */
#define char_to_int(p) (p - '0')
#define STR_MAX(p,q) ((p >= q) ? p : q)
#define NUL '\0'

static void
dopr(
    char *buffer,
    size_t maxlen,
    size_t *retlen,
    const char *format,
    va_list args)
{
    char ch;
    LLONG value;
    LDOUBLE fvalue;
    char *strvalue;
    int min;
    int max;
    int state;
    int flags;
    int cflags;
    size_t currlen;

    state = DP_S_DEFAULT;
    flags = currlen = cflags = min = 0;
    max = -1;
    ch = *format++;

    if (buffer == NULL)
        maxlen = 999999;

    while (state != DP_S_DONE) {
        if ((ch == NUL) || (currlen >= maxlen))
            state = DP_S_DONE;

        switch (state) {
        case DP_S_DEFAULT:
            if (ch == '%')
                state = DP_S_FLAGS;
            else
                dopr_outch(buffer, &currlen, maxlen, ch);
            ch = *format++;
            break;
        case DP_S_FLAGS:
            switch (ch) {
                case '-':
                    flags |= DP_F_MINUS;
                    ch = *format++;
                    break;
                case '+':
                    flags |= DP_F_PLUS;
                    ch = *format++;
                    break;
                case ' ':
                    flags |= DP_F_SPACE;
                    ch = *format++;
                    break;
                case '#':
                    flags |= DP_F_NUM;
                    ch = *format++;
                    break;
                case '0':
                    flags |= DP_F_ZERO;
                    ch = *format++;
                    break;
                default:
                    state = DP_S_MIN;
                    break;
            }
            break;
        case DP_S_MIN:
            if (isdigit((unsigned char)ch)) {
                min = 10 * min + char_to_int(ch);
                ch = *format++;
            } else if (ch == '*') {
                min = va_arg(args, int);
                ch = *format++;
                state = DP_S_DOT;
            } else
                state = DP_S_DOT;
            break;
        case DP_S_DOT:
            if (ch == '.') {
                state = DP_S_MAX;
                ch = *format++;
            } else
                state = DP_S_MOD;
            break;
        case DP_S_MAX:
            if (isdigit((unsigned char)ch)) {
                if (max < 0)
                    max = 0;
                max = 10 * max + char_to_int(ch);
                ch = *format++;
            } else if (ch == '*') {
                max = va_arg(args, int);
                ch = *format++;
                state = DP_S_MOD;
            } else
                state = DP_S_MOD;
            break;
        case DP_S_MOD:
            switch (ch) {
                case 'h':
                    cflags = DP_C_SHORT;
                    ch = *format++;
                    break;
                case 'l':
                    if (*format == 'l') {
                        cflags = DP_C_LLONG;
                        format++;
                    } else
                        cflags = DP_C_LONG;
                    ch = *format++;
                    break;
                case 'q':
                    cflags = DP_C_LLONG;
                    ch = *format++;
                    break;
                case 'L':
                    cflags = DP_C_LDOUBLE;
                    ch = *format++;
                    break;
                default:
                    break;
            }
            state = DP_S_CONV;
            break;
        case DP_S_CONV:
            switch (ch) {
            case 'd':
            case 'i':
                switch (cflags) {
                case DP_C_SHORT:
                    value = (short int)va_arg(args, int);
                    break;
                case DP_C_LONG:
                    value = va_arg(args, long int);
                    break;
                case DP_C_LLONG:
                    value = va_arg(args, LLONG);
                    break;
                default:
                    value = va_arg(args, int);
                    break;
                }
                fmtint(buffer, &currlen, maxlen, value, 10, min, max, flags);
                break;
            case 'X':
                flags |= DP_F_UP;
                /* FALLTHROUGH */
            case 'x':
            case 'o':
            case 'u':
                flags |= DP_F_UNSIGNED;
                switch (cflags) {
                    case DP_C_SHORT:
                        value = (unsigned short int)va_arg(args, unsigned int);
                        break;
                    case DP_C_LONG:
                        value = (LLONG)va_arg(args, unsigned long int);
                        break;
                    case DP_C_LLONG:
                        value = va_arg(args, unsigned LLONG);
                        break;
                    default:
                        value = (LLONG)va_arg(args, unsigned int);
                        break;
                }
                fmtint(buffer, &currlen, maxlen, value,
                       ch == 'o' ? 8 : (ch == 'u' ? 10 : 16),
                       min, max, flags);
                break;
            case 'f':
                if (cflags == DP_C_LDOUBLE)
                    fvalue = va_arg(args, LDOUBLE);
                else
                    fvalue = va_arg(args, double);
                fmtfp(buffer, &currlen, maxlen, fvalue, min, max, flags);
                break;
            case 'E':
                flags |= DP_F_UP;
                /* FALLTHROUGH */
            case 'e':
                if (cflags == DP_C_LDOUBLE)
                    fvalue = va_arg(args, LDOUBLE);
                else
                    fvalue = va_arg(args, double);
                break;
            case 'G':
                flags |= DP_F_UP;
                /* FALLTHROUGH */
            case 'g':
                if (cflags == DP_C_LDOUBLE)
                    fvalue = va_arg(args, LDOUBLE);
                else
                    fvalue = va_arg(args, double);
                break;
            case 'c':
                dopr_outch(buffer, &currlen, maxlen, va_arg(args, int));
                break;
            case 's':
                strvalue = va_arg(args, char *);
                if (max < 0)
                    max = maxlen;
                fmtstr(buffer, &currlen, maxlen, strvalue, flags, min, max);
                break;
            case 'p':
                value = (long)va_arg(args, void *);
                fmtint(buffer, &currlen, maxlen, value, 16, min, max, flags);
                break;
            case 'n': /* XXX */
                if (cflags == DP_C_SHORT) {
                    short int *num;
                    num = va_arg(args, short int *);
                    *num = currlen;
                } else if (cflags == DP_C_LONG) { /* XXX */
                    long int *num;
                    num = va_arg(args, long int *);
                    *num = (long int) currlen;
                } else if (cflags == DP_C_LLONG) { /* XXX */
                    LLONG *num;
                    num = va_arg(args, LLONG *);
                    *num = (LLONG) currlen;
                } else {
                    int *num;
                    num = va_arg(args, int *);
                    *num = currlen;
                }
                break;
            case '%':
                dopr_outch(buffer, &currlen, maxlen, ch);
                break;
            case 'w':
                /* not supported yet, treat as next char */
                ch = *format++;
                break;
            default:
                /* unknown, skip */
                break;
            }
            ch = *format++;
            state = DP_S_DEFAULT;
            flags = cflags = min = 0;
            max = -1;
            break;
        case DP_S_DONE:
            break;
        default:
            break;
        }
    }
    if (currlen >= maxlen - 1)
        currlen = maxlen - 1;
    if (buffer != NULL)
        buffer[currlen] = NUL;
    *retlen = currlen;
    return;
}

static void
fmtstr(
    char *buffer,
    size_t *currlen,
    size_t maxlen,
    char *value,
    int flags,
    int min,
    int max)
{
    int padlen, strln;
    int cnt = 0;

    if (value == NULL)
        value = "<NULL>";
    for (strln = 0; value[strln] != '\0'; strln++)
        ;
    padlen = min - strln;
    if (padlen < 0)
        padlen = 0;
    if (flags & DP_F_MINUS)
        padlen = -padlen;

    while ((padlen > 0) && (cnt < max)) {
        dopr_outch(buffer, currlen, maxlen, ' ');
        --padlen;
        ++cnt;
    }
    while (*value && (cnt < max)) {
        dopr_outch(buffer, currlen, maxlen, *value++);
        ++cnt;
    }
    while ((padlen < 0) && (cnt < max)) {
        dopr_outch(buffer, currlen, maxlen, ' ');
        ++padlen;
        ++cnt;
    }
}

static void
fmtint(
    char *buffer,
    size_t *currlen,
    size_t maxlen,
    LLONG value,
    int base,
    int min,
    int max,
    int flags)
{
    int signvalue = 0;
    unsigned LLONG uvalue;
    char convert[20];
    int place = 0;
    int spadlen = 0;
    int zpadlen = 0;
    int caps = 0;

    if (max < 0)
        max = 0;
    uvalue = value;
    if (!(flags & DP_F_UNSIGNED)) {
        if (value < 0) {
            signvalue = '-';
            uvalue = -value;
        } else if (flags & DP_F_PLUS)
            signvalue = '+';
        else if (flags & DP_F_SPACE)
            signvalue = ' ';
    }
    if (flags & DP_F_UP)
        caps = 1;
    do {
        convert[place++] =
            (caps ? "0123456789ABCDEF" : "0123456789abcdef")
            [uvalue % (unsigned) base];
        uvalue = (uvalue / (unsigned) base);
    } while (uvalue && (place < 20));
    if (place == 20)
        place--;
    convert[place] = 0;

    zpadlen = max - place;
    spadlen = min - STR_MAX(max, place) - (signvalue ? 1 : 0);
    if (zpadlen < 0)
        zpadlen = 0;
    if (spadlen < 0)
        spadlen = 0;
    if (flags & DP_F_ZERO) {
        zpadlen = STR_MAX(zpadlen, spadlen);
        spadlen = 0;
    }
    if (flags & DP_F_MINUS)
        spadlen = -spadlen;

    /* spaces */
    while (spadlen > 0) {
        dopr_outch(buffer, currlen, maxlen, ' ');
        --spadlen;
    }

    /* sign */
    if (signvalue)
        dopr_outch(buffer, currlen, maxlen, signvalue);

    /* zeros */
    if (zpadlen > 0) {
        while (zpadlen > 0) {
            dopr_outch(buffer, currlen, maxlen, '0');
            --zpadlen;
        }
    }
    /* digits */
    while (place > 0)
        dopr_outch(buffer, currlen, maxlen, convert[--place]);

    /* left justified spaces */
    while (spadlen < 0) {
        dopr_outch(buffer, currlen, maxlen, ' ');
        ++spadlen;
    }
    return;
}

static LDOUBLE
math_abs(LDOUBLE value)
{
    LDOUBLE result = value;
    if (value < 0)
        result = -value;
    return result;
}

static LDOUBLE
math_pow10(int exponent)
{
    LDOUBLE result = 1;
    while (exponent > 0) {
        result *= 10;
        exponent--;
    }
    return result;
}

static long
math_round(LDOUBLE value)
{
    long intpart;
    intpart = (long) value;
    value = value - intpart;
    if (value >= 0.5)
        intpart++;
    return intpart;
}

static void
fmtfp(
    char *buffer,
    size_t *currlen,
    size_t maxlen,
    LDOUBLE fvalue,
    int min,
    int max,
    int flags)
{
    int signvalue = 0;
    LDOUBLE ufvalue;
    char iconvert[20];
    char fconvert[20];
    int iplace = 0;
    int fplace = 0;
    int padlen = 0;
    int zpadlen = 0;
    int caps = 0;
    long intpart;
    long fracpart;

    if (max < 0)
        max = 6;
    ufvalue = math_abs(fvalue);
    if (fvalue < 0)
        signvalue = '-';
    else if (flags & DP_F_PLUS)
        signvalue = '+';
    else if (flags & DP_F_SPACE)
        signvalue = ' ';

    intpart = (long)ufvalue;

    /* sorry, we only support 9 digits past the decimal because of our
       conversion method */
    if (max > 9)
        max = 9;

    /* we "cheat" by converting the fractional part to integer by
       multiplying by a factor of 10 */
    fracpart = math_round((math_pow10(max)) * (ufvalue - intpart));

    if (fracpart >= math_pow10(max)) {
        intpart++;
        fracpart -= (long)math_pow10(max);
    }

    /* convert integer part */
    do {
        iconvert[iplace++] =
            (caps ? "0123456789ABCDEF"
              : "0123456789abcdef")[intpart % 10];
        intpart = (intpart / 10);
    } while (intpart && (iplace < 20));
    if (iplace == 20)
        iplace--;
    iconvert[iplace] = 0;

    /* convert fractional part */
    do {
        fconvert[fplace++] =
            (caps ? "0123456789ABCDEF"
              : "0123456789abcdef")[fracpart % 10];
        fracpart = (fracpart / 10);
    } while (fracpart && (fplace < 20));
    if (fplace == 20)
        fplace--;
    fconvert[fplace] = 0;

    /* -1 for decimal point, another -1 if we are printing a sign */
    padlen = min - iplace - max - 1 - ((signvalue) ? 1 : 0);
    zpadlen = max - fplace;
    if (zpadlen < 0)
        zpadlen = 0;
    if (padlen < 0)
        padlen = 0;
    if (flags & DP_F_MINUS)
        padlen = -padlen;

    if ((flags & DP_F_ZERO) && (padlen > 0)) {
        if (signvalue) {
            dopr_outch(buffer, currlen, maxlen, signvalue);
            --padlen;
            signvalue = 0;
        }
        while (padlen > 0) {
            dopr_outch(buffer, currlen, maxlen, '0');
            --padlen;
        }
    }
    while (padlen > 0) {
        dopr_outch(buffer, currlen, maxlen, ' ');
        --padlen;
    }
    if (signvalue)
        dopr_outch(buffer, currlen, maxlen, signvalue);

    while (iplace > 0)
        dopr_outch(buffer, currlen, maxlen, iconvert[--iplace]);

    /*
     * Decimal point. This should probably use locale to find the correct
     * char to print out.
     */
    if (max > 0) {
        dopr_outch(buffer, currlen, maxlen, '.');

        while (fplace > 0)
            dopr_outch(buffer, currlen, maxlen, fconvert[--fplace]);
    }
    while (zpadlen > 0) {
        dopr_outch(buffer, currlen, maxlen, '0');
        --zpadlen;
    }

    while (padlen < 0) {
        dopr_outch(buffer, currlen, maxlen, ' ');
        ++padlen;
    }
    return;
}

static void
dopr_outch(
    char *buffer,
    size_t *currlen,
    size_t maxlen,
    int c)
{
    if (*currlen < maxlen) {
        if (buffer != NULL)
            buffer[(*currlen)] = (char)c;
        (*currlen)++;
    }
    return;
}

int
str_vsnprintf(
    char *str,
    size_t count,
    const char *fmt,
    va_list args)
{
    size_t retlen;

    if (str != NULL)
        str[0] = NUL;
    dopr(str, count, &retlen, fmt, args);
    return retlen;
}

int
str_snprintf(
    char *str,
    size_t count,
    const char *fmt,
    ...)
{
    va_list ap;
    int rv;

    va_start(ap, fmt);
    rv = str_vsnprintf(str, count, fmt, ap);
    va_end(ap);
    return rv;
}

char *
str_vasprintf(
    const char *fmt,
    va_list ap)
{
    char *rv;
    int n;
    va_list ap_tmp;

    va_copy(ap_tmp, ap);
    n = str_vsnprintf(NULL, 0, fmt, ap_tmp);
    if ((rv = (char *)malloc(n+1)) == NULL)
        return NULL;
    str_vsnprintf(rv, n+1, fmt, ap);
    return rv;
}

char *
str_asprintf(
    const char *fmt,
    ...)
{
    va_list ap;
    char *rv;

    va_start(ap, fmt);
    rv = str_vasprintf(fmt, ap);
    va_end(ap);
    return rv;
}

int
str_vrsprintf(
    char **str,
    const char *fmt,
    va_list ap)
{
    int rv;
    size_t n;
    va_list ap_tmp;

    if (str == NULL)
        return -1;
    if (*str == NULL) {
        *str = str_vasprintf(fmt, ap);
        rv = strlen(*str);
    }
    else {
        va_copy(ap_tmp, ap);
        n = strlen(*str);
        rv = str_vsnprintf(NULL, 0, fmt, ap_tmp);
        if ((*str = (char *)realloc(*str, n+rv+1)) == NULL)
            return -1;
        str_vsnprintf((*str)+n, rv+1, fmt, ap);
    }
    return rv;
}

int
str_rsprintf(
    char **str,
    const char *fmt,
    ...)
{
    va_list ap;
    int rv;

    va_start(ap, fmt);
    rv = str_vrsprintf(str, fmt, ap);
    va_end(ap);
    return rv;
}

