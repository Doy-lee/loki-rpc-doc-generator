#define _CRT_SECURE_NO_WARNINGS

#include "loki_rpc_doc_generator.h"

bool string_lit_cmp(string_lit a, string_lit b)
{
    bool result = (a.len == b.len && (strncmp(a.str, b.str, a.len) == 0));
    return result;
}

bool operator==(string_lit const &lhs, string_lit const &rhs)
{
  bool result = string_lit_cmp(lhs, rhs);
  return result;
}

string_lit token_to_string_lit(token_t token)
{
    string_lit result = {};
    result.str        = token.str;
    result.len        = token.len;
    return result;
}

string_lit trim_whitespace_around(string_lit in)
{
    string_lit result = in;
    while (result.str && isspace(result.str[0]))
    {
        result.str++;
        result.len--;
    }

    while (result.str && isspace(result.str[result.len - 1]))
        result.len--;

    return result;
}

string_lit string_lit_till_eol(char const *ptr)
{
    char const *end = ptr;
    while (end[0] != '\n' && end[0] != 0)
        end++;

    string_lit result = {};
    result.str        = ptr;
    result.len        = end - ptr;
    return result;
}

char *read_entire_file(char const *file_path, ptrdiff_t *file_size)
{
    char *result = nullptr;
    FILE *handle = fopen(file_path, "rb");
    if (!handle)
        return result;

    DEFER { fclose(handle); };
    fseek(handle, 0, SEEK_END);
    *file_size = ftell(handle);
    rewind(handle);

    result = static_cast<char *>(malloc(sizeof(char) * ((*file_size) + 1)));
    if (!result)
        return result;

    size_t elements_read = fread(result, *file_size, 1, handle);
    if (elements_read != 1)
    {
        free(result);
        return nullptr;
    }

    result[*file_size] = '\0';
    return result;
}

char *str_find(char *start, string_lit find, int *num_new_lines = nullptr, char **last_new_line = nullptr)
{
    int num_new_lines_ = 0;
    if (!num_new_lines) num_new_lines = &num_new_lines_;

    for (char *ptr = start; ptr && ptr[0]; ptr++)
    {
        if (ptr[0] == '\n')
        {
            (*num_new_lines)++;
            if (last_new_line) *last_new_line = (ptr + 1);
        }
        if (strncmp(ptr, find.str, find.len) == 0)
            return ptr;
    }

    return nullptr;
}

bool char_is_alpha     (char ch) { return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'); }
bool char_is_digit     (char ch) { return (ch >= '0' && ch <= '9'); }
bool char_is_alphanum  (char ch) { return char_is_alpha(ch) || char_is_digit(ch); }

char char_to_lower(char ch)
{
   if (ch >= 'A' && ch <= 'Z')
     ch += ('a' - 'A');
   return ch;
}

bool token_is_eof(token_t token)
{
    bool result = token.type == token_type::end_of_stream;
    return result;
}

auto INTROSPECT_MARKER = STRING_LIT("LOKI_RPC_DOC_INTROSPECT");
bool tokeniser_next_marker(tokeniser_t *tokeniser, string_lit marker)
{
    char *last_new_line = nullptr;
    int num_new_lines   = 0;
    char *ptr           = str_find(tokeniser->ptr, marker, &num_new_lines, &last_new_line);
    if (ptr)
    {
        // TODO(doyle): We don't account for new lines in the marker string
        if (marker == INTROSPECT_MARKER)
        {
            // NOTE: Sort of hacky. If we're looking for an introspect marker,
            // we don't care about the current state if we're in a function
            // list, template argument etc, we're likely finished parsing that
            // C++ construct, or trying to leave that context.
            tokeniser->paren_level         = 0;
            tokeniser->scope_level         = 0;
            tokeniser->angle_bracket_level = 0;
        }

        tokeniser->line         += num_new_lines;
        tokeniser->last_new_line = last_new_line;
        tokeniser->ptr           = ptr + marker.len;
    }

    return ptr != nullptr;
}

// -------------------------------------------------------------------------------------------------
//
// Tokeniser Report Error
//
// -------------------------------------------------------------------------------------------------
void tokeniser_report_error_preamble(tokeniser_t *tokeniser)
{
    if (tokeniser->file) fprintf(stderr, "Tokenising error in file %s:%d", tokeniser->file, tokeniser->line + 1);
    else                 fprintf(stderr, "Tokenising error buffer");
}

void tokeniser_report_parse_struct_custom_error(tokeniser_t *tokeniser, string_lit *parent_name, char const *fmt = nullptr, ...)
{
    string_lit line = string_lit_till_eol(tokeniser->last_new_line);
    tokeniser_report_error_preamble(tokeniser);

    if (parent_name)
        fprintf(stderr, " in struct '%.*s'", parent_name->len, parent_name->str);

    fprintf(stderr, "\n");
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);

    fprintf(stderr, "\n\n%.*s\n\n", line.len, line.str);
}

void tokeniser_report_last_required_token(tokeniser_t *tokeniser, char const *fmt = nullptr, ...)
{
    tokeniser_report_error_preamble(tokeniser);
    string_lit const line = (tokeniser->file) ? string_lit_till_eol(tokeniser->last_new_line) : string_lit_till_eol(tokeniser->ptr);

    if (fmt)
    {
        fprintf(stderr, "\n");
        va_list va;
        va_start(va, fmt);
        vfprintf(stderr, fmt, va);
        va_end(va);
    }

    fprintf(stderr,
            "\nThe last required token was \"%.*s\", tokeniser received \"%.*s\"\n\n"
            "%.*s\n\n",
            token_type_string[(int)tokeniser->last_required_token_type].len,
            token_type_string[(int)tokeniser->last_required_token_type].str,
            tokeniser->last_received_token.len,
            tokeniser->last_received_token.str,
            line.len,
            line.str);
}

void tokeniser_report_custom_error(tokeniser_t *tokeniser, char const *fmt, ...)
{
    tokeniser_report_error_preamble(tokeniser);
    string_lit const line = (tokeniser->file) ? string_lit_till_eol(tokeniser->last_new_line) : string_lit_till_eol(tokeniser->ptr);

    fprintf(stderr, "\n");
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);

    fprintf(stderr, "\n\n%.*s\n\n", line.len, line.str);
}

// -------------------------------------------------------------------------------------------------
//
// Tokeniser Iteration
//
// -------------------------------------------------------------------------------------------------
token_t tokeniser_peek_token2(tokeniser_t *tokeniser)
{
    token_t result = {};
    char *ptr      = tokeniser->ptr;

    while (isspace(ptr[0]))
    {
        if (ptr[0] == '\n')
        {
            result.new_lines_encountered++;
            result.last_new_line = (ptr + 1);
        }
        ptr++;
    }

    result.str = ptr;
    result.len = 1;
    switch (result.str[0])
    {
        case '{': result.type = token_type::left_curly_brace; break;
        case '}': result.type = token_type::right_curly_brace; break;
        case ';': result.type = token_type::semicolon; break;
        case '=': result.type = token_type::equal; break;
        case '<': result.type = token_type::less_than; break;
        case '>': result.type = token_type::greater_than; break;
        case '(': result.type = token_type::open_paren; break;
        case ')': result.type = token_type::close_paren; break;
        case ',': result.type = token_type::comma; break;
        case '*': result.type = token_type::asterisks; break;
        case '&': result.type = token_type::ampersand; break;
        case '-': result.type = token_type::minus; break;
        case '.': result.type = token_type::dot; break;
        case 0: result.type   = token_type::end_of_stream; break;

        case '"':
        {
          result.type = token_type::string;
          result.str  = ++ptr;
          while (ptr[0] != '"')
            ptr++;
          result.len = static_cast<int>(ptr - result.str);
          ptr++;
        }
        break;

        case ':':
        {
          result.type = token_type::colon;
          if ((++ptr)[0] == ':')
          {
            ptr++;
            result.type = token_type::namespace_colon;
            result.len  = 2;
          }
        }
        break;

        case '/':
        {
          ptr++;
          result.type = token_type::fwd_slash;
          if (ptr[0] == '/' || ptr[0] == '*')
          {
            result.type = token_type::comment;
            if (ptr[0] == '/')
            {
              ptr++;
              if (ptr[0] == ' ' && ptr[1] && ptr[1] == ' ')
              {
                result.str = ptr++;
              }
              else
              {
                while (ptr[0] == ' ' || ptr[0] == '\t')
                  ptr++;
                result.str = ptr;
              }

              while (ptr[0] != '\n')
                ptr++;
              result.len = static_cast<int>(ptr - result.str);
            }
            else
            {
              result.str = ++ptr;
              for (;;)
              {
                while (ptr[0] != '*')
                  ptr++;
                ptr++;
                if (ptr[0] == '/')
                {
                  result.len = static_cast<int>(ptr - result.str - 2);
                  ptr++;
                  break;
                }
              }
            }
          }
        }
        break;

        default:
        {
          if (char_is_alpha(ptr[0]) || ptr[0] == '_' || ptr[0] == '!')
          {
            ptr++;
            while (char_is_alphanum(ptr[0]) || ptr[0] == '_')
              ptr++;
            result.type = token_type::identifier;
          }
          else if(char_is_digit(ptr[0]))
          {
            ptr++;
            while (char_is_digit(ptr[0]) || ptr[0] == '_')
              ptr++;
            result.type = token_type::number;
          }

          result.len = static_cast<int>(ptr - result.str);
        }
        break;
    }

    if (result.type == token_type::invalid)
    {
        tokeniser_report_custom_error(tokeniser, "Unhandled token '%c' in character stream when iterating tokeniser", result.str[0]);
        assert(result.type != token_type::invalid);
    }
    return result;
}

