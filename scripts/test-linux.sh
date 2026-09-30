#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build-linux
base=src/minecraft/net/minecraft
cc=${CC:-gcc}
for target in protocol world; do
    "$cc" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$base" \
        "tests/test_$target.c" "$base/network/protocol.c" "$base/network/transport.c" \
        "$base/world/world.c" "$base/world/storage.c" -lz -lm -o "build-linux/test-$target"
    "build-linux/test-$target"
done
"$cc" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$base" \
    "$base/server/server.c" "$base/network/protocol.c" "$base/network/transport.c" \
    "$base/world/world.c" "$base/world/storage.c" -lz -lm -o build-linux/c919-server
C919_SERVER="$(pwd)/build-linux/c919-server" python3 tests/test_multiplayer.py
python3 tests/test_public_audit.py
