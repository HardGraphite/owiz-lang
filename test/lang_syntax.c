#include <assert.h>
#include <string.h>

#include <owiz.h>
#include "test_util.h"

// Execute the given source string and return whether succeeded.
// If no exception was thrown, push the result on stack.
static bool eval(owiz_machine_t *om, const char *src) {
    if (owiz_make_module(om, "", src, OWIZ_MKMOD_STRING | OWIZ_MKMOD_RETLAST) ||
            owiz_invoke(om, 0, OWIZ_IVK_MODULE)) {
        owiz_read_exception(om, 0, OWIZ_RDEXC_MSG | OWIZ_RDEXC_BT | OWIZ_RDEXC_PRINT);
        owiz_drop(om, 1);
        return false;
    }
    return true;
}

// Compile the given source string and return whether succeeded.
static bool check(owiz_machine_t *om, const char *src) {
    if (owiz_make_module(om, "", src, OWIZ_MKMOD_STRING)) {
        owiz_read_exception(om, 0, OWIZ_RDEXC_MSG | OWIZ_RDEXC_BT | OWIZ_RDEXC_PRINT);
        owiz_drop(om, 1);
        return false;
    }
    return true;
}

static bool eval_and_cmp_int(owiz_machine_t *om, const char *expr, intmax_t val) {
    if (!eval(om, expr))
        return false;
    intmax_t ret;
    if (owiz_read_int(om, 0, &ret))
        return false;
    owiz_drop(om, 1);
    return val == ret;
}

static bool eval_and_cmp_flt(owiz_machine_t *om, const char *expr, double val) {
    if (!eval(om, expr))
        return false;
    double ret;
    if (owiz_read_float(om, 0, &ret))
        return false;
    owiz_drop(om, 1);
    return val == ret;
}

static bool eval_and_cmp_str(owiz_machine_t *om, const char *expr, const char *val) {
    if (!eval(om, expr))
        return false;
    const char *ret;
    if (owiz_read_string(om, 0, &ret, NULL))
        return false;
    owiz_drop(om, 1);
    return strcmp(val, ret) == 0;
}

static bool eval_and_cmp_sym(owiz_machine_t *om, const char *expr, const char *val) {
    if (!eval(om, expr))
        return false;
    const char *ret;
    if (owiz_read_symbol(om, 0, &ret, NULL))
        return false;
    owiz_drop(om, 1);
    return strcmp(val, ret) == 0;
}

static void test_literals(owiz_machine_t *om) {
    // integer - dec
    TEST_ASSERT(eval_and_cmp_int(om, "0", 0));
    TEST_ASSERT(eval_and_cmp_int(om, "1234", 1234));
    TEST_ASSERT(eval_and_cmp_int(om, "12'34", 1234));
    TEST_ASSERT(eval_and_cmp_int(om, "12_34", 1234));
    // integer - hex
    TEST_ASSERT(eval_and_cmp_int(om, "0x1234", 0x1234));
    TEST_ASSERT(eval_and_cmp_int(om, "0xffff", 0xffff));
    TEST_ASSERT(eval_and_cmp_int(om, "0X12ab", 0X12ab));
    TEST_ASSERT(eval_and_cmp_int(om, "0xff'ff", 0xffff));
    // integer - bin
    TEST_ASSERT(eval_and_cmp_int(om, "0b10", 2));
    TEST_ASSERT(eval_and_cmp_int(om, "0b1010", 10));
    TEST_ASSERT(eval_and_cmp_int(om, "0B1111", 15));
    // integer - oct
    TEST_ASSERT(eval_and_cmp_int(om, "0o1234", 01234));
    TEST_ASSERT(eval_and_cmp_int(om, "01234", 01234));
    // floating point
    TEST_ASSERT(eval_and_cmp_flt(om, "0.0", 0.0));
    TEST_ASSERT(eval_and_cmp_flt(om, "0.1", 0.1));
    TEST_ASSERT(eval_and_cmp_flt(om, "0.125", 0.125));
    TEST_ASSERT(eval_and_cmp_flt(om, "1.0", 1.0));
    TEST_ASSERT(eval_and_cmp_flt(om, "1234.0", 1234.0));
    // string
    TEST_ASSERT(eval_and_cmp_str(om, "'hello, world'", "hello, world"));
    TEST_ASSERT(eval_and_cmp_str(om, "\"hello, world\"", "hello, world"));
    TEST_ASSERT(eval_and_cmp_str(
        om, "'\\a\\b\\e\\f\\n\\r\\t\\v\\''", "\a\b\x1b\f\n\r\t\v'"));
    TEST_ASSERT(eval_and_cmp_str(om, "'\\x20'", "\x20"));
    TEST_ASSERT(eval_and_cmp_str(om, "'\\u6587'", "\u6587"));
    TEST_ASSERT(eval_and_cmp_str(om, "'\\U0001f603'", "\U0001f603"));
    // symbol
    TEST_ASSERT(eval_and_cmp_sym(om, "`symbol", "symbol"));
}

