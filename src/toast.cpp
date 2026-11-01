#include "toast.h"

#include "log.h"
#include "tray.h"

namespace toast
{

bool supported()
{
    return true;
}

void show(HINSTANCE instance, UINT title_id, UINT text_id)
{
    logger::info(L"显示气泡提示");
    tray::show_balloon(instance, title_id, text_id);
}

} // namespace toast
