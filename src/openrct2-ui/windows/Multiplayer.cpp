/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <openrct2-ui/interface/Dropdown.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Window.h>
#include <openrct2/Game.h>
#include <openrct2/actions/NetworkModifyGroupAction.h>
#include <openrct2/config/Config.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/localisation/Localisation.h>
#include <openrct2/network/network.h>
#include <openrct2/sprites.h>
#include <openrct2/util/Util.h>

// clang-format off
enum {
    WINDOW_MULTIPLAYER_PAGE_INFORMATION,
    WINDOW_MULTIPLAYER_PAGE_PLAYERS,
    WINDOW_MULTIPLAYER_PAGE_GROUPS,
    WINDOW_MULTIPLAYER_PAGE_OPTIONS
};

enum WINDOW_MULTIPLAYER_WIDGET_IDX {
    WIDX_BACKGROUND,
    WIDX_TITLE,
    WIDX_CLOSE,
    WIDX_CONTENT_PANEL,
    WIDX_TAB1,
    WIDX_TAB2,
    WIDX_TAB3,
    WIDX_TAB4,

    WIDX_HEADER_PLAYER = 8,
    WIDX_HEADER_GROUP,
    WIDX_HEADER_LAST_ACTION,
    WIDX_HEADER_PING,
    WIDX_LIST,

    WIDX_DEFAULT_GROUP = 8,
    WIDX_DEFAULT_GROUP_DROPDOWN,
    WIDX_ADD_GROUP,
    WIDX_REMOVE_GROUP,
    WIDX_RENAME_GROUP,
    WIDX_SELECTED_GROUP,
    WIDX_SELECTED_GROUP_DROPDOWN,
    WIDX_PERMISSIONS_LIST,

    WIDX_LOG_CHAT_CHECKBOX = 8,
    WIDX_LOG_SERVER_ACTIONS_CHECKBOX,
    WIDX_KNOWN_KEYS_ONLY_CHECKBOX,
};

#define MAIN_MULTIPLAYER_WIDGETS \
    MakeWidget({  0,  0}, {340, 240}, WindowWidgetType::Frame,    WindowColour::Primary                                        ), /* panel / background */ \
    MakeWidget({  1,  1}, {338,  14}, WindowWidgetType::Caption,  WindowColour::Primary,  STR_NONE,    STR_WINDOW_TITLE_TIP    ), /* title bar */ \
    MakeWidget({327,  2}, { 11,  12}, WindowWidgetType::CloseBox, WindowColour::Primary,  STR_CLOSE_X, STR_CLOSE_WINDOW_TIP    ), /* close x button */ \
    MakeWidget({  0, 43}, {340, 197}, WindowWidgetType::Resize,   WindowColour::Secondary                                      ), /* content panel */ \
    MakeTab   ({  3, 17},                                                                STR_SHOW_SERVER_INFO_TIP), /* tab */ \
    MakeTab   ({ 34, 17},                                                                STR_PLAYERS_TIP         ), /* tab */ \
    MakeTab   ({ 65, 17},                                                                STR_GROUPS_TIP          ), /* tab */ \
    MakeTab   ({ 96, 17},                                                                STR_OPTIONS_TIP         )  /* tab */

static rct_widget window_multiplayer_information_widgets[] = {
    MAIN_MULTIPLAYER_WIDGETS,
    WIDGETS_END,
};

static rct_widget window_multiplayer_players_widgets[] = {
    MAIN_MULTIPLAYER_WIDGETS,
    MakeWidget({  3, 46}, {173,  15}, WindowWidgetType::TableHeader, WindowColour::Primary  , STR_PLAYER     ), // Player name
    MakeWidget({176, 46}, { 83,  15}, WindowWidgetType::TableHeader, WindowColour::Primary  , STR_GROUP      ), // Player name
    MakeWidget({259, 46}, {100,  15}, WindowWidgetType::TableHeader, WindowColour::Primary  , STR_LAST_ACTION), // Player name
    MakeWidget({359, 46}, { 42,  15}, WindowWidgetType::TableHeader, WindowColour::Primary  , STR_PING       ), // Player name
    MakeWidget({  3, 60}, {334, 177}, WindowWidgetType::Scroll,       WindowColour::Secondary, SCROLL_VERTICAL), // list
    WIDGETS_END,
};

static rct_widget window_multiplayer_groups_widgets[] = {
    MAIN_MULTIPLAYER_WIDGETS,
    MakeWidget({141, 46}, {175,  12}, WindowWidgetType::DropdownMenu, WindowColour::Secondary                    ), // default group
    MakeWidget({305, 47}, { 11,  10}, WindowWidgetType::Button,   WindowColour::Secondary, STR_DROPDOWN_GLYPH),
    MakeWidget({ 11, 65}, { 92,  12}, WindowWidgetType::Button,   WindowColour::Secondary, STR_ADD_GROUP     ), // add group button
    MakeWidget({113, 65}, { 92,  12}, WindowWidgetType::Button,   WindowColour::Secondary, STR_REMOVE_GROUP  ), // remove group button
    MakeWidget({215, 65}, { 92,  12}, WindowWidgetType::Button,   WindowColour::Secondary, STR_RENAME_GROUP  ), // rename group button
    MakeWidget({ 72, 80}, {175,  12}, WindowWidgetType::DropdownMenu, WindowColour::Secondary                    ), // selected group
    MakeWidget({236, 81}, { 11,  10}, WindowWidgetType::Button,   WindowColour::Secondary, STR_DROPDOWN_GLYPH),
    MakeWidget({  3, 94}, {314, 207}, WindowWidgetType::Scroll,   WindowColour::Secondary, SCROLL_VERTICAL   ), // permissions list
    WIDGETS_END,
};