void tokeniser_accept_token(tokeniser_t *tokeniser, token_t token)
{
    if (token.type == token_type::left_curly_brace)
    {
        tokeniser->scope_level++;
    }
    else if (token.type == token_type::right_curly_brace)
    {
        tokeniser->scope_level--;
    }
    else if (token.type == token_type::open_paren)
    {
        tokeniser->paren_level++;
    }
    else if (token.type == token_type::close_paren)
    {
        tokeniser->paren_level--;
    }
    else if (token.type == token_type::less_than)
    {
        tokeniser->angle_bracket_level++;
    }
    else if (token.type == token_type::greater_than)
    {
        tokeniser->angle_bracket_level--;
    }

    assert(tokeniser->paren_level >= 0);
    assert(tokeniser->scope_level >= 0);
    assert(tokeniser->angle_bracket_level >= 0);
    tokeniser->ptr = token.str + token.len;

    if (!tokeniser->last_new_line) tokeniser->last_new_line = token.str;
    if (token.new_lines_encountered) tokeniser->last_new_line = token.last_new_line;
    tokeniser->line += token.new_lines_encountered;

    if (token.type == token_type::string)
        tokeniser->ptr++; // Include the ending quotation mark " of a "string"
}

bool tokeniser_require_token_type(tokeniser_t *tokeniser, token_type type, token_t *token = nullptr)
{
    token_t token_ = {};
    if (!token) token = &token_;
    *token                              = tokeniser_peek_token2(tokeniser);
    tokeniser->last_required_token_type = type;
    tokeniser->last_received_token      = *token;
    bool result = (token->type == type);
    if (result) tokeniser_accept_token(tokeniser, *token);
    return result;
}

bool tokeniser_require_token(tokeniser_t *tokeniser, string_lit string, token_t *token = nullptr)
{
    token_t token_ = {};
    if (!token) token = &token_;
    *token                              = tokeniser_peek_token2(tokeniser);
    tokeniser->last_required_token_type = token->type;
    tokeniser->last_received_token      = *token;
    bool result = (token_to_string_lit(*token) == string);
    if (result) tokeniser_accept_token(tokeniser, *token);
    return result;
}

token_t tokeniser_next_token(tokeniser_t *tokeniser)
{
    token_t result = tokeniser_peek_token2(tokeniser);
    tokeniser_accept_token(tokeniser, result);
    return result;
}

bool tokeniser_advance_until_token_type(tokeniser_t *tokeniser, token_type type, token_t *token = nullptr)
{

    token_t token_ = {};
    if (!token) token = &token_;
    for (;;)
    {
        *token = tokeniser_next_token(tokeniser);
        if (token->type == type) return true;
        if (token->type == token_type::end_of_stream) return false;
    }
}

bool tokeniser_advance_until_scope_level(tokeniser_t *tokeniser, int scope_level)
{
    for (;;)
    {
        token_t result = tokeniser_next_token(tokeniser);
        if (tokeniser->scope_level == scope_level) return true;
        if (result.type == token_type::end_of_stream) return false;
    }
}

bool tokeniser_advance_until_paren_level(tokeniser_t *tokeniser, int paren_level)
{
    for (;;)
    {
        token_t result = tokeniser_next_token(tokeniser);
        if (tokeniser->paren_level == paren_level) return true;
        if (result.type == token_type::end_of_stream) return false;
    }
}

decl_var_metadata derive_metadata_from_variable(decl_var const *variable)
{
  decl_var_metadata result  = {};
  string_lit const var_type = (variable->is_array) ? variable->template_expr : variable->type;

  if (var_type == STRING_LIT("std::string"))
  {
    local_persist string_lit const NICE_NAME = STRING_LIT("string");
    result.converted_type = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("uint64_t"))
  {
    local_persist string_lit const NICE_NAME = STRING_LIT("uint64");
    result.converted_type                    = &NICE_NAME;

  }
  else if (var_type == STRING_LIT("uint32_t"))
  {
    local_persist string_lit const NICE_NAME = STRING_LIT("uint32");
    result.converted_type                    = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("uint16_t"))
  {
    local_persist string_lit const NICE_NAME = STRING_LIT("uint16");
    result.converted_type = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("uint8_t"))
  {
    local_persist string_lit const NICE_NAME = STRING_LIT("uint8");
    result.converted_type = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("int64_t"))
  {
    local_persist string_lit const NICE_NAME = STRING_LIT("int64");
    result.converted_type                    = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("int32_t"))
  {
    local_persist string_lit const NICE_NAME = STRING_LIT("int32");
    result.converted_type                    = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("int16_t"))
  {
    local_persist string_lit const NICE_NAME = STRING_LIT("int16");
    result.converted_type = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("int8_t"))
  {
    local_persist string_lit const NICE_NAME = STRING_LIT("int8");
    result.converted_type = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("blobdata"))
  {
      local_persist string_lit const NICE_NAME = STRING_LIT("string");
      result.converted_type                    = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("crypto::hash"))
  {
      local_persist string_lit const NICE_NAME = STRING_LIT("string[64]");
      result.converted_type                    = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("difficulty_type"))
  {
      local_persist string_lit const NICE_NAME = STRING_LIT("uint64");
      result.converted_type                    = &NICE_NAME;
  }
  return result;
};

static bool is_type_modifier(token_t token)
{
    static string_lit const modifiers[] = {
        STRING_LIT("constexpr"),
        STRING_LIT("const"),
        STRING_LIT("static"),
    };

    for (string_lit const &mod : modifiers)
    {
        if (token_to_string_lit(token) == mod) return true;
    }

  return false;
}

bool parse_function_starting_from_function_name(tokeniser_t *tokeniser)
{
    // NOTE: This function *should* be able to handle constructors and free standing functions (probably).

    /*
       PARSING SCENARIO
       struct Foo
       {
              +------------------ Tokeniser should be here, or, before
              V
           Foo(...) { ... }
           int x;
       };
    */

    // NOTE: Eat all parameters in function
    {
        int original_paren_level = tokeniser->paren_level;
        if (!tokeniser_advance_until_token_type(tokeniser, token_type::open_paren))
        {
            // @TODO(doyle): Logging
            return false;
        }

        if (!tokeniser_advance_until_paren_level(tokeniser, original_paren_level))
        {
            // @TODO(doyle): Logging
            return false;
        }

    }

    // NOTE: Remove const from any member functions
    token_t peek = tokeniser_peek_token2(tokeniser);
    if (is_type_modifier(peek))
        tokeniser_next_token(tokeniser);


    // NOTE: If next token is '=' then we have a 'Foo = default();' situation
    if (tokeniser_require_token_type(tokeniser, token_type::equal))
    {
        tokeniser_advance_until_token_type(tokeniser, token_type::semicolon);
    }
    else if (tokeniser_require_token_type(tokeniser, token_type::left_curly_brace))
    {
        /*
           PARSING SCENARIO
           struct Foo
           {
                        +------------------ Tokeniser hit a open brace, '{'
                        V
               Foo(...) { ... }
               int x;
           };
        */

        int original_scope_level = tokeniser->scope_level - 1;
        assert(tokeniser->scope_level > 0);
        assert(tokeniser->scope_level != original_scope_level);
        tokeniser_advance_until_scope_level(tokeniser, original_scope_level);
    }
    else if (tokeniser_require_token_type(tokeniser, token_type::colon))
    {
        /*
           PARSING SCENARIO
           struct Foo
           {
                        +------------------ Tokeniser hit a colon, ':'
                        V
               Foo(...) : x(1) { ... }
               int x;
           };
        */

        int original_scope_level = tokeniser->scope_level;
        tokeniser_advance_until_token_type(tokeniser, token_type::left_curly_brace);
        assert(tokeniser->scope_level != original_scope_level);
        tokeniser_advance_until_scope_level(tokeniser, original_scope_level);
    }
    else if (tokeniser_require_token_type(tokeniser, token_type::semicolon))
    {
        /*
           PARSING SCENARIO
           struct Foo
           {
                       +------------------ Tokeniser hit a semicolon, ';'
                       V
               Foo(...);
               int x;
           };
        */
        // NOTE: Do nothing, we're at the end of the construct declaration
    }
    else
    {
        // @TODO(doyle): What case is this? No idea
        return false;
    }
}

enum struct parse_variable_status
{
    not_variable,
    member_function,
    failed,
    success,
};

