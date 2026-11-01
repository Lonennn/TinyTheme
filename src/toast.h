#pragma once

#include <windows.h>

// v0.1 只用托盘气泡，不做 24 小时去重（F-09 完整版留 v0.2）。
// 是否需要显示由调用方（App）根据配置与业务规则决定。
namespace toast
{

[[nodiscard]] bool supported();

void show(HINSTANCE instance, UINT title_id, UINT text_id);

} // namespace toast
