// Runs the logic of tools/cell-editor.html (its "cells-core" script) in node.
// Usage: node cell_editor.mjs EDITOR.html IN.hex OUT.hex N=V ...   writes OUT like the editor
//        node cell_editor.mjs EDITOR.html --meta HEX                prints min,max,default per Cell
//                                                                   for the firmware version in HEX
//        node cell_editor.mjs EDITOR.html --version HEX             prints the firmware version in HEX
//        node cell_editor.mjs EDITOR.html --count                   Cells shown for some versions
import { readFileSync, writeFileSync } from "node:fs";
const [html, inp, out, ...sets] = process.argv.slice(2);
const core = readFileSync(html, "utf8").match(/<script id="cells-core">([\s\S]*?)<\/script>/)[1];
const api = new Function(core + "; return { CELLS, parseHex, checkLayout, readCells, writeCells, hexText, fromBcd, fwVersion, defaultsFor, cellCount };")();
if (inp === "--version") {
  console.log(api.fwVersion(api.parseHex(readFileSync(out, "latin1"))));
  process.exit(0);
}
if (inp === "--count") {
  console.log(["0.9.5", "1.0.0", "1.1.0", "2.0.0"].map(api.cellCount).join(" ") + " " + api.cellCount(null));
  process.exit(0);
}
if (inp === "--meta") {
  const def = api.defaultsFor(api.fwVersion(api.parseHex(readFileSync(out, "latin1"))));
  console.log(api.CELLS.map((c, i) => `${c[2]},${c[3]},${def[i]}`).join(" "));
  process.exit(0);
}
const h = api.parseHex(readFileSync(inp, "latin1"));
const err = api.checkLayout(h);
if (err) { console.error(err); process.exit(1); }
const v = api.readCells(h).map((raw, i) => api.fromBcd(raw, i));
for (const s of sets) { const [n, val] = s.split("=").map(Number); v[n - 1] = val; }
api.writeCells(h, v);
writeFileSync(out, api.hexText(h), "latin1");
