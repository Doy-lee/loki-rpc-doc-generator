#define _CRT_SECURE_NO_WARNINGS

#include "LokiRPCDocGenerator.h"

// -------------------------------------------------------------------------------------------------
//
// String
//
// -------------------------------------------------------------------------------------------------
bool String_Compare(String a, String b)
{
    bool result = (a.len == b.len && (strncmp(a.str, b.str, a.len) == 0));
    return result;
}

bool operator==(String const &lhs, String const &rhs)
{
  bool result = String_Compare(lhs, rhs);
  return result;
}

String String_TrimWhitespaceAround(String in)
{
    String result = in;
    while (result.str && isspace(result.str[0]))
    {
        result.str++;
        result.len--;
    }

    while (result.str && isspace(result.str[result.len - 1]))
        result.len--;

    return result;
}

String String_TillEOL(char const *ptr)
{
    char const *end = ptr;
    while (end[0] != '\n' && end[0] != 0)
        end++;

    String result = {};
    result.str        = ptr;
    result.len        = end - ptr;
    return result;
}

char *String_Find(char *start, String find, int *num_new_lines = nullptr, char **last_new_line = nullptr)
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

// -------------------------------------------------------------------------------------------------
//
// Char
//
// -------------------------------------------------------------------------------------------------
bool Char_IsAlpha   (char ch) { return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'); }
bool Char_IsDigit   (char ch) { return (ch >= '0' && ch <= '9'); }
bool Char_IsAlphanum(char ch) { return Char_IsAlpha(ch) || Char_IsDigit(ch); }
char Char_ToLower   (char ch) { if (ch >= 'A' && ch <= 'Z') ch += ('a' - 'A'); return ch; }

// -------------------------------------------------------------------------------------------------
//
// Utils
//
// -------------------------------------------------------------------------------------------------
char *File_ReadAll(char const *file_path, ptrdiff_t *file_size)
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

#include "Tokeniser.cpp"

// -------------------------------------------------------------------------------------------------
//
// Doc Generator
//
// -------------------------------------------------------------------------------------------------
DeclVariableMetadata DeriveVariableMetadata(DeclVariable const *variable)
{
  DeclVariableMetadata result  = {};
  String const var_type = (variable->is_array) ? variable->template_expr : variable->type;

  if (var_type == STRING_LIT("std::string"))
  {
    local_persist String const NICE_NAME = STRING_LIT("string");
    result.converted_type = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("uint64_t"))
  {
    local_persist String const NICE_NAME = STRING_LIT("uint64");
    result.converted_type                    = &NICE_NAME;

  }
  else if (var_type == STRING_LIT("uint32_t"))
  {
    local_persist String const NICE_NAME = STRING_LIT("uint32");
    result.converted_type                    = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("uint16_t"))
  {
    local_persist String const NICE_NAME = STRING_LIT("uint16");
    result.converted_type = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("uint8_t"))
  {
    local_persist String const NICE_NAME = STRING_LIT("uint8");
    result.converted_type = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("int64_t"))
  {
    local_persist String const NICE_NAME = STRING_LIT("int64");
    result.converted_type                    = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("int32_t"))
  {
    local_persist String const NICE_NAME = STRING_LIT("int32");
    result.converted_type                    = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("int16_t"))
  {
    local_persist String const NICE_NAME = STRING_LIT("int16");
    result.converted_type = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("int8_t"))
  {
    local_persist String const NICE_NAME = STRING_LIT("int8");
    result.converted_type = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("blobdata"))
  {
      local_persist String const NICE_NAME = STRING_LIT("string");
      result.converted_type                    = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("crypto::hash"))
  {
      local_persist String const NICE_NAME = STRING_LIT("string[64]");
      result.converted_type                    = &NICE_NAME;
  }
  else if (var_type == STRING_LIT("difficulty_type"))
  {
      local_persist String const NICE_NAME = STRING_LIT("uint64");
      result.converted_type                    = &NICE_NAME;
  }
  return result;
};

DeclStruct const *LookupTypeDefinition(std::vector<DeclStruct const *> *global_helper_structs,
                                       std::vector<DeclStruct const *> *rpc_helper_structs,
                                       String const var_type)
{
  String const *check_type = &var_type;
  DeclStruct const *result    = nullptr;
  for (int tries = 0; tries < 2; tries++)
  {
    for (DeclStruct const *decl : *global_helper_structs)
    {
      if (String_Compare(*check_type, decl->name))
      {
        result = decl;
        break;
      }
    }

    if (!result)
    {
      for (DeclStruct const *decl : *rpc_helper_structs)
      {
        if (String_Compare(*check_type, decl->name))
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
      String const skip_namespace = STRING_LIT("cryptonote::");
      if (var_type.len >= skip_namespace.len && strncmp(skip_namespace.str, var_type.str, skip_namespace.len) == 0)
      {
        static String var_type_without_cryptonote;
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

void Print_Indented(int indent_level, FILE *dest, char const *fmt...)
{
  for (int i = 0; i < indent_level * 2; ++i)
    fprintf(dest, " ");

  va_list va;
  va_start(va, fmt);
  vfprintf(dest, fmt, va);
  va_end(va);
}

struct DeclStructHierarchy
{
    DeclStruct const *children[16];
    int               children_index;
};

static void Print_VariableExample(DeclStructHierarchy const *hierarchy, String var_type, DeclVariable const *variable)
{
  String var_name = variable->name;
  // -----------------------------------------------------------------------------------------------
  //
  // Specialised Type Handling
  //
  // -----------------------------------------------------------------------------------------------
#define PRINT_AND_RETURN(str) do { fprintf(stdout, str); return; } while(0)
  if (hierarchy->children_index > 0)
  {
      DeclStruct const *root = hierarchy->children[0];
      String alias   = root->aliases[0];
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

void Print_CurlJsonRPCParam(std::vector<DeclStruct const *> *global_helper_structs,
                                std::vector<DeclStruct const *> *rpc_helper_structs,
                                DeclStructHierarchy const *hierarchy,
                                DeclVariable const *variable,
                                int indent_level)
{
  DeclVariableMetadata const *metadata = &variable->metadata;
  Print_Indented(indent_level, stdout, "\"%.*s\": ", variable->name.len, variable->name.str);

  String var_type = variable->type;
  if (variable->is_array)
  {
    fprintf(stdout, "[");
    var_type = variable->template_expr;
  }

  if (DeclStruct const *resolved_decl = LookupTypeDefinition(global_helper_structs, rpc_helper_structs, var_type))
  {
    fprintf(stdout, "{\n");
    indent_level++;
    DeclStructHierarchy sub_hierarchy                    = *hierarchy;
    sub_hierarchy.children[sub_hierarchy.children_index++] = resolved_decl;
    for (size_t var_index = 0; var_index < resolved_decl->variables.size(); ++var_index)
    {
      DeclVariable const *inner_variable = &resolved_decl->variables[var_index];
      Print_CurlJsonRPCParam(global_helper_structs, rpc_helper_structs, &sub_hierarchy, inner_variable, indent_level);
      if (var_index < (resolved_decl->variables.size() - 1))
      {
        fprintf(stdout, ",");
        fprintf(stdout, "\n");
      }
    }

    fprintf(stdout, "\n");
    Print_Indented(--indent_level, stdout, "}");
  }
  else
  {
    Print_VariableExample(hierarchy, var_type, variable);
  }

  if (variable->is_array)
    fprintf(stdout, "]");
}

void Print_CurlExample(std::vector<DeclStruct const *> *global_helper_structs, std::vector<DeclStruct const *> *rpc_helper_structs, DeclStructHierarchy const *hierarchy, DeclStruct const *request, DeclStruct const *response, String const rpc_endpoint)
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
    Print_Indented(indent_level, stdout, "\"jsonrpc\":\"2.0\",\n");
    Print_Indented(indent_level, stdout, "\"id\":\"0\",\n");
    Print_Indented(indent_level, stdout, "\"method\":\"%.*s\"", rpc_endpoint.len, rpc_endpoint.str);
  }


  if (request->variables.size() > 0)
  {
    if (is_json_rpc)
    {
      fprintf(stdout, ",\n");
      Print_Indented(indent_level++, stdout, "\"params\": {\n");
    }

    DeclStructHierarchy sub_hierarchy                    = *hierarchy;
    sub_hierarchy.children[sub_hierarchy.children_index++] = request;
    for (size_t var_index = 0; var_index < request->variables.size(); ++var_index)
    {
      DeclVariable const *variable = &request->variables[var_index];
      Print_CurlJsonRPCParam(global_helper_structs, rpc_helper_structs, &sub_hierarchy, variable, indent_level);
      if (var_index < (request->variables.size() - 1))
        fprintf(stdout, ",\n");
    }

    if (is_json_rpc)
    {
      fprintf(stdout, "\n");
      Print_Indented(--indent_level, stdout, "}");
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
    DeclStructHierarchy sub_hierarchy                    = *hierarchy;
    sub_hierarchy.children[sub_hierarchy.children_index++] = response;
    for (size_t var_index = 0; var_index < response->variables.size(); ++var_index)
    {
      DeclVariable const *variable = &response->variables[var_index];
      Print_CurlJsonRPCParam(global_helper_structs, rpc_helper_structs, &sub_hierarchy, variable, 1 /*indent_level*/);
      if (var_index < (response->variables.size() - 1))
        fprintf(stdout, ",\n");
    }
    fprintf(stdout, "\n}\n\n");
  }
}

void Print_Variable(std::vector<DeclStruct const *> *global_helper_structs, std::vector<DeclStruct const *> *rpc_helper_structs, DeclVariable const *variable, int indent_level = 0)
{
    bool is_array              = variable->template_expr.len > 0;
    String const *var_type = &variable->type;
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
        DeclStruct const *resolved_decl = LookupTypeDefinition(global_helper_structs, rpc_helper_structs, *var_type);
        if (resolved_decl)
        {
          ++indent_level;
          for (DeclVariable const &inner_variable : resolved_decl->variables)
            Print_Variable(global_helper_structs, rpc_helper_structs, &inner_variable, indent_level);
        }
    }
}

void Print_BackslashEscapedString(FILE *file, String const *string, char char_to_escape)
{
  for (int i = 0; i < string->len; ++i)
  {
    char ch = string->str[i];
    if (ch == char_to_escape) fputc('\\', file);
    fputc(ch, file);
  }
}


void CollectChildrenStructs(std::vector<DeclStruct const *> *rpc_helper_structs, std::vector<DeclStruct> const *inner_structs)
{
  for (auto &inner_decl : *inner_structs)
  {
    CollectChildrenStructs(rpc_helper_structs, &inner_decl.inner_structs);
    if (inner_decl.type == DeclStructType::Helper)
        rpc_helper_structs->push_back(&inner_decl);
  }
}

DeclStruct *DeclContext_SearchStruct(DeclContext *context, String name)
{
    DeclStruct *result = nullptr;
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

DeclStruct *DeclStruct_SearchChildren(DeclStruct *src, String name)
{
    DeclStruct *result = nullptr;
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

DeclStruct *DeclContext_SearchStructParent(DeclContext *context, DeclStruct const *src)
{
    // NOTE: We have something of the sort,
    //
    //                         +---------- Inheritance Parent Name
    //                         V
    //     struct Foo : public Namespace::FooParent
    //

    DeclStruct *result = nullptr;
    if (src->inheritance_parent_name.len == 0)
        return result;

    Tokeniser tokeniser = {};
    tokeniser.ptr         = src->inheritance_parent_name.str_;

    Token namespace_token = {};
    if (!Tokeniser_RequireTokenType(&tokeniser, TokenType::identifier, &namespace_token))
    {
        Tokeniser_ErrorLastRequiredToken(&tokeniser);
        return result;
    }

    if (DeclStruct *namespace_struct = DeclContext_SearchStruct(context, Token_String(namespace_token)))
    {
        if (Tokeniser_RequireTokenType(&tokeniser, TokenType::namespace_colon))
        {
            Token namespace_child = {};
            if (!Tokeniser_RequireTokenType(&tokeniser, TokenType::identifier, &namespace_child))
            {
                Tokeniser_ErrorLastRequiredToken(&tokeniser);
                return result;
            }

            if (DeclStruct *child_struct = DeclStruct_SearchChildren(namespace_struct, Token_String(namespace_child)))
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
bool fprint_struct_variables(DeclContext *context,
                             std::vector<DeclStruct const *> *visible_helper_structs,
                             DeclStruct *src)
{
    // NOTE: Print parent
    if (response->inheritance_parent.len)
    {
        if (DeclStruct *parent = DeclContext_SearchStructParent(context, response))
        {
            for (DeclVariable const &variable : parent->variables)
                Print_Variable(&global_helper_structs, &rpc_helper_structs, &variable);
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
    for (DeclVariable const &variable : response->variables)
        Print_Variable(&global_helper_structs, &rpc_helper_structs, &variable);

    return true;
}
#endif

void generate_markdown(DeclContext *context)
{
    time_t now;
    time(&now);

    std::vector<DeclStruct> const *declarations       = &context->declarations;
    std::vector<DeclStruct> const *alias_declarations = &context->alias_declarations;
    struct tm *gmtime_result = gmtime(&now);
    fprintf(stdout,
           "# Introduction\n\n"
           "This is a list of the RPC calls, their inputs and outputs, and examples of each. This list is autogenerated and was last generated on: %d-%02d-%02d\n\n"
           "Many RPC calls use the JSON RPC interface while others use their own interfaces, as demonstrated below.\n\n"
           "Note: \"atomic units\" refer to the smallest fraction of 1 LOKI which is 1e9 atomic units.\n\n"
           "## RPC Methods\n\n",
           (1900 + gmtime_result->tm_year), (gmtime_result->tm_mon + 1), gmtime_result->tm_mday);

    for (DeclStruct const &decl : (*declarations))
    {
        if (decl.type == DeclStructType::RPCCommand)
        {
            fputs(" - [", stdout);
#if 0
            Print_BackslashEscapedString(stdout, &decl.name, '_');
#else
            fprintf(stdout, "%.*s", decl.name.len, decl.name.str);
#endif

            fputs("](#", stdout);
            for (int i = 0; i < decl.name.len; ++i)
            {
              char ch = Char_ToLower(decl.name.str[i]);
              fputc(ch, stdout);
            }
            fputs(")\n", stdout);
        }
    }
    fprintf(stdout, "\n\n");

    std::vector<DeclStruct const *> global_helper_structs;
    std::vector<DeclStruct const *> rpc_helper_structs;

    for (DeclStruct const &global_decl : (*declarations))
    {
        if (global_decl.type == DeclStructType::Invalid)
        {
            assert(!"invalid?");
            continue;
        }

        if (global_decl.type == DeclStructType::Helper)
        {
            global_helper_structs.push_back(&global_decl);
            continue;
        }

        if (global_decl.type != DeclStructType::RPCCommand)
            continue;

        DeclStruct const *request  = nullptr;
        DeclStruct const *response = nullptr;
        rpc_helper_structs.clear();
        CollectChildrenStructs(&rpc_helper_structs, &global_decl.inner_structs);
        for (auto &inner_decl : global_decl.inner_structs)
        {
            switch(inner_decl.type)
            {
                case DeclStructType::RPCRequest: request = &inner_decl; break;
                case DeclStructType::RPCResponse: response = &inner_decl; break;
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
        Print_BackslashEscapedString(stdout, &global_decl.name, '_');
#else
        fprintf(stdout, "%.*s", global_decl.name.len, global_decl.name.str);
#endif
        fputs("\n\n", stdout);

        if (global_decl.aliases.size() > 0 || global_decl.pre_decl_comments.size() || global_decl.description.len > 0)
        {
            fprintf(stdout, "```\n");
            if (global_decl.pre_decl_comments.size() > 0)
            {
              for (String const &comment : global_decl.pre_decl_comments)
                fprintf(stdout, "%.*s\n", comment.len, comment.str);
            }
            if (global_decl.description.len > 0)
                fprintf(stdout, "%.*s\n", global_decl.description.len, global_decl.description.str);
            fprintf(stdout, "\n");

            if (global_decl.aliases.size() > 0)
            {
              fprintf(stdout, "Endpoints: ");

              String const *main_alias = &global_decl.aliases[0];
              for(int i = 0; i < global_decl.aliases.size(); i++)
              {
                String const &alias = global_decl.aliases[i];
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
                  DeclVariable const &constants = global_decl.variables[i];
                  Print_Variable(&global_helper_structs, &rpc_helper_structs, &constants);
                }
              }

              if (global_decl.pre_decl_comments.size() > 0 || global_decl.description.len > 0)
                fprintf(stdout, "\n");

              DeclStructHierarchy hierarchy                  = {};
              hierarchy.children[hierarchy.children_index++] = &global_decl;
              Print_CurlExample(&global_helper_structs, &rpc_helper_structs, &hierarchy, request, response, *main_alias);
            }

            fprintf(stdout, "```\n");
        }


        fprintf(stdout, "**Inputs:**\n");
        fprintf(stdout, "\n");
        if (request->inheritance_parent_name.len) // @TODO(doyle): Recursive inheritance
        {
            if (DeclStruct *parent = DeclContext_SearchStructParent(context, request))
            {
                for (DeclVariable const &variable : parent->variables)
                    Print_Variable(&global_helper_structs, &rpc_helper_structs, &variable);
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
        for (DeclVariable const &variable : request->variables)
            Print_Variable(&global_helper_structs, &rpc_helper_structs, &variable);
        fprintf(stdout, "\n");

        fprintf(stdout, "**Outputs:**\n");
        fprintf(stdout, "\n");
        if (response->inheritance_parent_name.len)
        {
            if (DeclStruct *parent = DeclContext_SearchStructParent(context, response))
            {
                for (DeclVariable const &variable : parent->variables)
                    Print_Variable(&global_helper_structs, &rpc_helper_structs, &variable);
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
        for (DeclVariable const &variable : response->variables)
            Print_Variable(&global_helper_structs, &rpc_helper_structs, &variable);
        fprintf(stdout, "\n\n");
    }

    fprintf(stdout, "\n");
}

int main(int argc, char *argv[])
{
    DeclContext context = {};
    context.declarations.reserve(1024);
    context.alias_declarations.reserve(128);

    for (int arg_index = 1; arg_index < argc; arg_index++)
    {
        ptrdiff_t buf_size = 0;
        char *buf          = File_ReadAll(argv[arg_index], &buf_size);

        Tokeniser tokeniser = {};
        tokeniser.ptr         = buf;
        tokeniser.file        = argv[arg_index];

        while (Tokeniser_NextMarker(&tokeniser, STRING_LIT("LOKI_RPC_DOC_INTROSPECT")))
        {
            DeclStruct decl = {};
            for (Token comment = {}; Tokeniser_RequireTokenType(&tokeniser, TokenType::comment, &comment);)
            {
                String comment_lit = Token_String(comment);
                decl.pre_decl_comments.push_back(comment_lit);
            }

            Token peek = Tokeniser_PeekToken(&tokeniser);
            if (peek.type == TokenType::identifier)
            {
                String peek_lit = Token_String(peek);
                if (peek_lit ==  STRING_LIT("struct") || peek_lit == STRING_LIT("class"))
                {
                    if (Tokeniser_ParseStruct(&tokeniser, &decl, true /*root_struct*/, nullptr /*parent name*/))
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
    // NOTE: Iterate all global structs and infer them as a RPCCommand, if
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

                Tokeniser tokeniser = {};
                tokeniser.ptr         = variable.name.str_;

                Token name = Tokeniser_NextToken(&tokeniser);
                if (!Tokeniser_RequireTokenType(&tokeniser, TokenType::equal))
                {
                    Tokeniser_ErrorLastRequiredToken(&tokeniser);
                    return false;
                }

                Token namespace_token = {};
                if (!Tokeniser_RequireTokenType(&tokeniser, TokenType::identifier, &namespace_token))
                {
                    Tokeniser_ErrorLastRequiredToken(&tokeniser);
                    return false;
                }

                String namespace_lit = Token_String(namespace_token);
                if (namespace_lit == STRING_LIT("std"))
                {
                    // NOTE: Handle the standard library specially by creating
                    // "fake" structs that contain 1 member with the standard
                    // lib container

                    context.alias_declarations.emplace_back();
                    DeclStruct &alias_struct = context.alias_declarations.back();
                    variable.aliases_to       = &alias_struct; // Patch the alias pointer

                    alias_struct.type = DeclStructType::Helper;
                    alias_struct.name = Token_String(name);

                    DeclVariable variable_no_alias = variable;
                    variable_no_alias.aliases_to   = nullptr;
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
                            if (!Tokeniser_RequireTokenType(&tokeniser, TokenType::namespace_colon))
                            {
                                Tokeniser_ErrorLastRequiredToken(&tokeniser);
                                return false;
                            }

                            Token namespace_child = {};
                            if (!Tokeniser_RequireTokenType(&tokeniser, TokenType::identifier, &namespace_child))
                            {
                                Tokeniser_ErrorLastRequiredToken(&tokeniser);
                                return false;
                            }

                            for (auto &inner_struct : other_struct.inner_structs)
                            {
                                if (inner_struct.name == Token_String(namespace_child))
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
                                String eol_string = String_TillEOL(variable.name.str);
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
            decl.type = DeclStructType::RPCCommand;
        }
    }

    std::sort(context.declarations.begin(), context.declarations.end(), [](auto const &lhs, auto const &rhs) {
        return std::lexicographical_compare(
            lhs.name.str, lhs.name.str + lhs.name.len, rhs.name.str, rhs.name.str + rhs.name.len);
    });

    generate_markdown(&context);
    return 0;
}
