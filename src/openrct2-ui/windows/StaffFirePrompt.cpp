/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Window.h>
#include <openrct2/Game.h>
#include <openrct2/actions/StaffFireAction.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/interface/Colour.h>
#include <openrct2/localisation/Localisation.h>
#include <openrct2/world/Entity.h>

static constexpr const rct_string_id WINDOW_TITLE = STR_SACK_STAFF;
static constexpr const int32_t WW = 200;
static constexpr const int32_t WH = 100;

// clang-format off
enum WINDOW_STAFF_FIRE_WIDGET_IDX {
    WIDX_BACKGROUND,
    WIDX_TITLE,
    WIDX_CLOSE,
    WIDX_YES,
    WIDX_CANCEL
};

// 0x9AFB4C
static rct_widget window_staff_fire_widgets[] = {
    WINDOW_SHIM_WHITE(WINDOW_TITLE, WW, WH),
    MakeWidget({     10, WH - 20}, {85, 14}, WindowWidgetType::Button, WindowColour::Primary, STR_YES               ),
    MakeWidget({WW - 95, WH - 20}, {85, 14}, WindowWidgetType::Button, WindowColour::Primary, STR_SAVE_PROMPT_CANCEL),
    WIDGETS_END,
};

static void window_staff_fire_mouseup(rct_window *w, rct_widgetindex widgetIndex);
static void window_staff_fire_paint(rct_window *w, rct_drawpixelinfo *dpi);

//0x9A3F7C
static rct_window_event_list window_staff_fire_events([](auto& events)
{
    events.mouse_up = &window_staff_fire_mouseup;
    events.paint = &window_staff_fire_paint;
});
// clang-format on

/** Based off of rct2: 0x6C0A77 */
rct_window* window_staff_fire_prompt_open(Peep* peep)
{
    rct_window* w;

    // Check if the confirm window already exists.
    w = window_bring_to_front_by_number(WC_FIRE_PROMPT, peep->sprite_index);
    if (w != nullptr)
    {
        return w;
    }

    w = WindowCreateCentred(WW, WH, &window_staff_fire_events, WC_FIRE_PROMPT, WF_TRANSPARENT);
    w->widgets = window_staff_fire_widgets;
    w->enabled_widgets |= (1ULL << WIDX_CLOSE) | (1ULL << WIDX_YES) | (1ULL << WIDX_CANCEL);

    WindowInitScrollWidgets(w);

    w->number = peep->sprite_index;

    return w;
}

/**
 *
 *  rct2: 0x006C0B40
 */
static void window_staff_fire_mouseup(rct_window* w, rct_widgetindex widgetIndex)
{
    switch (widgetIndex)
    {
        case WIDX_YES:
        {
            auto staffFireAction = StaffFireAction(w->number);
            GameActions::Execute(&staffFireAction);
            break;
        }
        case WIDX_CANCEL:
        case WIDX_CLOSE:
            window_close(w);
    }
}

/**
 *
 *  rct2: 0x006C0AF2
 */
static void window_staff_fire_paint(rct_window* w, rct_drawpixelinfo* dpi)
{
    WindowDrawWidgets(w, dpi);

    Peep* peep = GetEntity<Staff>(w->number);
    auto ft = Formatter();
    peep->FormatNameTo(ft);

    ScreenCoordsXY stringCoords(w->windowPos.x + WW / 2, w->windowPos.y + (WH / 2) - 3);
    DrawTextWrapped(dpi, stringCoords, WW - 4, STR_FIRE_STAFF_ID, ft, { TextAlignment::CENTRE });
}