static rct_widget window_multiplayer_options_widgets[] = {
    MAIN_MULTIPLAYER_WIDGETS,
    MakeWidget({3, 50}, {295, 12}, WindowWidgetType::Checkbox, WindowColour::Secondary, STR_LOG_CHAT,              STR_LOG_CHAT_TIP             ),
    MakeWidget({3, 64}, {295, 12}, WindowWidgetType::Checkbox, WindowColour::Secondary, STR_LOG_SERVER_ACTIONS,    STR_LOG_SERVER_ACTIONS_TIP   ),
    MakeWidget({3, 78}, {295, 12}, WindowWidgetType::Checkbox, WindowColour::Secondary, STR_ALLOW_KNOWN_KEYS_ONLY, STR_ALLOW_KNOWN_KEYS_ONLY_TIP),
    WIDGETS_END,
};

static rct_widget *window_multiplayer_page_widgets[] = {
    window_multiplayer_information_widgets,
    window_multiplayer_players_widgets,
    window_multiplayer_groups_widgets,
    window_multiplayer_options_widgets
};

static constexpr const uint64_t window_multiplayer_page_enabled_widgets[] = {
    (1ULL << WIDX_CLOSE) | (1ULL << WIDX_TAB1) | (1ULL << WIDX_TAB2) | (1ULL << WIDX_TAB3) | (1ULL << WIDX_TAB4),
    (1ULL << WIDX_CLOSE) | (1ULL << WIDX_TAB1) | (1ULL << WIDX_TAB2) | (1ULL << WIDX_TAB3) | (1ULL << WIDX_TAB4),
    (1ULL << WIDX_CLOSE) | (1ULL << WIDX_TAB1) | (1ULL << WIDX_TAB2) | (1ULL << WIDX_TAB3) | (1ULL << WIDX_TAB4) | (1ULL << WIDX_DEFAULT_GROUP) | (1ULL << WIDX_DEFAULT_GROUP_DROPDOWN) | (1ULL << WIDX_ADD_GROUP) | (1ULL << WIDX_REMOVE_GROUP) | (1ULL << WIDX_RENAME_GROUP) | (1ULL << WIDX_SELECTED_GROUP) | (1ULL << WIDX_SELECTED_GROUP_DROPDOWN),
    (1ULL << WIDX_CLOSE) | (1ULL << WIDX_TAB1) | (1ULL << WIDX_TAB2) | (1ULL << WIDX_TAB3) | (1ULL << WIDX_TAB4) | (1ULL << WIDX_LOG_CHAT_CHECKBOX) | (1ULL << WIDX_LOG_SERVER_ACTIONS_CHECKBOX) | (1ULL << WIDX_KNOWN_KEYS_ONLY_CHECKBOX),
};

static constexpr rct_string_id WindowMultiplayerPageTitles[] = {
    STR_MULTIPLAYER_INFORMATION_TITLE,
    STR_MULTIPLAYER_PLAYERS_TITLE,
    STR_MULTIPLAYER_GROUPS_TITLE,
    STR_MULTIPLAYER_OPTIONS_TITLE,
};

static uint8_t _selectedGroup = 0;

static void window_multiplayer_information_mouseup(rct_window *w, rct_widgetindex widgetIndex);
static void window_multiplayer_information_resize(rct_window *w);
static void window_multiplayer_information_update(rct_window *w);
static void window_multiplayer_information_invalidate(rct_window *w);
static void window_multiplayer_information_paint(rct_window *w, rct_drawpixelinfo *dpi);

static void window_multiplayer_players_mouseup(rct_window *w, rct_widgetindex widgetIndex);
static void window_multiplayer_players_resize(rct_window *w);
static void window_multiplayer_players_update(rct_window *w);
static void window_multiplayer_players_scrollgetsize(rct_window *w, int32_t scrollIndex, int32_t *width, int32_t *height);
static void window_multiplayer_players_scrollmousedown(rct_window *w, int32_t scrollIndex, const ScreenCoordsXY& screenCoords);
static void window_multiplayer_players_scrollmouseover(rct_window *w, int32_t scrollIndex, const ScreenCoordsXY& screenCoords);
static void window_multiplayer_players_invalidate(rct_window *w);
static void window_multiplayer_players_paint(rct_window *w, rct_drawpixelinfo *dpi);
static void window_multiplayer_players_scrollpaint(rct_window *w, rct_drawpixelinfo *dpi, int32_t scrollIndex);

static void window_multiplayer_groups_mouseup(rct_window *w, rct_widgetindex widgetIndex);
static void window_multiplayer_groups_resize(rct_window *w);
static void window_multiplayer_groups_mousedown(rct_window *w, rct_widgetindex widgetIndex, rct_widget* widget);
static void window_multiplayer_groups_dropdown(rct_window *w, rct_widgetindex widgetIndex, int32_t dropdownIndex);
static void window_multiplayer_groups_update(rct_window *w);
static void window_multiplayer_groups_scrollgetsize(rct_window *w, int32_t scrollIndex, int32_t *width, int32_t *height);
static void window_multiplayer_groups_scrollmousedown(rct_window *w, int32_t scrollIndex, const ScreenCoordsXY& screenCoords);
static void window_multiplayer_groups_scrollmouseover(rct_window *w, int32_t scrollIndex, const ScreenCoordsXY& screenCoords);
static void window_multiplayer_groups_text_input(rct_window *w, rct_widgetindex widgetIndex, char *text);
static void window_multiplayer_groups_invalidate(rct_window *w);
static void window_multiplayer_groups_paint(rct_window *w, rct_drawpixelinfo *dpi);
static void window_multiplayer_groups_scrollpaint(rct_window *w, rct_drawpixelinfo *dpi, int32_t scrollIndex);

