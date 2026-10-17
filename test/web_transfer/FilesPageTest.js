// Run the actual FilesPage inline script with a minimal DOM. No npm dependencies.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

async function render(html, fixture) {
  const scripts = [...html.matchAll(/<script\b[^>]*>([\s\S]*?)<\/script>/gi)]
    .map(match => match[1]).filter(script => script.trim());
  assert.equal(scripts.length, 1, 'test the complete inline page script');
  assert.match(scripts[0], /\n\s*hydrate\(\);\s*$/);
  // Control the initial hydration so it can be awaited by the test.
  const script = scripts[0].replace(/\n\s*hydrate\(\);\s*$/, '\n');
  const listeners = {};
  const tableListeners = [];
  const elements = new Map(['file-table', 'directory-breadcrumbs', 'folder-summary'].map(id => [id, {
    innerHTML: '',
    addEventListener(type, handler) {
      assert.equal(type, 'click');
      tableListeners.push(handler);
    }
  }]));
  const document = {
    getElementById: id => elements.get(id) || null,
    querySelectorAll: () => [],
    addEventListener(type, handler) { (listeners[type] ||= []).push(handler); }
  };
  const context = vm.createContext({
    document,
    window: {location: {search: '?' + new URLSearchParams({path: fixture.path})}, addEventListener() {}},
    navigator: {onLine: true},
    URLSearchParams,
    console: {log() {}, error(...args) { throw new Error(args.join(' ')); }},
    setTimeout() {},
    fetch: async url => ({
      ok: true,
      json: async () => url === '/api/status' ? {version: 'test'} : fixture.files.map(file => ({...file}))
    })
  });
  vm.runInContext(script, context, {filename: 'FilesPage.html'});
  for (const handler of listeners.DOMContentLoaded || []) handler();
  assert.equal(tableListeners.length, 1, 'file actions must be delegated once');
  await context.hydrate();
  const markup = elements.get('file-table').innerHTML;
  await context.hydrate();
  assert.equal(tableListeners.length, 1, 're-hydration must not duplicate event handlers');

  const calls = [];
  for (const action of ['Move', 'Rename', 'Delete']) {
    context['open' + action + 'Modal'] = (...args) => calls.push({action: action.toLowerCase(), args});
  }
  for (const file of fixture.files) {
    const path = (fixture.path.endsWith('/') ? fixture.path : fixture.path + '/') + file.name;
    for (const action of file.isDirectory ? ['delete'] : ['move', 'rename', 'delete']) {
      const dataset = {name: file.name, path, action, folder: String(file.isDirectory)};
      tableListeners[0]({target: {closest: selector => {
        assert.equal(selector, 'button[data-action]');
        return {dataset};
      }}});
      const expected = [file.name, path];
      if (action === 'delete') expected.push(file.isDirectory);
      const call = calls.pop();
      assert.equal(call.action, action);
      assert.deepEqual(call.args, expected, 'modal arguments must stay literal text');
    }
  }
  tableListeners[0]({target: {closest: () => null}});
  assert.equal(calls.length, 0, 'unrelated clicks must be ignored');
  return {path: fixture.path, files: fixture.files, markup, breadcrumbs: elements.get('directory-breadcrumbs').innerHTML};
}

async function main() {
  const html = fs.readFileSync(process.argv[2], 'utf8');
  const fixtures = JSON.parse(fs.readFileSync(process.argv[3], 'utf8'));
  const results = [];
  for (const fixture of fixtures) results.push(await render(html, fixture));
  process.stdout.write(JSON.stringify(results));
}

main().catch(error => { console.error(error); process.exitCode = 1; });
