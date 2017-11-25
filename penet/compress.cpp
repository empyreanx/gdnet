/** 
 @file compress.c
 @brief An adaptive order-2 PPM range coder
*/
#define PENET_BUILDING_LIB 1
#include <string.h>
#include "penet/penet.h"

typedef struct _PENetSymbol
{
    /* binary indexed tree of symbols */
    penet_uint8 value;
    penet_uint8 count;
    penet_uint16 under;
    penet_uint16 left, right;

    /* context defined by this symbol */
    penet_uint16 symbols;
    penet_uint16 escapes;
    penet_uint16 total;
    penet_uint16 parent;
} PENetSymbol;

/* adaptation constants tuned aggressively for small packet sizes rather than large file compression */
enum
{
    PENET_RANGE_CODER_TOP    = 1<<24,
    PENET_RANGE_CODER_BOTTOM = 1<<16,

    PENET_CONTEXT_SYMBOL_DELTA = 3,
    PENET_CONTEXT_SYMBOL_MINIMUM = 1,
    PENET_CONTEXT_ESCAPE_MINIMUM = 1,

    PENET_SUBCONTEXT_ORDER = 2,
    PENET_SUBCONTEXT_SYMBOL_DELTA = 2,
    PENET_SUBCONTEXT_ESCAPE_DELTA = 5
};

/* context exclusion roughly halves compression speed, so disable for now */
#undef PENET_CONTEXT_EXCLUSION

typedef struct _PENetRangeCoder
{
    /* only allocate enough symbols for reasonable MTUs, would need to be larger for large file compression */
    PENetSymbol symbols[4096];
} PENetRangeCoder;

void *
penet_range_coder_create (void)
{
    PENetRangeCoder * rangeCoder = (PENetRangeCoder *) penet_malloc (sizeof (PENetRangeCoder));
    if (rangeCoder == NULL)
      return NULL;

    return rangeCoder;
}

void
penet_range_coder_destroy (void * context)
{
    PENetRangeCoder * rangeCoder = (PENetRangeCoder *) context;
    if (rangeCoder == NULL)
      return;

    penet_free (rangeCoder);
}

#define PENET_SYMBOL_CREATE(symbol, value_, count_) \
{ \
    symbol = & rangeCoder -> symbols [nextSymbol ++]; \
    symbol -> value = value_; \
    symbol -> count = count_; \
    symbol -> under = count_; \
    symbol -> left = 0; \
    symbol -> right = 0; \
    symbol -> symbols = 0; \
    symbol -> escapes = 0; \
    symbol -> total = 0; \
    symbol -> parent = 0; \
}

#define PENET_CONTEXT_CREATE(context, escapes_, minimum) \
{ \
    PENET_SYMBOL_CREATE (context, 0, 0); \
    (context) -> escapes = escapes_; \
    (context) -> total = escapes_ + 256*minimum; \
    (context) -> symbols = 0; \
}

static penet_uint16
penet_symbol_rescale (PENetSymbol * symbol)
{
    penet_uint16 total = 0;
    for (;;)
    {
        symbol -> count -= symbol->count >> 1;
        symbol -> under = symbol -> count;
        if (symbol -> left)
          symbol -> under += penet_symbol_rescale (symbol + symbol -> left);
        total += symbol -> under;
        if (! symbol -> right) break;
        symbol += symbol -> right;
    }
    return total;
}

#define PENET_CONTEXT_RESCALE(context, minimum) \
{ \
    (context) -> total = (context) -> symbols ? penet_symbol_rescale ((context) + (context) -> symbols) : 0; \
    (context) -> escapes -= (context) -> escapes >> 1; \
    (context) -> total += (context) -> escapes + 256*minimum; \
}

#define PENET_RANGE_CODER_OUTPUT(value) \
{ \
    if (outData >= outEnd) \
      return 0; \
    * outData ++ = value; \
}

