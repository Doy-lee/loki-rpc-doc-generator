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
enum struct TokenType
{
    X_MACRO
};
#undef X_ENTRY

#define X_ENTRY(enum_val, stringified) DQN_STRING_LITERAL(stringified),
Dqn_String const TokenType_string[] =
{
    X_MACRO
};
#undef X_ENTRY
#undef X_MACRO

struct Token
{
  int        new_lines_encountered;
  char      *last_new_line;
  char      *str;
  int        size;
  TokenType type;
};

struct Tokeniser
{
    char *file;
    char *last_new_line;
    int   line; // 0 based, i.e text file line 1 -> line 0
    char *ptr;
    int   scope_level;
    int   paren_level;
    int   angle_bracket_level;

    Dqn_String last_required_token_lit;
    TokenType  last_required_TokenType;
    Token      last_received_token;
};

// -------------------------------------------------------------------------------------------------
//
// Token
//
// -------------------------------------------------------------------------------------------------
Dqn_String Token_String(Token token)
{
    Dqn_String result = Dqn_String_Init(token.str, token.size);
    return result;
}

b32 Token_IsEOF(Token token)
{
    b32 result = token.type == TokenType::end_of_stream;
    return result;
}

b32 Token_IsCPPTypeModifier(Token token)
{
    static Dqn_String const modifiers[] = {
        DQN_STRING_LITERAL("constexpr"),
        DQN_STRING_LITERAL("const"),
        DQN_STRING_LITERAL("static"),
    };

    for (Dqn_String const &mod : modifiers)
    {
        if (Token_String(token) == mod)
            return true;
    }

  return false;
}

// -------------------------------------------------------------------------------------------------
//
// Tokeniser Report Error
//
// -------------------------------------------------------------------------------------------------
void Tokeniser_ErrorPreamble(Tokeniser *tokeniser)
{
    if (tokeniser->file) fprintf(stderr, "Tokenising error in file %s:%d", tokeniser->file, tokeniser->line + 1);
    else                 fprintf(stderr, "Tokenising error buffer");
}

void Tokeniser_ErrorParseStruct(Tokeniser *tokeniser, Dqn_String *parent_name, char const *fmt = nullptr, ...)
{
    Dqn_String line = String_TillEOL(tokeniser->last_new_line);
    Tokeniser_ErrorPreamble(tokeniser);

    if (parent_name)
        fprintf(stderr, " in struct '%.*s'", (int)parent_name->size, parent_name->str);

    fprintf(stderr, "\n");
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);

    fprintf(stderr, "\n\n%.*s\n\n", (int)line.size, line.str);
}

void Tokeniser_ErrorLastRequiredToken(Tokeniser *tokeniser, char const *fmt = nullptr, ...)
{
    Tokeniser_ErrorPreamble(tokeniser);
    Dqn_String const line = (tokeniser->file) ? String_TillEOL(tokeniser->last_new_line) : String_TillEOL(tokeniser->ptr);

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
            (int)TokenType_string[(int)tokeniser->last_required_TokenType].size,
            TokenType_string[(int)tokeniser->last_required_TokenType].str,
            (int)tokeniser->last_received_token.size,
            tokeniser->last_received_token.str,
            (int)line.size,
            line.str);
}

void Tokeniser_ReportCustomError(Tokeniser *tokeniser, char const *fmt, ...)
{
    Tokeniser_ErrorPreamble(tokeniser);
    Dqn_String const line = (tokeniser->file) ? String_TillEOL(tokeniser->last_new_line) : String_TillEOL(tokeniser->ptr);

    fprintf(stderr, "\n");
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);

    fprintf(stderr, "\n\n%.*s\n\n", (int)line.size, line.str);
}

// -------------------------------------------------------------------------------------------------
//
// Tokeniser Iteration
//
// -------------------------------------------------------------------------------------------------
Dqn_String const INTROSPECT_MARKER = DQN_STRING_LITERAL("LOKI_RPC_DOC_INTROSPECT");
b32 Tokeniser_NextMarker(Tokeniser *tokeniser, Dqn_String marker)
{
    char *last_new_line = nullptr;
    int num_new_lines   = 0;
    char *ptr           = String_Find(tokeniser->ptr, marker, &num_new_lines, &last_new_line);
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
        tokeniser->ptr           = ptr + marker.size;
    }

    return ptr != nullptr;
}

