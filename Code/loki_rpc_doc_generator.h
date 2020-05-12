//
// Copyright 2019, Loki Project
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include <vector>
#include <map>
#include <csignal>
#include <algorithm>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#define local_persist static

template <typename proc_t>
struct defer
{
    proc_t p;
    defer(proc_t proc) : p(proc) {}
    ~defer() { p(); }
};

struct defer_helper
{
    template <typename proc_t>
    defer<proc_t> operator+(proc_t p) { return defer<proc_t>(p); }
};

#define TOKEN_COMBINE2(x, y) x ## y
#define TOKEN_COMBINE(x, y) TOKEN_COMBINE2(x, y)
#define DEFER const auto TOKEN_COMBINE2(defer_lambda_, __COUNTER__) = defer_helper() + [&]()
#define MIN_VAL(a, b) (a) < (b) ? a : b

#define ARRAY_COUNT(array) sizeof(array)/sizeof(array[0])
#define CHAR_COUNT(str) (ARRAY_COUNT(str) - 1)

struct string_lit
{
    string_lit() = default;
    string_lit(char const *str_, int len_) : str(str_), len(len_) {}
    union {
        char const *str;
        char *str_;
    };
    int len;
};
#define STRING_LIT(str) (string_lit){str, CHAR_COUNT(str)}

#define X_MACRO \
    X_ENTRY(invalid, "XX INVALID") \
    X_ENTRY(left_curly_brace, "{") \
    X_ENTRY(right_curly_brace, "}") \
    X_ENTRY(identifier, "<identifier>") \
    X_ENTRY(fwd_slash, "/") \
    X_ENTRY(semicolon, ";") \
    X_ENTRY(colon, ":") \
    X_ENTRY(comma, ",") \
    X_ENTRY(open_paren, "(") \
    X_ENTRY(close_paren, ")") \
    X_ENTRY(namespace_colon, "::") \
    X_ENTRY(comment, "<comment>") \
    X_ENTRY(less_than, "<") \
    X_ENTRY(greater_than, ">") \
    X_ENTRY(equal, "=") \
    X_ENTRY(string, "<string>") \
    X_ENTRY(asterisks, "*") \
    X_ENTRY(number, "{0-9}*") \
    X_ENTRY(ampersand, "&") \
    X_ENTRY(minus, "-") \
    X_ENTRY(dot, ".") \
    X_ENTRY(end_of_stream, "<eof>")

#define X_ENTRY(enum_val, stringified) enum_val,
enum struct token_type
{
    X_MACRO
};
#undef X_ENTRY

#define X_ENTRY(enum_val, stringified) STRING_LIT(stringified),
string_lit const token_type_string[] =
{
    X_MACRO
};
#undef X_ENTRY
#undef X_MACRO

struct decl_var_metadata
{
  string_lit const *example;
  string_lit const *converted_type;          // Convert c-isms to more generic pseudo code if availablei.e. uint64_t to uint64
  string_lit const *converted_template_expr; // Convert c-isms to more generic pseudo code if availablei.e. uint64_t to uint64
};

struct decl_var
{
  string_lit          template_expr;
  string_lit          type;
  string_lit          name;
  string_lit          comment;
  string_lit          value;    // NOTE: Most of the times this is empty
  bool                is_array;
  decl_var_metadata   metadata;
  struct decl_struct *aliases_to;
};

enum decl_struct_type
{
    invalid,
    rpc_command,
    request,
    response,
    helper,
};

struct decl_struct
{
  std::vector<string_lit>  aliases;
  std::vector<string_lit>  pre_decl_comments;

  decl_struct             *inheritance_parent; // NOTE: We only support 1, please, think of the children.
  string_lit               inheritance_parent_name;

  decl_struct_type         type;
  string_lit               description;
  string_lit               name;
  std::vector<decl_struct> inner_structs;
  std::vector<decl_var>    variables;
};

struct decl_context
{
    std::vector<decl_struct> declarations;
    std::vector<decl_struct> alias_declarations; // Declarations that were created to resolve C++ `using new_type = <type>;`
};

struct token_t
{
  int        new_lines_encountered;
  char      *last_new_line;
  char      *str;
  int        len;
  token_type type;
};

struct tokeniser_t
{
    char *file;
    char *last_new_line;
    int   line; // 0 based, i.e text file line 1 -> line 0
    char *ptr;
    int   scope_level;
    int   paren_level;
    int   angle_bracket_level;

    string_lit last_required_token_lit;
    token_type last_required_token_type;
    token_t    last_received_token;
};

enum struct decl_type_t
{
  unknown,
  string,
  string64,
  uint64,
  uint32,
  uint16,
  uint8,
  int64,
  int32,
  int16,
  int8,
  boolean,
};

struct type_conversion
{
    string_lit const from;
    string_lit const to;
};