#define PENET_RANGE_CODER_ENCODE(under, count, total) \
{ \
    encodeRange /= (total); \
    encodeLow += (under) * encodeRange; \
    encodeRange *= (count); \
    for (;;) \
    { \
        if((encodeLow ^ (encodeLow + encodeRange)) >= PENET_RANGE_CODER_TOP) \
        { \
            if(encodeRange >= PENET_RANGE_CODER_BOTTOM) break; \
            encodeRange = -encodeLow & (PENET_RANGE_CODER_BOTTOM - 1); \
        } \
        PENET_RANGE_CODER_OUTPUT (encodeLow >> 24); \
        encodeRange <<= 8; \
        encodeLow <<= 8; \
    } \
}

#define PENET_RANGE_CODER_FLUSH \
{ \
    while (encodeLow) \
    { \
        PENET_RANGE_CODER_OUTPUT (encodeLow >> 24); \
        encodeLow <<= 8; \
    } \
}

#define PENET_RANGE_CODER_FREE_SYMBOLS \
{ \
    if (nextSymbol >= sizeof (rangeCoder -> symbols) / sizeof (PENetSymbol) - PENET_SUBCONTEXT_ORDER ) \
    { \
        nextSymbol = 0; \
        PENET_CONTEXT_CREATE (root, PENET_CONTEXT_ESCAPE_MINIMUM, PENET_CONTEXT_SYMBOL_MINIMUM); \
        predicted = 0; \
        order = 0; \
    } \
}

#define PENET_CONTEXT_ENCODE(context, symbol_, value_, under_, count_, update, minimum) \
{ \
    under_ = value*minimum; \
    count_ = minimum; \
    if (! (context) -> symbols) \
    { \
        PENET_SYMBOL_CREATE (symbol_, value_, update); \
        (context) -> symbols = symbol_ - (context); \
    } \
    else \
    { \
        PENetSymbol * node = (context) + (context) -> symbols; \
        for (;;) \
        { \
            if (value_ < node -> value) \
            { \
                node -> under += update; \
                if (node -> left) { node += node -> left; continue; } \
                PENET_SYMBOL_CREATE (symbol_, value_, update); \
                node -> left = symbol_ - node; \
            } \
            else \
            if (value_ > node -> value) \
            { \
                under_ += node -> under; \
                if (node -> right) { node += node -> right; continue; } \
                PENET_SYMBOL_CREATE (symbol_, value_, update); \
                node -> right = symbol_ - node; \
            } \
            else \
            { \
                count_ += node -> count; \
                under_ += node -> under - node -> count; \
                node -> under += update; \
                node -> count += update; \
                symbol_ = node; \
            } \
            break; \
        } \
    } \
}

#ifdef PENET_CONTEXT_EXCLUSION
static const PENetSymbol emptyContext = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

#define PENET_CONTEXT_WALK(context, body) \
{ \
    const PENetSymbol * node = (context) + (context) -> symbols; \
    const PENetSymbol * stack [256]; \
    size_t stackSize = 0; \
    while (node -> left) \
    { \
        stack [stackSize ++] = node; \
        node += node -> left; \
    } \
    for (;;) \
    { \
        body; \
        if (node -> right) \
        { \
            node += node -> right; \
            while (node -> left) \
            { \
                stack [stackSize ++] = node; \
                node += node -> left; \
            } \
        } \
        else \
        if (stackSize <= 0) \
            break; \
        else \
            node = stack [-- stackSize]; \
    } \
}

#define PENET_CONTEXT_ENCODE_EXCLUDE(context, value_, under, total, minimum) \
PENET_CONTEXT_WALK(context, { \
    if (node -> value != value_) \
    { \
        penet_uint16 parentCount = rangeCoder -> symbols [node -> parent].count + minimum; \
        if (node -> value < value_) \
          under -= parentCount; \
        total -= parentCount; \
    } \
})
#endif