Token Tokeniser_PeekToken(Tokeniser *tokeniser)
{
    Token result = {};
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
    result.size = 1;
    switch (result.str[0])
    {
        case '{': result.type = TokenType::left_curly_brace; break;
        case '}': result.type = TokenType::right_curly_brace; break;
        case ';': result.type = TokenType::semicolon; break;
        case '=': result.type = TokenType::equal; break;
        case '<': result.type = TokenType::less_than; break;
        case '>': result.type = TokenType::greater_than; break;
        case '(': result.type = TokenType::open_paren; break;
        case ')': result.type = TokenType::close_paren; break;
        case ',': result.type = TokenType::comma; break;
        case '*': result.type = TokenType::asterisks; break;
        case '&': result.type = TokenType::ampersand; break;
        case '-': result.type = TokenType::minus; break;
        case '.': result.type = TokenType::dot; break;
        case 0: result.type   = TokenType::end_of_stream; break;

        case '"':
        {
          result.type = TokenType::string;
          result.str  = ++ptr;
          while (ptr[0] != '"')
            ptr++;
          result.size = static_cast<int>(ptr - result.str);
          ptr++;
        }
        break;

        case ':':
        {
          result.type = TokenType::colon;
          if ((++ptr)[0] == ':')
          {
            ptr++;
            result.type = TokenType::namespace_colon;
            result.size  = 2;
          }
        }
        break;

        case '/':
        {
          ptr++;
          result.type = TokenType::fwd_slash;
          if (ptr[0] == '/' || ptr[0] == '*')
          {
            result.type = TokenType::comment;
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
              result.size = static_cast<int>(ptr - result.str);
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
                  result.size = static_cast<int>(ptr - result.str - 2);
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
          if (Dqn_Char_IsAlpha(ptr[0]) || ptr[0] == '_' || ptr[0] == '!')
          {
            ptr++;
            while (Dqn_Char_IsAlphaNum(ptr[0]) || ptr[0] == '_')
              ptr++;
            result.type = TokenType::identifier;
          }
          else if(Dqn_Char_IsDigit(ptr[0]))
          {
            ptr++;
            while (Dqn_Char_IsDigit(ptr[0]) || ptr[0] == '_')
              ptr++;
            result.type = TokenType::number;
          }

          result.size = static_cast<int>(ptr - result.str);
        }
        break;
    }

    if (result.type == TokenType::invalid)
    {
        Tokeniser_ReportCustomError(tokeniser, "Unhandled token '%c' in character stream when iterating tokeniser", result.str[0]);
        assert(result.type != TokenType::invalid);
    }
    return result;
}

void Tokeniser_AcceptToken(Tokeniser *tokeniser, Token token)
{
    if (token.type == TokenType::left_curly_brace)
    {
        tokeniser->scope_level++;
    }
    else if (token.type == TokenType::right_curly_brace)
    {
        tokeniser->scope_level--;
    }
    else if (token.type == TokenType::open_paren)
    {
        tokeniser->paren_level++;
    }
    else if (token.type == TokenType::close_paren)
    {
        tokeniser->paren_level--;
    }
    else if (token.type == TokenType::less_than)
    {
        tokeniser->angle_bracket_level++;
    }
    else if (token.type == TokenType::greater_than)
    {
        tokeniser->angle_bracket_level--;
    }

    assert(tokeniser->paren_level >= 0);
    assert(tokeniser->scope_level >= 0);
    assert(tokeniser->angle_bracket_level >= 0);
    tokeniser->ptr = token.str + token.size;

    if (!tokeniser->last_new_line) tokeniser->last_new_line = token.str;
    if (token.new_lines_encountered) tokeniser->last_new_line = token.last_new_line;
    tokeniser->line += token.new_lines_encountered;

    if (token.type == TokenType::string)
        tokeniser->ptr++; // Include the ending quotation mark " of a "string"
}

b32 Tokeniser_RequireTokenType(Tokeniser *tokeniser, TokenType type, Token *token = nullptr)
{
    Token token_ = {};
    if (!token) token = &token_;
    *token                             = Tokeniser_PeekToken(tokeniser);
    tokeniser->last_required_TokenType = type;
    tokeniser->last_received_token     = *token;
    b32 result                         = (token->type == type);
    if (result) Tokeniser_AcceptToken(tokeniser, *token);
    return result;
}

b32 Tokeniser_RequireToken(Tokeniser *tokeniser, Dqn_String string, Token *token = nullptr)
{
    Token token_ = {};
    if (!token) token = &token_;
    *token                              = Tokeniser_PeekToken(tokeniser);
    tokeniser->last_required_TokenType = token->type;
    tokeniser->last_received_token      = *token;
    b32 result = (Token_String(*token) == string);
    if (result) Tokeniser_AcceptToken(tokeniser, *token);
    return result;
}

Token Tokeniser_NextToken(Tokeniser *tokeniser)
{
    Token result = Tokeniser_PeekToken(tokeniser);
    Tokeniser_AcceptToken(tokeniser, result);
    return result;
}

b32 Tokeniser_AdvanceToTokenType(Tokeniser *tokeniser, TokenType type, Token *token = nullptr)
{

    Token token_ = {};
    if (!token) token = &token_;
    for (;;)
    {
        *token = Tokeniser_NextToken(tokeniser);
        if (token->type == type) return true;
        if (token->type == TokenType::end_of_stream) return false;
    }
}

b32 Tokeniser_AdvanceToScopeLevel(Tokeniser *tokeniser, int scope_level)
{
    for (;;)
    {
        Token result = Tokeniser_NextToken(tokeniser);
        if (tokeniser->scope_level == scope_level) return true;
        if (result.type == TokenType::end_of_stream) return false;
    }
}

b32 Tokeniser_AdvanceToParenLevel(Tokeniser *tokeniser, int paren_level)
{
    for (;;)
    {
        Token result = Tokeniser_NextToken(tokeniser);
        if (tokeniser->paren_level == paren_level) return true;
        if (result.type == TokenType::end_of_stream) return false;
    }
}

// -------------------------------------------------------------------------------------------------
//
// Tokeniser Parsing
//
// -------------------------------------------------------------------------------------------------
b32 Tokeniser_ParseFuctionStartFromName(Tokeniser *tokeniser)
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
        if (!Tokeniser_AdvanceToTokenType(tokeniser, TokenType::open_paren))
        {
            // @TODO(doyle): Logging
            return false;
        }

        if (!Tokeniser_AdvanceToParenLevel(tokeniser, original_paren_level))
        {
            // @TODO(doyle): Logging
            return false;
        }
    }

    // NOTE: Remove const from any member functions
    Token peek = Tokeniser_PeekToken(tokeniser);
    if (Token_IsCPPTypeModifier(peek))
        Tokeniser_NextToken(tokeniser);


    // NOTE: If next token is '=' then we have a 'Foo = default();' situation
    if (Tokeniser_RequireTokenType(tokeniser, TokenType::equal))
    {
        Tokeniser_AdvanceToTokenType(tokeniser, TokenType::semicolon);
    }
    else if (Tokeniser_RequireTokenType(tokeniser, TokenType::left_curly_brace))
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
        Tokeniser_AdvanceToScopeLevel(tokeniser, original_scope_level);
    }
    else if (Tokeniser_RequireTokenType(tokeniser, TokenType::colon))
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
        Tokeniser_AdvanceToTokenType(tokeniser, TokenType::left_curly_brace);
        assert(tokeniser->scope_level != original_scope_level);
        Tokeniser_AdvanceToScopeLevel(tokeniser, original_scope_level);
    }
    else if (Tokeniser_RequireTokenType(tokeniser, TokenType::semicolon))
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

    return true;
}

