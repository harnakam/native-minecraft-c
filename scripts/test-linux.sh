#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build-linux
base=src/minecraft/net/minecraft
cc=${CC:-gcc}
for target in protocol world item nbt inventory crafting item_entity transfer; do
    "$cc" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$base" \
        "tests/test_$target.c" "$base/network/protocol.c" "$base/network/transport.c" \
        "$base/world/world.c" "$base/world/storage.c" "$base/nbt/nbt.c" \
        "$base/item/item.c" "$base/block/block.c" "$base/inventory/inventory.c" \
        "$base/crafting/crafting.c" "$base/entity/item/item_entity.c" "$base/util/transfer.c" \
        -lz -lm -o "build-linux/test-$target"
    "build-linux/test-$target"
done
"$cc" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$base" \
    "$base/server/server.c" "$base/network/protocol.c" "$base/network/transport.c" \
    "$base/world/world.c" "$base/world/storage.c" "$base/nbt/nbt.c" \
    "$base/item/item.c" "$base/block/block.c" "$base/inventory/inventory.c" \
    "$base/crafting/crafting.c" "$base/entity/item/item_entity.c" "$base/util/transfer.c" \
    -lz -lm -o build-linux/c919-server
C919_SERVER="$(pwd)/build-linux/c919-server" python3 tests/test_multiplayer.py
C919_SERVER="$(pwd)/build-linux/c919-server" python3 tests/test_inventory_network.py
C919_SERVER="$(pwd)/build-linux/c919-server" python3 tests/test_drops_crafting_network.py
python3 tests/test_public_audit.py
"$cc" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$base" \
    "$base/client/client.c" "$base/client/renderer.c" \
    "$base/network/protocol.c" "$base/network/transport.c" \
    "$base/world/world.c" "$base/world/storage.c" "$base/nbt/nbt.c" \
    "$base/item/item.c" "$base/block/block.c" "$base/inventory/inventory.c" \
    "$base/crafting/crafting.c" "$base/entity/item/item_entity.c" "$base/util/transfer.c" \
    -lz -lm -o build-linux/c919-client
build-linux/c919-client --self-test
C919_SERVER="$(pwd)/build-linux/c919-server" C919_CLIENT="$(pwd)/build-linux/c919-client" python3 tests/test_client.py
C919_SERVER="$(pwd)/build-linux/c919-server" C919_CLIENT="$(pwd)/build-linux/c919-client" python3 tests/test_client_inventory.py
C919_SERVER="$(pwd)/build-linux/c919-server" C919_CLIENT="$(pwd)/build-linux/c919-client" python3 tests/test_client_entities.py