size_t
penet_range_coder_compress (void * context, const PENetBuffer * inBuffers, size_t inBufferCount, size_t inLimit, penet_uint8 * outData, size_t outLimit)
{
    PENetRangeCoder * rangeCoder = (PENetRangeCoder *) context;
    penet_uint8 * outStart = outData, * outEnd = & outData [outLimit];
    const penet_uint8 * inData, * inEnd;
    penet_uint32 encodeLow = 0, encodeRange = ~0;
    PENetSymbol * root;
    penet_uint16 predicted = 0;
    size_t order = 0, nextSymbol = 0;

    if (rangeCoder == NULL || inBufferCount <= 0 || inLimit <= 0)
      return 0;

    inData = (const penet_uint8 *) inBuffers -> data;
    inEnd = & inData [inBuffers -> dataLength];
    inBuffers ++;
    inBufferCount --;

    PENET_CONTEXT_CREATE (root, PENET_CONTEXT_ESCAPE_MINIMUM, PENET_CONTEXT_SYMBOL_MINIMUM);

    for (;;)
    {
        PENetSymbol * subcontext, * symbol;
#ifdef PENET_CONTEXT_EXCLUSION
        const PENetSymbol * childContext = & emptyContext;
#endif
        penet_uint8 value;
        penet_uint16 count, under, * parent = & predicted, total;
        if (inData >= inEnd)
        {
            if (inBufferCount <= 0)
              break;
            inData = (const penet_uint8 *) inBuffers -> data;
            inEnd = & inData [inBuffers -> dataLength];
            inBuffers ++;
            inBufferCount --;
        }
        value = * inData ++;

        for (subcontext = & rangeCoder -> symbols [predicted];
             subcontext != root;
#ifdef PENET_CONTEXT_EXCLUSION
             childContext = subcontext,
#endif
                subcontext = & rangeCoder -> symbols [subcontext -> parent])
        {
            PENET_CONTEXT_ENCODE (subcontext, symbol, value, under, count, PENET_SUBCONTEXT_SYMBOL_DELTA, 0);
            * parent = symbol - rangeCoder -> symbols;
            parent = & symbol -> parent;
            total = subcontext -> total;
#ifdef PENET_CONTEXT_EXCLUSION
            if (childContext -> total > PENET_SUBCONTEXT_SYMBOL_DELTA + PENET_SUBCONTEXT_ESCAPE_DELTA)
              PENET_CONTEXT_ENCODE_EXCLUDE (childContext, value, under, total, 0);
#endif
            if (count > 0)
            {
                PENET_RANGE_CODER_ENCODE (subcontext -> escapes + under, count, total);
            }
            else
            {
                if (subcontext -> escapes > 0 && subcontext -> escapes < total)
                    PENET_RANGE_CODER_ENCODE (0, subcontext -> escapes, total);
                subcontext -> escapes += PENET_SUBCONTEXT_ESCAPE_DELTA;
                subcontext -> total += PENET_SUBCONTEXT_ESCAPE_DELTA;
            }
            subcontext -> total += PENET_SUBCONTEXT_SYMBOL_DELTA;
            if (count > 0xFF - 2*PENET_SUBCONTEXT_SYMBOL_DELTA || subcontext -> total > PENET_RANGE_CODER_BOTTOM - 0x100)
              PENET_CONTEXT_RESCALE (subcontext, 0);
            if (count > 0) goto nextInput;
        }

        PENET_CONTEXT_ENCODE (root, symbol, value, under, count, PENET_CONTEXT_SYMBOL_DELTA, PENET_CONTEXT_SYMBOL_MINIMUM);
        * parent = symbol - rangeCoder -> symbols;
        parent = & symbol -> parent;
        total = root -> total;
#ifdef PENET_CONTEXT_EXCLUSION
        if (childContext -> total > PENET_SUBCONTEXT_SYMBOL_DELTA + PENET_SUBCONTEXT_ESCAPE_DELTA)
          PENET_CONTEXT_ENCODE_EXCLUDE (childContext, value, under, total, PENET_CONTEXT_SYMBOL_MINIMUM);
#endif
        PENET_RANGE_CODER_ENCODE (root -> escapes + under, count, total);
        root -> total += PENET_CONTEXT_SYMBOL_DELTA;
        if (count > 0xFF - 2*PENET_CONTEXT_SYMBOL_DELTA + PENET_CONTEXT_SYMBOL_MINIMUM || root -> total > PENET_RANGE_CODER_BOTTOM - 0x100)
          PENET_CONTEXT_RESCALE (root, PENET_CONTEXT_SYMBOL_MINIMUM);

    nextInput:
        if (order >= PENET_SUBCONTEXT_ORDER)
          predicted = rangeCoder -> symbols [predicted].parent;
        else
          order ++;
        PENET_RANGE_CODER_FREE_SYMBOLS;
    }

    PENET_RANGE_CODER_FLUSH;

    return (size_t) (outData - outStart);
}