b32 Tokeniser_ParseRPCAliasNamess(Tokeniser *tokeniser, DeclStruct *result)
{
    /*
       PARSING SCENARIO
                                  +-------- Tokeniser is here
                                  V
       static constexpr auto names() { return NAMES("get_block_hash", "on_get_block_hash", "on_getblockhash"); }
    */

    if (!Tokeniser_RequireTokenType(tokeniser, TokenType::open_paren))
        return false;

    if (!Tokeniser_RequireTokenType(tokeniser, TokenType::close_paren))
        return false;

    if (!Tokeniser_RequireTokenType(tokeniser, TokenType::left_curly_brace))
        return false;

    if (!Tokeniser_RequireToken(tokeniser, DQN_STRING_LITERAL("return")))
        return false;

    if (!Tokeniser_RequireToken(tokeniser, DQN_STRING_LITERAL("NAMES")))
        return false;

    if (!Tokeniser_RequireTokenType(tokeniser, TokenType::open_paren))
        return false;

    Token alias = {};
    for (;;)
    {
        if (Tokeniser_RequireTokenType(tokeniser, TokenType::string, &alias))
            result->aliases.push_back(Token_String(alias));

        Token token = Tokeniser_NextToken(tokeniser);
        if (token.type == TokenType::close_paren)
            return true;
        else if (token.type != TokenType::comma)
            return false;
    }
}

