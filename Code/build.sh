./build_gcc.sh

echo "title: Loki.network Documentation | Loki and Monero Daemon RPC Guide" > DaemonRPCBeta.md
echo "description: This is a list of the daemon RPC calls, their inputs and outputs, and examples of each automatically generated. This guide can be used by coins who share a codebase with Loki and Monero." >> DaemonRPCBeta.md
echo "" >> DaemonRPCBeta.md
../Bin/LokiRPCDocGenerator \
    subaddress_index.h \
    rpc_handler.h \
    service_node_list.h \
    verification_context.h \
    cryptonote_protocol_defs.h \
    core_rpc_server_commands_defs.h \
    >> DaemonRPCBeta.md

echo "title: Loki.network Documentation | Loki and Monero Wallet RPC Guide" > WalletRPCBeta.md
echo "description: This is a list of the wallet RPC calls, their inputs and outputs, and examples of each automatically updated. This guide can be used by coins who share a codebase with Loki and Monero." >> WalletRPCBeta.md
echo "" >> WalletRPCBeta.md
../Bin/LokiRPCDocGenerator \
    subaddress_index.h \
    rpc_handler.h \
    wallet2.h \
    wallet_rpc_server.h \
    wallet_rpc_server_commands_defs.h \
    >> WalletRPCBeta.md