#define PENET_RANGE_CODER_SEED \
{ \
    if (inData < inEnd) decodeCode |= * inData ++ << 24; \
    if (inData < inEnd) decodeCode |= * inData ++ << 16; \
    if (inData < inEnd) decodeCode |= * inData ++ << 8; \
    if (inData < inEnd) decodeCode |= * inData ++; \
}

#define PENET_RANGE_CODER_READ(total) ((decodeCode - decodeLow) / (decodeRange /= (total)))

#define PENET_RANGE_CODER_DECODE(under, count, total) \
{ \
    decodeLow += (under) * decodeRange; \
    decodeRange *= (count); \
    for (;;) \
    { \
        if((decodeLow ^ (decodeLow + decodeRange)) >= PENET_RANGE_CODER_TOP) \
        { \
            if(decodeRange >= PENET_RANGE_CODER_BOTTOM) break; \
            decodeRange = -decodeLow & (PENET_RANGE_CODER_BOTTOM - 1); \
        } \
        decodeCode <<= 8; \
        if (inData < inEnd) \
          decodeCode |= * inData ++; \
        decodeRange <<= 8; \
        decodeLow <<= 8; \
    } \
}

#define PENET_CONTEXT_DECODE(context, symbol_, code, value_, under_, count_, update, minimum, createRoot, visitNode, createRight, createLeft) \
{ \
    under_ = 0; \
    count_ = minimum; \
    if (! (context) -> symbols) \
    { \
        createRoot; \
    } \
    else \
    { \
        PENetSymbol * node = (context) + (context) -> symbols; \
        for (;;) \
        { \
            penet_uint16 after = under_ + node -> under + (node -> value + 1)*minimum, before = node -> count + minimum; \
            visitNode; \
            if (code >= after) \
            { \
                under_ += node -> under; \
                if (node -> right) { node += node -> right; continue; } \
                createRight; \
            } \
            else \
            if (code < after - before) \
            { \
                node -> under += update; \
                if (node -> left) { node += node -> left; continue; } \
                createLeft; \
            } \
            else \
            { \
                value_ = node -> value; \
                count_ += node -> count; \
                under_ = after - before; \
                node -> under += update; \
                node -> count += update; \
                symbol_ = node; \
            } \
            break; \
        } \
    } \
}

#define PENET_CONTEXT_TRY_DECODE(context, symbol_, code, value_, under_, count_, update, minimum, exclude) \
PENET_CONTEXT_DECODE (context, symbol_, code, value_, under_, count_, update, minimum, return 0, exclude (node -> value, after, before), return 0, return 0)

#define PENET_CONTEXT_ROOT_DECODE(context, symbol_, code, value_, under_, count_, update, minimum, exclude) \
PENET_CONTEXT_DECODE (context, symbol_, code, value_, under_, count_, update, minimum, \
    { \
        value_ = code / minimum; \
        under_ = code - code%minimum; \
        PENET_SYMBOL_CREATE (symbol_, value_, update); \
        (context) -> symbols = symbol_ - (context); \
    }, \
    exclude (node -> value, after, before), \
    { \
        value_ = node->value + 1 + (code - after)/minimum; \
        under_ = code - (code - after)%minimum; \
        PENET_SYMBOL_CREATE (symbol_, value_, update); \
        node -> right = symbol_ - node; \
    }, \
    { \
        value_ = node->value - 1 - (after - before - code - 1)/minimum; \
        under_ = code - (after - before - code - 1)%minimum; \
        PENET_SYMBOL_CREATE (symbol_, value_, update); \
        node -> left = symbol_ - node; \
    }) \

#ifdef PENET_CONTEXT_EXCLUSION
typedef struct _PENetExclude
{
    penet_uint8 value;
    penet_uint16 under;
} PENetExclude;