// NOTE: In a response/request struct for RPC commands, the primitives as
// inputs and outputs of the command are converted into something more generic
// and language agnostic.
static DeclVariableMetadata DeriveVariableMetadata(DeclVariable *variable)
{
  DeclVariableMetadata result  = {};
  Dqn_String const var_type = (variable->is_array) ? variable->template_expr : variable->type;

  if (variable->type == DQN_STRING_LITERAL("std::array<int, 3>"))
  {
      // @TODO(doyle): Yes, I know .. not cool. This can be improved by
      // constructing the tokeniser inplace and decoding the template.
      LOCAL_PERSIST Dqn_String const NICE_NAME = DQN_STRING_LITERAL("int[3]");
      result.is_std_array                      = true;
      result.converted_type                    = &NICE_NAME;
  }
  else if (variable->type == DQN_STRING_LITERAL("std::array<uint16_t, 3>"))
  {
      // @TODO(doyle): Yes, I know .. not cool. This can be improved by
      // constructing the tokeniser inplace and decoding the template.
      LOCAL_PERSIST Dqn_String const NICE_NAME = DQN_STRING_LITERAL("uint16[3]");
      result.is_std_array                  = true;
      result.converted_type                = &NICE_NAME;
  }
  else if (var_type == DQN_STRING_LITERAL("std::string"))
  {
    LOCAL_PERSIST Dqn_String const NICE_NAME = DQN_STRING_LITERAL("string");
    result.converted_type = &NICE_NAME;
  }
  else if (var_type == DQN_STRING_LITERAL("uint64_t") || var_type == DQN_STRING_LITERAL("std::uint64_t"))
  {
    LOCAL_PERSIST Dqn_String const NICE_NAME = DQN_STRING_LITERAL("uint64");
    result.converted_type                    = &NICE_NAME;
  }
  else if (var_type == DQN_STRING_LITERAL("size_t"))
  {
    LOCAL_PERSIST Dqn_String const NICE_NAME = DQN_STRING_LITERAL("uint64");
    result.converted_type                    = &NICE_NAME;
  }
  else if (var_type == DQN_STRING_LITERAL("uint32_t"))
  {
    LOCAL_PERSIST Dqn_String const NICE_NAME = DQN_STRING_LITERAL("uint32");
    result.converted_type                    = &NICE_NAME;
  }
  else if (var_type == DQN_STRING_LITERAL("uint16_t"))
  {
    LOCAL_PERSIST Dqn_String const NICE_NAME = DQN_STRING_LITERAL("uint16");
    result.converted_type = &NICE_NAME;
  }
  else if (var_type == DQN_STRING_LITERAL("uint8_t"))
  {
    LOCAL_PERSIST Dqn_String const NICE_NAME = DQN_STRING_LITERAL("uint8");
    result.converted_type = &NICE_NAME;
  }
  else if (var_type == DQN_STRING_LITERAL("int64_t"))
  {
    LOCAL_PERSIST Dqn_String const NICE_NAME = DQN_STRING_LITERAL("int64");
    result.converted_type                    = &NICE_NAME;
  }
  else if (var_type == DQN_STRING_LITERAL("int32_t") || var_type == DQN_STRING_LITERAL("int"))
  {
    LOCAL_PERSIST Dqn_String const NICE_NAME = DQN_STRING_LITERAL("int32");
    result.converted_type                    = &NICE_NAME;
  }
  else if (var_type == DQN_STRING_LITERAL("int16_t"))
  {
    LOCAL_PERSIST Dqn_String const NICE_NAME = DQN_STRING_LITERAL("int16");
    result.converted_type = &NICE_NAME;
  }
  else if (var_type == DQN_STRING_LITERAL("int8_t"))
  {
    LOCAL_PERSIST Dqn_String const NICE_NAME = DQN_STRING_LITERAL("int8");
    result.converted_type = &NICE_NAME;
  }
  else if (var_type == DQN_STRING_LITERAL("blobdata"))
  {
      LOCAL_PERSIST Dqn_String const NICE_NAME = DQN_STRING_LITERAL("string");
      result.converted_type                    = &NICE_NAME;
  }
  else if (var_type == DQN_STRING_LITERAL("crypto::hash") ||
           var_type == DQN_STRING_LITERAL("crypto::public_key") ||
           var_type == DQN_STRING_LITERAL("rct::key"))
  {
      LOCAL_PERSIST Dqn_String const NICE_NAME = DQN_STRING_LITERAL("string[64]");
      result.converted_type                    = &NICE_NAME;
  }
  else if (var_type == DQN_STRING_LITERAL("crypto::signature"))
  {
      LOCAL_PERSIST Dqn_String const NICE_NAME = DQN_STRING_LITERAL("string[128]");
      result.converted_type                    = &NICE_NAME;
  }
  else if (var_type == DQN_STRING_LITERAL("difficulty_type"))
  {
      LOCAL_PERSIST Dqn_String const NICE_NAME = DQN_STRING_LITERAL("uint64");
      result.converted_type                    = &NICE_NAME;
  }
  if (result.converted_type || var_type == DQN_STRING_LITERAL("bool"))
      result.recognised = true;
  return result;
};

