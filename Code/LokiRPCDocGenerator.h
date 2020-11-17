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

using b32 = int;
#define LOCAL_PERSIST static
#define FILE_SCOPE static

#define MIN_VAL(a, b) (a) < (b) ? a : b
#define ARRAY_COUNT(array) sizeof(array)/sizeof(array[0])
#define CHAR_COUNT(str) (ARRAY_COUNT(str) - 1)

struct DeclVariableMetadata
{
  // NOTE: For basic primitives like 'bool', this is already recognisable and
  // doesn't need to be converted to a nicer representation, so we encode that
  // situation into 'recognised'.
  bool              is_std_array;
  bool              recognised;
  Dqn_String const *converted_type;          // Convert c-isms to more generic pseudo code if available i.e. uint64_t to uint64
  Dqn_String const *converted_template_expr; // Ditto, but for the contents inside the template expression
};

struct DeclVariable
{
  Dqn_String            template_expr;
  Dqn_String            type;
  Dqn_String            name;
  Dqn_String            comment;
  Dqn_String            value;    // NOTE: Most of the times this is empty
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
  std::vector<Dqn_String>   aliases;
  std::vector<Dqn_String>   pre_decl_comments;
  Dqn_String                inheritance_parent_name;
  DeclStructType            type;
  Dqn_String                name;
  std::vector<DeclStruct>   inner_structs;
  std::vector<DeclVariable> variables;
};

struct DeclContext
{
    std::vector<DeclStruct> declarations;
    std::vector<DeclStruct> alias_declarations; // Declarations that were created to resolve C++ `using new_type = <type>;`
};
