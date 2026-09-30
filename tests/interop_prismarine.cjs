/* Optional independent protocol interoperability test. No production dependency. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const net = require('node:net');
const { spawn } = require('node:child_process');
const root = path.resolve(__dirname, '..');
const mc = require(path.join(root, '.local/interop/node_modules/minecraft-protocol'));
const executable = process.env.C919_SERVER || path.join(root, 'build', 'c919-server.exe');
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
function until(predicate, description, timeout = 6000) {
  return new Promise((resolve, reject) => {
    const started = Date.now();
    const timer = setInterval(() => {
      if (predicate()) { clearInterval(timer); resolve(); }
      else if (Date.now() - started > timeout) { clearInterval(timer); reject(new Error(description)); }
    }, 10);
  });
}
async function freePort() {
  const listener = net.createServer();
  await new Promise(resolve => listener.listen(0, '127.0.0.1', resolve));
  const port = listener.address().port;
  await new Promise(resolve => listener.close(resolve));
  return port;
}
function connect(port, username, errors) {
  const state = { chunks: [], chats: [], moves: [], changes: [], slots: [], windows: [], opens: [], transactions: [], equipment: [], objects: [], metadata: [], collects: [], destroys: [], spawn: null, ended: false };
  const client = mc.createClient({ host: '127.0.0.1', port, username, auth: 'offline', version: '1.8.9' });
  client.on('error', error => { if (!state.ended) errors.push(error); });
  client.on('disconnect', packet => errors.push(new Error(JSON.stringify(packet))));
  client.on('map_chunk', packet => state.chunks.push(packet));
  client.on('position', packet => {
    state.spawn = packet;
    client.write('position_look', { x: packet.x, y: packet.y, z: packet.z, yaw: packet.yaw, pitch: packet.pitch, onGround: false });
  });
  client.on('chat', packet => state.chats.push(packet.message));
  client.on('entity_teleport', packet => state.moves.push(packet));
  client.on('block_change', packet => state.changes.push(packet));
  client.on('set_slot', packet => state.slots.push(packet));
  client.on('window_items', packet => state.windows.push(packet));
  client.on('open_window', packet => state.opens.push(packet));
  client.on('transaction', packet => state.transactions.push(packet));
  client.on('entity_equipment', packet => state.equipment.push(packet));
  client.on('spawn_entity', packet => state.objects.push(packet));
  client.on('entity_metadata', packet => state.metadata.push(packet));
  client.on('collect', packet => state.collects.push(packet));
  client.on('entity_destroy', packet => state.destroys.push(packet));
  state.client = client;
  return state;
}
async function main() {
  fs.mkdirSync(path.join(root, '.local/interop'), { recursive: true });
  const temporary = fs.mkdtempSync(path.join(root, '.local/interop/session-'));
  const world = path.join(temporary, 'interop-test.c919');
  const port = await freePort();
  const server = spawn(executable, ['--port', String(port), '--world', world, '--run-seconds', '20'], { windowsHide: true });
  let log = '';
  const errors = [];
  const clients = [];
  server.stdout.on('data', data => { log += data; });
  server.stderr.on('data', data => { log += data; });
  server.on('error', error => errors.push(error));
  try {
    await until(() => log.includes('listening'), 'C server did not listen: ' + log);
    const a = connect(port, 'PrismarineOne', errors); clients.push(a);
    await until(() => a.spawn && a.chunks.length === 25, 'independent client did not receive 25 chunks');
    const b = connect(port, 'PrismarineTwo', errors); clients.push(b);
    await until(() => b.spawn && b.chunks.length === 25, 'second independent client did not join');
    const tag = { type: 'compound', name: '', value: {
      display: { type: 'compound', value: { Name: { type: 'string', value: '日本語の剣' } } },
      ench: { type: 'list', value: { type: 'compound', value: [{
        id: { type: 'short', value: 16 }, lvl: { type: 'short', value: 5 }
      }] } },
      C919Ints: { type: 'intArray', value: [1, -2, 3] }
    } };
    const sword = { blockId: 276, itemCount: 1, itemDamage: 7, nbtData: tag };
    a.client.write('set_creative_slot', { slot: 36, item: sword });
    await until(() => a.slots.some(packet => packet.slot === 36 && packet.item.blockId === 276), 'NBT creative slot not synchronized');
    assert.deepEqual(a.slots.find(packet => packet.slot === 36 && packet.item.blockId === 276).item, sword);
    await until(() => b.equipment.some(packet => packet.slot === 0 && packet.item.blockId === 276), 'NBT equipment not synchronized');
    assert.deepEqual(b.equipment.find(packet => packet.slot === 0 && packet.item.blockId === 276).item, sword);
    const planks = { blockId: 5, itemCount: 31, itemDamage: 2, nbtData: tag };
    a.client.write('set_creative_slot', { slot: 9, item: planks });
    await until(() => a.slots.some(packet => packet.slot === 9 && packet.item.itemCount === 31), 'main inventory not synchronized');
    a.client.write('window_click', { windowId: 0, slot: 9, mouseButton: 1, action: 1, mode: 0, item: planks });
    await until(() => a.transactions.some(packet => packet.action === 1), 'click confirmation missing');
    assert.equal(a.transactions.find(packet => packet.action === 1).accepted, true);
    await until(() => a.slots.some(packet => packet.windowId === -1 && packet.item.itemCount === 16), 'split cursor not synchronized');
    await until(() => a.windows.some(packet => packet.items[9].itemCount === 15), 'split source not synchronized');
    assert.deepEqual(a.windows.find(packet => packet.items[9].itemCount === 15).items[9].nbtData, tag);
    assert.equal(a.chunks[0].chunkData.length, 256 + 12288 * a.chunks[0].bitMap.toString(2).replace(/0/g, '').length);
    a.client.write('chat', { message: '独立実装からのマルチプレイ検証' });
    await until(() => a.chats.length && b.chats.length, 'chat was not broadcast');
    assert.match(b.chats.at(-1), /独立実装/);
    a.client.write('position_look', { x: a.spawn.x + 0.25, y: a.spawn.y, z: a.spawn.z, yaw: 30, pitch: 0, onGround: false });
    await until(() => b.moves.length, 'movement was not synchronized');
    const ground = Math.floor(a.spawn.y) - 2;
    a.client.write('block_dig', { status: 0, location: { x: 8, y: ground, z: 8 }, face: 1 });
    await until(() => b.changes.length, 'block dig was not synchronized');
    assert.equal(b.changes.at(-1).type, 0);
    b.client.write('position_look', { x: a.spawn.x + 8, y: a.spawn.y, z: a.spawn.z, yaw: 0, pitch: 0, onGround: false });
    a.client.write('position_look', { x: a.spawn.x, y: a.spawn.y, z: a.spawn.z, yaw: 0, pitch: 90, onGround: false });
    a.client.write('close_window', { windowId: 0 });
    const droppedSlot = packet => packet.metadata.find(entry => entry.key === 10 && entry.type === 5)?.value;
    await until(() => a.metadata.some(packet => droppedSlot(packet)?.blockId === 5 && droppedSlot(packet).itemCount === 16), 'closing cursor did not create an item');
    const cursorDrop = a.metadata.find(packet => droppedSlot(packet)?.blockId === 5 && droppedSlot(packet).itemCount === 16);
    assert.deepEqual(droppedSlot(cursorDrop), { ...planks, itemCount: 16 });
    a.client.write('set_creative_slot', { slot: 1, item: { blockId: 17, itemCount: 1, itemDamage: 0 } });
    await until(() => a.slots.some(packet => packet.windowId === 0 && packet.slot === 0 && packet.item.blockId === 5 && packet.item.itemCount === 4), '2x2 crafting result missing');
    a.client.write('window_click', { windowId: 0, slot: 0, mouseButton: 0, action: 2, mode: 4, item: { blockId: -1 } });
    await until(() => a.transactions.some(packet => packet.action === 2), 'craft output drop confirmation missing');
    assert.equal(a.transactions.find(packet => packet.action === 2).accepted, true);
    await until(() => a.metadata.some(packet => droppedSlot(packet)?.blockId === 5 && droppedSlot(packet).itemCount === 4), 'crafted output did not become a world item');
    const crafted = a.metadata.find(packet => droppedSlot(packet)?.blockId === 5 && droppedSlot(packet).itemCount === 4);
    assert.deepEqual(droppedSlot(crafted), { blockId: 5, itemCount: 4, itemDamage: 0, nbtData: undefined });
    a.client.write('block_dig', { status: 4, location: { x: 0, y: 0, z: 0 }, face: 0 });
    await until(() => b.metadata.some(packet => droppedSlot(packet)?.blockId === 276), 'NBT Q drop did not reach another client');
    const swordDrop = b.metadata.find(packet => droppedSlot(packet)?.blockId === 276);
    assert.deepEqual(droppedSlot(swordDrop), sword);
    const object = b.objects.find(packet => packet.entityId === swordDrop.entityId);
    assert.equal(object.type, 2);
    assert.equal(object.objectData, 1);
    a.client.write('position_look', { x: a.spawn.x + 8, y: a.spawn.y, z: a.spawn.z, yaw: 0, pitch: 0, onGround: false });
    // The earlier digging assertion removed the block beneath this column.
    b.client.write('position_look', { x: a.spawn.x, y: a.spawn.y - 2, z: a.spawn.z, yaw: 0, pitch: 0, onGround: false });
    await until(() => b.collects.some(packet => packet.collectedEntityId === swordDrop.entityId), 'other player did not collect the NBT sword');
    await until(() => b.destroys.some(packet => packet.entityIds.includes(swordDrop.entityId)), 'collected item was not destroyed');
    assert.ok(b.windows.some(packet => packet.items.some(item => item.blockId === 276 && item.itemDamage === 7 && item.itemCount === 1 && item.nbtData?.value.display?.value.Name?.value === '日本語の剣')));
    a.client.write('position_look', { x: a.spawn.x, y: a.spawn.y, z: a.spawn.z, yaw: 0, pitch: 0, onGround: false });
    const table = { x: 8, y: Math.floor(a.spawn.y) - 1, z: 10 };
    a.client.write('set_creative_slot', { slot: 36, item: { blockId: 58, itemCount: 1, itemDamage: 0 } });
    await until(() => a.slots.some(packet => packet.slot === 36 && packet.item.blockId === 58), 'table item not supplied');
    a.client.write('block_place', { location: { ...table, y: table.y - 1 }, direction: 1, heldItem: { blockId: 58, itemCount: 1, itemDamage: 0 }, cursorX: 8, cursorY: 16, cursorZ: 8 });
    await until(() => a.changes.some(packet => packet.location.x === table.x && packet.location.y === table.y && packet.location.z === table.z && packet.type === (58 << 4)), 'real table placement missing');
    a.client.write('set_creative_slot', { slot: 36, item: { blockId: -1 } });
    a.client.write('set_creative_slot', { slot: 9, item: { blockId: 5, itemCount: 8, itemDamage: 0 } });
    a.client.write('block_place', { location: table, direction: 1, heldItem: { blockId: -1 }, cursorX: 8, cursorY: 8, cursorZ: 8 });
    await until(() => a.opens.length, 'empty-hand workbench activation missing');
    const window = a.opens.at(-1).windowId;
    assert.equal(a.opens.at(-1).inventoryType, 'minecraft:crafting_table');
    assert.equal(a.opens.at(-1).slotCount, 0);
    await until(() => a.windows.some(packet => packet.windowId === window && packet.items.length === 46), '46 mapped slots missing');
    let action = 10;
    async function tableClick(slot, button, item) {
      const current = action++;
      a.client.write('window_click', { windowId: window, slot, mouseButton: button, action: current, mode: 0, item });
      await until(() => a.transactions.some(packet => packet.windowId === window && packet.action === current), 'workbench confirmation missing');
      assert.equal(a.transactions.find(packet => packet.windowId === window && packet.action === current).accepted, true);
    }
    await tableClick(10, 0, { blockId: 5, itemCount: 8, itemDamage: 0 });
    for (const index of [1, 2, 3, 4, 6, 7, 8, 9]) await tableClick(index, 1, { blockId: -1 });
    await until(() => a.windows.some(packet => packet.windowId === window && packet.items[0].blockId === 54), '3x3 chest preview missing');
    await tableClick(0, 0, { blockId: 54, itemCount: 1, itemDamage: 0 });
    await until(() => a.slots.some(packet => packet.windowId === -1 && packet.item.blockId === 54), 'crafted chest cursor missing');
    a.client.write('close_window', { windowId: 255 });
    await until(() => a.metadata.some(packet => droppedSlot(packet)?.blockId === 54), 'workbench cursor close did not create a chest item');
    assert.equal(errors.length, 0, errors.map(String).join('\n'));
    console.log('Prismarine minecraft-protocol 1.68.0: two 1.8.9 clients, full NBT, drops/pickup, 2x2 and real 3x3 table crafting, 46 mapped slots, close drops, chunks/chat/movement/digging passed.');
  } finally {
    for (const peer of clients) { peer.ended = true; peer.client.end('test finished'); }
    await delay(100);
    server.kill();
    await new Promise(resolve => { if (server.exitCode !== null) resolve(); else server.once('exit', resolve); });
    const relative = path.relative(path.join(root, '.local/interop'), temporary);
    if (!relative || relative.startsWith('..') || path.isAbsolute(relative)) throw new Error('Invalid temporary session path');
    fs.rmSync(temporary, { recursive: true, force: true });
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