static void window_multiplayer_options_mouseup(rct_window *w, rct_widgetindex widgetIndex);
static void window_multiplayer_options_resize(rct_window *w);
static void window_multiplayer_options_update(rct_window *w);
static void window_multiplayer_options_invalidate(rct_window *w);
static void window_multiplayer_options_paint(rct_window *w, rct_drawpixelinfo *dpi);

static rct_window_event_list window_multiplayer_information_events([](auto& events)
{
    events.mouse_up = &window_multiplayer_information_mouseup;
    events.resize = &window_multiplayer_information_resize;
    events.update = &window_multiplayer_information_update;
    events.invalidate = &window_multiplayer_information_invalidate;
    events.paint = &window_multiplayer_information_paint;
});

static rct_window_event_list window_multiplayer_players_events([](auto& events)
{
    events.mouse_up = &window_multiplayer_players_mouseup;
    events.resize = &window_multiplayer_players_resize;
    events.update = &window_multiplayer_players_update;
    events.get_scroll_size = &window_multiplayer_players_scrollgetsize;
    events.scroll_mousedown = &window_multiplayer_players_scrollmousedown;
    events.scroll_mouseover = &window_multiplayer_players_scrollmouseover;
    events.invalidate = &window_multiplayer_players_invalidate;
    events.paint = &window_multiplayer_players_paint;
    events.scroll_paint = &window_multiplayer_players_scrollpaint;
});

static rct_window_event_list window_multiplayer_groups_events([](auto& events)
{
    events.mouse_up = &window_multiplayer_groups_mouseup;
    events.resize = &window_multiplayer_groups_resize;
    events.mouse_down = &window_multiplayer_groups_mousedown;
    events.dropdown = &window_multiplayer_groups_dropdown;
    events.update = &window_multiplayer_groups_update;
    events.get_scroll_size = &window_multiplayer_groups_scrollgetsize;
    events.scroll_mousedown = &window_multiplayer_groups_scrollmousedown;
    events.scroll_mouseover = &window_multiplayer_groups_scrollmouseover;
    events.text_input = &window_multiplayer_groups_text_input;
    events.invalidate = &window_multiplayer_groups_invalidate;
    events.paint = &window_multiplayer_groups_paint;
    events.scroll_paint = &window_multiplayer_groups_scrollpaint;
});

static rct_window_event_list window_multiplayer_options_events([](auto& events)
{
    events.mouse_up = &window_multiplayer_options_mouseup;
    events.resize = &window_multiplayer_options_resize;
    events.update = &window_multiplayer_options_update;
    events.invalidate = &window_multiplayer_options_invalidate;
    events.paint = &window_multiplayer_options_paint;
});

static rct_window_event_list *window_multiplayer_page_events[] = {
    &window_multiplayer_information_events,
    &window_multiplayer_players_events,
    &window_multiplayer_groups_events,
    &window_multiplayer_options_events
};
// clang-format on

static constexpr const int32_t window_multiplayer_animation_divisor[] = { 4, 4, 2, 2 };
static constexpr const int32_t window_multiplayer_animation_frames[] = { 8, 8, 7, 4 };

static void window_multiplayer_draw_tab_images(rct_window* w, rct_drawpixelinfo* dpi);
static void window_multiplayer_set_page(rct_window* w, int32_t page);

static bool _windowInformationSizeDirty;
static ScreenCoordsXY _windowInformationSize;

rct_window* window_multiplayer_open()
{
    // Check if window is already open
    rct_window* window = window_bring_to_front_by_class(WC_MULTIPLAYER);
    if (window == nullptr)
    {
        window = WindowCreateAutoPos(320, 144, &window_multiplayer_players_events, WC_MULTIPLAYER, WF_10 | WF_RESIZABLE);
        window_multiplayer_set_page(window, WINDOW_MULTIPLAYER_PAGE_INFORMATION);
    }

    return window;
}

static void window_multiplayer_set_page(rct_window* w, int32_t page)
{
    _windowInformationSizeDirty = true;

    w->page = page;
    w->frame_no = 0;
    w->no_list_items = 0;
    w->selected_list_item = -1;

    w->enabled_widgets = window_multiplayer_page_enabled_widgets[page];
    w->hold_down_widgets = 0;
    w->event_handlers = window_multiplayer_page_events[page];
    w->pressed_widgets = 0;
    w->widgets = window_multiplayer_page_widgets[page];
    w->widgets[WIDX_TITLE].text = WindowMultiplayerPageTitles[page];

    window_event_resize_call(w);
    window_event_invalidate_call(w);
    WindowInitScrollWidgets(w);
    w->Invalidate();
}

static void window_multiplayer_anchor_border_widgets(rct_window* w)
{
    w->widgets[WIDX_BACKGROUND].right = w->width - 1;
    w->widgets[WIDX_BACKGROUND].bottom = w->height - 1;
    w->widgets[WIDX_TITLE].right = w->width - 2;
    w->widgets[WIDX_CONTENT_PANEL].right = w->width - 1;
    w->widgets[WIDX_CONTENT_PANEL].bottom = w->height - 1;
    w->widgets[WIDX_CLOSE].left = w->width - 13;
    w->widgets[WIDX_CLOSE].right = w->width - 3;
}

