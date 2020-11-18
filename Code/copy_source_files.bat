@echo OFF

if "%~1"=="" (
  echo Please enter the root directory of Loki so we can copy over the source files to generate docs from
  goto :eof
)

set script_location=%~dp0
set source_files=^
  "%1\src\cryptonote_protocol\cryptonote_protocol_defs.h" ^
  "%1\src\cryptonote_basic\subaddress_index.h" ^
  "%1\src\rpc\core_rpc_server_commands_defs.h" ^
  "%1\src\wallet\wallet2.h" ^
  "%1\src\wallet\transfer_destination.h" ^
  "%1\src\wallet\transfer_view.h" ^
  "%1\src\wallet\wallet_rpc_server.h" ^
  "%1\src\wallet\wallet_rpc_server_commands_defs.h" ^
  "%1\src\cryptonote_basic\verification_context.h" ^
  "%1\src\cryptonote_core\service_node_list.h" ^
  "%1\src\rpc\rpc_handler.h"

(for %%a in (%source_files%) do (
  echo Copy %%a to %script_location%
  copy /Y %%a %script_location% 1>nul
))
