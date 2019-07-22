if (( $# != 1 )); then
  echo "Please enter the root directory of Loki so we can copy over the source files to generate docs from"
  exit
fi;

source_files=("${1}/src/cryptonote_protocol/cryptonote_protocol_defs.h" \
  "${1}/src/rpc/core_rpc_server.h" \
  "${1}/src/rpc/core_rpc_server_commands_defs.h" \
  "${1}/src/wallet/wallet_rpc_server.h" \
  "${1}/src/wallet/wallet_rpc_server_commands_defs.h" \
  )

for file in ${source_files[@]};
do
  cp -f $file .
done
