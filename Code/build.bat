@echo OFF
call build_msvc.bat

set daemon_files=%daemon_files% subaddress_index.h
set daemon_files=%daemon_files% rpc_handler.h
set daemon_files=%daemon_files% service_node_list.h
set daemon_files=%daemon_files% verification_context.h
set daemon_files=%daemon_files% cryptonote_protocol_defs.h
set daemon_files=%daemon_files% core_rpc_server_commands_defs.h

set wallet_files=%wallet_files% subaddress_index.h
set wallet_files=%wallet_files% transfer_view.h
set wallet_files=%wallet_files% transfer_destination.h
set wallet_files=%wallet_files% rpc_handler.h
set wallet_files=%wallet_files% wallet2.h
set wallet_files=%wallet_files% wallet_rpc_server.h
set wallet_files=%wallet_files% wallet_rpc_server_commands_defs.h

REM echo title: Loki.network Documentation ^| Loki and Monero Daemon RPC Guide > DaemonRPCBeta.md
REM echo description: This is a list of the daemon RPC calls, their inputs and outputs, and examples of each automatically generated. This guide can be used by coins who share a codebase with Loki and Monero. >> DaemonRPCBeta.md
REM echo.>> DaemonRPCBeta.md
REM ..\Bin\LokiRPCDocGenerator.exe %daemon_files% >> DaemonRPCBeta.md

echo title: Loki.network Documentation ^| Loki and Monero Wallet RPC Guide > WalletRPCBeta.md
echo description: This is a list of the wallet RPC calls, their inputs and outputs, and examples of each automatically updated. This guide can be used by coins who share a codebase with Loki and Monero. >> WalletRPCBeta.md
echo.>> WalletRPCBeta.md
..\Bin\LokiRPCDocGenerator.exe %wallet_files% >> WalletRPCBeta.md