// NOTE: Parse any declaration of the sort <Type> <variable_name> = <value>
//       But since C++ function declarations are *almost* like <Type> <name>
//       including constructors, as side bonus this function handles parsing the
//       function name and type which is placed into the variable.
parse_variable_status parse_type_and_var_decl(tokeniser_t *tokeniser, decl_var *variable, string_lit parent_struct_name)
{
    // NOTE: Parse Variable Type and Name
    {
        token_t token = {};
        for (;;) // ignore type modifiers like const and static
        {
            token_t next = tokeniser_next_token(tokeniser);
            if (!is_type_modifier(next))
            {
                token = next;
                break;
            }
        }

        // NOTE: Tokeniser is parsing a constructor like --->  Foo(...) : foo(...), { ... }
        if (token_to_string_lit(token) == parent_struct_name) // C++ Constructor
        {
            if (!parse_function_starting_from_function_name(tokeniser))
            {
                tokeniser_report_custom_error(
                    tokeniser,
                    "Failed to parse constructor '%.*s', internal logic error (most likely, C++ "
                    "has 10 gazillion ways to write the same thing)",
                    parent_struct_name.len,
                    parent_struct_name.str);
                return parse_variable_status::failed;
            }

            return parse_variable_status::not_variable;
        }

        // NOTE: Otherwise, struct member function, or variable

        // NOTE: Because of C++ namespaces and stuff in variable types, just
        // iterate the tokeniser until we hit the first token with
        // a preceeding whitespace where AND the '<>' templates scope level
        // is 0 to capture the variable type.

        // NOTE: Tokeniser is parsing a line like, for example,
        // std::unordered_map<std::string, Foo> foo
        //   or
        // Foo foo

        token_t variable_type_first_token         = token;
        token_t variable_type_one_past_last_token = {};
        assert(tokeniser->angle_bracket_level == 0);

        for (;;)
        {
            token_t next = tokeniser_next_token(tokeniser);
            if (next.type == token_type::identifier &&
                isspace(next.str[-1]) &&
                tokeniser->angle_bracket_level == 0)
            {
                variable_type_one_past_last_token = next;
                break;
            }
        }

        token_t variable_name = variable_type_one_past_last_token;
        for (;;) // ignore type modifiers like const and static
        {
            if (!is_type_modifier(variable_name))
                break;
            variable_name = tokeniser_next_token(tokeniser);
        }

        // @TODO(doyle): This captures type modifiers that are placed after the variable decl
        variable->name     = token_to_string_lit(variable_name);
        variable->type.str = variable_type_first_token.str;
        variable->type.len = &variable_type_one_past_last_token.str[-1] - variable_type_first_token.str;
        variable->type     = trim_whitespace_around(variable->type);

        token_t peek = tokeniser_peek_token2(tokeniser);
        if (token_to_string_lit(variable_name) == STRING_LIT("operator") || peek.type == token_type::open_paren)
        {
            if (!parse_function_starting_from_function_name(tokeniser))
            {
                tokeniser_report_custom_error(tokeniser,
                                              "Failed to parse function '%.*s', internal logic error (most likely, C++ "
                                              "has 10 gazillion ways to write the same thing)",
                                              variable->name.len,
                                              variable->name.str);
                return parse_variable_status::failed;
            }

            return parse_variable_status::member_function;
        }

        if (token_is_eof(variable_name) || token_is_eof(peek))
            return parse_variable_status::failed;
    }

    for (int i = 0; i < variable->type.len; ++i)
    {
        if (variable->type.str[i] == '<')
        {
            variable->template_expr.str = variable->type.str + (++i);
            for (int j = ++i; j < variable->type.len; ++j)
            {
                if (variable->type.str[j] == '>')
                {
                    char const *template_expr_end = variable->type.str + j;
                    variable->template_expr.len    = static_cast<int>(template_expr_end - variable->template_expr.str);
                    break;
                }
            }
            break;
        }
    }

    // TODO(doyle): This is horribly incomplete, but we only specify arrays in
    // terms of C++ containers in loki which all have templates. So meh.
    variable->is_array = variable->template_expr.len > 0;
    variable->metadata = derive_metadata_from_variable(variable);

    token_t token = tokeniser_next_token(tokeniser);
    if (token.type == token_type::equal)
    {
        token_t variable_value_first_token         = tokeniser_next_token(tokeniser);
        token_t variable_value_one_past_last_token = {};
        if (!tokeniser_advance_until_token_type(tokeniser, token_type::semicolon, &variable_value_one_past_last_token))
            return parse_variable_status::failed;

        variable->value.str = variable_value_first_token.str;
        variable->value.len = &variable_value_one_past_last_token.str[-1] - variable_value_first_token.str;
    }
    else if (token.type != token_type::semicolon)
    {
       tokeniser_advance_until_token_type(tokeniser, token_type::semicolon);
    }


    token = tokeniser_peek_token2(tokeniser);
    if (token.type == token_type::comment)
    {
        variable->comment = token_to_string_lit(token);
        token = tokeniser_next_token(tokeniser);
    }

    string_lit const skip_marker = STRING_LIT("@NoLokiRPCDocGen");
    if (variable->comment.len >= skip_marker.len && strncmp(variable->comment.str, skip_marker.str, skip_marker.len) == 0)
        return parse_variable_status::not_variable;

    return parse_variable_status::success;
}

bool parse_rpc_alias_names(tokeniser_t *tokeniser, decl_struct *result)
{
    /*
       PARSING SCENARIO
                                  +-------- Tokeniser is here
                                  V
       static constexpr auto names() { return NAMES("get_block_hash", "on_get_block_hash", "on_getblockhash"); }
    */

    if (!tokeniser_require_token_type(tokeniser, token_type::open_paren))
        return false;

    if (!tokeniser_require_token_type(tokeniser, token_type::close_paren))
        return false;

    if (!tokeniser_require_token_type(tokeniser, token_type::left_curly_brace))
        return false;

    if (!tokeniser_require_token(tokeniser, STRING_LIT("return")))
        return false;

    if (!tokeniser_require_token(tokeniser, STRING_LIT("NAMES")))
        return false;

    if (!tokeniser_require_token_type(tokeniser, token_type::open_paren))
        return false;

    token_t alias = {};
    for (;;)
    {
        if (tokeniser_require_token_type(tokeniser, token_type::string, &alias))
            result->aliases.push_back(token_to_string_lit(alias));

        token_t token = tokeniser_next_token(tokeniser);
        if (token.type == token_type::close_paren)
            return true;
        else if (token.type != token_type::comma)
            return false;
    }
}

