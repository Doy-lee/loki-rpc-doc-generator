#define _CRT_SECURE_NO_WARNINGS

#include "loki_rpc_doc_generator.h"

type_conversion const TYPE_CONVERSION_TABLE[] =
{
    {STRING_LIT("std::string"), STRING_LIT("string")},
    {STRING_LIT("uint64_t"),    STRING_LIT("u64")},
    {STRING_LIT("uint32_t"),    STRING_LIT("u32")},
    {STRING_LIT("uint16_t"),    STRING_LIT("u8")},
    {STRING_LIT("uint8_t"),     STRING_LIT("u8")},
    {STRING_LIT("int64_t"),     STRING_LIT("i64")},
    {STRING_LIT("blobdata"),    STRING_LIT("string")},
};

bool string_lit_cmp(string_lit a, string_lit b)
{
    bool result = (a.len == b.len && (strncmp(a.str, b.str, a.len) == 0));
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

char *str_find(char *start, string_lit find)
{
    for (char *ptr = start; ptr; ptr++)
    {
        if (strncmp(ptr, find.str, find.len) == 0)
            return ptr;
    }

    return nullptr;
}

bool char_is_alpha     (char ch) { return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'); }
bool char_is_digit     (char ch) { return (ch >= '0' && ch <= '9'); }
bool char_is_alphanum  (char ch) { return char_is_alpha(ch) || char_is_digit(ch); }

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

static string_lit const COMMAND_RPC_PREFIX = STRING_LIT("COMMAND_RPC_");
decl_struct fill_struct(tokeniser_t *tokeniser)
{
    decl_struct result = {};
    if (!tokeniser_accept_token_if_identifier(tokeniser, STRING_LIT("struct")))
        return result;

    token_t token = {};
    if (!tokeniser_accept_token_if_type(tokeniser, token_type::identifier, &token))
        return result;

    int original_indent_level = tokeniser->indent_level;
    result.name               = token_to_string_lit(token);
    if (result.name.len > COMMAND_RPC_PREFIX.len &&
        strncmp(result.name.str, COMMAND_RPC_PREFIX.str, COMMAND_RPC_PREFIX.len) == 0)
    {
        result.type = decl_struct_type::rpc_command;
    }
    else if (string_lit_cmp(result.name, STRING_LIT("request")))
    {
        result.type = decl_struct_type::request;
    }
    else if (string_lit_cmp(result.name, STRING_LIT("response")))
    {
        result.type = decl_struct_type::response;
    }
    else
    {
        result.type = decl_struct_type::helper;
    }

    if (!tokeniser_accept_token_if_type(tokeniser, token_type::left_curly_brace, &token))
        return result;

    for (token       = tokeniser_peek_token(tokeniser);
         token.type != token_type::end_of_stream && tokeniser->indent_level != original_indent_level;
         token       = tokeniser_peek_token(tokeniser))
    {
        if (token.type == token_type::identifier)
        {
            string_lit token_lit = token_to_string_lit(token);
            if (string_lit_cmp(token_lit, STRING_LIT("struct")))
            {
                decl_struct decl = fill_struct(tokeniser);
                result.inner_structs.push_back(decl);
                continue;
            }
            else
            {
                char *type_decl_start = token.str;

                token_t var_decl = {};
                for (token_t sub_token = tokeniser_peek_token(tokeniser);; sub_token = tokeniser_peek_token(tokeniser))
                {
                    if (sub_token.type == token_type::semicolon)
                    {
                        var_decl = tokeniser_prev_token(tokeniser);
                        break;
                    }
                    sub_token = tokeniser_next_token(tokeniser);
                }

                char *type_decl_end = var_decl.str - 1;
                decl_var variable   = {};
                variable.type.str   = type_decl_start;
                variable.type.len   = static_cast<int>(type_decl_end - type_decl_start);
                variable.name       = token_to_string_lit(var_decl);
                variable.type       = trim_whitespace_around(variable.type);

                for (int i = 0; i < variable.type.len; ++i)
                {
                    if (variable.type.str[i] == '<')
                    {
                        variable.template_expr.str = variable.type.str + (++i);
                        for (int j = ++i; j < variable.type.len; ++j)
                        {
                            if (variable.type.str[j] == '>')
                            {
                                char const *template_expr_end = variable.type.str + j;
                                variable.template_expr.len    = static_cast<int>(template_expr_end - variable.template_expr.str);
                                break;
                            }
                        }
                        break;
                    }
                }

                token = tokeniser_next_token(tokeniser);
                if (token.type != token_type::semicolon)
                    continue;

                token = tokeniser_peek_token(tokeniser);
                if (token.type == token_type::comment)
                {
                    variable.comment = token_to_string_lit(token);
                    token = tokeniser_next_token(tokeniser);
                }

                result.variables.push_back(variable);
            }
        }
        else
        {
            token = tokeniser_next_token(tokeniser);
        }
    }

    return result;
}

void fprint_variable(decl_var const &variable)
{
    bool is_array              = variable.template_expr.len > 0;
    string_lit const *var_type = &variable.type;
    if (is_array) var_type     = &variable.template_expr;

    for (int i = 0; i < ARRAY_COUNT(TYPE_CONVERSION_TABLE); ++i)
    {
        type_conversion const *conversion = TYPE_CONVERSION_TABLE + i;
        if (conversion->from.len < var_type->len) continue;

        if (strncmp(conversion->from.str, var_type->str, MIN_VAL(conversion->from.len, var_type->len)) == 0)
        {
            var_type = &conversion->to;
            break;
        }
    }
    fprintf(stdout, " * **%.*s**", variable.name.len, variable.name.str);
    fprintf(stdout, " - %.*s", var_type->len, var_type->str);
    if (is_array) fprintf(stdout, "[]");
    if (variable.comment.len > 0) fprintf(stdout, " ; %.*s", variable.comment.len, variable.comment.str);
    fprintf(stdout, "\n");
}

void generate_html_doc(std::vector<decl_struct> const *declarations)
{
    fprintf(stdout,
           "# Introduction\n\n"
           "This is a list of the lokid daemon RPC calls, their inputs and outputs, and examples of each.\n\n"
           "Many RPC calls use the daemon's JSON RPC interface while others use their own interfaces, as demonstrated below.\n\n"
           "Note: \"atomic units\" refer to the smallest fraction of 1 LOKI according to the lokid implementation. 1 LOKI = 1e12 atomic units.\n\n"
           "## RPC Methods\n\n"
           );

    for (decl_struct const &decl : (*declarations))
    {
        if (decl.type == decl_struct_type::rpc_command)
            fprintf(stdout, " - [%.*s](#%.*s) \n", decl.name.len, decl.name.str, decl.name.len, decl.name.str);
    }
    fprintf(stdout, "\n\n");

    std::vector<decl_struct const *> global_helper_structs;
    std::vector<decl_struct const *> rpc_decl_helper_structs;

    for (decl_struct const &global_decl : (*declarations))
    {
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
        rpc_decl_helper_structs.clear();
        for (auto &inner_decl : global_decl.inner_structs)
        {
            switch(inner_decl.type)
            {
                case decl_struct_type::request: request   = &inner_decl; break;
                case decl_struct_type::response: response = &inner_decl; break;
                case decl_struct_type::helper: rpc_decl_helper_structs.push_back(&inner_decl); break;
                default: break; // TODO(doyle): Warning unexpected decl inside rpc declaration
            }
        }

        if (!(request && response))
        {
            // TODO(doyle): Warning
            continue;
        }

        fprintf(stdout, "###%.*s \n\n", global_decl.name.len, global_decl.name.str);
        fprintf(stdout, "**Inputs:**\n");
        fprintf(stdout, "\n");
        for (decl_var const &variable : request->variables)
            fprint_variable(variable);
        fprintf(stdout, "\n");

        fprintf(stdout, "**Outputs:**\n");
        fprintf(stdout, "\n");
        for (decl_var const &variable : response->variables)
            fprint_variable(variable);
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
    std::vector<token_t> token_list;
    token_list.reserve(16384);

    std::vector<decl_struct> declarations;
    declarations.reserve(128);

    for (int arg_index = 1; arg_index < argc; arg_index++)
    {
        declarations.clear();
        token_list.clear();

        ptrdiff_t buf_size = 0;
        char *buf          = read_entire_file(argv[arg_index], &buf_size);
        if (!buf) continue; // TODO(doyle): Log
        DEFER { free(buf); };

        //
        // Lex File into token_list
        //
        {
            string_lit const GENERATOR_START = STRING_LIT("GENERATE_LOKI_DOCS");
            char *ptr = str_find(buf, GENERATOR_START);
            ptr      += GENERATOR_START.len;
            for (;ptr && *ptr;)
            {
                token_t curr_token = {};
                curr_token.str   = next_token(ptr);
                curr_token.len   = 1;
                ptr              = curr_token.str + 1;
                switch(curr_token.str[0])
                {
                    case '{': curr_token.type = token_type::left_curly_brace; break;
                    case '}': curr_token.type = token_type::right_curly_brace; break;
                    case ';': curr_token.type = token_type::semicolon; break;
                    case '<': curr_token.type = token_type::less_than; break;
                    case '>': curr_token.type = token_type::greater_than; break;

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
                                while (ptr[0] == ' ' || ptr[0] == '\t') ptr++;
                                curr_token.str = ptr;
                                while (ptr[0] != '\n' && ptr[0] != '\r') ptr++;
                            }
                            else
                            {
                                for (;;)
                                {
                                    while (ptr[0] != '*') ptr++;
                                    ptr++;
                                    if (ptr[0] == '\\') break;
                                    curr_token.len = static_cast<int>(ptr - curr_token.str);
                                }
                            }

                            curr_token.len = static_cast<int>(ptr - curr_token.str);
                        }
                    }
                    break;

                    default:
                    {
                        curr_token.type = token_type::identifier;
                        if (char_is_alpha(ptr[0]) || ptr[0] == '_')
                        {
                            ptr++;
                            while (char_is_alphanum(ptr[0]) || ptr[0] == '_') ptr++;
                        }
                        curr_token.len = static_cast<int>(ptr - curr_token.str);
                    }
                    break;
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
                    else if (string_lit_cmp(token_lit, STRING_LIT("STOP_GEN_LOKI_DOCS")))
                    {
                        break;
                    }
                }

                assert(curr_token.type != token_type::invalid);
                if (curr_token.len > 0)
                    token_list.push_back(curr_token);
            }

            token_t sentinel_token = {};
            sentinel_token.type  = token_type::end_of_stream;
            token_list.push_back(sentinel_token);
        }

        //
        // Parse lexed tokens into declarations
        //
        {
            tokeniser_t tokeniser = {};
            tokeniser.tokens   = token_list.data();
            for (token_t token = tokeniser_peek_token(&tokeniser);
                 token.type   != token_type::end_of_stream;
                 token         = tokeniser_peek_token(&tokeniser))
            {
                if (token.type == token_type::end_of_stream)
                    break;

                bool handled = true;
                if (token.type == token_type::identifier)
                {
                    string_lit token_lit = token_to_string_lit(token);
                    if (string_lit_cmp(token_lit, STRING_LIT("struct")))
                    {
                        decl_struct decl = fill_struct(&tokeniser);
                        declarations.push_back(decl);
                    }
                    else if (string_lit_cmp(token_lit, STRING_LIT("enum")))
                    {
                        // decl_struct doc = fill_struct(&tokeniser);
                    }
                    else
                    {
                        handled = false;
                    }
                }
                else
                {
                    handled = false;
                }

                if (!handled)
                    token = tokeniser_next_token(&tokeniser);
            }
        }
        generate_html_doc(&declarations);
    }

    return 0;
}
