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
struct Defer
{
    proc_t p;
    Defer(proc_t proc) : p(proc) {}
    ~Defer() { p(); }
};

struct DeferHelper
{
    template <typename proc_t>
    Defer<proc_t> operator+(proc_t p) { return Defer<proc_t>(p); }
};

#define TOKEN_COMBINE2(x, y) x ## y
#define TOKEN_COMBINE(x, y) TOKEN_COMBINE2(x, y)
#define DEFER const auto TOKEN_COMBINE2(DeferLambda_, __COUNTER__) = DeferHelper() + [&]()
#define MIN_VAL(a, b) (a) < (b) ? a : b

#define ARRAY_COUNT(array) sizeof(array)/sizeof(array[0])
#define CHAR_COUNT(str) (ARRAY_COUNT(str) - 1)

struct String
{
    String() = default;
    String(char const *str_, int len_) : str(str_), len(len_) {}
    union {
        char const *str;
        char *str_;
    };
    int len;
};
#define STRING_LIT(str) (String) {str, CHAR_COUNT(str)}

struct DeclVariableMetadata
{
  // NOTE: For basic primitives like 'bool', this is already recognisable and
  // doesn't need to be converted to a nicer representation, so we encode that
  // situation into 'recognised'.
  bool          is_std_array;
  bool          recognised;
  String const *converted_type;          // Convert c-isms to more generic pseudo code if available i.e. uint64_t to uint64
  String const *converted_template_expr; // Ditto, but for the contents inside the template expression
};

struct DeclVariable
{
  String                template_expr;
  String                type;
  String                name;
  String                comment;
  String                value;    // NOTE: Most of the times this is empty
  bool                  is_array;
  DeclVariableMetadata  metadata;
  struct DeclStruct    *aliases_to;
};

enum DeclStructType
{
    Invalid,
    JsonRPCCommand,
    BinaryRPCCommand,
    RPCRequest,
    RPCResponse,
    Helper,
};

struct DeclStruct
{
  std::vector<String>       aliases;
  std::vector<String>       pre_decl_comments;
  String                    inheritance_parent_name;
  DeclStructType            type;
  String                    name;
  std::vector<DeclStruct>   inner_structs;
  std::vector<DeclVariable> variables;
};

struct DeclContext
{
    std::vector<DeclStruct> declarations;
    std::vector<DeclStruct> alias_declarations; // Declarations that were created to resolve C++ `using new_type = <type>;`
};
