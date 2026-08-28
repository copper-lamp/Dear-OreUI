// M1 资产生成器：把 assets/*.js 嵌入成 C++ 头文件（内置资源层）。
// 用法：node scripts/gen_assets.mjs
// 产出：src/generated/BuiltinAssets.gen.h
// `assets/*.js` 是规范来源（.js 单一来源，生成头文件由本脚本从它们字节级导出）。
import { readFileSync, readdirSync, writeFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const assetsDir = join(root, "assets");
const outHeader = join(root, "src", "generated", "BuiltinAssets.gen.h");
const DELIM = "DOA";

function cxxName(file) {
    const base = file.replace(/\.js$/i, "");
    return "k" + base.split(/[-_]/).map((p) => p.charAt(0).toUpperCase() + p.slice(1)).join("");
}

const files = readdirSync(assetsDir).filter((f) => f.endsWith(".js")).sort();
const entries = files.map((f) => {
    const content = readFileSync(join(assetsDir, f), "utf8");
    if (content.includes(`)${DELIM}`)) {
        throw new Error(`assets/${f} 含 ` + `)` + DELIM + `，请更换 DELIM`);
    }
    return { name: cxxName(f), raw: content };
});

const lines = [
    "// 本文件由 scripts/gen_assets.mjs 自动生成，请勿手改。",
    "// 规范来源：assets/*.js（改 JS 后运行 node scripts/gen_assets.mjs 重新生成）。",
    "#pragma once",
    "#include <string_view>",
    "namespace dearoreui::asset {",
];
for (const e of entries) {
    lines.push(`inline constexpr std::string_view ${e.name} = R"${DELIM}(${e.raw})${DELIM}";`);
}
lines.push("} // namespace dearoreui::asset");
lines.push("");

writeFileSync(outHeader, lines.join("\n"));
console.log(`[gen_assets] wrote ${outHeader} (${entries.length} assets)`);
for (const e of entries) console.log(`  - ${e.name}: ${e.raw.length} bytes`);