static void window_multiplayer_set_pressed_tab(rct_window* w)
{
    for (int32_t i = 0; i < 2; i++)
    {
        w->pressed_widgets &= ~(1 << (WIDX_TAB1 + i));
    }
    w->pressed_widgets |= 1LL << (WIDX_TAB1 + w->page);
}

static void window_multiplayer_groups_show_group_dropdown(rct_window* w, rct_widget* widget)
{
    rct_widget* dropdownWidget;
    int32_t numItems, i;

    dropdownWidget = widget - 1;

    numItems = network_get_num_groups();

    WindowDropdownShowTextCustomWidth(
        { w->windowPos.x + dropdownWidget->left, w->windowPos.y + dropdownWidget->top }, dropdownWidget->height() + 1,
        w->colours[1], 0, 0, numItems, widget->right - dropdownWidget->left);

    for (i = 0; i < network_get_num_groups(); i++)
    {
        gDropdownItemsFormat[i] = STR_OPTIONS_DROPDOWN_ITEM;
        gDropdownItemsArgs[i] = reinterpret_cast<uintptr_t>(network_get_group_name(i));
    }
    if (widget == &window_multiplayer_groups_widgets[WIDX_DEFAULT_GROUP_DROPDOWN])
    {
        Dropdown::SetChecked(network_get_group_index(network_get_default_group()), true);
    }
    else if (widget == &window_multiplayer_groups_widgets[WIDX_SELECTED_GROUP_DROPDOWN])
    {
        Dropdown::SetChecked(network_get_group_index(_selectedGroup), true);
    }
}

#pragma region Information page

static void window_multiplayer_information_mouseup(rct_window* w, rct_widgetindex widgetIndex)
{
    switch (widgetIndex)
    {
        case WIDX_CLOSE:
            window_close(w);
            break;
        case WIDX_TAB1:
        case WIDX_TAB2:
        case WIDX_TAB3:
        case WIDX_TAB4:
            if (w->page != widgetIndex - WIDX_TAB1)
            {
                window_multiplayer_set_page(w, widgetIndex - WIDX_TAB1);
            }
            break;
    }
}

static ScreenCoordsXY window_multiplayer_information_get_size()
{
    if (!_windowInformationSizeDirty)
    {
        return _windowInformationSize;
    }

    int32_t lineHeight = font_get_line_height(FontSpriteBase::MEDIUM);

    // Base dimensions.
    const int32_t width = 450;
    int32_t height = 55;
    int32_t numLines;

    // Server name is displayed word-wrapped, so figure out how high it will be.
    {
        utf8* buffer = _strdup(network_get_server_name());
        gfx_wrap_string(buffer, width, FontSpriteBase::MEDIUM, &numLines);
        free(buffer);
        height += ++numLines * lineHeight + (LIST_ROW_HEIGHT / 2);
    }

    // Likewise, for the optional server description -- which can be a little longer.
    const utf8* descString = network_get_server_description();
    if (!str_is_null_or_empty(descString))
    {
        utf8* buffer = _strdup(descString);
        gfx_wrap_string(buffer, width, FontSpriteBase::MEDIUM, &numLines);
        free(buffer);
        height += ++numLines * lineHeight + (LIST_ROW_HEIGHT / 2);
    }

    // Finally, account for provider info, if present.
    {
        const utf8* providerName = network_get_server_provider_name();
        if (!str_is_null_or_empty(providerName))
            height += LIST_ROW_HEIGHT;

        const utf8* providerEmail = network_get_server_provider_email();
        if (!str_is_null_or_empty(providerEmail))
            height += LIST_ROW_HEIGHT;

        const utf8* providerWebsite = network_get_server_provider_website();
        if (!str_is_null_or_empty(providerWebsite))
            height += LIST_ROW_HEIGHT;
    }

    _windowInformationSizeDirty = false;
    _windowInformationSize = { static_cast<int16_t>(width), static_cast<int16_t>(height) };
    return _windowInformationSize;
}

static void window_multiplayer_information_resize(rct_window* w)
{
    auto size = window_multiplayer_information_get_size();
    window_set_resize(w, size.x, size.y, size.x, size.y);
}

static void window_multiplayer_information_update(rct_window* w)
{
    w->frame_no++;
    widget_invalidate(w, WIDX_TAB1 + w->page);
}

static void window_multiplayer_information_invalidate(rct_window* w)
{
    window_multiplayer_set_pressed_tab(w);
    window_multiplayer_anchor_border_widgets(w);
    window_align_tabs(w, WIDX_TAB1, WIDX_TAB4);
}

