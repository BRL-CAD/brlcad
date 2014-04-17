/*
   b64.c - c source to a base64 encoding algorithm implementation

   This is a reworked version of the C encoder/decoder from the libb64
   project, and has been placed in the public domain.
   For details, see http://sourceforge.net/projects/libb64
   */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "bu/cv.h"
#include "bu/malloc.h"

typedef enum {
        step_A, step_B, step_C
} bu_b64_encodestep;

typedef enum {
        step_a, step_b, step_c, step_d
} bu_b64_decodestep;


typedef struct {
        bu_b64_encodestep step;
	    char result;
	        int stepcount;
} bu_b64_encodestate;

typedef struct {
        bu_b64_decodestep step;
	    char plainchar;
} bu_b64_decodestate;

const int CHARS_PER_LINE = 72;

HIDDEN
void bu_b64_init_encodestate(bu_b64_encodestate* state_in)
{
    state_in->step = step_A;
    state_in->result = 0;
    state_in->stepcount = 0;
}

HIDDEN
char bu_b64_encode_value(char value_in)
{
    static const char* encoding = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (value_in > 63) return '=';
    return encoding[(int)value_in];
}

HIDDEN
int bu_b64_encode_block_internal(const char* plaintext_in, int length_in, char* code_out, bu_b64_encodestate* state_in)
{
    const char* plainchar = plaintext_in;
    const char* const plaintextend = plaintext_in + length_in;
    char* codechar = code_out;
    char result;
    char fragment;

    result = state_in->result;

    switch (state_in->step)
    {
	while (1)
	{
	    case step_A:
		if (plainchar == plaintextend)
		{
		    state_in->result = result;
		    state_in->step = step_A;
		    return codechar - code_out;
		}
		fragment = *plainchar++;
		result = (fragment & 0x0fc) >> 2;
		*codechar++ = bu_b64_encode_value(result);
		result = (fragment & 0x003) << 4;
	    case step_B:
		if (plainchar == plaintextend)
		{
		    state_in->result = result;
		    state_in->step = step_B;
		    return codechar - code_out;
		}
		fragment = *plainchar++;
		result |= (fragment & 0x0f0) >> 4;
		*codechar++ = bu_b64_encode_value(result);
		result = (fragment & 0x00f) << 2;
	    case step_C:
		if (plainchar == plaintextend)
		{
		    state_in->result = result;
		    state_in->step = step_C;
		    return codechar - code_out;
		}
		fragment = *plainchar++;
		result |= (fragment & 0x0c0) >> 6;
		*codechar++ = bu_b64_encode_value(result);
		result  = (fragment & 0x03f) >> 0;
		*codechar++ = bu_b64_encode_value(result);

		++(state_in->stepcount);
		if (state_in->stepcount == CHARS_PER_LINE/4)
		{
		    *codechar++ = '\n';
		    state_in->stepcount = 0;
		}
	}
    }
    /* control should not reach here */
    return codechar - code_out;
}

HIDDEN
int bu_b64_encode_blockend(char* code_out, bu_b64_encodestate* state_in)
{
    char* codechar = code_out;

    switch (state_in->step)
    {
	case step_B:
	    *codechar++ = bu_b64_encode_value(state_in->result);
	    *codechar++ = '=';
	    *codechar++ = '=';
	    break;
	case step_C:
	    *codechar++ = bu_b64_encode_value(state_in->result);
	    *codechar++ = '=';
	    break;
	case step_A:
	    break;
    }
    *codechar++ = '\n';

    return codechar - code_out;
}

HIDDEN
int bu_b64_decode_value(char value_in)
{
    static const char decoding[] = {62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51};
    static const char decoding_size = sizeof(decoding);
    value_in -= 43;
    if (value_in < 0 || value_in >= decoding_size) return -1;
    return decoding[(int)value_in];
}

HIDDEN
void bu_b64_init_decodestate(bu_b64_decodestate* state_in)
{
    state_in->step = step_a;
    state_in->plainchar = 0;
}

