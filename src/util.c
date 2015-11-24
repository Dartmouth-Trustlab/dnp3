#include <dnp3hammer.h>

#include <hammer/glue.h>
#include "hammer.h" // XXX placeholder for extensions
#include <assert.h>
#include "util.h"


static bool is_zero(HParseResult *p, void *user)
{
    assert(p->ast != NULL);
    return (H_CAST_UINT(p->ast) == 0);
}

HParser *dnp3_p_reserved(size_t n)
{
    return h_ignore(h_attr_bool(h_bits(n, false), is_zero, NULL));
}

HParser *dnp3_p_int_exact(HParser *p, uint64_t x)
{
    return h_int_range(p, x, x);
}

HParser *dnp3_p_objchoice(HParser *p, ...)
{
    va_list ap;
    va_start(ap, p);

    // XXX don't generate these anew each call!
    H_RULE(octet,       h_uint8());
    H_RULE(ohdr_,       h_repeat_n(octet, 3));  // (grp,var,qc)
    H_RULE(unk,         h_right(ohdr_, dnp3_p_err_obj_unknown));

    H_RULE(ps,          h_choice__v(p, ap));
    H_RULE(ochoice,     h_choice(ps, unk, NULL));

    va_end(ap);
    return ochoice;
}

static bool not_err(HParseResult *p, void *user)
{
    return !H_ISERR(p->ast->token_type);
}

static HParser *many_(HParser *(*fmany)(const HParser *), HParser *p)
{
    H_RULE(p_ok,    h_attr_bool(p, not_err, NULL));

    H_RULE(ps,      fmany(p_ok));
    H_RULE(err,     h_right(ps, p));    // fails or yields error
    H_RULE(many,    h_choice(err, ps, NULL));

    return many;
}

HParser *dnp3_p_many(HParser *p)
{
    return many_(h_many, p);
}

HParser *dnp3_p_many1(HParser *p)
{
    return many_(h_many1, p);
}

HParser *dnp3_p_seq(HParser *p, HParser *q)
{
    H_RULE(p_ok,    h_attr_bool(p, not_err, NULL));
    H_RULE(q_ok,    h_attr_bool(q, not_err, NULL));
    H_RULE(pq_ok,   h_sequence(p_ok, q_ok, NULL));

    H_RULE(err,     dnp3_p_err_param_error);
    H_RULE(p_err,   h_right(p_ok, h_choice(q, err, NULL)));

    return h_choice(pq_ok, p_err, p, NULL);
}

HParsedToken *dnp3_p_act_flatten(const HParseResult *p, void* user)
{
    if(p->ast->token_type == TT_SEQUENCE)
        return h_act_flatten(p, user);
    else
        return p->ast;   // XXX discarding const
}

static bool is_err(HParseResult *p, void *user)
{
    return H_ISERR(p->ast->token_type);
}

HParser *dnp3_p_packet__m(HAllocator *mm__, HParser *p)
{
    H_RULE(err, h_attr_bool__m(mm__, p, is_err, NULL));
    H_RULE(ok,  h_left__m(mm__, p, h_end_p__m(mm__)));
    H_RULE(pkt, h_choice__m(mm__, err, ok, NULL));

    return pkt;
}

HParser *dnp3_p_pad;
HParser *dnp3_p_dnp3time;
HParser *dnp3_p_reltime;
HParser *dnp3_p_err_param_error;
HParser *dnp3_p_err_func_not_supp;
HParser *dnp3_p_err_obj_unknown;

void dnp3_p_init_util(void)
{
    // byte-alignment (used in block())
    H_RULE(zero,    dnp3_p_reserved(1));
    H_RULE(pad,     h_indirect());
    h_bind_indirect(pad,
                    h_choice(h_aligned(8), h_right(zero, pad), NULL));
    dnp3_p_pad = pad;

    dnp3_p_dnp3time = h_bits(48, false);
    dnp3_p_reltime  = h_bits(16, false);

    dnp3_p_err_param_error = h_error(ERR_PARAM_ERROR);
    dnp3_p_err_obj_unknown = h_error(ERR_OBJ_UNKNOWN);
    dnp3_p_err_func_not_supp = h_error(ERR_FUNC_NOT_SUPP);
}