static void window_multiplayer_information_paint(rct_window* w, rct_drawpixelinfo* dpi)
{
    WindowDrawWidgets(w, dpi);
    window_multiplayer_draw_tab_images(w, dpi);

    rct_drawpixelinfo clippedDPI;
    if (clip_drawpixelinfo(&clippedDPI, dpi, w->windowPos, w->width, w->height))
    {
        dpi = &clippedDPI;

        auto screenCoords = ScreenCoordsXY{ 3, 50 };
        int32_t width = w->width - 6;

        const utf8* name = network_get_server_name();
        {
            auto ft = Formatter();
            ft.Add<const char*>(name);
            screenCoords.y += DrawTextWrapped(dpi, screenCoords, width, STR_STRING, ft, { w->colours[1] });
            screenCoords.y += LIST_ROW_HEIGHT / 2;
        }

        const utf8* description = network_get_server_description();
        if (!str_is_null_or_empty(description))
        {
            auto ft = Formatter();
            ft.Add<const char*>(description);
            screenCoords.y += DrawTextWrapped(dpi, screenCoords, width, STR_STRING, ft, { w->colours[1] });
            screenCoords.y += LIST_ROW_HEIGHT / 2;
        }

        const utf8* providerName = network_get_server_provider_name();
        if (!str_is_null_or_empty(providerName))
        {
            auto ft = Formatter();
            ft.Add<const char*>(providerName);
            DrawTextBasic(dpi, screenCoords, STR_PROVIDER_NAME, ft);
            screenCoords.y += LIST_ROW_HEIGHT;
        }

        const utf8* providerEmail = network_get_server_provider_email();
        if (!str_is_null_or_empty(providerEmail))
        {
            auto ft = Formatter();
            ft.Add<const char*>(providerEmail);
            DrawTextBasic(dpi, screenCoords, STR_PROVIDER_EMAIL, ft);
            screenCoords.y += LIST_ROW_HEIGHT;
        }

        const utf8* providerWebsite = network_get_server_provider_website();
        if (!str_is_null_or_empty(providerWebsite))
        {
            auto ft = Formatter();
            ft.Add<const char*>(providerWebsite);
            DrawTextBasic(dpi, screenCoords, STR_PROVIDER_WEBSITE, ft);
        }
    }
}

#pragma endregion

#pragma region Players page

static void window_multiplayer_players_mouseup(rct_window* w, rct_widgetindex widgetIndex)
{
    switch (widgetIndex)
    {
        case WIDX_CLOSE:
            window_close(w);
            break;
        case WIDX_TAB1:
        case WIDX_TAB2:
        case WIDX_TAB3:
        case WIDX_TAB4:
            if (w->page != widgetIndex - WIDX_TAB1)
            {
                window_multiplayer_set_page(w, widgetIndex - WIDX_TAB1);
            }
            break;
    }
}

static void window_multiplayer_players_resize(rct_window* w)
{
    window_set_resize(w, 420, 124, 500, 450);

    w->no_list_items = network_get_num_players();
    w->list_item_positions[0] = 0;

    w->widgets[WIDX_HEADER_PING].right = w->width - 5;

    w->selected_list_item = -1;
    w->Invalidate();
}

static void window_multiplayer_players_update(rct_window* w)
{
    w->frame_no++;
    widget_invalidate(w, WIDX_TAB1 + w->page);
}

static void window_multiplayer_players_scrollgetsize(rct_window* w, int32_t scrollIndex, int32_t* width, int32_t* height)
{
    int32_t i;

    if (w->selected_list_item != -1)
    {
        w->selected_list_item = -1;
        w->Invalidate();
    }

    *height = network_get_num_players() * SCROLLABLE_ROW_HEIGHT;
    i = *height - window_multiplayer_players_widgets[WIDX_LIST].bottom + window_multiplayer_players_widgets[WIDX_LIST].top + 21;
    if (i < 0)
        i = 0;
    if (i < w->scrolls[0].v_top)
    {
        w->scrolls[0].v_top = i;
        w->Invalidate();
    }
}

static void window_multiplayer_players_scrollmousedown(rct_window* w, int32_t scrollIndex, const ScreenCoordsXY& screenCoords)
{
    int32_t index;

    index = screenCoords.y / SCROLLABLE_ROW_HEIGHT;
    if (index >= w->no_list_items)
        return;

    w->selected_list_item = index;
    w->Invalidate();

    window_player_open(network_get_player_id(index));
}

static void window_multiplayer_players_scrollmouseover(rct_window* w, int32_t scrollIndex, const ScreenCoordsXY& screenCoords)
{
    int32_t index;

    index = screenCoords.y / SCROLLABLE_ROW_HEIGHT;
    if (index >= w->no_list_items)
        return;

    w->selected_list_item = index;
    w->Invalidate();
}

static void window_multiplayer_players_invalidate(rct_window* w)
{
    window_multiplayer_set_pressed_tab(w);
    window_multiplayer_anchor_border_widgets(w);
    window_multiplayer_players_widgets[WIDX_LIST].right = w->width - 4;
    window_multiplayer_players_widgets[WIDX_LIST].bottom = w->height - 0x0F;
    window_align_tabs(w, WIDX_TAB1, WIDX_TAB4);
}

static void window_multiplayer_players_paint(rct_window* w, rct_drawpixelinfo* dpi)
{
    rct_string_id stringId;

    WindowDrawWidgets(w, dpi);
    window_multiplayer_draw_tab_images(w, dpi);

    // Number of players
    stringId = w->no_list_items == 1 ? STR_MULTIPLAYER_PLAYER_COUNT : STR_MULTIPLAYER_PLAYER_COUNT_PLURAL;
    auto screenCoords = w->windowPos + ScreenCoordsXY{ 4, w->widgets[WIDX_LIST].bottom + 2 };
    auto ft = Formatter();
    ft.Add<uint16_t>(w->no_list_items);
    DrawTextBasic(dpi, screenCoords, stringId, ft, { w->colours[2] });
}