static void test_expressions(owiz_machine_t *om) {
    TEST_ASSERT(eval_and_cmp_int(om, "1 + 2 * 3", 7));
    TEST_ASSERT(eval_and_cmp_int(om, "(1+2)*3", 9));
    TEST_ASSERT(eval_and_cmp_int(om, "(((1)+(2))*(3))", 9));

    TEST_ASSERT(check(om, "()"));
    TEST_ASSERT(check(om, "(1,)"));
    TEST_ASSERT(check(om, "(1, 2)"));
    TEST_ASSERT(check(om, "[]"));
    TEST_ASSERT(check(om, "[1]"));
    TEST_ASSERT(check(om, "[1,2,]"));
    TEST_ASSERT(check(om, "{,}"));
    TEST_ASSERT(check(om, "{1}"));
    TEST_ASSERT(check(om, "{}"));
    TEST_ASSERT(check(om, "{1 => '1'}"));
    TEST_ASSERT(check(om, "{1=>'1',2=>'2',}"));
    TEST_ASSERT(check(om, "((),())"));
    TEST_ASSERT(check(om, "[[],[]]"));
    TEST_ASSERT(check(om, "{{},{}}"));
    TEST_ASSERT(check(om, "(\n1\n)"));
    TEST_ASSERT(check(om, "(\n1\n,\n)"));
    TEST_ASSERT(check(om, "[\n1\n]"));
    TEST_ASSERT(check(om, "{\n1\n}"));
    TEST_ASSERT(check(om, "{\n1\n=>'1'\n,\n2\n=>'2'\n}"));
    TEST_ASSERT(!check(om, "(,)"));
    TEST_ASSERT(!check(om, "[,]"));
    TEST_ASSERT(!check(om, "{,,}"));

    TEST_ASSERT(check(om, "function()"));
    TEST_ASSERT(check(om, "function(1)"));
    TEST_ASSERT(check(om, "function(1,)"));
    TEST_ASSERT(check(om, "function(\n1\n,\n2\n)"));
    TEST_ASSERT(check(om, "object[1]"));
    TEST_ASSERT(check(om, "object[1,2]"));
    TEST_ASSERT(check(om, "object[\n1\n,\n2\n]"));

    TEST_ASSERT(eval_and_cmp_int(om, "f=func(a,b,c)=>a*b+c; f(3,2,1)", 7));
    TEST_ASSERT(eval_and_cmp_int(om, "f=func(a,b) if a<b; return a; end; return b; end; f(-1,1)", -1));
}

static void test_statements(owiz_machine_t *om) {
    // return statement
    TEST_ASSERT(check(om, "func foo(); return; end"));
    TEST_ASSERT(eval_and_cmp_int(om, "func foo(); return 1; end; foo()", 1));
    // import statement
    TEST_ASSERT(check(om, "import sys"));
    // if-else statement
    TEST_ASSERT(eval_and_cmp_int(
        om, "a=1; b=0; if a<b; y=1; elif a==b; y=0; else; y=-1; end; y", -1));
    // while statement
    TEST_ASSERT(eval_and_cmp_int(om, "i=0; while i<100; i+=1; end; i", 100));
}

int main(void) {
    owiz_machine_t *const om = owiz_create();
    test_literals(om);
    test_expressions(om);
    test_statements(om);
    owiz_destroy(om);
}