decl_struct UNRESOLVED_ALIAS_STRUCT        = {};
static string_lit const COMMAND_RPC_PREFIX = STRING_LIT("COMMAND_RPC_");
bool parse_struct(tokeniser_t *tokeniser, decl_struct *result, bool root_struct, string_lit *parent_name)
{
    if (!tokeniser_require_token(tokeniser, STRING_LIT("struct")) &&
        !tokeniser_require_token(tokeniser, STRING_LIT("class")))
    {
        tokeniser_report_parse_struct_custom_error(
            tokeniser,
            parent_name,
            "parse_struct called on child struct did not have expected 'struct' or 'class' identifier");
        return false;
    }

    token_t struct_name = {};
    if (!tokeniser_require_token_type(tokeniser, token_type::identifier, &struct_name))
    {
        tokeniser_report_last_required_token(tokeniser, "Parsing struct, expected <identifier> for the struct name");
        return false;
    }

    result->name = token_to_string_lit(struct_name);
    if (!root_struct)
    {
        if (result->name == STRING_LIT("request"))
        {
            result->type = decl_struct_type::request;
        }
        if (result->name == STRING_LIT("response"))
        {
            result->type = decl_struct_type::response;
        }
    }

    if (result->type == decl_struct_type::invalid)
        result->type = decl_struct_type::helper;

    if (tokeniser_require_token_type(tokeniser, token_type::colon)) // Struct Inheritance
    {
        for (;;)
        {
            token_t inheritance;
            if (tokeniser_require_token_type(tokeniser, token_type::identifier, &inheritance))
            {
                string_lit inheritance_lit = token_to_string_lit(inheritance);
                if (inheritance_lit == STRING_LIT("PUBLIC")) { }
                else if (inheritance_lit == STRING_LIT("BINARY")) { }
                else if (inheritance_lit == STRING_LIT("LEGACY")) { }
                else if (inheritance_lit == STRING_LIT("EMPTY")) { }
                else if (inheritance_lit == STRING_LIT("RPC_COMMAND")) { }
                else if (inheritance_lit == STRING_LIT("STATUS"))
                {
                    // @TODO(doyle):
                }
                else if (inheritance_lit == STRING_LIT("public"))
                {
                    // NOTE: Ignore public modifier
                    continue;
                }
                else
                {
                    /*
                       (POTENTIAL) PARSING SCENARIO
                       We are parsing inheritance construct. We may or may not
                       have a namespace, it could directly reference the class.


                                           +---- inheritance token
                                           |
                                           |        +-+---------------- Tokeniser is in either of these positions (depending on if it is namespaced or not)
                                           V        V V
                       struct Foo : public Namespace::Parent { ...  };
                    */

                    for (token_t next = tokeniser_peek_token2(tokeniser);  // NOTE: Advance tokeniser until end of parent inheritance name
                         ;
                         next = tokeniser_next_token(tokeniser))
                    {
                        if (next.type == token_type::comma)
                        {
                            tokeniser_report_parse_struct_custom_error(
                                tokeniser,
                                parent_name,
                                "Encountered commma ',' in inheritance line indicating multiple "
                                "inheritance which we don't support for arbitrary types yet, unless they're the RPC tags "
                                "like PUBLIC, BINARY, LEGACY, EMPTY which are handled in the special case");
                            return false;
                        }

                        if (next.type == token_type::namespace_colon)
                        {
                            if (!tokeniser_require_token_type(tokeniser, token_type::identifier))
                            {
                                tokeniser_report_parse_struct_custom_error(
                                    tokeniser,
                                    parent_name,
                                    "Namespace colon '::' should be followed by an identifier in 'struct %.*s'",
                                    result->name.len,
                                    result->name.str);
                                return false;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }

                    token_t one_after_last_inheritance_name = tokeniser_peek_token2(tokeniser);
                    if (one_after_last_inheritance_name.type == token_type::left_curly_brace)
                    {
                        result->inheritance_parent_name.len     = &one_after_last_inheritance_name.str[-1] - inheritance_lit.str;
                        result->inheritance_parent_name.str     = inheritance_lit.str;
                        result->inheritance_parent_name = trim_whitespace_around(result->inheritance_parent_name);
                        break;
                    }
                    else
                    {
                        tokeniser_report_parse_struct_custom_error(
                            tokeniser,
                            parent_name,
                            "Detected inheritance identifier '%.*s' following struct name '%.*s'",
                            result->inheritance_parent_name.len,
                            result->inheritance_parent_name.str,
                            result->name.len,
                            result->name.str);
                    }
                }

                if (!tokeniser_require_token_type(tokeniser, token_type::comma))
                    break;
            }
            else
            {
                tokeniser_report_last_required_token(tokeniser, "Struct inheritance detected with ':' after the struct name, but no <identifiers> detected after the name");
                return false;
            }
        }
    }

    int original_scope_level = tokeniser->scope_level;
    if (!tokeniser_require_token_type(tokeniser, token_type::left_curly_brace, nullptr))
    {
        tokeniser_report_last_required_token(tokeniser, "Struct inheritance detected with ':' after the struct name, but no <identifiers> detected after the name");
        return false;
    }

    // ---------------------------------------------------------------------------------------------
    //
    // Parse Struct Contents
    //
    // ---------------------------------------------------------------------------------------------
    for (token_t token = tokeniser_peek_token2(tokeniser);
         token.type == token_type::identifier || token.type == token_type::comment;
         token = tokeniser_peek_token2(tokeniser))
    {
        if (token.type == token_type::comment)
        {
            tokeniser_next_token(tokeniser);
            continue;
        }

        string_lit token_lit = token_to_string_lit(token);
        if (token_lit == STRING_LIT("struct") || token_lit == STRING_LIT("class"))
        {
            decl_struct decl = {};
            if (!parse_struct(tokeniser, &decl, false /*root_struct*/, &result->name))
                return false;

            result->inner_structs.push_back(std::move(decl));
        }
        else if (token_lit == STRING_LIT("enum"))
        {
            // @TODO(doyle): Actually handle
            tokeniser_advance_until_token_type(tokeniser, token_type::semicolon);
        }
        else if (token_lit == STRING_LIT("KV_MAP_SERIALIZABLE"))
        {
            tokeniser_next_token(tokeniser);
        }
        else if (token_lit == STRING_LIT("BEGIN_SERIALIZE"))
        {
            if (!tokeniser_next_marker(tokeniser, STRING_LIT("END_SERIALIZE()")))
            {
                tokeniser_report_parse_struct_custom_error(
                    tokeniser,
                    parent_name,
                    "Tokeniser encountered BEGIN_SERIALIZE() macro, failed to find closing macro END_SERIALIZE()");
                return false;
            }
        }
        else if (token_lit == STRING_LIT("BEGIN_KV_SERIALIZE_MAP"))
        {
            if (!tokeniser_next_marker(tokeniser, STRING_LIT("END_KV_SERIALIZE_MAP()")))
            {
                tokeniser_report_parse_struct_custom_error(
                    tokeniser,
                    parent_name,
                    "Tokeniser encountered END_KV_SERIALIZE_MAP() macro, failed to find closing macro END_SERIALIZE()");
                return false;
            }
        }
        else if (token_lit == STRING_LIT("using"))
        {
            tokeniser_next_token(tokeniser);
            token_t using_name = {};
            if (tokeniser_require_token_type(tokeniser, token_type::identifier, &using_name))
            {
                if (tokeniser_require_token_type(tokeniser, token_type::equal))
                {
                    token_t using_value_first_token         = tokeniser_next_token(tokeniser);
                    token_t using_value_one_past_last_token = {};
                    tokeniser_advance_until_token_type(tokeniser, token_type::semicolon, &using_value_one_past_last_token);

                    string_lit using_value = {using_value_first_token.str,
                                              (int)(&using_value_one_past_last_token.str[-1] - using_value_first_token.str)};

                    decl_var variable   = {};
                    variable.aliases_to = &UNRESOLVED_ALIAS_STRUCT; // NOTE: Resolve the declaration later
                    variable.name       = token_to_string_lit(using_name);
                    variable.type       = trim_whitespace_around(using_value);

                    token_t comment = {};
                    if (tokeniser_require_token_type(tokeniser, token_type::comment, &comment))
                        variable.comment = token_to_string_lit(comment);

                    result->variables.push_back(variable);
                }
                else
                {
                    tokeniser_report_last_required_token(tokeniser);
                    return false;
                }
            }
            else
            {
                tokeniser_report_last_required_token(tokeniser);
                return false;
            }
        }
        else
        {
            decl_var variable            = {};
            parse_variable_status status = parse_type_and_var_decl(tokeniser, &variable, result->name);

            if (status == parse_variable_status::success)
                result->variables.push_back(variable);
            else if (status == parse_variable_status::member_function)
            {
                if (variable.name == STRING_LIT("names"))
                {
                    tokeniser_t name_tokeniser = *tokeniser;
                    name_tokeniser.ptr         = variable.name.str_ + variable.name.len;
                    name_tokeniser.file        = tokeniser->file;
                    if (!parse_rpc_alias_names(&name_tokeniser, result))
                    {
                        tokeniser_report_parse_struct_custom_error(tokeniser,
                                                                   parent_name,
                                                                   "Failed to parse RPC names in struct '%.*s'",
                                                                   result->name.len,
                                                                   result->name.str);
                        return false;
                    }
                }
            }
            else if (status == parse_variable_status::failed)
            {
                tokeniser_report_last_required_token(tokeniser);
                return false;
            }
        }
    }

    tokeniser_advance_until_scope_level(tokeniser, original_scope_level);
    if (!tokeniser_require_token_type(tokeniser, token_type::semicolon))
    {
        tokeniser_report_last_required_token(tokeniser);
        return false;
    }

    return true;
}

decl_struct const *lookup_type_definition(std::vector<decl_struct const *> *global_helper_structs,
                                          std::vector<decl_struct const *> *rpc_helper_structs,
                                          string_lit const var_type)
{
  string_lit const *check_type = &var_type;
  decl_struct const *result    = nullptr;
  for (int tries = 0; tries < 2; tries++)
  {
    for (decl_struct const *decl : *global_helper_structs)
    {
      if (string_lit_cmp(*check_type, decl->name))
      {
        result = decl;
        break;
      }
    }

    if (!result)
    {
      for (decl_struct const *decl : *rpc_helper_structs)
      {
        if (string_lit_cmp(*check_type, decl->name))
        {
          result = decl;
          break;
        }
      }
    }

    // TODO(doyle): Hacky workaround for handling cryptonote namespace, just
    // take the next adjacent namespace afterwards and hope its the only one so
    // variable -> type declaration can be resolved.
    if (!result)
    {
      string_lit const skip_namespace = STRING_LIT("cryptonote::");
      if (var_type.len >= skip_namespace.len && strncmp(skip_namespace.str, var_type.str, skip_namespace.len) == 0)
      {
        static string_lit var_type_without_cryptonote;
        var_type_without_cryptonote     = {};
        var_type_without_cryptonote.len = var_type.len - skip_namespace.len;
        var_type_without_cryptonote.str = var_type.str + skip_namespace.len;
        check_type                      = &var_type_without_cryptonote;
      }
    }
    else
    {
      break;
    }
  }

  return result;
}

void fprintf_indented(int indent_level, FILE *dest, char const *fmt...)
{
  for (int i = 0; i < indent_level * 2; ++i)
    fprintf(dest, " ");

  va_list va;
  va_start(va, fmt);
  vfprintf(dest, fmt, va);
  va_end(va);
}

struct decl_struct_hierarchy
{
    decl_struct const *children[16];
    int                children_index;
};

static void fprint_variable_example(decl_struct_hierarchy const *hierarchy, string_lit var_type, decl_var const *variable)
{
  string_lit var_name = variable->name;
  // -----------------------------------------------------------------------------------------------
  //
  // Specialised Type Handling
  //
  // -----------------------------------------------------------------------------------------------
#define PRINT_AND_RETURN(str) do { fprintf(stdout, str); return; } while(0)
  bool specialised_handling = false;
  if (hierarchy->children_index > 0)
  {
      decl_struct const *root = hierarchy->children[0];
      string_lit alias   = root->aliases[0];
      if (alias == STRING_LIT("lns_buy_mapping") ||
          alias == STRING_LIT("lns_update_mapping") ||
          alias == STRING_LIT("lns_names_to_owners") ||
          alias == STRING_LIT("lns_owners_to_names") ||
          alias == STRING_LIT("lns_make_update_mapping_signature"))
      {
          if (var_name == STRING_LIT("request_index"))  PRINT_AND_RETURN("0");
          if (var_name == STRING_LIT("type"))  PRINT_AND_RETURN("session");
          if (var_name == STRING_LIT("name"))  PRINT_AND_RETURN("\"My_Lns_Name\"");
          if (var_name == STRING_LIT("name_hash"))  PRINT_AND_RETURN("\"BzT9ln2zY7/DxSqNeNXeEpYx3fxu2B+guA0ClqtSb0E=\"");
          if (var_name == STRING_LIT("encrypted_value"))  PRINT_AND_RETURN("\"8fe253e6f15addfbce5c87583e970cb09294ec5b9fc7a1891c2ac34937e5a5c116c210ddf313f5fcccd8ee28cfeb0fa8e9\"");
          if (var_name == STRING_LIT("value")) PRINT_AND_RETURN("\"059f5a1ac2d04d0c09daa21b08699e8e2e0fd8d6fbe119207e5f241043cf77c30d\"");
          if (var_name == STRING_LIT("entries")) PRINT_AND_RETURN("\"059f5a1ac2d04d0c09daa21b08699e8e2e0fd8d6fbe119207e5f241043cf77c30d\"");
      }
  }

  // -----------------------------------------------------------------------------------------------
  //
  // Handle any type that didn't need special handling
  //
  // -----------------------------------------------------------------------------------------------
  if (var_type == STRING_LIT("std::string"))
  {
    if (var_name ==  STRING_LIT("operator_cut"))
    {
      auto string = STRING_LIT("\"1.1%\"");
      fprintf(stdout, "%.*s", string.len, string.str);
      return;
    }

    if (var_name == STRING_LIT("destination"))                   PRINT_AND_RETURN("\"L8ssYFtxi1HTFQdbmG9Lt71tyudgageDgBqBLcgLnw5XBiJ1NQLFYNAAfYpYS3jHaSe8UsFYjSgKadKhC7edTSQB15s6T7g\"");
    if (var_name == STRING_LIT("owner"))                         PRINT_AND_RETURN("\"L8ssYFtxi1HTFQdbmG9Lt71tyudgageDgBqBLcgLnw5XBiJ1NQLFYNAAfYpYS3jHaSe8UsFYjSgKadKhC7edTSQB15s6T7g\"");
    if (var_name == STRING_LIT("backup_owner"))                  PRINT_AND_RETURN("\"L8PYYYTh6yEewvuPmF75uhjDn9fBzKXp8CeMuwKNZBvZT8wAoe9hJ4favnZMvTTkNdT56DMNDcdWyheb3icfk4MS3udsP4R\"");
    if (var_name == STRING_LIT("wallet_address") ||
        var_name == STRING_LIT("miner_address") ||
        var_name == STRING_LIT("change_address") ||
        var_name == STRING_LIT("operator_address") ||
        var_name == STRING_LIT("base_address") ||
        var_name == STRING_LIT("address"))                       PRINT_AND_RETURN("\"L8KJf3nRQ53NTX1YLjtHryjegFRa3ZCEGLKmRxUfvkBWK19UteEacVpYqpYscSJ2q8WRuHPFdk7Q5W8pQB7Py5kvUs8vKSk\"");
    if (var_name == STRING_LIT("hash") ||
        var_name == STRING_LIT("top_block_hash") ||
        var_name == STRING_LIT("pow_hash") ||
        var_name == STRING_LIT("block_hash") ||
        var_name == STRING_LIT("block_hashes") ||
        var_name == STRING_LIT("main_chain_parent_block") ||
        var_name == STRING_LIT("id_hash") ||
        var_name == STRING_LIT("last_failed_id_hash") ||
        var_name == STRING_LIT("max_used_block_id_hash") ||
        var_name == STRING_LIT("miner_tx_hash") ||
        var_name == STRING_LIT("blks_hashes") ||
        var_name == STRING_LIT("prev_hash"))                     PRINT_AND_RETURN("\"bf430a3279f576ed8a814be25193e5a1ec61d3ee5729e64f47d8480ce5a2da70\"");
    if (var_name == STRING_LIT("key"))                           PRINT_AND_RETURN("\"bf430a3279f576ed8a814be25193e5a1ec61d3ee5729e64f47d8480ce5a2da70\"");
    if (var_name == STRING_LIT("mask"))                          PRINT_AND_RETURN("\"bf430a3279f576ed8a814be25193e5a1ec61d3ee5729e64f47d8480ce5a2da70\"");

      // TODO(doyle): Some examples need 32 byte payment ids
    if (var_name == STRING_LIT("payment_id"))                    PRINT_AND_RETURN("\"f378710e54eeeb8d\"");
    if (var_name == STRING_LIT("txids") ||
        var_name == STRING_LIT("tx_hashes") ||
        var_name == STRING_LIT("tx_hash_list") ||
        var_name == STRING_LIT("txs_hashes") ||
        var_name == STRING_LIT("missed_tx") ||
        var_name == STRING_LIT("tx_hash") ||
        var_name == STRING_LIT("prunable_hash") ||
        var_name == STRING_LIT("txid"))                          PRINT_AND_RETURN("\"b605cab7e3b9fe1f6d322e3167cd26e1e61c764afa9d733233ef716787786123\"");
    if (var_name == STRING_LIT("prev_txid"))                     PRINT_AND_RETURN("\"f26efb11e8eb6b446c5e0247e8883f41689591356f7abe65afe9fe75f567d40e\"");
    if (var_name == STRING_LIT("tx_key") ||
        var_name == STRING_LIT("tx_key_list"))                   PRINT_AND_RETURN("\"1982e99c69d8acc993cfc94ce59ff8f113d23482d9a25c892a3fc01c77dd8c4c\"");
    if (var_name == STRING_LIT("tx_blob") ||
        var_name == STRING_LIT("tx_blob_list"))                  PRINT_AND_RETURN("\"0402f78b05f78b05f78b0501ffd98b0502b888ddcf730229f056f5594cfcfd8d44f8033c9fda22450693d1694038e1cecaaaac25a8fc12af8992bc800102534df00c14ead3b3dedea9e7bdcf71c44803349b5e9aee2f73e22d5385ac147b7601008e5729d9329320444666d9d9d9dc602a3ae585de91ab2ca125665e3a363610021100000001839fdb0000000000000000000001200408d5ad7ab79d9b05c94033c2029f4902a98ec51f5175564f6978467dbb28723f929cf806d4ee1c781d7771183a93a1fd74f0827bddee9baac7e3083ab2b5840000\"");
    if (var_name == STRING_LIT("service_node_pubkey") ||
        var_name == STRING_LIT("quorum_nodes") ||
        var_name == STRING_LIT("validators") ||
        var_name == STRING_LIT("workers") ||
        var_name == STRING_LIT("service_node_key") ||
        var_name == STRING_LIT("nodes_to_test") ||
        var_name == STRING_LIT("service_node_pubkeys"))          PRINT_AND_RETURN("\"4a8c30cea9e729b06c91132295cce32d2a8e6e5bcf7b74a998e2ee1b3ed590b3\"");

    if (var_name == STRING_LIT("key_image") ||
        var_name == STRING_LIT("key_images"))                    PRINT_AND_RETURN("\"8d1bd8181bf7d857bdb281e0153d84cd55a3fcaa57c3e570f4a49f935850b5e3\"");
    if (var_name == STRING_LIT("bootstrap_daemon_address") ||
        var_name == STRING_LIT("remote_address"))                PRINT_AND_RETURN("\"127.0.0.1:22023\"");
    if (var_name == STRING_LIT("key_image_pub_key"))             PRINT_AND_RETURN("\"b1b696dd0a0d1815e341d9fed85708703c57b5d553a3615bcf4a06a36fa4bc38\"");
    if (var_name == STRING_LIT("connection_id"))                 PRINT_AND_RETURN("\"083c301a3030329a487adb12ad981d2c\"");
    if (var_name == STRING_LIT("nettype"))                       PRINT_AND_RETURN("\"MAINNET\"");
    if (var_name == STRING_LIT("status"))                        PRINT_AND_RETURN("\"OK\"");
    if (var_name == STRING_LIT("host"))                          PRINT_AND_RETURN("\"127.0.0.1\"");
    if (var_name == STRING_LIT("public_ip"))                     PRINT_AND_RETURN("\"8.8.8.8\"");
    if (var_name == STRING_LIT("extra"))                         PRINT_AND_RETURN("\"01008e5729d9329320444666d9d9d9dc602a3ae585de91ab2ca125665e3a363610021100000001839fdb0000000000000000000001200408d5ad7ab79d9b05c94033c2029f4902a98ec51f5175564f6978467dbb28723f929cf806d4ee1c781d7771183a93a1fd74f0827bddee9baac7e3083ab2b584\"");
    if (var_name == STRING_LIT("peer_id"))                       PRINT_AND_RETURN("\"c959fbfbed9e44fb\"");
    if (var_name == STRING_LIT("port"))                          PRINT_AND_RETURN("\"62950\"");
    if (var_name == STRING_LIT("registration_cmd"))              PRINT_AND_RETURN("\"register_service_node 18446744073709551612 L8KJf3nRQ53NTX1YLjtHryjegFRa3ZCEGLKmRxUfvkBWK19UteEacVpYqpYscSJ2q8WRuHPFdk7Q5W8pQB7Py5kvUs8vKSk 18446744073709551612 1555894565 f90424b23c7969bb2f0191bca45e6433a59b0b37039a5e38a2ba8cc7ea1075a3 ba24e4bfb4af0f5f9f74e35f1a5685dc9250ee83f62a9ee8964c9a689cceb40b4f125c83d0cbb434e56712d0300e5a23fd37a5b60cddbcd94e2d578209532a0d\"");
    if (var_name == STRING_LIT("tag"))                           PRINT_AND_RETURN("\"My tag\"");
    if (var_name == STRING_LIT("label"))                         PRINT_AND_RETURN("\"My label\"");
    if (var_name == STRING_LIT("description"))                   PRINT_AND_RETURN("\"My account description\"");
    if (var_name == STRING_LIT("transfer_type"))                 PRINT_AND_RETURN("\"all\"");
    if (var_name == STRING_LIT("recipient_name"))                PRINT_AND_RETURN("\"Thor\"");
    if (var_name == STRING_LIT("password"))                      PRINT_AND_RETURN("\"not_a_secure_password\"");
    if (var_name == STRING_LIT("msg"))                           PRINT_AND_RETURN("\"Message returned by the sender (wallet/daemon)\"");
    if (var_name == STRING_LIT("note") ||
        var_name == STRING_LIT("message") ||
        var_name == STRING_LIT("tx_description"))                PRINT_AND_RETURN("\"User assigned note describing something\"");
  }
  if (var_type == STRING_LIT("uint64_t"))
  {
    if (var_name == STRING_LIT("staking_requirement"))           PRINT_AND_RETURN("100000000000");
    if (var_name == STRING_LIT("height") ||
        var_name == STRING_LIT("registration_height") ||
        var_name == STRING_LIT("last_reward_block_height"))      PRINT_AND_RETURN("234767");
    if (var_name == STRING_LIT("amount"))                        PRINT_AND_RETURN("26734261552878");
    if (var_name == STRING_LIT("request_unlock_height"))         PRINT_AND_RETURN("251123");
    if (var_name == STRING_LIT("last_reward_transaction_index")) PRINT_AND_RETURN("0");
    if (var_name == STRING_LIT("portions_for_operator"))         PRINT_AND_RETURN("18446744073709551612");
    if (var_name == STRING_LIT("last_seen"))                     PRINT_AND_RETURN("1554685440");
    if (var_name == STRING_LIT("filename"))                      PRINT_AND_RETURN("wallet");
    if (var_name == STRING_LIT("password"))                      PRINT_AND_RETURN("password");
    if (var_name == STRING_LIT("language"))                      PRINT_AND_RETURN("English");
    if (var_name == STRING_LIT("ring_size"))                     PRINT_AND_RETURN("10");
    if (var_name == STRING_LIT("outputs"))                       PRINT_AND_RETURN("10");
    if (var_name == STRING_LIT("checkpointed"))                  PRINT_AND_RETURN("1");
    else                                                         PRINT_AND_RETURN("123");
  }
  if (var_type == STRING_LIT("uint32_t"))
  {
    if (var_name == STRING_LIT("threads_count"))                 PRINT_AND_RETURN("8");
    if (var_name == STRING_LIT("account_index"))                 PRINT_AND_RETURN("0");
    if (var_name == STRING_LIT("address_indices"))               PRINT_AND_RETURN("0");
    if (var_name == STRING_LIT("address_index"))                 PRINT_AND_RETURN("0");
    if (var_name == STRING_LIT("subaddr_indices"))               PRINT_AND_RETURN("0");
    if (var_name == STRING_LIT("priority"))                      PRINT_AND_RETURN("0");
    else                                                         PRINT_AND_RETURN("2130706433");
  }
  if (var_type == STRING_LIT("uint16_t"))
  {
    if (var_name == STRING_LIT("service_node_version"))          PRINT_AND_RETURN("4, 0, 3");
    else                                                         PRINT_AND_RETURN("12345");
  }
  if (var_type == STRING_LIT("uint8_t"))                         PRINT_AND_RETURN("11");
  if (var_type == STRING_LIT("int8_t"))                          PRINT_AND_RETURN("8");
  if (var_type == STRING_LIT("int"))
  {
    if (var_name == STRING_LIT("spent_status"))                  PRINT_AND_RETURN("0, 1");
    else                                                         PRINT_AND_RETURN("12345");
  }
  if (var_type == STRING_LIT("blobdata"))                        PRINT_AND_RETURN("\"sd2b5f838e8cc7774d92f5a6ce0d72cb9bd8db2ef28948087f8a50ff46d188dd9\"");
  if (var_type == STRING_LIT("bool"))
  {
    if (var_name == STRING_LIT("untrusted"))                     PRINT_AND_RETURN("false");
    else                                                         PRINT_AND_RETURN("true");
  }
  if (var_type == STRING_LIT("crypto::hash"))                    PRINT_AND_RETURN("\"bf430a3279f576ed8a814be25193e5a1ec61d3ee5729e64f47d8480ce5a2da70\"");
  if (var_type == STRING_LIT("difficulty_type"))                 PRINT_AND_RETURN("25179406071");

  PRINT_AND_RETURN("\"TODO(loki): Write example string\"");
#undef PRINT_AND_RETURN
}

void fprint_curl_json_rpc_param(std::vector<decl_struct const *> *global_helper_structs,
                                std::vector<decl_struct const *> *rpc_helper_structs,
                                decl_struct_hierarchy const *hierarchy,
                                decl_var const *variable,
                                int indent_level)
{
  decl_var_metadata const *metadata = &variable->metadata;
  fprintf_indented(indent_level, stdout, "\"%.*s\": ", variable->name.len, variable->name.str);

  string_lit var_type = variable->type;
  if (variable->is_array)
  {
    fprintf(stdout, "[");
    var_type = variable->template_expr;
  }

  if (decl_struct const *resolved_decl = lookup_type_definition(global_helper_structs, rpc_helper_structs, var_type))
  {
    fprintf(stdout, "{\n");
    indent_level++;
    decl_struct_hierarchy sub_hierarchy                    = *hierarchy;
    sub_hierarchy.children[sub_hierarchy.children_index++] = resolved_decl;
    for (size_t var_index = 0; var_index < resolved_decl->variables.size(); ++var_index)
    {
      decl_var const *inner_variable = &resolved_decl->variables[var_index];
      fprint_curl_json_rpc_param(global_helper_structs, rpc_helper_structs, &sub_hierarchy, inner_variable, indent_level);
      if (var_index < (resolved_decl->variables.size() - 1))
      {
        fprintf(stdout, ",");
        fprintf(stdout, "\n");
      }
    }

    fprintf(stdout, "\n");
    fprintf_indented(--indent_level, stdout, "}");
  }
  else
  {
    fprint_variable_example(hierarchy, var_type, variable);
  }

  if (variable->is_array)
    fprintf(stdout, "]");
}

void fprint_curl_example(std::vector<decl_struct const *> *global_helper_structs, std::vector<decl_struct const *> *rpc_helper_structs, decl_struct_hierarchy const *hierarchy, decl_struct const *request, decl_struct const *response, string_lit const rpc_endpoint)
{
  //
  // NOTE: Print the Curl Example
  //


  // fprintf(stdout, "curl -X POST http://127.0.0.1:22023/json_rpc -d '{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"%s\"}' -H 'Content-Type: application/json'")
  fprintf(stdout, "Example Request\n");
  int indent_level = 0;

  fprintf(stdout, "curl -X POST http://127.0.0.1:22023");

  bool is_json_rpc   = rpc_endpoint.str[0] != '/';
  bool has_arguments = is_json_rpc || request->variables.size() > 0;

  if (is_json_rpc) fprintf(stdout, "/json_rpc");
  else             fprintf(stdout, "%.*s", rpc_endpoint.len, rpc_endpoint.str);

  if (has_arguments)
    fprintf(stdout, " \\\n");
  else
    fprintf(stdout, " ");

  fprintf(stdout, "-H 'Content-Type: application/json'");
  if (has_arguments)
  {
    fprintf(stdout, " \\\n");
    fprintf(stdout, "-d @- << EOF\n");
  }
  else
    fprintf(stdout, "\n");

  if (has_arguments)
  {
    indent_level++;
    fprintf(stdout, "{\n");
  }

  if (is_json_rpc)
  {
    fprintf_indented(indent_level, stdout, "\"jsonrpc\":\"2.0\",\n");
    fprintf_indented(indent_level, stdout, "\"id\":\"0\",\n");
    fprintf_indented(indent_level, stdout, "\"method\":\"%.*s\"", rpc_endpoint.len, rpc_endpoint.str);
  }


  if (request->variables.size() > 0)
  {
    if (is_json_rpc)
    {
      fprintf(stdout, ",\n");
      fprintf_indented(indent_level++, stdout, "\"params\": {\n");
    }

    decl_struct_hierarchy sub_hierarchy                    = *hierarchy;
    sub_hierarchy.children[sub_hierarchy.children_index++] = request;
    for (size_t var_index = 0; var_index < request->variables.size(); ++var_index)
    {
      decl_var const *variable = &request->variables[var_index];
      fprint_curl_json_rpc_param(global_helper_structs, rpc_helper_structs, &sub_hierarchy, variable, indent_level);
      if (var_index < (request->variables.size() - 1))
        fprintf(stdout, ",\n");
    }

    if (is_json_rpc)
    {
      fprintf(stdout, "\n");
      fprintf_indented(--indent_level, stdout, "}");
    }
    --indent_level;
  }

  if (has_arguments)
  {
    fprintf(stdout, "\n}\n");
    indent_level--;
    fprintf(stdout, "EOF\n");
  }
  fprintf(stdout, "\n");

  fprintf(stdout, "Example Response\n");
  if (response->variables.size() > 0)
  {
    fprintf(stdout, "{\n");
    decl_struct_hierarchy sub_hierarchy                    = *hierarchy;
    sub_hierarchy.children[sub_hierarchy.children_index++] = response;
    for (size_t var_index = 0; var_index < response->variables.size(); ++var_index)
    {
      decl_var const *variable = &response->variables[var_index];
      fprint_curl_json_rpc_param(global_helper_structs, rpc_helper_structs, &sub_hierarchy, variable, 1 /*indent_level*/);
      if (var_index < (response->variables.size() - 1))
        fprintf(stdout, ",\n");
    }
    fprintf(stdout, "\n}\n\n");
  }
}

void fprint_variable(std::vector<decl_struct const *> *global_helper_structs, std::vector<decl_struct const *> *rpc_helper_structs, decl_var const *variable, int indent_level = 0)
{
    bool is_array              = variable->template_expr.len > 0;
    string_lit const *var_type = &variable->type;
    if (is_array) var_type     = &variable->template_expr;

    bool has_converted_type = (variable->metadata.converted_type != nullptr);
    if (has_converted_type)
      var_type = variable->metadata.converted_type;

    for (int i = 0; i < indent_level * 2; i++)
      fprintf(stdout, " ");

    fprintf(stdout, " * `%.*s - %.*s", variable->name.len, variable->name.str, var_type->len, var_type->str);
    if (is_array) fprintf(stdout, "[]");
    if (variable->value.len > 0) fprintf(stdout, " = %.*s", variable->value.len, variable->value.str);
    fprintf(stdout, "`");

    if (variable->comment.len > 0) fprintf(stdout, ": %.*s", variable->comment.len, variable->comment.str);
    fprintf(stdout, "\n");

    if (!has_converted_type)
    {
        decl_struct const *resolved_decl = lookup_type_definition(global_helper_structs, rpc_helper_structs, *var_type);
        if (resolved_decl)
        {
          ++indent_level;
          for (decl_var const &inner_variable : resolved_decl->variables)
            fprint_variable(global_helper_structs, rpc_helper_structs, &inner_variable, indent_level);
        }
    }
}

void collect_children_structs(std::vector<decl_struct const *> *rpc_helper_structs, std::vector<decl_struct> const *inner_structs)
{
  for (auto &inner_decl : *inner_structs)
  {
    collect_children_structs(rpc_helper_structs, &inner_decl.inner_structs);
    if (inner_decl.type == decl_struct_type::helper)
        rpc_helper_structs->push_back(&inner_decl);
  }
}

void fprint_string_and_escape_with_backslash(FILE *file, string_lit const *string, char char_to_escape)
{
  for (int i = 0; i < string->len; ++i)
  {
    char ch = string->str[i];
    if (ch == char_to_escape) fputc('\\', file);
    fputc(ch, file);
  }
}

decl_struct *find_decl_struct(decl_context *context, string_lit name)
{
    decl_struct *result = nullptr;
    for (auto &decl : context->declarations)
    {
        if (decl.name == name)
        {
            result = &decl;
            break;
        }
    }

    return result;
}

decl_struct *find_decl_struct_in_children(decl_struct *src, string_lit name)
{
    decl_struct *result = nullptr;
    for (auto &child : src->inner_structs)
    {
        if (child.name == name)
        {
            result = &child;
            break;
        }
    }

    return result;
}

decl_struct *find_decl_struct_inheritance_parent(decl_context *context, decl_struct const *src)
{
    // NOTE: We have something of the sort,
    //
    //                         +---------- Inheritance Parent Name
    //                         V
    //     struct Foo : public Namespace::FooParent
    //

    decl_struct *result = nullptr;
    if (src->inheritance_parent_name.len == 0)
        return result;

    tokeniser_t tokeniser = {};
    tokeniser.ptr         = src->inheritance_parent_name.str_;

    token_t namespace_token = {};
    if (!tokeniser_require_token_type(&tokeniser, token_type::identifier, &namespace_token))
    {
        tokeniser_report_last_required_token(&tokeniser);
        return result;
    }

    if (decl_struct *namespace_struct = find_decl_struct(context, token_to_string_lit(namespace_token)))
    {
        if (tokeniser_require_token_type(&tokeniser, token_type::namespace_colon))
        {
            token_t namespace_child = {};
            if (!tokeniser_require_token_type(&tokeniser, token_type::identifier, &namespace_child))
            {
                tokeniser_report_last_required_token(&tokeniser);
                return result;
            }

            if (decl_struct *child_struct = find_decl_struct_in_children(namespace_struct, token_to_string_lit(namespace_child)))
            {
                // NOTE: Hurray, found the parent struct this struct
                // inherits referencing, patch the pointer!
                result = child_struct;
            }
        }
        else
        {
            // NOTE: No namespace colon, so we just assume that the
            // parent was specified without a namespace, which
            // means, we've already found the parent that this
            // struct inherits from.
            result = namespace_struct;
        }
    }

    return result;
}


#if 0
bool fprint_struct_variables(decl_context *context,
                             std::vector<decl_struct const *> *visible_helper_structs,
                             decl_struct *src)
{
    // NOTE: Print parent
    if (response->inheritance_parent.len)
    {
        if (decl_struct *parent = find_decl_struct_inheritance_parent(context, response))
        {
            for (decl_var const &variable : parent->variables)
                fprint_variable(&global_helper_structs, &rpc_helper_structs, &variable);
        }
        else
        {
            fprintf(stderr,
                    "Failed to find the parent child_structaration that 'struct %.*s' inherits from, we were searching for '%.*s'", child_struct.name.len,
                    child_struct.name.str,
                    child_struct.inheritance_parent_name.len,
                    child_struct.inheritance_parent_name.str);
            return false;
        }
    }

    // NOTE: Print struct
    for (decl_var const &variable : response->variables)
        fprint_variable(&global_helper_structs, &rpc_helper_structs, &variable);

    return true;
}
#endif

void generate_markdown(decl_context *context)
{
    time_t now;
    time(&now);

    std::vector<decl_struct> const *declarations       = &context->declarations;
    std::vector<decl_struct> const *alias_declarations = &context->alias_declarations;
    struct tm *gmtime_result = gmtime(&now);
    fprintf(stdout,
           "# Introduction\n\n"
           "This is a list of the RPC calls, their inputs and outputs, and examples of each. This list is autogenerated and was last generated on: %d-%02d-%02d\n\n"
           "Many RPC calls use the JSON RPC interface while others use their own interfaces, as demonstrated below.\n\n"
           "Note: \"atomic units\" refer to the smallest fraction of 1 LOKI which is 1e9 atomic units.\n\n"
           "## RPC Methods\n\n",
           (1900 + gmtime_result->tm_year), (gmtime_result->tm_mon + 1), gmtime_result->tm_mday);

    for (decl_struct const &decl : (*declarations))
    {
        if (decl.type == decl_struct_type::rpc_command)
        {
            fputs(" - [", stdout);
#if 0
            fprint_string_and_escape_with_backslash(stdout, &decl.name, '_');
#else
            fprintf(stdout, "%.*s", decl.name.len, decl.name.str);
#endif

            fputs("](#", stdout);
            for (int i = 0; i < decl.name.len; ++i)
            {
              char ch = char_to_lower(decl.name.str[i]);
              fputc(ch, stdout);
            }
            fputs(")\n", stdout);
        }
    }
    fprintf(stdout, "\n\n");

    std::vector<decl_struct const *> global_helper_structs;
    std::vector<decl_struct const *> rpc_helper_structs;

    for (decl_struct const &global_decl : (*declarations))
    {
        if (global_decl.type == decl_struct_type::invalid)
        {
            assert(!"invalid?");
            continue;
        }

        if (global_decl.type == decl_struct_type::helper)
        {
            global_helper_structs.push_back(&global_decl);
            continue;
        }

        if (global_decl.type != decl_struct_type::rpc_command)
            continue;

        decl_struct const *request  = nullptr;
        decl_struct const *response = nullptr;
        rpc_helper_structs.clear();
        collect_children_structs(&rpc_helper_structs, &global_decl.inner_structs);
        for (auto &inner_decl : global_decl.inner_structs)
        {
            switch(inner_decl.type)
            {
                case decl_struct_type::request: request = &inner_decl; break;
                case decl_struct_type::response: response = &inner_decl; break;
            }
        }

        if (!response)
        {
            for (auto const &variable : global_decl.variables)
            {
                if (variable.aliases_to != &UNRESOLVED_ALIAS_STRUCT && variable.name == STRING_LIT("response"))
                    response = variable.aliases_to;
            }
        }

        if (!request)
        {
            for (auto const &variable : global_decl.variables)
            {
                if (variable.aliases_to != &UNRESOLVED_ALIAS_STRUCT && variable.name == STRING_LIT("request"))
                    request = variable.aliases_to;
            }
        }

        if (!(request && response))
        {
            fprintf(stderr, "Generating markdown error, the struct '%.*s' was marked as a RPC command but we could not find a RPC request/response for it.\n", global_decl.name.len, global_decl.name.str);
            raise(SIGTRAP);
            continue;
        }

        fputs("### ", stdout);
#if 0
        fprint_string_and_escape_with_backslash(stdout, &global_decl.name, '_');
#else
        fprintf(stdout, "%.*s", global_decl.name.len, global_decl.name.str);
#endif
        fputs("\n\n", stdout);

        if (global_decl.aliases.size() > 0 || global_decl.pre_decl_comments.size() || global_decl.description.len > 0)
        {
            fprintf(stdout, "```\n");
            if (global_decl.pre_decl_comments.size() > 0)
            {
              for (string_lit const &comment : global_decl.pre_decl_comments)
                fprintf(stdout, "%.*s\n", comment.len, comment.str);
            }
            if (global_decl.description.len > 0)
                fprintf(stdout, "%.*s\n", global_decl.description.len, global_decl.description.str);
            fprintf(stdout, "\n");

            if (global_decl.aliases.size() > 0)
            {
              fprintf(stdout, "Endpoints: ");

              string_lit const *main_alias = &global_decl.aliases[0];
              for(int i = 0; i < global_decl.aliases.size(); i++)
              {
                string_lit const &alias = global_decl.aliases[i];
                fprintf(stdout, "%.*s", alias.len, alias.str);
                if (i < (global_decl.aliases.size() - 1))
                  fprintf(stdout, ", ");

                // NOTE: Prefer aliases that don't start with a forward slash, i.e. these are the aliases for json RPC calls
                if (alias.str[0] != '/')
                  main_alias = &alias;
              }
              fprintf(stdout, "\n");

              // NOTE: Variables in the top-most/global declaration in the RPC
              // command structs are constants, responses and requests variables
              // are in child structs.
              if (global_decl.variables.size() > 0)
              {
                fprintf(stdout, "\nConstants: \n");
                for (int i = 0; i < global_decl.variables.size(); i++)
                {
                  decl_var const &constants = global_decl.variables[i];
                  fprint_variable(&global_helper_structs, &rpc_helper_structs, &constants);
                }
              }

              if (global_decl.pre_decl_comments.size() > 0 || global_decl.description.len > 0)
                fprintf(stdout, "\n");

              decl_struct_hierarchy hierarchy                = {};
              hierarchy.children[hierarchy.children_index++] = &global_decl;
              fprint_curl_example(&global_helper_structs, &rpc_helper_structs, &hierarchy, request, response, *main_alias);
            }

            fprintf(stdout, "```\n");
        }


        fprintf(stdout, "**Inputs:**\n");
        fprintf(stdout, "\n");
        if (request->inheritance_parent_name.len) // @TODO(doyle): Recursive inheritance
        {
            if (decl_struct *parent = find_decl_struct_inheritance_parent(context, request))
            {
                for (decl_var const &variable : parent->variables)
                    fprint_variable(&global_helper_structs, &rpc_helper_structs, &variable);
            }
            else
            {
                fprintf(stderr,
                        "Failed to find the parent child_structaration that 'struct %.*s' inherits from, we were searching for '%.*s'", request->name.len,
                        request->name.str,
                        request->inheritance_parent_name.len,
                        request->inheritance_parent_name.str);
                return;
            }
        }
        for (decl_var const &variable : request->variables)
            fprint_variable(&global_helper_structs, &rpc_helper_structs, &variable);
        fprintf(stdout, "\n");

        fprintf(stdout, "**Outputs:**\n");
        fprintf(stdout, "\n");
        if (response->inheritance_parent_name.len)
        {
            if (decl_struct *parent = find_decl_struct_inheritance_parent(context, response))
            {
                for (decl_var const &variable : parent->variables)
                    fprint_variable(&global_helper_structs, &rpc_helper_structs, &variable);
            }
            else
            {
                fprintf(stderr,
                        "Failed to find the parent child_structaration that 'struct %.*s' inherits from, we were searching for '%.*s'", response->name.len,
                        response->name.str,
                        response->inheritance_parent_name.len,
                        response->inheritance_parent_name.str);
                return;
            }
        }
        for (decl_var const &variable : response->variables)
            fprint_variable(&global_helper_structs, &rpc_helper_structs, &variable);
        fprintf(stdout, "\n\n");
    }

    fprintf(stdout, "\n");
}

int main(int argc, char *argv[])
{
    decl_context context = {};
    context.declarations.reserve(1024);
    context.alias_declarations.reserve(128);

    for (int arg_index = 1; arg_index < argc; arg_index++)
    {
        ptrdiff_t buf_size = 0;
        char *buf          = read_entire_file(argv[arg_index], &buf_size);

        tokeniser_t tokeniser = {};
        tokeniser.ptr         = buf;
        tokeniser.file        = argv[arg_index];

        while (tokeniser_next_marker(&tokeniser, STRING_LIT("LOKI_RPC_DOC_INTROSPECT")))
        {
            decl_struct decl = {};
            for (token_t comment = {}; tokeniser_require_token_type(&tokeniser, token_type::comment, &comment);)
            {
                string_lit comment_lit = token_to_string_lit(comment);
                decl.pre_decl_comments.push_back(comment_lit);
            }

            token_t peek = tokeniser_peek_token2(&tokeniser);
            if (peek.type == token_type::identifier)
            {
                string_lit peek_lit = token_to_string_lit(peek);
                if (peek_lit ==  STRING_LIT("struct") || peek_lit == STRING_LIT("class"))
                {
                    if (parse_struct(&tokeniser, &decl, true /*root_struct*/, nullptr /*parent name*/))
                        context.declarations.push_back(std::move(decl));
                }
                else
                {
                }
            }
            else
            {
            }
        }
    }

    // ---------------------------------------------------------------------------------------------
    //
    // Infer which structs are containers for RPC commands
    //
    // ---------------------------------------------------------------------------------------------
    // NOTE: Iterate all global structs and infer them as a rpc_command, if
    // they have a child struct with 'response' or 'request' OR they have
    // a C++ using alias to the same affect.
    for (auto &decl : context.declarations)
    {
        // @TODO(doyle): We should store the pointers to the response/request
        // here since we already do the work to enumerate and patch it.
        int response = 0, request = 0;
        for (auto &child_struct : decl.inner_structs)
        {
            if (child_struct.name == STRING_LIT("response"))
                response++;
            else if (child_struct.name == STRING_LIT("request"))
                request++;
        }

        for (auto const &variable : decl.variables)
        {
            if (variable.aliases_to)
            {
                if (variable.name == STRING_LIT("response"))
                    response++;
                else if (variable.name == STRING_LIT("request"))
                    request++;
            }
        }

        for (auto &variable : decl.variables)
        {
            if (variable.aliases_to == &UNRESOLVED_ALIAS_STRUCT)
            {
                // NOTE: We have something of the sort,
                //
                //           +---------- Tokeniser is here
                //           V
                //     using response = <variable.type>;
                //
                // We need to try and patch this variable to point to the right
                // declaration.

                tokeniser_t tokeniser = {};
                tokeniser.ptr         = variable.name.str_;

                token_t name = tokeniser_next_token(&tokeniser);
                if (!tokeniser_require_token_type(&tokeniser, token_type::equal))
                {
                    tokeniser_report_last_required_token(&tokeniser);
                    return false;
                }

                token_t namespace_token = {};
                if (!tokeniser_require_token_type(&tokeniser, token_type::identifier, &namespace_token))
                {
                    tokeniser_report_last_required_token(&tokeniser);
                    return false;
                }

                string_lit namespace_lit = token_to_string_lit(namespace_token);
                if (namespace_lit == STRING_LIT("std"))
                {
                    // NOTE: Handle the standard library specially by creating
                    // "fake" structs that contain 1 member with the standard
                    // lib container

                    context.alias_declarations.emplace_back();
                    decl_struct &alias_struct = context.alias_declarations.back();
                    variable.aliases_to       = &alias_struct; // Patch the alias pointer

                    alias_struct.type = decl_struct_type::helper;
                    alias_struct.name = token_to_string_lit(name);

                    decl_var variable_no_alias   = variable;
                    variable_no_alias.aliases_to = nullptr;
                    alias_struct.variables.push_back(variable_no_alias);
                }
                else
                {
                    // @TODO(doyle): This bit of code assumes that the
                    // declaration isn't namespaced too crazy and that the
                    // declaration's first token is something that we can find
                    // in our list of declarations.

                    // For example
                    //
                    // using response = GET_OUTPUT_DISTRIBUTION::response;
                    //
                    // and not
                    //
                    // using response = cryptonote::rpc::GET_OUTPUT_DISTRIBUTION::response;
                    //

                    for (auto &other_struct : context.declarations)
                    {
                        if (other_struct.name == namespace_lit)
                        {
                            if (!tokeniser_require_token_type(&tokeniser, token_type::namespace_colon))
                            {
                                tokeniser_report_last_required_token(&tokeniser);
                                return false;
                            }

                            token_t namespace_child = {};
                            if (!tokeniser_require_token_type(&tokeniser, token_type::identifier, &namespace_child))
                            {
                                tokeniser_report_last_required_token(&tokeniser);
                                return false;
                            }

                            for (auto &inner_struct : other_struct.inner_structs)
                            {
                                if (inner_struct.name == token_to_string_lit(namespace_child))
                                {
                                    // NOTE: Hurray, found the struct this
                                    // variable was referencing, patch the
                                    // pointer!
                                    variable.aliases_to = &inner_struct;
                                    break;
                                }
                            }

                            if (variable.aliases_to == &UNRESOLVED_ALIAS_STRUCT)
                            {
                                string_lit eol_string = string_lit_till_eol(variable.name.str);
                                fprintf(stderr, "Failed to resolve C++ using alias to a declaration '%.*s'", eol_string.len, eol_string.str);
                                return false;
                            }
                        }
                    }
                }
            }
        }

        if (request != 0 || response != 0)
        {
            assert(request == 1);
            assert(response == 1);
            decl.type = decl_struct_type::rpc_command;
        }
    }

    std::sort(context.declarations.begin(), context.declarations.end(), [](auto const &lhs, auto const &rhs) {
        return std::lexicographical_compare(
            lhs.name.str, lhs.name.str + lhs.name.len, rhs.name.str, rhs.name.str + rhs.name.len);
    });

    generate_markdown(&context);
    return 0;
}