enum struct Tokeniser_ParseTypeAndNameStatus
{
    NotVariable,
    MemberFunction,
    Failed,
    Success,
};

// NOTE: Parse any declaration of the sort <Type> <variable_name> = <value>
//       But since C++ function declarations are *almost* like <Type> <name>
//       including constructors, as side bonus this function handles parsing the
//       function name and type which is placed into the variable.
Tokeniser_ParseTypeAndNameStatus Tokeniser_Tokeniser_ParseTypeAndName(Tokeniser *tokeniser, DeclVariable *variable, Dqn_String parent_struct_name)
{
    // NOTE: Parse Variable Type and Name
    {
        Token token = {};
        for (;;) // ignore type modifiers like const and static
        {
            Token next = Tokeniser_NextToken(tokeniser);
            if (!Token_IsCPPTypeModifier(next))
            {
                token = next;
                break;
            }
        }

        // NOTE: Tokeniser is parsing a constructor like --->  Foo(...) : foo(...), { ... }
        if (Token_String(token) == parent_struct_name) // C++ Constructor
        {
            if (!Tokeniser_ParseFuctionStartFromName(tokeniser))
            {
                Tokeniser_ReportCustomError(
                    tokeniser,
                    "Failed to parse constructor '%.*s', internal logic error (most likely, C++ "
                    "has 10 gazillion ways to write the same thing)",
                    parent_struct_name.size,
                    parent_struct_name.str);
                return Tokeniser_ParseTypeAndNameStatus::Failed;
            }

            return Tokeniser_ParseTypeAndNameStatus::NotVariable;
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

        Token variable_type_first_token         = token;
        Token variable_type_one_past_last_token = {};
        assert(tokeniser->angle_bracket_level == 0);

        for (;;)
        {
            Token next = Tokeniser_NextToken(tokeniser);
            if (next.type == TokenType::identifier &&
                isspace(next.str[-1]) &&
                tokeniser->angle_bracket_level == 0)
            {
                variable_type_one_past_last_token = next;
                break;
            }
        }

        Token variable_name = variable_type_one_past_last_token;
        for (;;) // ignore type modifiers like const and static
        {
            if (!Token_IsCPPTypeModifier(variable_name))
                break;
            variable_name = Tokeniser_NextToken(tokeniser);
        }

        // @TODO(doyle): This captures type modifiers that are placed after the variable decl
        variable->name      = Token_String(variable_name);
        variable->type.str  = variable_type_first_token.str;
        variable->type.size = &variable_type_one_past_last_token.str[-1] - variable_type_first_token.str;
        variable->type      = Dqn_String_TrimWhitespaceAround(variable->type);

        Token peek = Tokeniser_PeekToken(tokeniser);
        if (Token_String(variable_name) == DQN_STRING_LITERAL("operator") || peek.type == TokenType::open_paren)
        {
            if (!Tokeniser_ParseFuctionStartFromName(tokeniser))
            {
                Tokeniser_ReportCustomError(tokeniser,
                                            "Failed to parse function '%.*s', internal logic error (most likely, C++ "
                                            "has 10 gazillion ways to write the same thing)",
                                            variable->name.size,
                                            variable->name.str);
                return Tokeniser_ParseTypeAndNameStatus::Failed;
            }

            return Tokeniser_ParseTypeAndNameStatus::MemberFunction;
        }

        if (Token_IsEOF(variable_name) || Token_IsEOF(peek))
            return Tokeniser_ParseTypeAndNameStatus::Failed;
    }

    for (int i = 0; i < variable->type.size; ++i)
    {
        if (variable->type.str[i] == '<')
        {
            variable->template_expr.str = variable->type.str + (++i);
            for (int j = ++i; j < variable->type.size; ++j)
            {
                if (variable->type.str[j] == '>')
                {
                    char const *template_expr_end = variable->type.str + j;
                    variable->template_expr.size  = static_cast<int>(template_expr_end - variable->template_expr.str);
                    break;
                }
            }
            break;
        }
    }

    // TODO(doyle): This is horribly incomplete, but we only specify arrays in
    // terms of C++ containers in loki which all have templates. So meh.
    variable->is_array = variable->template_expr.size > 0;
    variable->metadata = DeriveVariableMetadata(variable);

    Token token = Tokeniser_NextToken(tokeniser);
    if (token.type == TokenType::equal)
    {
        Token variable_value_first_token         = Tokeniser_NextToken(tokeniser);
        Token variable_value_one_past_last_token = {};
        if (!Tokeniser_AdvanceToTokenType(tokeniser, TokenType::semicolon, &variable_value_one_past_last_token))
            return Tokeniser_ParseTypeAndNameStatus::Failed;

        variable->value.str = variable_value_first_token.str;
        variable->value.size = &variable_value_one_past_last_token.str[-1] - variable_value_first_token.str;
    }
    else if (token.type != TokenType::semicolon)
    {
       Tokeniser_AdvanceToTokenType(tokeniser, TokenType::semicolon);
    }


    token = Tokeniser_PeekToken(tokeniser);
    if (token.type == TokenType::comment)
    {
        variable->comment = Token_String(token);
        token = Tokeniser_NextToken(tokeniser);
    }

    Dqn_String const skip_marker = DQN_STRING_LITERAL("@NoLokiRPCDocGen");
    if (variable->comment.size >= skip_marker.size && strncmp(variable->comment.str, skip_marker.str, skip_marker.size) == 0)
        return Tokeniser_ParseTypeAndNameStatus::NotVariable;

    return Tokeniser_ParseTypeAndNameStatus::Success;
}


DeclStruct UNRESOLVED_ALIAS_STRUCT        = {};
static Dqn_String const COMMAND_RPC_PREFIX = DQN_STRING_LITERAL("COMMAND_RPC_");
b32 Tokeniser_ParseStruct(Tokeniser *tokeniser, DeclStruct *result, b32 root_struct, Dqn_String *parent_name)
{
    if (!Tokeniser_RequireToken(tokeniser, DQN_STRING_LITERAL("struct")) &&
        !Tokeniser_RequireToken(tokeniser, DQN_STRING_LITERAL("class")))
    {
        Tokeniser_ErrorParseStruct(
            tokeniser,
            parent_name,
            "Tokeniser_ParseStruct called on child struct did not have expected 'struct' or 'class' identifier");
        return false;
    }

    Token struct_name = {};
    if (!Tokeniser_RequireTokenType(tokeniser, TokenType::identifier, &struct_name))
    {
        Tokeniser_ErrorLastRequiredToken(tokeniser, "Parsing struct, expected <identifier> for the struct name");
        return false;
    }

    result->name = Token_String(struct_name);
    if (!root_struct)
    {
        if (result->name == DQN_STRING_LITERAL("request"))
        {
            result->type = DeclStructType::RPCRequest;
        }
        if (result->name == DQN_STRING_LITERAL("response"))
        {
            result->type = DeclStructType::RPCResponse;
        }
    }

    if (Tokeniser_RequireTokenType(tokeniser, TokenType::colon)) // Struct Inheritance
    {
        for (;;)
        {
            Token inheritance;
            if (Tokeniser_RequireTokenType(tokeniser, TokenType::identifier, &inheritance))
            {
                Dqn_String inheritance_lit = Token_String(inheritance);
                if (inheritance_lit == DQN_STRING_LITERAL("PUBLIC"))
                {
                }
                else if (inheritance_lit == DQN_STRING_LITERAL("BINARY")) result->type = DeclStructType::BinaryRPCCommand;
                else if (inheritance_lit == DQN_STRING_LITERAL("LEGACY")) result->type = DeclStructType::JsonRPCCommand;
                else if (inheritance_lit == DQN_STRING_LITERAL("EMPTY"))
                {
                }
                else if (inheritance_lit == DQN_STRING_LITERAL("RPC_COMMAND")) result->type = DeclStructType::JsonRPCCommand;
                else if (inheritance_lit == DQN_STRING_LITERAL("STATUS"))
                {
                    // @TODO(doyle):
                }
                else if (inheritance_lit == DQN_STRING_LITERAL("public"))
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

                    for (Token next = Tokeniser_PeekToken(tokeniser);  // NOTE: Advance tokeniser until end of parent inheritance name
                         ;
                         next = Tokeniser_NextToken(tokeniser))
                    {
                        if (next.type == TokenType::comma)
                        {
                            Tokeniser_ErrorParseStruct(
                                tokeniser,
                                parent_name,
                                "Encountered commma ',' in inheritance line indicating multiple "
                                "inheritance which we don't support for arbitrary types yet, unless they're the RPC tags "
                                "like PUBLIC, BINARY, LEGACY, EMPTY which are handled in the special case");
                            return false;
                        }

                        if (next.type == TokenType::namespace_colon)
                        {
                            Tokeniser_AcceptToken(tokeniser, next); // Accept the namespace token
                            Token peek_token = Tokeniser_PeekToken(tokeniser);
                            if (peek_token.type != TokenType::identifier)
                            {
                                Tokeniser_ErrorParseStruct(tokeniser,
                                                           parent_name,
                                                           "Namespace colon '::' should be followed by an identifier "
                                                           "in 'struct %.*s', received token '%.*s'",
                                                           DQN_STRING_PRINTF(result->name),
                                                           DQN_STRING_PRINTF(Token_String(peek_token))
                                                           );
                                return false;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }

                    Token one_after_last_inheritance_name = Tokeniser_PeekToken(tokeniser);
                    if (one_after_last_inheritance_name.type == TokenType::left_curly_brace)
                    {
                        result->inheritance_parent_name.size = &one_after_last_inheritance_name.str[-1] - inheritance_lit.str;
                        result->inheritance_parent_name.str  = inheritance_lit.str;
                        result->inheritance_parent_name = Dqn_String_TrimWhitespaceAround(result->inheritance_parent_name);
                        break;
                    }
                    else
                    {
                        Tokeniser_ErrorParseStruct(
                            tokeniser,
                            parent_name,
                            "Detected inheritance identifier '%.*s' following struct name '%.*s'",
                            result->inheritance_parent_name.size,
                            result->inheritance_parent_name.str,
                            result->name.size,
                            result->name.str);
                    }
                }

                if (!Tokeniser_RequireTokenType(tokeniser, TokenType::comma))
                    break;
            }
            else
            {
                Tokeniser_ErrorLastRequiredToken(tokeniser, "Struct inheritance detected with ':' after the struct name, but no <identifiers> detected after the name");
                return false;
            }
        }
    }

    if (result->type == DeclStructType::Invalid)
        result->type = DeclStructType::Helper;

    int original_scope_level = tokeniser->scope_level;
    if (!Tokeniser_RequireTokenType(tokeniser, TokenType::left_curly_brace, nullptr))
    {
        Tokeniser_ErrorLastRequiredToken(tokeniser, "Struct inheritance detected with ':' after the struct name, but no <identifiers> detected after the name");
        return false;
    }

    // ---------------------------------------------------------------------------------------------
    //
    // Parse Struct Contents
    //
    // ---------------------------------------------------------------------------------------------
    for (Token token = Tokeniser_PeekToken(tokeniser);
         token.type == TokenType::identifier || token.type == TokenType::comment;
         token = Tokeniser_PeekToken(tokeniser))
    {
        if (token.type == TokenType::comment)
        {
            Tokeniser_NextToken(tokeniser);
            continue;
        }

        Dqn_String token_lit = Token_String(token);
        if (token_lit == DQN_STRING_LITERAL("struct") || token_lit == DQN_STRING_LITERAL("class"))
        {
            DeclStruct decl = {};
            if (!Tokeniser_ParseStruct(tokeniser, &decl, false /*root_struct*/, &result->name))
                return false;

            result->inner_structs.push_back(std::move(decl));
        }
        else if (token_lit == DQN_STRING_LITERAL("enum"))
        {
            // @TODO(doyle): Actually handle
            Tokeniser_AdvanceToTokenType(tokeniser, TokenType::semicolon);
        }
        else if (token_lit == DQN_STRING_LITERAL("KV_MAP_SERIALIZABLE"))
        {
            Tokeniser_NextToken(tokeniser);
        }
        else if (token_lit == DQN_STRING_LITERAL("BEGIN_SERIALIZE"))
        {
            if (!Tokeniser_NextMarker(tokeniser, DQN_STRING_LITERAL("END_SERIALIZE()")))
            {
                Tokeniser_ErrorParseStruct(
                    tokeniser,
                    parent_name,
                    "Tokeniser encountered BEGIN_SERIALIZE() macro, failed to find closing macro END_SERIALIZE()");
                return false;
            }
        }
        else if (token_lit == DQN_STRING_LITERAL("BEGIN_SERIALIZE_OBJECT"))
        {
            if (!Tokeniser_NextMarker(tokeniser, DQN_STRING_LITERAL("END_SERIALIZE()")))
            {
                Tokeniser_ErrorParseStruct(
                    tokeniser,
                    parent_name,
                    "Tokeniser encountered BEGIN_SERIALIZE() macro, failed to find closing macro END_SERIALIZE_OBJECT()");
                return false;
            }
        }
        else if (token_lit == DQN_STRING_LITERAL("BEGIN_KV_SERIALIZE_MAP"))
        {
            if (!Tokeniser_NextMarker(tokeniser, DQN_STRING_LITERAL("END_KV_SERIALIZE_MAP()")))
            {
                Tokeniser_ErrorParseStruct(
                    tokeniser,
                    parent_name,
                    "Tokeniser encountered END_KV_SERIALIZE_MAP() macro, failed to find closing macro END_SERIALIZE()");
                return false;
            }
        }
        else if (token_lit == DQN_STRING_LITERAL("using"))
        {
            Tokeniser_NextToken(tokeniser);
            Token using_name = {};
            if (Tokeniser_RequireTokenType(tokeniser, TokenType::identifier, &using_name))
            {
                if (Tokeniser_RequireTokenType(tokeniser, TokenType::equal))
                {
                    Token using_value_first_token         = Tokeniser_NextToken(tokeniser);
                    Token using_value_one_past_last_token = {};
                    Tokeniser_AdvanceToTokenType(tokeniser, TokenType::semicolon, &using_value_one_past_last_token);

                    auto using_value = Dqn_String_Init(using_value_first_token.str, using_value_one_past_last_token.str - using_value_first_token.str);
                    DeclVariable variable = {};
                    variable.aliases_to = &UNRESOLVED_ALIAS_STRUCT; // NOTE: Resolve the declaration later
                    variable.name       = Token_String(using_name);
                    variable.type       = Dqn_String_TrimWhitespaceAround(using_value);
                    variable.metadata   = DeriveVariableMetadata(&variable);

                    Token comment = {};
                    if (Tokeniser_RequireTokenType(tokeniser, TokenType::comment, &comment))
                        variable.comment = Token_String(comment);

                    result->variables.push_back(variable);
                }
                else
                {
                    Tokeniser_ErrorLastRequiredToken(tokeniser);
                    return false;
                }
            }
            else
            {
                Tokeniser_ErrorLastRequiredToken(tokeniser);
                return false;
            }
        }
        else
        {
            DeclVariable variable         = {};
            Tokeniser_ParseTypeAndNameStatus status = Tokeniser_Tokeniser_ParseTypeAndName(tokeniser, &variable, result->name);

            if (status == Tokeniser_ParseTypeAndNameStatus::Success)
                result->variables.push_back(variable);
            else if (status == Tokeniser_ParseTypeAndNameStatus::MemberFunction)
            {
                if (variable.name == DQN_STRING_LITERAL("names"))
                {
                    Tokeniser name_tokeniser = *tokeniser;
                    name_tokeniser.ptr       = variable.name.str + variable.name.size;
                    name_tokeniser.file      = tokeniser->file;
                    if (!Tokeniser_ParseRPCAliasNamess(&name_tokeniser, result))
                    {
                        Tokeniser_ErrorParseStruct(tokeniser,
                                                   parent_name,
                                                   "Failed to parse RPC names in struct '%.*s'",
                                                   result->name.size,
                                                   result->name.str);
                        return false;
                    }
                }
            }
            else if (status == Tokeniser_ParseTypeAndNameStatus::Failed)
            {
                Tokeniser_ErrorLastRequiredToken(tokeniser);
                return false;
            }
        }
    }

    Tokeniser_AdvanceToScopeLevel(tokeniser, original_scope_level);
    if (!Tokeniser_RequireTokenType(tokeniser, TokenType::semicolon))
    {
        Tokeniser_ErrorLastRequiredToken(tokeniser);
        return false;
    }

    return true;
}
