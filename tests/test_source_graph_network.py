"""Real TCP source-alias and durable multi-owner contracts, with independent NBT."""
import contextlib
import gzip
from pathlib import Path
import struct
import tempfile
import unittest

from test_drops_crafting_network import item_snapshot, move
from test_inventory_network import (InventoryPeer, compound, inventory_payload, item_metadata,
                                    named, nbt_string, nbt_values, parse_slot, player_file,
                                    wire_slot)
from test_multiplayer import read_vint, running_server


class SurvivalPeer(InventoryPeer):
    expected_gamemode = 0


def book_tag(generation):
    return compound(named(3, "generation", struct.pack(">i", generation)),
                    named(8, "title", nbt_string("共有参照")),
                    named(8, "author", nbt_string("AliasOwner")),
                    named(9, "pages", b"\x08" + struct.pack(">i", 1) + nbt_string("本文\0\ud800")))


def stack_entry(index, item, count, tag=None):
    fields = [named(1, "Slot", struct.pack(">b", index)),
              named(8, "id", nbt_string("minecraft:" + item)),
              named(1, "Count", struct.pack(">b", count)),
              named(2, "Damage", b"\0\0")]
    if tag is not None:
        fields.append(named(10, "tag", tag))
    return compound(*fields)


def seed_book_grid(world, name):
    # This is the documented native recovery NBT format, read through the real
    # Storage -> InventoryPlayer/InventoryCrafting paths, not graph injection.
    inventory = b"\x0a" + struct.pack(">i", 36) + b"".join(
        stack_entry(index, "stone", 64) for index in range(36))
    grid = b"\x0a" + struct.pack(">i", 2) + stack_entry(1, "written_book", 2, book_tag(0)) + \
        stack_entry(2, "writable_book", 1)
    root = named(10, "SourceAlias", compound(named(9, "Inventory", inventory),
                                          named(9, "C919Crafting", grid),
                                          named(3, "SelectedItemSlot", b"\0" * 4)))
    path = player_file(world, name)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(gzip.compress(root))


class SourceGraphNetworkTests(unittest.TestCase):
    def test_book_remainder_pickup_changes_other_players_exact_grid_reference(self):
        with tempfile.TemporaryDirectory(prefix="c919-source-alias-") as directory:
            world = Path(directory) / "world.c919"
            seed_book_grid(world, "AliasOwner")
            with contextlib.ExitStack() as cleanup, running_server(world, crash=True, gamemode=0) as port:
                owner, picker = SurvivalPeer(port), SurvivalPeer(port)
                cleanup.callback(owner.close)
                cleanup.callback(picker.close)
                owner.login("AliasOwner")
                picker.login("AliasPicker")
                x, y, z = owner.spawn[:3]
                move(picker, x + 8, y, z)
                move(owner, x, y, z, pitch=90)
                slots = inventory_payload(next(p for k, p in owner.initial if k == 0x30))
                self.assertEqual(slots[1][:3], (387, 2, 0))
                self.assertEqual(slots[0][:3], (387, 1, 0))
                result = wire_slot(387, 1, 0, named(10, "", book_tag(1)))
                slots, cursor = owner.click(0, 0, 1, 0, result)
                self.assertEqual(cursor[:3], (387, 1, 0))
                # getRemainingItems retains the input reference; decr makes
                # it count1; full InventoryPlayer cannot insert it; dropItem
                # passes that SAME reference to the new EntityItem watcher.
                self.assertEqual(slots[1][:3], (387, 1, 0))
                metadata = next((p for k, p in owner.observed if k == 0x1c and item_metadata(p) is not None), None)
                if metadata is None:
                    metadata = owner.wait(0x1c, lambda p: item_metadata(p) is not None)
                entity, dropped = item_metadata(metadata)
                self.assertEqual(dropped[:3], (387, 1, 0))
                move(owner, x + 8, y, z)
                move(picker, x, y - 1, z)
                picker.wait(0x0d, lambda p: read_vint(p)[0] == entity)
                # The pickup mutates a shared stack through another player.
                # Source detectAndSendChanges must report non-NULL count0,
                # without an owned DTO clearing or copying the first grid.
                changed = owner.wait(0x2f, lambda p: struct.unpack_from(">bh", p) == (0, 1) and
                                     parse_slot(p, 3)[0][:2] == (387, 0))
                self.assertEqual(parse_slot(changed, 3)[0][:3], (387, 0, 0))
                taken, _ = picker.snapshot()
                self.assertEqual(sum(s[1] for s in taken if s[0] == 387), 1)
                saved_owner = nbt_values(gzip.decompress(player_file(world, "AliasOwner").read_bytes()))
                saved_picker = nbt_values(gzip.decompress(player_file(world, "AliasPicker").read_bytes()))
                self.assertEqual(next(s for s in saved_owner["C919Crafting"] if s["Slot"] == 1)["Count"], 0)
                self.assertEqual(sum(s["Count"] for s in saved_picker["Inventory"]
                                     if s["id"] == "minecraft:written_book"), 1)
                self.assertEqual(item_snapshot(world), [])
                self.assertFalse(Path(str(world) + ".transfer.dat").exists())
            with running_server(world, gamemode=0) as port:
                restored = SurvivalPeer(port)
                try:
                    restored.login("AliasPicker")
                    slots = inventory_payload(next(p for k, p in restored.initial if k == 0x30))
                    books = [s for s in slots if s[0] == 387]
                    self.assertEqual(sum(s[1] for s in books), 1)
                    self.assertEqual(nbt_values(books[0][3])["generation"], 0)
                    self.assertEqual(item_snapshot(world), [])
                finally:
                    restored.close()


if __name__ == "__main__":
    unittest.main()
