#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Parser {
    const char *s;
    size_t len;
    size_t pos;
    cl_arena *a;
    bool ok;
    size_t err_pos;
} Parser;

static void p_err(Parser *p) {
    if (p->ok) p->err_pos = p->pos;
    p->ok = false;
}

static void p_ws(Parser *p) {
    while (p->pos < p->len &&
           (p->s[p->pos] == ' ' || p->s[p->pos] == '\t' ||
            p->s[p->pos] == '\n' || p->s[p->pos] == '\r')) {
        p->pos += 1;
    }
}

static int p_peek(Parser *p) {
    return p->pos < p->len ? (unsigned char)p->s[p->pos] : -1;
}

static void p_advance(Parser *p) {
    if (p->pos < p->len) p->pos += 1;
}

static int hex_digit(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void utf8_encode(cl_arena_buf *b, unsigned cp) {
    if (cp < 0x80) {
        char c = (char)cp;
        cl_arena_buf_append(b, &c, 1);
    } else if (cp < 0x800) {
        char c2[2] = {(char)(0xC0 | (cp >> 6)), (char)(0x80 | (cp & 0x3F))};
        cl_arena_buf_append(b, c2, 2);
    } else if (cp < 0x10000) {
        char c3[3] = {(char)(0xE0 | (cp >> 12)), (char)(0x80 | ((cp >> 6) & 0x3F)),
                      (char)(0x80 | (cp & 0x3F))};
        cl_arena_buf_append(b, c3, 3);
    } else {
        char c4[4] = {(char)(0xF0 | (cp >> 18)),
                      (char)(0x80 | ((cp >> 12) & 0x3F)),
                      (char)(0x80 | ((cp >> 6) & 0x3F)),
                      (char)(0x80 | (cp & 0x3F))};
        cl_arena_buf_append(b, c4, 4);
    }
}

/* Consumes exactly one complete string literal (quotes included). */
static void skip_string(Parser *p) {
    p_advance(p); /* opening quote                                */
    while (p->ok && p->pos < p->len) {
        int c = p_peek(p);
        if (c == '\\') {
            p_advance(p);
            p_advance(p);
        } else if (c == '"') {
            p_advance(p);
            return;
        } else {
            p_advance(p);
        }
    }
    p_err(p);
}

/* Consumes exactly one complete value (any nesting). */
static void skip_one_value(Parser *p) {
    p_ws(p);
    int c = p_peek(p);
    if (c == '"') {
        skip_string(p);
        return;
    }
    if (c == '{' || c == '[') {
        int depth = 0;
        while (p->ok && p->pos < p->len) {
            int d = p_peek(p);
            if (d == '"') {
                skip_string(p);
                continue;
            }
            if (d == '{' || d == '[') depth += 1;
            else if (d == '}' || d == ']') {
                depth -= 1;
                if (depth == 0) {
                    p_advance(p);
                    return;
                }
            }
            p_advance(p);
        }
        p_err(p);
        return;
    }
    if (c == 't' || c == 'f' || c == 'n' || c == '-' || (c >= '0' && c <= '9')) {
        while (p->pos < p->len) {
            int d = p_peek(p);
            if (d == ',' || d == '}' || d == ']' || d == ' ' || d == '\t' ||
                d == '\n' || d == '\r')
                break;
            p_advance(p);
        }
        return;
    }
    p_err(p);
}

static cl_json_node *node_alloc(Parser *p) {
    return (cl_json_node *)cl_arena_alloc(p->a, sizeof(cl_json_node),
                                          _Alignof(cl_json_node));
}

static cl_str parse_string(Parser *p, bool alloc) {
    cl_arena_buf b = cl_arena_buf_make(p->a, 32);
    p_advance(p); /* quote                                             */
    while (p->ok && p->pos < p->len) {
        int c = p_peek(p);
        if (c == '"') {
            p_advance(p);
            if (alloc) return cl_arena_buf_deposit(&b);
            cl_arena_return(b.frame);
            return cl_str_make(NULL, 0);
        }
        if (c == '\\') {
            p_advance(p);
            int e = p_peek(p);
            p_advance(p);
            switch (e) {
            case '"': cl_arena_buf_append(&b, "\"", 1); break;
            case '\\': cl_arena_buf_append(&b, "\\", 1); break;
            case '/': cl_arena_buf_append(&b, "/", 1); break;
            case 'b': cl_arena_buf_append(&b, "\b", 1); break;
            case 'f': cl_arena_buf_append(&b, "\f", 1); break;
            case 'n': cl_arena_buf_append(&b, "\n", 1); break;
            case 'r': cl_arena_buf_append(&b, "\r", 1); break;
            case 't': cl_arena_buf_append(&b, "\t", 1); break;
            case 'u': {
                unsigned cp = 0;
                for (int i = 0; i < 4; i++) {
                    int h = hex_digit(p_peek(p));
                    if (h < 0) {
                        p_err(p);
                        break;
                    }
                    p_advance(p);
                    cp = (cp << 4) | (unsigned)h;
                }
                utf8_encode(&b, cp);
                break;
            }
            case -1: p_err(p); break;
            default: p_err(p); break;
            }
        } else if (c == '\n' || c == '\r') {
            p_err(p);
        } else {
            char ch = (char)c;
            cl_arena_buf_append(&b, &ch, 1);
            p_advance(p);
        }
    }
    p_err(p);
    if (alloc) return cl_arena_buf_deposit(&b);
    cl_arena_return(b.frame);
    return cl_str_make(NULL, 0);
}

/* Manual, locale-free double parse so replay/config values never depend on
 * the process environment. */
static double parse_number_value(Parser *p) {
    size_t start = p->pos;
    while (p->pos < p->len) {
        int c = p_peek(p);
        if (c == ',' || c == '}' || c == ']' || c == ' ' || c == '\t' ||
            c == '\n' || c == '\r')
            break;
        p_advance(p);
    }
    cl_str tok = cl_str_make(p->s + start, p->pos - start);
    size_t k = 0;
    bool neg = tok.len > 0 && tok.data[0] == '-';
    if (neg) k = 1;
    double intpart = 0.0;
    while (k < tok.len && tok.data[k] >= '0' && tok.data[k] <= '9') {
        intpart = intpart * 10.0 + (double)(tok.data[k] - '0');
        k++;
    }
    double frac = 0.0;
    if (k < tok.len && tok.data[k] == '.') {
        k++;
        double place = 0.1;
        while (k < tok.len && tok.data[k] >= '0' && tok.data[k] <= '9') {
            frac += (double)(tok.data[k] - '0') * place;
            place *= 0.1;
            k++;
        }
    }
    if (k < tok.len && (tok.data[k] == 'e' || tok.data[k] == 'E')) {
        k++;
        bool eneg = false;
        if (k < tok.len && (tok.data[k] == '+' || tok.data[k] == '-')) {
            eneg = tok.data[k] == '-';
            k++;
        }
        int exp = 0;
        while (k < tok.len && tok.data[k] >= '0' && tok.data[k] <= '9') {
            exp = exp * 10 + (tok.data[k] - '0');
            k++;
        }
        if (eneg) exp = -exp;
        intpart += frac;
        while (exp > 0) { intpart *= 10.0; exp--; }
        while (exp < 0) { intpart *= 0.1; exp++; }
        return neg ? -intpart : intpart;
    }
    return neg ? -(intpart + frac) : (intpart + frac);
}

static cl_json_node *parse_value(Parser *p);

static cl_json_node *parse_object(Parser *p) {
    p_advance(p); /* '{'                                             */
    size_t after_open = p->pos; /* rewind target for the build pass      */
    size_t n = 0;
    for (;;) {
        p_ws(p);
        int c = p_peek(p);
        if (c == '}') {
            p_advance(p);
            break;
        }
        if (c != '"') {
            p_err(p);
            return NULL;
        }
        skip_string(p);
        if (!p->ok) return NULL;
        p_ws(p);
        if (p_peek(p) != ':') {
            p_err(p);
            return NULL;
        }
        p_advance(p);
        skip_one_value(p);
        if (!p->ok) return NULL;
        n++;
        p_ws(p);
        c = p_peek(p);
        if (c == ',') {
            p_advance(p);
            continue;
        }
        if (c == '}') {
            p_advance(p);
            break;
        }
        p_err(p);
        return NULL;
    }
    cl_json_node *out = node_alloc(p);
    out->kind = CLAY_J_OBJ;
    cl_json_pair *pairs = (cl_json_pair *)cl_arena_alloc(p->a, n * sizeof(cl_json_pair),
                                                         _Alignof(cl_json_pair));
    out->obj.pairs = pairs;
    out->obj.n = n;
    /* Second pass re-parses the body for ownership, rewound to just after
     * the opener that the counting pass already consumed. */
    p->pos = after_open;
    n = 0;
    for (;;) {
        p_ws(p);
        if (p_peek(p) == '}') {
            p_advance(p);
            break;
        }
        cl_str key = parse_string(p, true);
        if (!p->ok) return NULL;
        p_ws(p);
        p_advance(p); /* ':'                                       */
        cl_json_node *val = parse_value(p);
        if (!p->ok) return NULL;
        cl_json_pair pair = {key, val};
        pairs[n++] = pair;
        p_ws(p);
        if (p_peek(p) == ',') {
            p_advance(p);
            continue;
        }
        if (p_peek(p) == '}') {
            p_advance(p);
            break;
        }
        p_err(p);
        return NULL;
    }
    return out;
}

static cl_json_node *parse_array(Parser *p) {
    p_advance(p); /* '['                                             */
    size_t after_open = p->pos; /* rewind target for the build pass  */
    size_t n = 0;
    for (;;) {
        p_ws(p);
        int c = p_peek(p);
        if (c == ']') {
            p_advance(p);
            break;
        }
        skip_one_value(p);
        if (!p->ok) return NULL;
        n++;
        p_ws(p);
        c = p_peek(p);
        if (c == ',') {
            p_advance(p);
            continue;
        }
        if (c == ']') {
            p_advance(p);
            break;
        }
        p_err(p);
        return NULL;
    }
    cl_json_node *out = node_alloc(p);
    out->kind = CLAY_J_ARR;
    cl_json_node **items = (cl_json_node **)cl_arena_alloc(p->a, n * sizeof(cl_json_node *),
                                                           _Alignof(cl_json_node *));
    out->arr.items = items;
    out->arr.n = n;
    /* Second pass re-parses the body for ownership, rewound to just after
     * the opener that the counting pass already consumed. */
    p->pos = after_open;
    n = 0;
    for (;;) {
        p_ws(p);
        if (p_peek(p) == ']') {
            p_advance(p);
            break;
        }
        cl_json_node *val = parse_value(p);
        if (!p->ok) return NULL;
        items[n++] = val;
        p_ws(p);
        if (p_peek(p) == ',') {
            p_advance(p);
            continue;
        }
        if (p_peek(p) == ']') {
            p_advance(p);
            break;
        }
        p_err(p);
        return NULL;
    }
    return out;
}

static cl_json_node *parse_value(Parser *p) {
    p_ws(p);
    int c = p_peek(p);
    if (c == '{') return parse_object(p);
    if (c == '[') return parse_array(p);
    if (c == '"') {
        cl_json_node *out = node_alloc(p);
        out->kind = CLAY_J_STR;
        out->s = parse_string(p, true);
        return out;
    }
    if (c == 't') {
        if (p->pos + 4 <= p->len && memcmp(p->s + p->pos, "true", 4) == 0) {
            p->pos += 4;
            cl_json_node *out = node_alloc(p);
            out->kind = CLAY_J_BOOL;
            out->b = true;
            return out;
        }
        p_err(p);
        return NULL;
    }
    if (c == 'f') {
        if (p->pos + 5 <= p->len && memcmp(p->s + p->pos, "false", 5) == 0) {
            p->pos += 5;
            cl_json_node *out = node_alloc(p);
            out->kind = CLAY_J_BOOL;
            out->b = false;
            return out;
        }
        p_err(p);
        return NULL;
    }
    if (c == 'n') {
        if (p->pos + 4 <= p->len && memcmp(p->s + p->pos, "null", 4) == 0) {
            p->pos += 4;
            cl_json_node *out = node_alloc(p);
            out->kind = CLAY_J_NIL;
            return out;
        }
        p_err(p);
        return NULL;
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
        size_t start = p->pos;
        double real = parse_number_value(p);
        cl_json_node *out = node_alloc(p);
        /* integer when the token is an unbroken [-]digits run */
        bool integral = true;
        for (size_t i = start; i < p->pos; i++) {
            char ch = p->s[i];
            if (ch == '.' || ch == 'e' || ch == 'E') integral = false;
        }
        if (integral) {
            out->kind = CLAY_J_I64;
            out->i = (int64_t)real;
        } else {
            out->kind = CLAY_J_F64;
            out->f = real;
        }
        return out;
    }
    p_err(p);
    return NULL;
}

cl_err cl_json_parse(cl_json_node *out, cl_arena *a, cl_str text) {
    Parser p;
    p.s = text.data;
    p.len = text.len;
    p.pos = 0;
    p.a = a;
    p.ok = true;
    p.err_pos = 0;
    cl_json_node *root = parse_value(&p);
    p_ws(&p);
    if (!p.ok || root == NULL) return CLAY_ERR_PARSE;
    if (p.pos != p.len) return CLAY_ERR_PARSE;
    *out = *root;
    return CLAY_OK;
}

/* ------------------------------------------------ dotted-path lookup ------ */

static bool name_matches(cl_str key, const char *seg, size_t seg_len) {
    return key.len == seg_len && memcmp(key.data, seg, seg_len) == 0;
}

cl_json_node *cl_json_get(cl_json_node *root, cl_str path) {
    cl_json_node *cur = root;
    if (cur == NULL) return NULL;
    size_t pos = 0;
    while (pos <= path.len) {
        /* skip leading '.' */
        while (pos < path.len && path.data[pos] == '.') pos++;
        size_t seg_start = pos;
        while (pos < path.len && path.data[pos] != '.' && path.data[pos] != '[') {
            pos++;
        }
        if (pos == seg_start) return NULL;
        size_t seg_len = pos - seg_start;
        const char *seg = path.data + seg_start;
        if (cur->kind == CLAY_J_OBJ) {
            cl_json_node *found = NULL;
            for (size_t i = 0; i < cur->obj.n; i++) {
                if (name_matches(cur->obj.pairs[i].key, seg, seg_len)) {
                    found = cur->obj.pairs[i].val;
                    break;
                }
            }
            if (!found) return NULL;
            cur = found;
        } else {
            return NULL;
        }
        /* bracket index / trailing */
        if (pos < path.len && path.data[pos] == '[') {
            pos++;
            size_t idx = 0;
            bool any = false;
            while (pos < path.len && path.data[pos] >= '0' &&
                   path.data[pos] <= '9') {
                idx = idx * 10 + (size_t)(path.data[pos] - '0');
                any = true;
                pos++;
            }
            if (!any || pos >= path.len || path.data[pos] != ']') return NULL;
            pos++;
            if (cur->kind != CLAY_J_ARR || idx >= cur->arr.n) return NULL;
            cur = cur->arr.items[idx];
        }
        if (pos == path.len) return cur;
        if (pos > path.len) return NULL;
    }
    return cur;
}

cl_json_node *cl_json_get_cstr(cl_json_node *root, const char *path) {
    return cl_json_get(root, cl_str_c(path));
}

cl_variant cl_json_lookup(cl_json_node *root, cl_str path, bool *ok) {
    cl_json_node *node = cl_json_get(root, path);
    if (node == NULL) {
        if (ok) *ok = false;
        return cl_variant_nil();
    }
    if (ok) *ok = true;
    return cl_json_to_variant(node);
}

cl_variant cl_json_to_variant(cl_json_node *node) {
    switch (node->kind) {
    case CLAY_J_NIL: return cl_variant_nil();
    case CLAY_J_BOOL: return cl_variant_bool(node->b);
    case CLAY_J_I64: return cl_variant_i64(node->i);
    case CLAY_J_F64: return cl_variant_f64(node->f);
    case CLAY_J_STR: return cl_variant_str(node->s);
    case CLAY_J_OBJ:
    case CLAY_J_ARR: return cl_variant_ptr(node);
    }
    return cl_variant_nil();
}

/* ------------------------------------------------------------- serializing */

static void write_escaped(cl_arena_buf *b, cl_str s) {
    cl_arena_buf_append(b, "\"", 1);
    for (size_t i = 0; i < s.len; i++) {
        char c = s.data[i];
        switch (c) {
        case '"': cl_arena_buf_append(b, "\\\"", 2); break;
        case '\\': cl_arena_buf_append(b, "\\\\", 2); break;
        case '\n': cl_arena_buf_append(b, "\\n", 2); break;
        case '\t': cl_arena_buf_append(b, "\\t", 2); break;
        case '\r': cl_arena_buf_append(b, "\\r", 2); break;
        case '\b': cl_arena_buf_append(b, "\\b", 2); break;
        case '\f': cl_arena_buf_append(b, "\\f", 2); break;
        default:
            if ((unsigned char)c < 0x20) {
                cl_arena_buf_printf(b, "\\u%04x", (unsigned char)c);
            } else {
                cl_arena_buf_append(b, &c, 1);
            }
        }
    }
    cl_arena_buf_append(b, "\"", 1);
}

static void write_node(cl_arena_buf *b, cl_json_node *node) {
    switch (node->kind) {
    case CLAY_J_NIL: cl_arena_buf_append(b, "null", 4); break;
    case CLAY_J_BOOL: cl_arena_buf_append(b, node->b ? "true" : "false",
                                          node->b ? 4 : 5);
        break;
    case CLAY_J_I64: cl_arena_buf_printf(b, "%lld", (long long)node->i); break;
    case CLAY_J_F64:
        if (node->f == (double)(int64_t)node->f && node->f >= -9e15 &&
            node->f <= 9e15) {
            cl_arena_buf_printf(b, "%.1f", node->f);
        } else {
            cl_arena_buf_printf(b, "%.17g", node->f);
        }
        break;
    case CLAY_J_STR: write_escaped(b, node->s); break;
    case CLAY_J_OBJ: {
        cl_arena_buf_append(b, "{", 1);
        for (size_t i = 0; i < node->obj.n; i++) {
            if (i) cl_arena_buf_append(b, ",", 1);
            write_escaped(b, node->obj.pairs[i].key);
            cl_arena_buf_append(b, ":", 1);
            write_node(b, node->obj.pairs[i].val);
        }
        cl_arena_buf_append(b, "}", 1);
        break;
    }
    case CLAY_J_ARR: {
        cl_arena_buf_append(b, "[", 1);
        for (size_t i = 0; i < node->arr.n; i++) {
            if (i) cl_arena_buf_append(b, ",", 1);
            write_node(b, node->arr.items[i]);
        }
        cl_arena_buf_append(b, "]", 1);
        break;
    }
    }
}

cl_err cl_json_write(cl_json_node *root, cl_arena *a, cl_str *out) {
    cl_arena_buf b = cl_arena_buf_make(a, 256);
    write_node(&b, root);
    *out = cl_arena_buf_deposit(&b);
    return CLAY_OK;
}