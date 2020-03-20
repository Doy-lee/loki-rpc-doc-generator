#define _CRT_SECURE_NO_WARNINGS

#include "loki_rpc_doc_generator.h"
#include <csignal>

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

char *str_multi_find(char *start, string_lit const *find, int find_len, int *find_index)
{
    for (char *ptr = start; ptr && ptr[0]; ptr++)
    {
        for (int i = 0; i < find_len; ++i)
        {
            string_lit const *check = find + i;
            if (strncmp(ptr, check->str, check->len) == 0)
            {
                if (find_index) *find_index = i;
                return ptr;
            }
        }
    }

    return nullptr;
}

char *str_find(char *start, string_lit find)
{
    for (char *ptr = start; ptr && ptr[0]; ptr++)
    {
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

#if 0
decl_enum fill_enum(std::vector<token_t> *tokens)
{
    struct decl_enum enumer;
    while (tokens->size() > 0)
    {
        if ((tokens->at(0)).type == LeftCurlyBrace) // DONE
        {
            tokens->erase(tokens->begin());
        }
        else if ((tokens->at(0)).type == RightCurlyBrace) // DONE
        {
            tokens->erase(tokens->begin());
            return enumer;
        }
        else if ((tokens->at(0)).type == EnumString)
        {
            enumer.name     = (tokens->at(0)).str;
            enumer.name_len = (tokens->at(0)).len;
            tokens->erase(tokens->begin());
        }
        else if ((tokens->at(0)).type == EnumVal)
        {
            enumer.vals.push_back((tokens->at(0)).str);
            tokens->erase(tokens->begin());
        }
        else if ((tokens->at(0)).type == EnumDef)
        {
            enumer.names.push_back((tokens->at(0)).str);
            tokens->erase(tokens->begin());
        }
    }
    return enumer;
}
#endif

token_t tokeniser_peek_token(tokeniser_t *tokeniser)
{
    token_t result = tokeniser->tokens[tokeniser->tokens_index];
    return result;
}

void tokeniser_rewind_token(tokeniser_t *tokeniser)
{
    tokeniser->tokens[--tokeniser->tokens_index];
}

token_t tokeniser_prev_token(tokeniser_t *tokeniser)
{
    token_t result = {};
    result.type = token_type::end_of_stream;
    if (tokeniser->tokens_index > 0)
        result = tokeniser->tokens[tokeniser->tokens_index - 1];
    return result;
}

token_t tokeniser_next_token(tokeniser_t *tokeniser, int amount = 1)
{
    token_t result = tokeniser->tokens[tokeniser->tokens_index];
    if (result.type != token_type::end_of_stream)
    {
        for (int i = 0; i < amount; i++)
        {
            result = tokeniser->tokens[tokeniser->tokens_index++];
            if (result.type      == token_type::left_curly_brace)  tokeniser->indent_level++;
            else if (result.type == token_type::right_curly_brace) tokeniser->indent_level--;
            assert(tokeniser->indent_level >= 0);

            if (result.type == token_type::end_of_stream)
                break;
        }
    }
    return result;
}

token_t tokeniser_advance_until_token(tokeniser_t *tokeniser, token_type type)
{
    token_t result = {};
    for (result = tokeniser_next_token(tokeniser);
         result.type != token_type::end_of_stream && result.type != type;
         result = tokeniser_next_token(tokeniser)
        )
    {
    }
    return result;
}

bool is_identifier_token(token_t token, string_lit expect_str)
{
    bool result = (token.type == token_type::identifier) && (string_lit_cmp(token_to_string_lit(token), expect_str));
    return result;
}

bool tokeniser_accept_token_if_identifier(tokeniser_t *tokeniser, string_lit expect, token_t *token = nullptr)
{
    token_t check = tokeniser_peek_token(tokeniser);
    if (is_identifier_token(check, expect))
    {
        if (token) *token = check;
        tokeniser_next_token(tokeniser);
        return true;
    }

    return false;
}

bool tokeniser_accept_token_if_type(tokeniser_t *tokeniser, token_type type, token_t *token = nullptr)
{
    token_t check = tokeniser_peek_token(tokeniser);
    if (check.type == type)
    {
        if (token) *token = check;
        tokeniser_next_token(tokeniser);
        return true;
    }

    return false;
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

bool parse_type_and_var_decl(tokeniser_t *tokeniser, decl_var *variable)
{
    token_t token = tokeniser_peek_token(tokeniser);

    bool member_function  = false;
    char *var_value_start = nullptr;
    char *type_decl_start = token.str;
    token_t var_decl = {};
    for (token_t sub_token = tokeniser_peek_token(tokeniser);
         sub_token.type != token_type::end_of_stream;
         sub_token = tokeniser_peek_token(tokeniser))
    {
        // Member function decl
        if (sub_token.type == token_type::open_paren || (token_to_string_lit(sub_token) == STRING_LIT("operator")))
        {
          sub_token       = tokeniser_next_token(tokeniser);
          member_function = true;

          for (sub_token = tokeniser_next_token(tokeniser);
               sub_token.type != token_type::end_of_stream;
               sub_token = tokeniser_next_token(tokeniser))
          {
            if (sub_token.type == token_type::semicolon)
              break;

            // Inline function decl
            if (sub_token.type == token_type::left_curly_brace)
            {
              int orig_scope_level = tokeniser->indent_level - 1;
              assert(orig_scope_level >= 0);

              while (tokeniser->indent_level != orig_scope_level)
                sub_token = tokeniser_next_token(tokeniser);
              break;
            }
          }
        }

        if (member_function)
          break;

        if (sub_token.type == token_type::equal)
        {
            var_decl = tokeniser_prev_token(tokeniser);
            tokeniser_next_token(tokeniser);
            var_value_start = tokeniser_peek_token(tokeniser).str;
        }
        else if (sub_token.type == token_type::semicolon)
        {
            // NOTE: Maybe already resolved if there was an equals earlier (assignment)
            if (var_decl.len == 0) var_decl = tokeniser_prev_token(tokeniser);
            break;
        }
        sub_token = tokeniser_next_token(tokeniser);
    }

    if (member_function)
      return false;

    char const *type_decl_end = var_decl.str - 1;
    variable->type.str  = type_decl_start;
    variable->type.len  = static_cast<int>(type_decl_end - type_decl_start);
    variable->name      = token_to_string_lit(var_decl);
    variable->type      = trim_whitespace_around(variable->type);
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

    // TODO(doyle): This is horribly incomplete, but we only specify arrays on
    // terms of dynamic structures in loki. So meh.
    variable->is_array = variable->template_expr.len > 0;
    variable->metadata = derive_metadata_from_variable(variable);

    token = tokeniser_next_token(tokeniser);
    if (token.type != token_type::semicolon)
    {
       token = tokeniser_advance_until_token(tokeniser, token_type::semicolon);
    }

    if (var_value_start)
    {
        variable->value.str = var_value_start;
        variable->value.len = token.str - var_value_start;
    }

    token = tokeniser_peek_token(tokeniser);
    if (token.type == token_type::comment)
    {
        variable->comment = token_to_string_lit(token);
        token = tokeniser_next_token(tokeniser);
    }

    string_lit const skip_marker = STRING_LIT("@NoLokiRPCDocGen");
    if (variable->comment.len >= skip_marker.len && strncmp(variable->comment.str, skip_marker.str, skip_marker.len) == 0)
        return false;

    return true;
}


static bool ignore_var_modifier(token_t token)
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

static string_lit const COMMAND_RPC_PREFIX = STRING_LIT("COMMAND_RPC_");
bool fill_struct(tokeniser_t *tokeniser, decl_struct *result)
{
    // ---------------------------------------------------------------------------------------------
    //
    // Classify if struct
    //
    // ---------------------------------------------------------------------------------------------
    if (!tokeniser_accept_token_if_identifier(tokeniser, STRING_LIT("struct")))
        return false;

    token_t token = {};
    if (!tokeniser_accept_token_if_type(tokeniser, token_type::identifier, &token))
        return false;

    int original_indent_level = tokeniser->indent_level;
    result->name               = token_to_string_lit(token);
    if (result->name.len > COMMAND_RPC_PREFIX.len &&
        strncmp(result->name.str, COMMAND_RPC_PREFIX.str, COMMAND_RPC_PREFIX.len) == 0)
    {
        result->type = decl_struct_type::rpc_command;
    }
    else if (result->name == STRING_LIT("request_t") || result->name == STRING_LIT("request"))
    {
        result->type = decl_struct_type::request;
    }
    else if (result->name == STRING_LIT("response_t") || result->name == STRING_LIT("response"))
    {
        result->type = decl_struct_type::response;
    }
    else
    {
        result->type = decl_struct_type::helper;
    }

    if (!tokeniser_accept_token_if_type(tokeniser, token_type::left_curly_brace, &token))
        return false;

    // ---------------------------------------------------------------------------------------------
    //
    // Parse Struct Contents
    //
    // ---------------------------------------------------------------------------------------------
    for (token       = tokeniser_peek_token(tokeniser);
         token.type != token_type::end_of_stream && tokeniser->indent_level != original_indent_level;
         token       = tokeniser_peek_token(tokeniser))
    {
        bool handled = true;
        if (token.type == token_type::identifier)
        {
            string_lit token_lit = token_to_string_lit(token);
            if (string_lit_cmp(token_lit, STRING_LIT("struct")))
            {
                decl_struct decl = {};
                if (fill_struct(tokeniser, &decl)) result->inner_structs.push_back(std::move(decl));
                continue;
            }
            else if (string_lit_cmp(token_lit, STRING_LIT("typedef")))
            {
              token = tokeniser_next_token(tokeniser);
              decl_struct decl = {};
              decl_var variable = {};
              if (parse_type_and_var_decl(tokeniser, &variable))
              {
                decl.name = variable.name;

                if (string_lit_cmp(variable.name, STRING_LIT("response")) ||
                    string_lit_cmp(variable.name, STRING_LIT("request")))
                {
                  decl.type = decl_struct_type::helper;
                }
                else
                {
                  continue;
                }

                decl.variables.push_back(variable);
                result->inner_structs.push_back(decl);
              }
            }
            else if (string_lit_cmp(token_lit, STRING_LIT("enum")))
            {
                handled = true;
                tokeniser_advance_until_token(tokeniser, token_type::semicolon);
            }
            else
            {
#if 0
              if (result->name == STRING_LIT("COMMAND_RPC_LNS_NAMES_TO_OWNERS"))
                  raise(SIGTRAP);
#endif

              tokeniser_t tokeniser_copy = *tokeniser; // a way to reverse state if the following stream is not what we expect
              bool matched = false;
              while (ignore_var_modifier(token)) token = tokeniser_next_token(tokeniser);

              if (string_lit_cmp(token_to_string_lit(token), STRING_LIT("char")))
              {
                  token = tokeniser_next_token(tokeniser);
                  while (ignore_var_modifier(token)) token = tokeniser_next_token(tokeniser);
                  if (string_lit_cmp(token_to_string_lit(token), STRING_LIT("*")))
                  {
                      token = tokeniser_next_token(tokeniser);
                      while (ignore_var_modifier(token)) token = tokeniser_next_token(tokeniser);
                      if (string_lit_cmp(token_to_string_lit(token), STRING_LIT("description")))
                      {
                          token = tokeniser_next_token(tokeniser, 1);
                          if (string_lit_cmp(token_to_string_lit(token), STRING_LIT("=")))
                          {
                              token = tokeniser_next_token(tokeniser, 1);
                              if (string_lit_cmp(token_to_string_lit(token), STRING_LIT("R")))
                              {
                                  token = tokeniser_next_token(tokeniser, 1);
                                  if (token.type == token_type::string)
                                  {
                                      // NOTE: Raw c-string literal, we have enclosing brackets.
                                      token.len -= 2;
                                      token.str += 1;
                                      result->description = token_to_string_lit(token);
                                      matched             = true;
                                  }
                              }
                          }
                      }
                  }
              }

              if (!matched)
              {
                *tokeniser = tokeniser_copy;
                decl_var variable = {};
                if (parse_type_and_var_decl(tokeniser, &variable))
                  result->variables.push_back(variable);
                else
                  handled = false;
               }
            }
        }
        else
        {
            handled = false;
        }

        if (!handled) token = tokeniser_next_token(tokeniser);
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

void generate_markdown(std::vector<decl_struct> const *declarations)
{
    time_t now;
    time(&now);

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
            continue;
        }

        if (global_decl.type == decl_struct_type::helper)
        {
            global_helper_structs.push_back(&global_decl);
            continue;
        }

        if (global_decl.type != decl_struct_type::rpc_command)
        {
            // TODO(doyle): Warning, unexpected non-rpc command in global scope
            continue;
        }

        decl_struct const *request  = nullptr;
        decl_struct const *response = nullptr;
        rpc_helper_structs.clear();
        collect_children_structs(&rpc_helper_structs, &global_decl.inner_structs);
        for (auto &inner_decl : global_decl.inner_structs)
        {
          // NOTE: Sometimes we have a typedef from request_t to request, so we can have multiple request structs same for responses.
          // We prefer response_t if we can find one. 
          switch(inner_decl.type)
          {
              case decl_struct_type::request:
              {
                  if (!request) request = &inner_decl;
                  else if (request && inner_decl.name == STRING_LIT("request_t"))
                      request = &inner_decl;
              }
              break;

              case decl_struct_type::response:
              {
                  if (!response) response = &inner_decl;
                  else if (response && inner_decl.name == STRING_LIT("response_t"))
                      response = &inner_decl;
              }
              break;
          }
        }

        if (!(request && response))
        {
            // TODO(doyle): Warning
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
        for (decl_var const &variable : request->variables)
            fprint_variable(&global_helper_structs, &rpc_helper_structs, &variable);
        fprintf(stdout, "\n");

        fprintf(stdout, "**Outputs:**\n");
        fprintf(stdout, "\n");
        for (decl_var const &variable : response->variables)
            fprint_variable(&global_helper_structs, &rpc_helper_structs, &variable);
        fprintf(stdout, "\n\n");
    }

    fprintf(stdout, "\n");
}

char *next_token(char *src)
{
    char *result = src;
    while (result && isspace(result[0])) result++;
    return result;
}

int main(int argc, char *argv[])
{
    std::vector<decl_struct> declarations;
    declarations.reserve(128);

    struct name_to_alias
    {
      string_lit name;
      string_lit alias;
    };
    std::vector<name_to_alias> struct_rpc_aliases;
    struct_rpc_aliases.reserve(128);

    std::vector<token_t> token_list;
    token_list.reserve(16384);
    for (int arg_index = 1; arg_index < argc; arg_index++, token_list.clear())
    {
        ptrdiff_t buf_size = 0;
        char *buf          = read_entire_file(argv[arg_index], &buf_size);
        //
        // Lex File into token_list
        //
        {
            token_type const LEX_MARKERS_TYPE[] =
            {
              token_type::introspect_marker,
              token_type::uri_json_rpc_marker,
              token_type::uri_json_rpc_marker,
              token_type::uri_binary_rpc_marker,
              token_type::json_rpc_marker,
              token_type::json_rpc_marker,
              token_type::json_rpc_marker,
            };

            string_lit const LEX_MARKERS[] =
            {
              STRING_LIT("LOKI_RPC_DOC_INTROSPECT"),
              STRING_LIT("MAP_URI_AUTO_JON2_IF"), // TODO(doyle): Length dependant, longer lines must come first because the checking is naiive and doesn't check that the strings are delimited
              STRING_LIT("MAP_URI_AUTO_JON2"),
              STRING_LIT("MAP_URI_AUTO_BIN2"),
              STRING_LIT("MAP_JON_RPC_WE_IF"),
              STRING_LIT("MAP_JON_RPC_WE"),
              STRING_LIT("MAP_JON_RPC"),
            };
            static_assert(ARRAY_COUNT(LEX_MARKERS) == ARRAY_COUNT(LEX_MARKERS_TYPE), "Mismatched enum to string mapping");

            char *ptr            = buf;
            int lex_marker_index = 0;
            for (ptr = str_multi_find(ptr, LEX_MARKERS, ARRAY_COUNT(LEX_MARKERS), &lex_marker_index);
                 ptr;
                 ptr = str_multi_find(ptr, LEX_MARKERS, ARRAY_COUNT(LEX_MARKERS), &lex_marker_index))
            {

              string_lit const *found_marker = LEX_MARKERS + lex_marker_index;
              token_t lexing_marker_token    = {};
              lexing_marker_token.str        = ptr;
              lexing_marker_token.len        = found_marker->len;
              lexing_marker_token.type       = LEX_MARKERS_TYPE[lex_marker_index];
              token_list.push_back(lexing_marker_token);
              ptr += lexing_marker_token.len;

              bool started_parsing_scope = false;
              int scope_level = 0;
              for (;ptr && *ptr;)
              {
                  if (started_parsing_scope && scope_level == 0)
                  {
                    break;
                  }

                  token_t curr_token = {};
                  curr_token.str   = next_token(ptr);
                  curr_token.len   = 1;
                  ptr              = curr_token.str + 1;
                  switch(curr_token.str[0])
                  {
                      case '{': curr_token.type = token_type::left_curly_brace; break;
                      case '}': curr_token.type = token_type::right_curly_brace; break;
                      case ';': curr_token.type = token_type::semicolon; break;
                      case '=': curr_token.type = token_type::equal; break;
                      case '<': curr_token.type = token_type::less_than; break;
                      case '>': curr_token.type = token_type::greater_than; break;
                      case '(': curr_token.type = token_type::open_paren; break;
                      case ')': curr_token.type = token_type::close_paren; break;
                      case ',': curr_token.type = token_type::comma; break;
                      case '*': curr_token.type = token_type::asterisks; break;

                      case '"':
                      {
                        curr_token.type = token_type::string;
                        curr_token.str  = ptr++;
                        while(ptr[0] != '"') ptr++;
                        curr_token.len  = static_cast<int>(ptr - curr_token.str);
                        ptr++;
                      }
                      break;

                      case ':':
                      {
                          curr_token.type = token_type::colon;
                          if (ptr[0] == ':')
                          {
                              ptr++;
                              curr_token.type = token_type::namespace_colon;
                              curr_token.len  = 2;
                          }
                      }
                      break;

                      case '/':
                      {
                          curr_token.type = token_type::fwd_slash;
                          if (ptr[0] == '/' || ptr[0] == '*')
                          {
                              curr_token.type = token_type::comment;
                              if (ptr[0] == '/')
                              {
                                  ptr++;
                                  if (ptr[0] == ' ' && ptr[1] && ptr[1] == ' ')
                                  {
                                    curr_token.str = ptr++;
                                  }
                                  else
                                  {
                                    while (ptr[0] == ' ' || ptr[0] == '\t') ptr++;
                                    curr_token.str = ptr;
                                  }

                                  while (ptr[0] != '\n' && ptr[0] != '\r') ptr++;
                                  curr_token.len = static_cast<int>(ptr - curr_token.str);
                              }
                              else
                              {
                                  curr_token.str = ++ptr;
                                  for (;;)
                                  {
                                      while (ptr[0] != '*') ptr++;
                                      ptr++;
                                      if (ptr[0] == '/')
                                      {
                                          curr_token.len = static_cast<int>(ptr - curr_token.str - 2);
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
                          curr_token.type = token_type::identifier;
                          if (char_is_alpha(ptr[0]) || ptr[0] == '_' || ptr[0] == '!')
                          {
                              ptr++;
                              while (char_is_alphanum(ptr[0]) || ptr[0] == '_') ptr++;
                          }
                          curr_token.len = static_cast<int>(ptr - curr_token.str);
                      }
                      break;
                  }

                  if (lexing_marker_token.type == token_type::introspect_marker)
                  {
                      if (curr_token.type == token_type::left_curly_brace)
                      {
                        started_parsing_scope = true;
                        scope_level++;
                      }
                      else if (curr_token.type == token_type::right_curly_brace)
                      {
                        scope_level--;
                      }
                  }
                  else
                  {
                      if (curr_token.type == token_type::open_paren)
                      {
                        started_parsing_scope = true;
                        scope_level++;
                      }
                      else if (curr_token.type == token_type::close_paren)
                      {
                        scope_level--;
                      }
                  }

                  string_lit token_lit = token_to_string_lit(curr_token);
                  if (curr_token.type == token_type::identifier)
                  {
                      if (string_lit_cmp(token_lit, STRING_LIT("BEGIN_KV_SERIALIZE_MAP")))
                      {
                          string_lit const END_STR = STRING_LIT("END_KV_SERIALIZE_MAP()");
                          ptr                      = str_find(ptr, END_STR);
                          ptr                     += END_STR.len;
                          continue;
                      }
                  }

                  assert(curr_token.type != token_type::invalid);
                  if (curr_token.len > 0)
                      token_list.push_back(curr_token);

              }
            }

            token_t sentinel_token = {};
            sentinel_token.type  = token_type::end_of_stream;
            token_list.push_back(sentinel_token);
        }

        //
        // Parse lexed tokens into declarations
        //
        {
            decl_struct decl      = {};
            tokeniser_t tokeniser = {};
            tokeniser.tokens      = token_list.data();
            for (token_t token = tokeniser_peek_token(&tokeniser);
                 token.type   != token_type::end_of_stream;
                 token         = tokeniser_peek_token(&tokeniser), decl.pre_decl_comments.clear())
            {
                if (token.type == token_type::end_of_stream)
                    break;

                bool handled = false;
                if (token.type == token_type::introspect_marker)
                {
                  token = tokeniser_next_token(&tokeniser); // accept marker
                  for (token = tokeniser_peek_token(&tokeniser); token.type == token_type::comment; token = tokeniser_peek_token(&tokeniser))
                  {
                    string_lit comment_lit = token_to_string_lit(token);
                    decl.pre_decl_comments.push_back(comment_lit);
                    token = tokeniser_next_token(&tokeniser);
                  }

                  if (token.type == token_type::identifier)
                  {
                      string_lit token_lit = token_to_string_lit(token);
                      if (token_lit == STRING_LIT("struct"))
                      {
                          if (fill_struct(&tokeniser, &decl))
                          {
                              declarations.push_back(std::move(decl));
                              handled = true;
                          }
                      }
                      else if (token_lit == STRING_LIT("enum"))
                      {
                          // decl_struct doc = fill_struct(&tokeniser);
                      }
                  }
                }
                else if (token.type == token_type::uri_json_rpc_marker || token.type == token_type::uri_binary_rpc_marker || token.type == token_type::json_rpc_marker)
                {
                    token = tokeniser_next_token(&tokeniser); // accept marker
                    if (tokeniser_accept_token_if_type(&tokeniser, token_type::open_paren) &&
                        tokeniser_accept_token_if_type(&tokeniser, token_type::string, &token))
                    {
                        string_lit rpc_alias = token_to_string_lit(token);
                        if (tokeniser_accept_token_if_type(&tokeniser, token_type::comma) &&
                            tokeniser_accept_token_if_type(&tokeniser, token_type::identifier) &&
                            tokeniser_accept_token_if_type(&tokeniser, token_type::comma) &&
                            tokeniser_accept_token_if_type(&tokeniser, token_type::identifier, &token))
                        {
                            string_lit struct_name   = token_to_string_lit(token);

                            // TODO(doyle): Hmm hacky.
                            if (struct_name == STRING_LIT("wallet_rpc"))
                            {
                              token       = tokeniser_next_token(&tokeniser, 2);
                              struct_name = token_to_string_lit(token);
                            }

                            name_to_alias name_alias = {};
                            name_alias.name          = struct_name;
                            name_alias.alias         = rpc_alias;

                            struct_rpc_aliases.push_back(name_alias);
                            handled = true;
                        }
                    }
                }

                if (!handled)
                    token = tokeniser_next_token(&tokeniser);
            }
        }
    }

    //
    // Resolve aliases to struct declarations
    //
    for (name_to_alias &name_alias : struct_rpc_aliases)
    {
        for(decl_struct &decl : declarations)
        {
            if (string_lit_cmp(decl.name, name_alias.name))
            {
                decl.aliases.push_back(name_alias.alias);
                break;
            }
        }
    }
    generate_markdown(&declarations);

    return 0;
}