static void window_multiplayer_players_scrollpaint(rct_window* w, rct_drawpixelinfo* dpi, int32_t scrollIndex)
{
    ScreenCoordsXY screenCoords;
    screenCoords.y = 0;
    for (int32_t i = 0; i < network_get_num_players(); i++)
    {
        if (screenCoords.y > dpi->y + dpi->height)
        {
            break;
        }

        if (screenCoords.y + SCROLLABLE_ROW_HEIGHT + 1 >= dpi->y)
        {
            thread_local std::string buffer;
            buffer.reserve(512);
            buffer.clear();

            // Draw player name
            colour_t colour = COLOUR_BLACK;
            if (i == w->selected_list_item)
            {
                const ScreenRect screenRect{ { 0, screenCoords.y }, { 800, screenCoords.y + SCROLLABLE_ROW_HEIGHT - 1 } };
                gfx_filter_rect(dpi, screenRect, FilterPaletteID::PaletteDarken1);
                buffer += network_get_player_name(i);
                colour = w->colours[2];
            }
            else
            {
                if (network_get_player_flags(i) & NETWORK_PLAYER_FLAG_ISSERVER)
                {
                    buffer += "{BABYBLUE}";
                }
                else
                {
                    buffer += "{BLACK}";
                }
                buffer += network_get_player_name(i);
            }
            screenCoords.x = 0;
            gfx_clip_string(buffer.data(), 230, FontSpriteBase::MEDIUM);
            gfx_draw_string(dpi, screenCoords, buffer.c_str(), { colour });

            // Draw group name
            buffer.resize(0);
            int32_t group = network_get_group_index(network_get_player_group(i));
            if (group != -1)
            {
                buffer += "{BLACK}";
                screenCoords.x = 173;
                buffer += network_get_group_name(group);
                gfx_clip_string(buffer.data(), 80, FontSpriteBase::MEDIUM);
                gfx_draw_string(dpi, screenCoords, buffer.c_str(), { colour });
            }

            // Draw last action
            int32_t action = network_get_player_last_action(i, 2000);
            auto ft = Formatter();
            if (action != -999)
            {
                ft.Add<rct_string_id>(network_get_action_name_string_id(action));
            }
            else
            {
                ft.Add<rct_string_id>(STR_ACTION_NA);
            }
            DrawTextEllipsised(dpi, { 256, screenCoords.y }, 100, STR_BLACK_STRING, ft);

            // Draw ping
            buffer.resize(0);
            int32_t ping = network_get_player_ping(i);
            if (ping <= 100)
            {
                buffer += "{GREEN}";
            }
            else if (ping <= 250)
            {
                buffer += "{YELLOW}";
            }
            else
            {
                buffer += "{RED}";
            }

            char pingBuffer[64]{};
            snprintf(pingBuffer, sizeof(pingBuffer), "%d ms", ping);
            buffer += pingBuffer;

            screenCoords.x = 356;
            gfx_draw_string(dpi, screenCoords, buffer.c_str(), { colour });
        }
        screenCoords.y += SCROLLABLE_ROW_HEIGHT;
    }
}

#pragma endregion

#pragma region Groups page

static void window_multiplayer_groups_mouseup(rct_window* w, rct_widgetindex widgetIndex)
{
    switch (widgetIndex)
    {
        case WIDX_CLOSE:
            window_close(w);
            break;
        case WIDX_TAB1:
        case WIDX_TAB2:
        case WIDX_TAB3:
        case WIDX_TAB4:
            if (w->page != widgetIndex - WIDX_TAB1)
            {
                window_multiplayer_set_page(w, widgetIndex - WIDX_TAB1);
            }
            break;
        case WIDX_ADD_GROUP:
        {
            auto networkModifyGroup = NetworkModifyGroupAction(ModifyGroupType::AddGroup);
            GameActions::Execute(&networkModifyGroup);
        }
        break;
        case WIDX_REMOVE_GROUP:
        {
            auto networkModifyGroup = NetworkModifyGroupAction(ModifyGroupType::RemoveGroup, _selectedGroup);
            GameActions::Execute(&networkModifyGroup);
        }
        break;
        case WIDX_RENAME_GROUP:;
            int32_t groupIndex = network_get_group_index(_selectedGroup);
            const utf8* groupName = network_get_group_name(groupIndex);
            window_text_input_raw_open(w, widgetIndex, STR_GROUP_NAME, STR_ENTER_NEW_NAME_FOR_THIS_GROUP, {}, groupName, 32);
            break;
    }
}

static void window_multiplayer_groups_resize(rct_window* w)
{
    window_set_resize(w, 320, 200, 320, 500);

    w->no_list_items = network_get_num_actions();
    w->list_item_positions[0] = 0;

    w->selected_list_item = -1;
    w->Invalidate();
}

static void window_multiplayer_groups_mousedown(rct_window* w, rct_widgetindex widgetIndex, rct_widget* widget)
{
    switch (widgetIndex)
    {
        case WIDX_DEFAULT_GROUP_DROPDOWN:
            window_multiplayer_groups_show_group_dropdown(w, widget);
            break;
        case WIDX_SELECTED_GROUP_DROPDOWN:
            window_multiplayer_groups_show_group_dropdown(w, widget);
            break;
    }
}

static void window_multiplayer_groups_dropdown(rct_window* w, rct_widgetindex widgetIndex, int32_t dropdownIndex)
{
    if (dropdownIndex == -1)
    {
        return;
    }

    switch (widgetIndex)
    {
        case WIDX_DEFAULT_GROUP_DROPDOWN:
        {
            auto networkModifyGroup = NetworkModifyGroupAction(
                ModifyGroupType::SetDefault, network_get_group_id(dropdownIndex));
            GameActions::Execute(&networkModifyGroup);
        }
        break;
        case WIDX_SELECTED_GROUP_DROPDOWN:
            _selectedGroup = network_get_group_id(dropdownIndex);
            break;
    }

    w->Invalidate();
}