#define PENET_CONTEXT_DECODE_EXCLUDE(context, total, minimum) \
{ \
    penet_uint16 under = 0; \
    nextExclude = excludes; \
    PENET_CONTEXT_WALK (context, { \
        under += rangeCoder -> symbols [node -> parent].count + minimum; \
        nextExclude -> value = node -> value; \
        nextExclude -> under = under; \
        nextExclude ++; \
    }); \
    total -= under; \
}

#define PENET_CONTEXT_EXCLUDED(value_, after, before) \
{ \
    size_t low = 0, high = nextExclude - excludes; \
    for(;;) \
    { \
        size_t mid = (low + high) >> 1; \
        const PENetExclude * exclude = & excludes [mid]; \
        if (value_ < exclude -> value) \
        { \
            if (low + 1 < high) \
            { \
                high = mid; \
                continue; \
            } \
            if (exclude > excludes) \
              after -= exclude [-1].under; \
        } \
        else \
        { \
            if (value_ > exclude -> value) \
            { \
                if (low + 1 < high) \
                { \
                    low = mid; \
                    continue; \
                } \
            } \
            else \
              before = 0; \
            after -= exclude -> under; \
        } \
        break; \
    } \
}
#endif

#define PENET_CONTEXT_NOT_EXCLUDED(value_, after, before)

