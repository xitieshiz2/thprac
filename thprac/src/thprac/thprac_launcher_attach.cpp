#include "thprac_launcher_attach.h"
#include "thprac_launcher_utils.h"
#include "thprac_launcher_games.h"
#include "thprac_launcher_main.h"
#include "thprac_main.h"
#include "thprac_gui_locale.h"
#include "thprac_utils.h"
#include "utils/utils.h"
#include <functional>
#include <string>
#include <vector>

namespace THPrac {

class THAttachGui {
private:
    THAttachGui()
    {
        mGuiUpdFunc = [&]() { return GuiContent(); };
    }
    SINGLETON(THAttachGui);

public:
    void GuiUpdate()
    {
        GuiMain();
    }

private:
    bool GuiContent()
    {
        if(ImGui::BeginTable("##attach_processes", 3)) {
            for (int row = 0; row < 4; row++) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Row %d", row);
                ImGui::TableNextColumn();
                ImGui::Text("Some contents");
                ImGui::TableNextColumn();
                ImGui::Text("123.456");
            }
            ImGui::EndTable();
        }
        return true;
    }
    void GuiMain()
    {
        if (!mGuiUpdFunc()) {
            mGuiUpdFunc = [&]() { return GuiContent(); };
        }
    }

    std::function<bool(void)> mGuiUpdFunc = []() { return true; };
};

bool LauncherAttachGuiUpd()
{
    THAttachGui::singleton().GuiUpdate();
    return true;
}

}