static void window_multiplayer_groups_update(rct_window* w)
{
    w->frame_no++;
    widget_invalidate(w, WIDX_TAB1 + w->page);
}

static void window_multiplayer_groups_scrollgetsize(rct_window* w, int32_t scrollIndex, int32_t* width, int32_t* height)
{
    int32_t i;

    if (w->selected_list_item != -1)
    {
        w->selected_list_item = -1;
        w->Invalidate();
    }

    *height = network_get_num_actions() * SCROLLABLE_ROW_HEIGHT;
    i = *height - window_multiplayer_groups_widgets[WIDX_LIST].bottom + window_multiplayer_groups_widgets[WIDX_LIST].top + 21;
    if (i < 0)
        i = 0;
    if (i < w->scrolls[0].v_top)
    {
        w->scrolls[0].v_top = i;
        w->Invalidate();
    }
}

static void window_multiplayer_groups_scrollmousedown(rct_window* w, int32_t scrollIndex, const ScreenCoordsXY& screenCoords)
{
    int32_t index;

    index = screenCoords.y / SCROLLABLE_ROW_HEIGHT;
    if (index >= w->no_list_items)
        return;

    w->selected_list_item = index;
    w->Invalidate();

    auto networkModifyGroup = NetworkModifyGroupAction(
        ModifyGroupType::SetPermissions, _selectedGroup, "", index, PermissionState::Toggle);
    GameActions::Execute(&networkModifyGroup);
}

static void window_multiplayer_groups_scrollmouseover(rct_window* w, int32_t scrollIndex, const ScreenCoordsXY& screenCoords)
{
    int32_t index;

    index = screenCoords.y / SCROLLABLE_ROW_HEIGHT;
    if (index >= w->no_list_items)
        return;

    w->selected_list_item = index;
    w->Invalidate();
}

static void window_multiplayer_groups_text_input(rct_window* w, rct_widgetindex widgetIndex, char* text)
{
    if (widgetIndex != WIDX_RENAME_GROUP)
        return;

    if (text == nullptr)
        return;

    auto networkModifyGroup = NetworkModifyGroupAction(ModifyGroupType::SetName, _selectedGroup, text);
    GameActions::Execute(&networkModifyGroup);
}

static void window_multiplayer_groups_invalidate(rct_window* w)
{
    window_multiplayer_set_pressed_tab(w);
    window_multiplayer_anchor_border_widgets(w);
    window_multiplayer_groups_widgets[WIDX_PERMISSIONS_LIST].right = w->width - 4;
    window_multiplayer_groups_widgets[WIDX_PERMISSIONS_LIST].bottom = w->height - 0x0F;
    window_align_tabs(w, WIDX_TAB1, WIDX_TAB4);

    // select other group if one is removed
    while (network_get_group_index(_selectedGroup) == -1 && _selectedGroup > 0)
    {
        _selectedGroup--;
    }
}

static void window_multiplayer_groups_paint(rct_window* w, rct_drawpixelinfo* dpi)
{
    thread_local std::string buffer;

    WindowDrawWidgets(w, dpi);
    window_multiplayer_draw_tab_images(w, dpi);

    rct_widget* widget = &window_multiplayer_groups_widgets[WIDX_DEFAULT_GROUP];
    int32_t group = network_get_group_index(network_get_default_group());
    if (group != -1)
    {
        buffer.assign("{WINDOW_COLOUR_2}");
        buffer += network_get_group_name(group);

        auto ft = Formatter();
        ft.Add<const char*>(buffer.c_str());
        DrawTextEllipsised(
            dpi, w->windowPos + ScreenCoordsXY{ widget->midX() - 5, widget->top }, widget->width() - 8, STR_STRING, ft,
            { TextAlignment::CENTRE });
    }

    auto screenPos = w->windowPos
        + ScreenCoordsXY{ window_multiplayer_groups_widgets[WIDX_CONTENT_PANEL].left + 4,
                          window_multiplayer_groups_widgets[WIDX_CONTENT_PANEL].top + 4 };

    DrawTextBasic(dpi, screenPos, STR_DEFAULT_GROUP, {}, { w->colours[2] });

    screenPos.y += 20;

    gfx_fill_rect_inset(
        dpi, { screenPos - ScreenCoordsXY{ 0, 6 }, screenPos + ScreenCoordsXY{ 310, -5 } }, w->colours[1],
        INSET_RECT_FLAG_BORDER_INSET);

    widget = &window_multiplayer_groups_widgets[WIDX_SELECTED_GROUP];
    group = network_get_group_index(_selectedGroup);
    if (group != -1)
    {
        buffer.assign("{WINDOW_COLOUR_2}");
        buffer += network_get_group_name(group);
        auto ft = Formatter();
        ft.Add<const char*>(buffer.c_str());
        DrawTextEllipsised(
            dpi, w->windowPos + ScreenCoordsXY{ widget->midX() - 5, widget->top }, widget->width() - 8, STR_STRING, ft,
            { TextAlignment::CENTRE });
    }
}

