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
  const state = { chunks: [], chats: [], moves: [], changes: [], spawn: null, ended: false };
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
  state.client = client;
  return state;
}
async function main() {
  fs.mkdirSync(path.join(root, '.local/interop'), { recursive: true });
  const world = path.join(root, '.local/interop/interop-test.c919');
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
    assert.equal(errors.length, 0, errors.map(String).join('\n'));
    console.log('Prismarine minecraft-protocol 1.68.0: two 1.8.9 clients, 25 chunks each, Japanese chat, movement and digging passed.');
  } finally {
    for (const peer of clients) { peer.ended = true; peer.client.end('test finished'); }
    await delay(100);
    server.kill();
    await new Promise(resolve => { if (server.exitCode !== null) resolve(); else server.once('exit', resolve); });
    fs.rmSync(world, { force: true });
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
