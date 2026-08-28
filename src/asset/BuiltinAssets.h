#pragma once

#include "generated/BuiltinAssets.gen.h"

#include <string_view>

// M1 资产化：运行时内置的 JS 资产（machinery）统一从资产层读取。
// `assets/*.js` 是规范来源，`scripts/gen_assets.mjs` 将其字节级嵌入本头；
// 真机注入、单测、离线预览（App）共用同一份资产，行为一致。
namespace dearoreui::asset {

// stage 8 runtime（JS<->C++ facet/ipc 通道 + window.oreui/DearOreUI 契约）。
[[nodiscard]] inline std::string_view stage5RuntimeJs() { return kStage5Runtime; }

// stage 7.1 UI 装载 machinery（buildDom / 状态机 / mount / append / unmount）。
[[nodiscard]] inline std::string_view stage7UiBootstrapJs() { return kStage7UiBootstrap; }

} // namespace dearoreui::asset