HIDDEN
int bu_b64_decode_block_internal(const char* code_in, const int length_in, char* plaintext_out, bu_b64_decodestate* state_in)
{
    const char* codechar = code_in;
    char* plainchar = plaintext_out;
    char fragment;

    *plainchar = state_in->plainchar;

    switch (state_in->step)
    {
	while (1)
	{
	    case step_a:
		do {
		    if (codechar == code_in+length_in)
		    {
			state_in->step = step_a;
			state_in->plainchar = *plainchar;
			return plainchar - plaintext_out;
		    }
		    fragment = (char)bu_b64_decode_value(*codechar++);
		} while (fragment < 0);
		*plainchar    = (fragment & 0x03f) << 2;
	    case step_b:
		do {
		    if (codechar == code_in+length_in)
		    {
			state_in->step = step_b;
			state_in->plainchar = *plainchar;
			return plainchar - plaintext_out;
		    }
		    fragment = (char)bu_b64_decode_value(*codechar++);
		} while (fragment < 0);
		*plainchar++ |= (fragment & 0x030) >> 4;
		*plainchar    = (fragment & 0x00f) << 4;
	    case step_c:
		do {
		    if (codechar == code_in+length_in)
		    {
			state_in->step = step_c;
			state_in->plainchar = *plainchar;
			return plainchar - plaintext_out;
		    }
		    fragment = (char)bu_b64_decode_value(*codechar++);
		} while (fragment < 0);
		*plainchar++ |= (fragment & 0x03c) >> 2;
		*plainchar    = (fragment & 0x003) << 6;
	    case step_d:
		do {
		    if (codechar == code_in+length_in)
		    {
			state_in->step = step_d;
			state_in->plainchar = *plainchar;
			return plainchar - plaintext_out;
		    }
		    fragment = (char)bu_b64_decode_value(*codechar++);
		} while (fragment < 0);
		*plainchar++   |= (fragment & 0x03f);
	}
    }
    /* control should not reach here */
    return plainchar - plaintext_out;
}

char *
bu_b64_encode_block(const char *input, int len)
{
    /* Calculate size of output needed and calloc the memory */
    char *output = (char *)bu_malloc(((floor(4*len/3)) + 4) * sizeof(char), "Malloc b64 buffer");
    char *c = output;
    int cnt = 0;
    bu_b64_encodestate s;

    /*---------- START ENCODING ----------*/
    /* initialise the encoder state */
    bu_b64_init_encodestate(&s);
    /* gather data from the input and send it to the output */
    cnt = bu_b64_encode_block_internal(input, len, c, &s);
    c += cnt;
    /* since we have encoded all of the input string we intend
     * to encode, we know that there is no more input data;
     * finalise the encoding */
    cnt = bu_b64_encode_blockend(c, &s);
    c += cnt;
    /*---------- STOP ENCODING  ----------*/

    /* we want to print the encoded data, so null-terminate it: */
    *c = '\0';

    return output;
}

char *
bu_b64_encode(const char *input)
{
    return bu_b64_encode_block(input, strlen(input));
}


char *
bu_b64_decode_block(const char *input, int len)
{
    /* Calculate size of output needed and calloc the memory */
    char *output = (char *)bu_malloc((floor(3*len/4) + 4) * sizeof(char), "Malloc b64 decoding buffer");
    char* c = output;
    int cnt = 0;
    bu_b64_decodestate s;

    /*---------- START DECODING ----------*/
    /* initialise the decoder state */
    bu_b64_init_decodestate(&s);
    /* decode the input data */
    cnt = bu_b64_decode_block_internal(input, len, c, &s);
    c += cnt;
    /* note: there is no bu_b64_decode_blockend! */
    /*---------- STOP DECODING  ----------*/

    /* we want to print the decoded data, so null-terminate it: */
    *c = '\0';

    return output;
}


char *
bu_b64_decode(const char *input)
{
    return bu_b64_decode_block(input, strlen(input));
}



/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