static void window_multiplayer_groups_scrollpaint(rct_window* w, rct_drawpixelinfo* dpi, int32_t scrollIndex)
{
    auto screenCoords = ScreenCoordsXY{ 0, 0 };

    auto dpiCoords = ScreenCoordsXY{ dpi->x, dpi->y };
    gfx_fill_rect(
        dpi, { dpiCoords, dpiCoords + ScreenCoordsXY{ dpi->width - 1, dpi->height - 1 } }, ColourMapA[w->colours[1]].mid_light);

    for (int32_t i = 0; i < network_get_num_actions(); i++)
    {
        if (i == w->selected_list_item)
        {
            const ScreenRect screenRect{ { 0, screenCoords.y }, { 800, screenCoords.y + SCROLLABLE_ROW_HEIGHT - 1 } };
            gfx_filter_rect(dpi, screenRect, FilterPaletteID::PaletteDarken1);
        }
        if (screenCoords.y > dpi->y + dpi->height)
        {
            break;
        }

        if (screenCoords.y + SCROLLABLE_ROW_HEIGHT + 1 >= dpi->y)
        {
            int32_t groupindex = network_get_group_index(_selectedGroup);
            if (groupindex != -1)
            {
                if (network_can_perform_action(groupindex, static_cast<NetworkPermission>(i)))
                {
                    screenCoords.x = 0;
                    gfx_draw_string(dpi, screenCoords, u8"{WINDOW_COLOUR_2}✓", {});
                }
            }

            // Draw action name
            auto ft = Formatter();
            ft.Add<uint16_t>(network_get_action_name_string_id(i));
            DrawTextBasic(dpi, { 10, screenCoords.y }, STR_WINDOW_COLOUR_2_STRINGID, ft);
        }
        screenCoords.y += SCROLLABLE_ROW_HEIGHT;
    }
}

#pragma endregion

#pragma region Options page

static void window_multiplayer_options_mouseup(rct_window* w, rct_widgetindex widgetIndex)
{
    switch (widgetIndex)
    {
        case WIDX_CLOSE:
            window_close(w);
            break;
        case WIDX_TAB1:
        case WIDX_TAB2:
        case WIDX_TAB3:
        case WIDX_TAB4:
            if (w->page != widgetIndex - WIDX_TAB1)
            {
                window_multiplayer_set_page(w, widgetIndex - WIDX_TAB1);
            }
            break;
        case WIDX_LOG_CHAT_CHECKBOX:
            gConfigNetwork.log_chat = !gConfigNetwork.log_chat;
            config_save_default();
            break;
        case WIDX_LOG_SERVER_ACTIONS_CHECKBOX:
            gConfigNetwork.log_server_actions = !gConfigNetwork.log_server_actions;
            config_save_default();
            break;
        case WIDX_KNOWN_KEYS_ONLY_CHECKBOX:
            gConfigNetwork.known_keys_only = !gConfigNetwork.known_keys_only;
            config_save_default();
            break;
    }
}

static void window_multiplayer_options_resize(rct_window* w)
{
    window_set_resize(w, 300, 100, 300, 100);
}

static void window_multiplayer_options_update(rct_window* w)
{
    w->frame_no++;
    widget_invalidate(w, WIDX_TAB1 + w->page);
}

static void window_multiplayer_options_invalidate(rct_window* w)
{
    window_multiplayer_set_pressed_tab(w);
    window_multiplayer_anchor_border_widgets(w);
    window_align_tabs(w, WIDX_TAB1, WIDX_TAB4);

    if (network_get_mode() == NETWORK_MODE_CLIENT)
    {
        w->widgets[WIDX_KNOWN_KEYS_ONLY_CHECKBOX].type = WindowWidgetType::Empty;
    }

    WidgetSetCheckboxValue(w, WIDX_LOG_CHAT_CHECKBOX, gConfigNetwork.log_chat);
    WidgetSetCheckboxValue(w, WIDX_LOG_SERVER_ACTIONS_CHECKBOX, gConfigNetwork.log_server_actions);
    WidgetSetCheckboxValue(w, WIDX_KNOWN_KEYS_ONLY_CHECKBOX, gConfigNetwork.known_keys_only);
}

static void window_multiplayer_options_paint(rct_window* w, rct_drawpixelinfo* dpi)
{
    WindowDrawWidgets(w, dpi);
    window_multiplayer_draw_tab_images(w, dpi);
}

#pragma endregion

static void window_multiplayer_draw_tab_image(rct_window* w, rct_drawpixelinfo* dpi, int32_t page, int32_t spriteIndex)
{
    rct_widgetindex widgetIndex = WIDX_TAB1 + page;

    if (!WidgetIsDisabled(w, widgetIndex))
    {
        if (w->page == page)
        {
            int32_t numFrames = window_multiplayer_animation_frames[w->page];
            if (numFrames > 1)
            {
                int32_t frame = w->frame_no / window_multiplayer_animation_divisor[w->page];
                spriteIndex += (frame % numFrames);
            }
        }

        gfx_draw_sprite(
            dpi, ImageId(spriteIndex),
            w->windowPos + ScreenCoordsXY{ w->widgets[widgetIndex].left, w->widgets[widgetIndex].top });
    }
}

static void window_multiplayer_draw_tab_images(rct_window* w, rct_drawpixelinfo* dpi)
{
    window_multiplayer_draw_tab_image(w, dpi, WINDOW_MULTIPLAYER_PAGE_INFORMATION, SPR_TAB_KIOSKS_AND_FACILITIES_0);
    window_multiplayer_draw_tab_image(w, dpi, WINDOW_MULTIPLAYER_PAGE_PLAYERS, SPR_TAB_GUESTS_0);
    window_multiplayer_draw_tab_image(w, dpi, WINDOW_MULTIPLAYER_PAGE_GROUPS, SPR_TAB_STAFF_OPTIONS_0);
    window_multiplayer_draw_tab_image(w, dpi, WINDOW_MULTIPLAYER_PAGE_OPTIONS, SPR_TAB_GEARS_0);
}
