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

enum struct token_type
{
    invalid,
    left_curly_brace,
    right_curly_brace,
    identifier,
    fwd_slash,
    semicolon,
    colon,
    comma,
    open_paren,
    close_paren,
    namespace_colon,
    comment,
    less_than,
    greater_than,
    equal,
    string,

    introspect_marker,
    json_rpc_marker,
    uri_binary_rpc_marker,
    uri_json_rpc_marker,

    end_of_stream,
};

struct string_lit
{
    string_lit() = default;
    string_lit(char const *str_, int len_) : str(str_), len(len_) {}
    char const *str;
    int         len;
};
#define STRING_LIT(str) (string_lit){str, CHAR_COUNT(str)}

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
  bool                is_array;
  decl_var_metadata   metadata;
  struct decl_struct *is_struct;
};

struct decl_enum
{
  const char *name;
  int name_len;
  std::vector<const char*> names;
  std::vector<int> names_lens;
  std::vector<const char*> vals;
  std::vector<int> vals_lens;
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
  decl_struct_type         type;
  string_lit               name;
  std::vector<decl_struct> inner_structs;
  std::vector<decl_var>    variables;
  std::vector<decl_enum>   enums;
};

struct decl_struct_wrapper
{
  std::vector<string_lit> aliases;
  std::vector<string_lit> pre_decl_comments;
  decl_struct             decl;
};

struct token_t
{
  char      *str;
  int        len;
  token_type type;
};

struct tokeniser_t
{
    token_t *tokens;
    size_t tokens_index;
    int    indent_level;
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