size_t
penet_range_coder_decompress (void * context, const penet_uint8 * inData, size_t inLimit, penet_uint8 * outData, size_t outLimit)
{
    PENetRangeCoder * rangeCoder = (PENetRangeCoder *) context;
    penet_uint8 * outStart = outData, * outEnd = & outData [outLimit];
    const penet_uint8 * inEnd = & inData [inLimit];
    penet_uint32 decodeLow = 0, decodeCode = 0, decodeRange = ~0;
    PENetSymbol * root;
    penet_uint16 predicted = 0;
    size_t order = 0, nextSymbol = 0;
#ifdef PENET_CONTEXT_EXCLUSION
    PENetExclude excludes [256];
    PENetExclude * nextExclude = excludes;
#endif

    if (rangeCoder == NULL || inLimit <= 0)
      return 0;

    PENET_CONTEXT_CREATE (root, PENET_CONTEXT_ESCAPE_MINIMUM, PENET_CONTEXT_SYMBOL_MINIMUM);

    PENET_RANGE_CODER_SEED;

    for (;;)
    {
        PENetSymbol * subcontext, * symbol, * patch;
#ifdef PENET_CONTEXT_EXCLUSION
        const PENetSymbol * childContext = & emptyContext;
#endif
        penet_uint8 value = 0;
        penet_uint16 code, under, count, bottom, * parent = & predicted, total;

        for (subcontext = & rangeCoder -> symbols [predicted];
             subcontext != root;
#ifdef PENET_CONTEXT_EXCLUSION
             childContext = subcontext,
#endif
                subcontext = & rangeCoder -> symbols [subcontext -> parent])
        {
            if (subcontext -> escapes <= 0)
              continue;
            total = subcontext -> total;
#ifdef PENET_CONTEXT_EXCLUSION
            if (childContext -> total > 0)
              PENET_CONTEXT_DECODE_EXCLUDE (childContext, total, 0);
#endif
            if (subcontext -> escapes >= total)
              continue;
            code = PENET_RANGE_CODER_READ (total);
            if (code < subcontext -> escapes)
            {
                PENET_RANGE_CODER_DECODE (0, subcontext -> escapes, total);
                continue;
            }
            code -= subcontext -> escapes;
#ifdef PENET_CONTEXT_EXCLUSION
            if (childContext -> total > 0)
            {
                PENET_CONTEXT_TRY_DECODE (subcontext, symbol, code, value, under, count, PENET_SUBCONTEXT_SYMBOL_DELTA, 0, PENET_CONTEXT_EXCLUDED);
            }
            else
#endif
            {
                PENET_CONTEXT_TRY_DECODE (subcontext, symbol, code, value, under, count, PENET_SUBCONTEXT_SYMBOL_DELTA, 0, PENET_CONTEXT_NOT_EXCLUDED);
            }
            bottom = symbol - rangeCoder -> symbols;
            PENET_RANGE_CODER_DECODE (subcontext -> escapes + under, count, total);
            subcontext -> total += PENET_SUBCONTEXT_SYMBOL_DELTA;
            if (count > 0xFF - 2*PENET_SUBCONTEXT_SYMBOL_DELTA || subcontext -> total > PENET_RANGE_CODER_BOTTOM - 0x100)
              PENET_CONTEXT_RESCALE (subcontext, 0);
            goto patchContexts;
        }

        total = root -> total;
#ifdef PENET_CONTEXT_EXCLUSION
        if (childContext -> total > 0)
          PENET_CONTEXT_DECODE_EXCLUDE (childContext, total, PENET_CONTEXT_SYMBOL_MINIMUM);
#endif
        code = PENET_RANGE_CODER_READ (total);
        if (code < root -> escapes)
        {
            PENET_RANGE_CODER_DECODE (0, root -> escapes, total);
            break;
        }
        code -= root -> escapes;
#ifdef PENET_CONTEXT_EXCLUSION
        if (childContext -> total > 0)
        {
            PENET_CONTEXT_ROOT_DECODE (root, symbol, code, value, under, count, PENET_CONTEXT_SYMBOL_DELTA, PENET_CONTEXT_SYMBOL_MINIMUM, PENET_CONTEXT_EXCLUDED);
        }
        else
#endif
        {
            PENET_CONTEXT_ROOT_DECODE (root, symbol, code, value, under, count, PENET_CONTEXT_SYMBOL_DELTA, PENET_CONTEXT_SYMBOL_MINIMUM, PENET_CONTEXT_NOT_EXCLUDED);
        }
        bottom = symbol - rangeCoder -> symbols;
        PENET_RANGE_CODER_DECODE (root -> escapes + under, count, total);
        root -> total += PENET_CONTEXT_SYMBOL_DELTA;
        if (count > 0xFF - 2*PENET_CONTEXT_SYMBOL_DELTA + PENET_CONTEXT_SYMBOL_MINIMUM || root -> total > PENET_RANGE_CODER_BOTTOM - 0x100)
          PENET_CONTEXT_RESCALE (root, PENET_CONTEXT_SYMBOL_MINIMUM);

    patchContexts:
        for (patch = & rangeCoder -> symbols [predicted];
             patch != subcontext;
             patch = & rangeCoder -> symbols [patch -> parent])
        {
            PENET_CONTEXT_ENCODE (patch, symbol, value, under, count, PENET_SUBCONTEXT_SYMBOL_DELTA, 0);
            * parent = symbol - rangeCoder -> symbols;
            parent = & symbol -> parent;
            if (count <= 0)
            {
                patch -> escapes += PENET_SUBCONTEXT_ESCAPE_DELTA;
                patch -> total += PENET_SUBCONTEXT_ESCAPE_DELTA;
            }
            patch -> total += PENET_SUBCONTEXT_SYMBOL_DELTA;
            if (count > 0xFF - 2*PENET_SUBCONTEXT_SYMBOL_DELTA || patch -> total > PENET_RANGE_CODER_BOTTOM - 0x100)
              PENET_CONTEXT_RESCALE (patch, 0);
        }
        * parent = bottom;

        PENET_RANGE_CODER_OUTPUT (value);

        if (order >= PENET_SUBCONTEXT_ORDER)
          predicted = rangeCoder -> symbols [predicted].parent;
        else
          order ++;
        PENET_RANGE_CODER_FREE_SYMBOLS;
    }

    return (size_t) (outData - outStart);
}

/** @defgroup host PENet host functions
    @{
*/

/** Sets the packet compressor the host should use to the default range coder.
    @param host host to enable the range coder for
    @returns 0 on success, < 0 on failure
*/
int
penet_host_compress_with_range_coder (PENetHost * host)
{
    PENetCompressor compressor;
    memset (& compressor, 0, sizeof (compressor));
    compressor.context = penet_range_coder_create();
    if (compressor.context == NULL)
      return -1;
    compressor.compress = penet_range_coder_compress;
    compressor.decompress = penet_range_coder_decompress;
    compressor.destroy = penet_range_coder_destroy;
    penet_host_compress (host, & compressor);
    return 0;
}

/** @} */
