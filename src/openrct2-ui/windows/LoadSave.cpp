/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <algorithm>
#include <ctime>
#include <iterator>
#include <memory>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Window.h>
#include <openrct2/Context.h>
#include <openrct2/Editor.h>
#include <openrct2/FileClassifier.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/config/Config.h>
#include <openrct2/core/FileScanner.h>
#include <openrct2/core/Guard.hpp>
#include <openrct2/core/Path.hpp>
#include <openrct2/core/String.hpp>
#include <openrct2/localisation/Localisation.h>
#include <openrct2/platform/Platform2.h>
#include <openrct2/platform/platform.h>
#include <openrct2/rct2/T6Exporter.h>
#include <openrct2/ride/TrackDesign.h>
#include <openrct2/scenario/Scenario.h>
#include <openrct2/title/TitleScreen.h>
#include <openrct2/ui/UiContext.h>
#include <openrct2/util/Util.h>
#include <openrct2/windows/Intent.h>
#include <openrct2/world/Park.h>
#include <string>
#include <vector>

#pragma region Widgets

static constexpr const rct_string_id WINDOW_TITLE = STR_NONE;
static constexpr const int32_t WW = 350;
static constexpr const int32_t WH = 400;

// clang-format off
enum
{
    WIDX_BACKGROUND,
    WIDX_TITLE,
    WIDX_CLOSE,
    WIDX_RESIZE,
    WIDX_DEFAULT,
    WIDX_UP,
    WIDX_NEW_FOLDER,
    WIDX_NEW_FILE,
    WIDX_SORT_NAME,
    WIDX_SORT_DATE,
    WIDX_SCROLL,
    WIDX_BROWSE,
};

// 0x9DE48C
static rct_widget window_loadsave_widgets[] =
{
    WINDOW_SHIM(WINDOW_TITLE, WW, WH),
    MakeWidget({               0,  WH - 1}, { WW,   1}, WindowWidgetType::Resize,       WindowColour::Secondary                                                             ), // tab content panel
    MakeWidget({               4,      36}, { 84,  14}, WindowWidgetType::Button,       WindowColour::Primary  , STR_LOADSAVE_DEFAULT,              STR_LOADSAVE_DEFAULT_TIP), // Go to default directory
    MakeWidget({              88,      36}, { 84,  14}, WindowWidgetType::Button,       WindowColour::Primary  , STR_FILEBROWSER_ACTION_UP                                  ), // Up
    MakeWidget({             172,      36}, { 87,  14}, WindowWidgetType::Button,       WindowColour::Primary  , STR_FILEBROWSER_ACTION_NEW_FOLDER                          ), // New
    MakeWidget({             259,      36}, { 87,  14}, WindowWidgetType::Button,       WindowColour::Primary  , STR_FILEBROWSER_ACTION_NEW_FILE                            ), // New
    MakeWidget({               4,      55}, {170,  14}, WindowWidgetType::TableHeader, WindowColour::Primary                                                               ), // Name
    MakeWidget({(WW - 5) / 2 + 1,      55}, {170,  14}, WindowWidgetType::TableHeader, WindowColour::Primary                                                               ), // Date
    MakeWidget({               4,      68}, {342, 303}, WindowWidgetType::Scroll,       WindowColour::Primary  , SCROLL_VERTICAL                                            ), // File list
    MakeWidget({               4, WH - 24}, {197,  19}, WindowWidgetType::Button,       WindowColour::Primary  , STR_FILEBROWSER_USE_SYSTEM_WINDOW                          ), // Use native browser
    WIDGETS_END,
};

#pragma endregion

#pragma region Events

static void window_loadsave_close(rct_window *w);
static void window_loadsave_mouseup(rct_window *w, rct_widgetindex widgetIndex);
static void window_loadsave_resize(rct_window *w);
static void window_loadsave_scrollgetsize(rct_window *w, int32_t scrollIndex, int32_t *width, int32_t *height);
static void window_loadsave_scrollmousedown(rct_window *w, int32_t scrollIndex, const ScreenCoordsXY& screenCoords);
static void window_loadsave_scrollmouseover(rct_window *w, int32_t scrollIndex, const ScreenCoordsXY& screenCoords);
static void window_loadsave_textinput(rct_window *w, rct_widgetindex widgetIndex, char *text);
static void window_loadsave_compute_max_date_width();
static void window_loadsave_invalidate(rct_window *w);
static void window_loadsave_paint(rct_window *w, rct_drawpixelinfo *dpi);
static void window_loadsave_scrollpaint(rct_window *w, rct_drawpixelinfo *dpi, int32_t scrollIndex);

static rct_window_event_list window_loadsave_events([](auto& events)
{
    events.close = &window_loadsave_close;
    events.mouse_up = &window_loadsave_mouseup;
    events.resize = &window_loadsave_resize;
    events.get_scroll_size = &window_loadsave_scrollgetsize;
    events.scroll_mousedown = &window_loadsave_scrollmousedown;
    events.scroll_mouseover = &window_loadsave_scrollmouseover;
    events.text_input = &window_loadsave_textinput;
    events.invalidate = &window_loadsave_invalidate;
    events.paint = &window_loadsave_paint;
    events.scroll_paint = &window_loadsave_scrollpaint;
});
// clang-format on

#pragma endregion

enum
{
    TYPE_DIRECTORY,
    TYPE_FILE,
};

struct LoadSaveListItem
{
    std::string name{};
    std::string path{};
    time_t date_modified{ 0 };
    std::string date_formatted{};
    std::string time_formatted{};
    uint8_t type{ 0 };
    bool loaded{ false };
};

static std::function<void(int32_t result, std::string_view)> _loadSaveCallback;
static TrackDesign* _trackDesign;

static std::vector<LoadSaveListItem> _listItems;
static char _directory[MAX_PATH];
static char _shortenedDirectory[MAX_PATH];
static char _parentDirectory[MAX_PATH];
static char _extension[256];
static std::string _defaultPath;
static int32_t _type;

static int32_t maxDateWidth = 0;
static int32_t maxTimeWidth = 0;

static void window_loadsave_populate_list(rct_window* w, int32_t includeNewItem, const char* directory, const char* extension);
static void window_loadsave_select(rct_window* w, const char* path);
static void window_loadsave_sort_list();

static rct_window* window_overwrite_prompt_open(const char* name, const char* path);

static utf8* getLastDirectoryByType(int32_t type)
{
    switch (type & 0x0E)
    {
        case LOADSAVETYPE_GAME:
            return gConfigGeneral.last_save_game_directory;

        case LOADSAVETYPE_LANDSCAPE:
            return gConfigGeneral.last_save_landscape_directory;

        case LOADSAVETYPE_SCENARIO:
            return gConfigGeneral.last_save_scenario_directory;

        case LOADSAVETYPE_TRACK:
            return gConfigGeneral.last_save_track_directory;

        default:
            return nullptr;
    }
}

static void getInitialDirectoryByType(const int32_t type, char* path, size_t pathSize)
{
    const char* subdir = nullptr;
    switch (type & 0x0E)
    {
        case LOADSAVETYPE_GAME:
            subdir = "save";
            break;

        case LOADSAVETYPE_LANDSCAPE:
            subdir = "landscape";
            break;

        case LOADSAVETYPE_SCENARIO:
            subdir = "scenario";
            break;

        case LOADSAVETYPE_TRACK:
            subdir = "track";
            break;

        case LOADSAVETYPE_HEIGHTMAP:
            subdir = "heightmap";
            break;
    }

    platform_get_user_directory(path, subdir, pathSize);
}

static const char* getFilterPatternByType(const int32_t type, const bool isSave)
{
    switch (type & 0x0E)
    {
        case LOADSAVETYPE_GAME:
            return isSave ? "*.sv6" : "*.sv6;*.sc6;*.sc4;*.sv4;*.sv7;*.sea;";

        case LOADSAVETYPE_LANDSCAPE:
            return isSave ? "*.sc6" : "*.sc6;*.sv6;*.sc4;*.sv4;*.sv7;*.sea;";

        case LOADSAVETYPE_SCENARIO:
            return "*.sc6";

        case LOADSAVETYPE_TRACK:
            return isSave ? "*.td6" : "*.td6;*.td4";

        case LOADSAVETYPE_HEIGHTMAP:
            return "*.bmp;*.png";

        default:
            openrct2_assert(true, "Unsupported load/save directory type.");
    }

    return "";
}

static int32_t window_loadsave_get_dir(const int32_t type, char* path, size_t pathSize)
{
    const char* last_save = getLastDirectoryByType(type);
    if (last_save != nullptr && platform_directory_exists(last_save))
        safe_strcpy(path, last_save, pathSize);
    else
        getInitialDirectoryByType(type, path, pathSize);
    return 1;
}

static bool browse(bool isSave, char* path, size_t pathSize);

rct_window* window_loadsave_open(
    int32_t type, std::string_view defaultPath, std::function<void(int32_t result, std::string_view)> callback,
    TrackDesign* trackDesign)
{
    _loadSaveCallback = callback;
    _trackDesign = trackDesign;
    _type = type;
    _defaultPath = defaultPath;

    bool isSave = (type & 0x01) == LOADSAVETYPE_SAVE;
    char path[MAX_PATH];
    bool success = window_loadsave_get_dir(type, path, sizeof(path));
    if (!success)
        return nullptr;

    // Bypass the lot?
    auto hasFilePicker = OpenRCT2::GetContext()->GetUiContext()->HasFilePicker();
    if (gConfigGeneral.use_native_browse_dialog && hasFilePicker)
    {
        if (browse(isSave, path, sizeof(path)))
        {
            window_loadsave_select(nullptr, path);
        }
        return nullptr;
    }

    rct_window* w = window_bring_to_front_by_class(WC_LOADSAVE);
    if (w == nullptr)
    {
        w = WindowCreateCentred(WW, WH, &window_loadsave_events, WC_LOADSAVE, WF_STICK_TO_FRONT | WF_RESIZABLE);
        w->widgets = window_loadsave_widgets;
        w->enabled_widgets = (1ULL << WIDX_CLOSE) | (1ULL << WIDX_UP) | (1ULL << WIDX_NEW_FOLDER) | (1ULL << WIDX_NEW_FILE)
            | (1ULL << WIDX_SORT_NAME) | (1ULL << WIDX_SORT_DATE) | (1ULL << WIDX_BROWSE) | (1ULL << WIDX_DEFAULT);

        w->min_width = WW;
        w->min_height = WH / 2;
        w->max_width = WW * 2;
        w->max_height = WH * 2;

        if (!hasFilePicker)
        {
            w->enabled_widgets &= ~(1ULL << WIDX_BROWSE);
            w->disabled_widgets |= (1ULL << WIDX_BROWSE);
            window_loadsave_widgets[WIDX_BROWSE].type = WindowWidgetType::Empty;
        }
    }

    const char* pattern = getFilterPatternByType(type, isSave);
    window_loadsave_populate_list(w, isSave, path, pattern);
    w->no_list_items = static_cast<uint16_t>(_listItems.size());
    w->selected_list_item = -1;

    switch (type & 0x0E)
    {
        case LOADSAVETYPE_GAME:
            w->widgets[WIDX_TITLE].text = isSave ? STR_FILE_DIALOG_TITLE_SAVE_GAME : STR_FILE_DIALOG_TITLE_LOAD_GAME;
            break;

        case LOADSAVETYPE_LANDSCAPE:
            w->widgets[WIDX_TITLE].text = isSave ? STR_FILE_DIALOG_TITLE_SAVE_LANDSCAPE : STR_FILE_DIALOG_TITLE_LOAD_LANDSCAPE;
            break;

        case LOADSAVETYPE_SCENARIO:
            w->widgets[WIDX_TITLE].text = STR_FILE_DIALOG_TITLE_SAVE_SCENARIO;
            break;

        case LOADSAVETYPE_TRACK:
            w->widgets[WIDX_TITLE].text = isSave ? STR_FILE_DIALOG_TITLE_SAVE_TRACK
                                                 : STR_FILE_DIALOG_TITLE_INSTALL_NEW_TRACK_DESIGN;
            break;

        case LOADSAVETYPE_HEIGHTMAP:
            openrct2_assert(!isSave, "Cannot save images through loadsave window");
            w->widgets[WIDX_TITLE].text = STR_FILE_DIALOG_TITLE_LOAD_HEIGHTMAP;
            break;

        default:
            openrct2_assert(true, "Unsupported load/save type: %d", type & 0x0F);
    }

    WindowInitScrollWidgets(w);
    window_loadsave_compute_max_date_width();

    return w;
}

static void window_loadsave_close(rct_window* w)
{
    _listItems.clear();
    window_close_by_class(WC_LOADSAVE_OVERWRITE_PROMPT);
}

static void window_loadsave_resize(rct_window* w)
{
    if (w->width < w->min_width)
    {
        w->Invalidate();
        w->width = w->min_width;
    }
    if (w->height < w->min_height)
    {
        w->Invalidate();
        w->height = w->min_height;
    }
}

static bool browse(bool isSave, char* path, size_t pathSize)
{
    file_dialog_desc desc = {};
    const utf8* extension = "";
    uint32_t fileType = FILE_EXTENSION_UNKNOWN;
    rct_string_id title = STR_NONE;
    switch (_type & 0x0E)
    {
        case LOADSAVETYPE_GAME:
            extension = ".sv6";
            fileType = FILE_EXTENSION_SV6;
            title = isSave ? STR_FILE_DIALOG_TITLE_SAVE_GAME : STR_FILE_DIALOG_TITLE_LOAD_GAME;
            desc.filters[0].name = language_get_string(STR_OPENRCT2_SAVED_GAME);
            desc.filters[0].pattern = getFilterPatternByType(_type, isSave);
            break;

        case LOADSAVETYPE_LANDSCAPE:
            extension = ".sc6";
            fileType = FILE_EXTENSION_SC6;
            title = isSave ? STR_FILE_DIALOG_TITLE_SAVE_LANDSCAPE : STR_FILE_DIALOG_TITLE_LOAD_LANDSCAPE;
            desc.filters[0].name = language_get_string(STR_OPENRCT2_LANDSCAPE_FILE);
            desc.filters[0].pattern = getFilterPatternByType(_type, isSave);
            break;

        case LOADSAVETYPE_SCENARIO:
            extension = ".sc6";
            fileType = FILE_EXTENSION_SC6;
            title = STR_FILE_DIALOG_TITLE_SAVE_SCENARIO;
            desc.filters[0].name = language_get_string(STR_OPENRCT2_SCENARIO_FILE);
            desc.filters[0].pattern = getFilterPatternByType(_type, isSave);
            break;

        case LOADSAVETYPE_TRACK:
            extension = ".td6";
            fileType = FILE_EXTENSION_TD6;
            title = isSave ? STR_FILE_DIALOG_TITLE_SAVE_TRACK : STR_FILE_DIALOG_TITLE_INSTALL_NEW_TRACK_DESIGN;
            desc.filters[0].name = language_get_string(STR_OPENRCT2_TRACK_DESIGN_FILE);
            desc.filters[0].pattern = getFilterPatternByType(_type, isSave);
            break;

        case LOADSAVETYPE_HEIGHTMAP:
            title = STR_FILE_DIALOG_TITLE_LOAD_HEIGHTMAP;
            desc.filters[0].name = language_get_string(STR_OPENRCT2_HEIGHTMAP_FILE);
            desc.filters[0].pattern = getFilterPatternByType(_type, isSave);
            break;
    }

    safe_strcpy(path, _directory, pathSize);
    if (isSave)
    {
        // The file browser requires a file path instead of just a directory
        if (!_defaultPath.empty())
        {
            safe_strcat_path(path, _defaultPath.c_str(), pathSize);
        }
        else
        {
            auto& park = OpenRCT2::GetContext()->GetGameState()->GetPark();
            auto buffer = park.Name;
            if (buffer.empty())
            {
                // Use localised "Unnamed Park" if park name was empty.
                buffer = format_string(STR_UNNAMED_PARK, nullptr);
            }
            safe_strcat_path(path, buffer.c_str(), pathSize);
        }
    }

    desc.initial_directory = _directory;
    desc.type = isSave ? FileDialogType::Save : FileDialogType::Open;
    desc.default_filename = isSave ? path : nullptr;

    // Add 'all files' filter. If the number of filters is increased, this code will need to be adjusted.
    desc.filters[1].name = language_get_string(STR_ALL_FILES);
    desc.filters[1].pattern = "*";

    desc.title = language_get_string(title);
    if (platform_open_common_file_dialog(path, &desc, pathSize))
    {
        // When the given save type was given, Windows still interprets a filename with a dot in its name as a custom extension,
        // meaning files like "My Coaster v1.2" will not get the .td6 extension by default.
        if (isSave && get_file_extension_type(path) != fileType)
            path_append_extension(path, extension, pathSize);

        return true;
    }

    return false;
}

static void window_loadsave_mouseup(rct_window* w, rct_widgetindex widgetIndex)
{
    char path[MAX_PATH];

    bool isSave = (_type & 0x01) == LOADSAVETYPE_SAVE;
    switch (widgetIndex)
    {
        case WIDX_CLOSE:
            window_close(w);
            break;

        case WIDX_UP:
            safe_strcpy(path, _parentDirectory, sizeof(path));
            window_loadsave_populate_list(w, isSave, path, _extension);
            WindowInitScrollWidgets(w);
            w->no_list_items = static_cast<uint16_t>(_listItems.size());
            break;

        case WIDX_NEW_FILE:
            window_text_input_open(
                w, WIDX_NEW_FILE, STR_NONE, STR_FILEBROWSER_FILE_NAME_PROMPT, {}, STR_STRING,
                reinterpret_cast<uintptr_t>(_defaultPath.c_str()), 64);
            break;

        case WIDX_NEW_FOLDER:
            window_text_input_raw_open(w, WIDX_NEW_FOLDER, STR_NONE, STR_FILEBROWSER_FOLDER_NAME_PROMPT, {}, "", 64);
            break;

        case WIDX_BROWSE:
            if (browse(isSave, path, sizeof(path)))
            {
                window_loadsave_select(w, path);
            }
            else
            {
                // If user cancels file dialog, refresh list
                safe_strcpy(path, _directory, sizeof(path));
                window_loadsave_populate_list(w, isSave, path, _extension);
                WindowInitScrollWidgets(w);
                w->no_list_items = static_cast<uint16_t>(_listItems.size());
            }
            break;

        case WIDX_SORT_NAME:
            if (gConfigGeneral.load_save_sort == Sort::NameAscending)
            {
                gConfigGeneral.load_save_sort = Sort::NameDescending;
            }
            else
            {
                gConfigGeneral.load_save_sort = Sort::NameAscending;
            }
            config_save_default();
            window_loadsave_sort_list();
            w->Invalidate();
            break;

        case WIDX_SORT_DATE:
            if (gConfigGeneral.load_save_sort == Sort::DateDescending)
            {
                gConfigGeneral.load_save_sort = Sort::DateAscending;
            }
            else
            {
                gConfigGeneral.load_save_sort = Sort::DateDescending;
            }
            config_save_default();
            window_loadsave_sort_list();
            w->Invalidate();
            break;

        case WIDX_DEFAULT:
            getInitialDirectoryByType(_type, path, sizeof(path));
            window_loadsave_populate_list(w, isSave, path, _extension);
            WindowInitScrollWidgets(w);
            w->no_list_items = static_cast<uint16_t>(_listItems.size());
            break;
    }
}

static void window_loadsave_scrollgetsize(rct_window* w, int32_t scrollIndex, int32_t* width, int32_t* height)
{
    *height = w->no_list_items * SCROLLABLE_ROW_HEIGHT;
}

static void window_loadsave_scrollmousedown(rct_window* w, int32_t scrollIndex, const ScreenCoordsXY& screenCoords)
{
    int32_t selectedItem;

    selectedItem = screenCoords.y / SCROLLABLE_ROW_HEIGHT;
    if (selectedItem >= w->no_list_items)
        return;

    if (_listItems[selectedItem].type == TYPE_DIRECTORY)
    {
        // The selected item is a folder
        int32_t includeNewItem;

        w->no_list_items = 0;
        w->selected_list_item = -1;
        includeNewItem = (_type & 1) == LOADSAVETYPE_SAVE;

        char directory[MAX_PATH];
        safe_strcpy(directory, _listItems[selectedItem].path.c_str(), sizeof(directory));

        window_loadsave_populate_list(w, includeNewItem, directory, _extension);
        WindowInitScrollWidgets(w);

        w->no_list_items = static_cast<uint16_t>(_listItems.size());
    }
    else
    {
        // TYPE_FILE
        // Load or overwrite
        if ((_type & 0x01) == LOADSAVETYPE_SAVE)
            window_overwrite_prompt_open(_listItems[selectedItem].name.c_str(), _listItems[selectedItem].path.c_str());
        else
            window_loadsave_select(w, _listItems[selectedItem].path.c_str());
    }
}

static void window_loadsave_scrollmouseover(rct_window* w, int32_t scrollIndex, const ScreenCoordsXY& screenCoords)
{
    int32_t selectedItem;

    selectedItem = screenCoords.y / SCROLLABLE_ROW_HEIGHT;
    if (selectedItem >= w->no_list_items)
        return;

    w->selected_list_item = selectedItem;

    w->Invalidate();
}

static void window_loadsave_textinput(rct_window* w, rct_widgetindex widgetIndex, char* text)
{
    char path[MAX_PATH];
    bool overwrite;

    if (text == nullptr || text[0] == 0)
        return;

    switch (widgetIndex)
    {
        case WIDX_NEW_FOLDER:
            if (!filename_valid_characters(text))
            {
                context_show_error(STR_ERROR_INVALID_CHARACTERS, STR_NONE, {});
                return;
            }

            safe_strcpy(path, _directory, sizeof(path));
            safe_strcat_path(path, text, sizeof(path));

            if (!platform_ensure_directory_exists(path))
            {
                context_show_error(STR_UNABLE_TO_CREATE_FOLDER, STR_NONE, {});
                return;
            }

            w->no_list_items = 0;
            w->selected_list_item = -1;

            window_loadsave_populate_list(w, (_type & 1) == LOADSAVETYPE_SAVE, path, _extension);
            WindowInitScrollWidgets(w);

            w->no_list_items = static_cast<uint16_t>(_listItems.size());
            w->Invalidate();
            break;

        case WIDX_NEW_FILE:
            safe_strcpy(path, _directory, sizeof(path));
            safe_strcat_path(path, text, sizeof(path));
            path_append_extension(path, _extension, sizeof(path));

            overwrite = false;
            for (auto& item : _listItems)
            {
                if (_stricmp(item.path.c_str(), path) == 0)
                {
                    overwrite = true;
                    break;
                }
            }

            if (overwrite)
                window_overwrite_prompt_open(text, path);
            else
                window_loadsave_select(w, path);
            break;
    }
}

constexpr uint16_t DATE_TIME_GAP = 2;

static void window_loadsave_compute_max_date_width()
{
    // Generate a time object for a relatively wide time: 2000-02-20 00:00:00
    std::tm tm;
    tm.tm_sec = 0;
    tm.tm_min = 0;
    tm.tm_hour = 0;
    tm.tm_mday = 20;
    tm.tm_mon = 2;
    tm.tm_year = 100;
    tm.tm_wday = 5;
    tm.tm_yday = 51;
    tm.tm_isdst = -1;

    std::time_t long_time = mktime(&tm);

    // Check how this date is represented (e.g. 2000-02-20, or 00/02/20)
    std::string date = Platform::FormatShortDate(long_time);
    maxDateWidth = gfx_get_string_width(date.c_str(), FontSpriteBase::MEDIUM) + DATE_TIME_GAP;

    // Some locales do not use leading zeros for months and days, so let's try October, too.
    tm.tm_mon = 10;
    tm.tm_yday = 294;
    long_time = mktime(&tm);

    // Again, check how this date is represented (e.g. 2000-10-20, or 00/10/20)
    date = Platform::FormatShortDate(long_time);
    maxDateWidth = std::max(maxDateWidth, gfx_get_string_width(date.c_str(), FontSpriteBase::MEDIUM) + DATE_TIME_GAP);

    // Time appears to be universally represented with two digits for minutes, so 12:00 or 00:00 should be representable.
    std::string time = Platform::FormatTime(long_time);
    maxTimeWidth = gfx_get_string_width(time.c_str(), FontSpriteBase::MEDIUM) + DATE_TIME_GAP;
}

static void window_loadsave_invalidate(rct_window* w)
{
    window_loadsave_widgets[WIDX_TITLE].right = w->width - 2;
    // close button has to move if it's on the right side
    window_loadsave_widgets[WIDX_CLOSE].left = w->width - 13;
    window_loadsave_widgets[WIDX_CLOSE].right = w->width - 3;

    window_loadsave_widgets[WIDX_BACKGROUND].right = w->width - 1;
    window_loadsave_widgets[WIDX_BACKGROUND].bottom = w->height - 1;
    window_loadsave_widgets[WIDX_RESIZE].top = w->height - 1;
    window_loadsave_widgets[WIDX_RESIZE].right = w->width - 1;
    window_loadsave_widgets[WIDX_RESIZE].bottom = w->height - 1;

    rct_widget* date_widget = &window_loadsave_widgets[WIDX_SORT_DATE];
    date_widget->right = w->width - 5;
    date_widget->left = date_widget->right - (maxDateWidth + maxTimeWidth + (4 * DATE_TIME_GAP) + (SCROLLBAR_WIDTH + 1));

    window_loadsave_widgets[WIDX_SORT_NAME].left = 4;
    window_loadsave_widgets[WIDX_SORT_NAME].right = window_loadsave_widgets[WIDX_SORT_DATE].left - 1;

    window_loadsave_widgets[WIDX_SCROLL].right = w->width - 4;
    window_loadsave_widgets[WIDX_SCROLL].bottom = w->height - 30;

    window_loadsave_widgets[WIDX_BROWSE].top = w->height - 24;
    window_loadsave_widgets[WIDX_BROWSE].bottom = w->height - 6;
}

static void window_loadsave_paint(rct_window* w, rct_drawpixelinfo* dpi)
{
    WindowDrawWidgets(w, dpi);

    if (_shortenedDirectory[0] == '\0')
    {
        shorten_path(_shortenedDirectory, sizeof(_shortenedDirectory), _directory, w->width - 8, FontSpriteBase::MEDIUM);
    }

    // Format text
    thread_local std::string buffer;
    buffer.assign("{BLACK}");
    buffer += _shortenedDirectory;

    // Draw path text
    auto ft = Formatter();
    ft.Add<const char*>(Platform::StrDecompToPrecomp(buffer.data()));
    DrawTextEllipsised(dpi, { w->windowPos.x + 4, w->windowPos.y + 20 }, w->width - 8, STR_STRING, ft);

    // Name button text
    rct_string_id id = STR_NONE;
    if (gConfigGeneral.load_save_sort == Sort::NameAscending)
        id = STR_UP;
    else if (gConfigGeneral.load_save_sort == Sort::NameDescending)
        id = STR_DOWN;

    // Draw name button indicator.
    rct_widget sort_name_widget = window_loadsave_widgets[WIDX_SORT_NAME];
    ft = Formatter();
    ft.Add<rct_string_id>(id);
    DrawTextBasic(
        dpi, w->windowPos + ScreenCoordsXY{ sort_name_widget.left + 11, sort_name_widget.top + 1 }, STR_NAME, ft,
        { COLOUR_GREY });

    // Date button text
    if (gConfigGeneral.load_save_sort == Sort::DateAscending)
        id = STR_UP;
    else if (gConfigGeneral.load_save_sort == Sort::DateDescending)
        id = STR_DOWN;
    else
        id = STR_NONE;

    rct_widget sort_date_widget = window_loadsave_widgets[WIDX_SORT_DATE];
    ft = Formatter();
    ft.Add<rct_string_id>(id);
    DrawTextBasic(
        dpi, w->windowPos + ScreenCoordsXY{ sort_date_widget.left + 5, sort_date_widget.top + 1 }, STR_DATE, ft,
        { COLOUR_GREY });
}

static void window_loadsave_scrollpaint(rct_window* w, rct_drawpixelinfo* dpi, int32_t scrollIndex)
{
    gfx_fill_rect(
        dpi, { { dpi->x, dpi->y }, { dpi->x + dpi->width - 1, dpi->y + dpi->height - 1 } },
        ColourMapA[w->colours[1]].mid_light);
    const int32_t listWidth = w->widgets[WIDX_SCROLL].width();
    const int32_t dateAnchor = w->widgets[WIDX_SORT_DATE].left + maxDateWidth + DATE_TIME_GAP;

    for (int32_t i = 0; i < w->no_list_items; i++)
    {
        int32_t y = i * SCROLLABLE_ROW_HEIGHT;
        if (y > dpi->y + dpi->height)
            break;

        if (y + SCROLLABLE_ROW_HEIGHT < dpi->y)
            continue;

        rct_string_id stringId = STR_BLACK_STRING;

        // If hovering over item, change the color and fill the backdrop.
        if (i == w->selected_list_item)
        {
            stringId = STR_WINDOW_COLOUR_2_STRINGID;
            gfx_filter_rect(dpi, { { 0, y }, { listWidth, y + SCROLLABLE_ROW_HEIGHT } }, FilterPaletteID::PaletteDarken1);
        }
        // display a marker next to the currently loaded game file
        if (_listItems[i].loaded)
        {
            auto ft = Formatter();
            ft.Add<rct_string_id>(STR_RIGHTGUILLEMET);
            DrawTextBasic(dpi, { 0, y }, stringId, ft);
        }

        // Print filename
        auto ft = Formatter();
        ft.Add<rct_string_id>(STR_STRING);
        ft.Add<char*>(_listItems[i].name.c_str());
        int32_t max_file_width = w->widgets[WIDX_SORT_NAME].width() - 10;
        DrawTextEllipsised(dpi, { 10, y }, max_file_width, stringId, ft);

        // Print formatted modified date, if this is a file
        if (_listItems[i].type == TYPE_FILE)
        {
            ft = Formatter();
            ft.Add<rct_string_id>(STR_STRING);
            ft.Add<char*>(_listItems[i].date_formatted.c_str());
            DrawTextEllipsised(dpi, { dateAnchor - DATE_TIME_GAP, y }, maxDateWidth, stringId, ft, { TextAlignment::RIGHT });

            ft = Formatter();
            ft.Add<rct_string_id>(STR_STRING);
            ft.Add<char*>(_listItems[i].time_formatted.c_str());
            DrawTextEllipsised(dpi, { dateAnchor + DATE_TIME_GAP, y }, maxTimeWidth, stringId, ft);
        }
    }
}

static bool list_item_sort(LoadSaveListItem& a, LoadSaveListItem& b)
{
    if (a.type != b.type)
        return a.type - b.type < 0;

    switch (gConfigGeneral.load_save_sort)
    {
        case Sort::NameAscending:
            return strlogicalcmp(a.name.c_str(), b.name.c_str()) < 0;
        case Sort::NameDescending:
            return -strlogicalcmp(a.name.c_str(), b.name.c_str()) < 0;
        case Sort::DateDescending:
            return -difftime(a.date_modified, b.date_modified) < 0;
        case Sort::DateAscending:
            return difftime(a.date_modified, b.date_modified) < 0;
    }
    return strlogicalcmp(a.name.c_str(), b.name.c_str()) < 0;
}

static void window_loadsave_sort_list()
{
    std::sort(_listItems.begin(), _listItems.end(), list_item_sort);
}

static void window_loadsave_populate_list(rct_window* w, int32_t includeNewItem, const char* directory, const char* extension)
{
    utf8 absoluteDirectory[MAX_PATH];
    Path::GetAbsolute(absoluteDirectory, std::size(absoluteDirectory), directory);
    safe_strcpy(_directory, absoluteDirectory, std::size(_directory));
    // Note: This compares the pointers, not values
    if (_extension != extension)
    {
        safe_strcpy(_extension, extension, std::size(_extension));
    }
    _shortenedDirectory[0] = '\0';

    _listItems.clear();

    // Show "new" buttons when saving
    window_loadsave_widgets[WIDX_NEW_FILE].type = includeNewItem ? WindowWidgetType::Button : WindowWidgetType::Empty;
    window_loadsave_widgets[WIDX_NEW_FOLDER].type = includeNewItem ? WindowWidgetType::Button : WindowWidgetType::Empty;

    int32_t drives = platform_get_drives();
    if (str_is_null_or_empty(directory) && drives)
    {
        // List Windows drives
        w->disabled_widgets |= (1ULL << WIDX_NEW_FILE) | (1ULL << WIDX_NEW_FOLDER) | (1ULL << WIDX_UP);
        for (int32_t x = 0; x < 26; x++)
        {
            if (drives & (1 << x))
            {
                // If the drive exists, list it
                LoadSaveListItem newListItem;
                newListItem.path = std::string(1, 'A' + x) + ":" PATH_SEPARATOR;
                newListItem.name = newListItem.path;
                newListItem.type = TYPE_DIRECTORY;

                _listItems.push_back(std::move(newListItem));
            }
        }
    }
    else
    {
        // Remove the separator at the end of the path, if present
        safe_strcpy(_parentDirectory, absoluteDirectory, std::size(_parentDirectory));
        if (_parentDirectory[strlen(_parentDirectory) - 1] == *PATH_SEPARATOR
            || _parentDirectory[strlen(_parentDirectory) - 1] == '/')
            _parentDirectory[strlen(_parentDirectory) - 1] = '\0';

        // Remove everything past the now last separator
        char* ch = strrchr(_parentDirectory, *PATH_SEPARATOR);
        char* posix_ch = strrchr(_parentDirectory, '/');
        ch = ch < posix_ch ? posix_ch : ch;
        if (ch != nullptr)
        {
            *(ch + 1) = '\0';
        }
        else if (drives)
        {
            // If on Windows, clear the entire path to show the drives
            _parentDirectory[0] = '\0';
        }
        else
        {
            // Else, go to the root directory
            snprintf(_parentDirectory, MAX_PATH, "%c", *PATH_SEPARATOR);
        }

        // Disable the Up button if the current directory is the root directory
        if (str_is_null_or_empty(_parentDirectory) && !drives)
            w->disabled_widgets |= (1ULL << WIDX_UP);
        else
            w->disabled_widgets &= ~(1ULL << WIDX_UP);

        // Re-enable the "new" buttons if these were disabled
        w->disabled_widgets &= ~(1ULL << WIDX_NEW_FILE);
        w->disabled_widgets &= ~(1ULL << WIDX_NEW_FOLDER);

        // List all directories
        auto subDirectories = Path::GetDirectories(absoluteDirectory);
        for (const auto& sdName : subDirectories)
        {
            auto subDir = sdName + PATH_SEPARATOR;

            LoadSaveListItem newListItem;
            newListItem.path = Path::Combine(absoluteDirectory, subDir);
            newListItem.name = subDir;
            newListItem.type = TYPE_DIRECTORY;
            newListItem.loaded = false;

            _listItems.push_back(std::move(newListItem));
        }

        // List all files with the wanted extensions
        char filter[MAX_PATH];
        char extCopy[64];
        safe_strcpy(extCopy, extension, std::size(extCopy));
        bool showExtension = false;
        char* extToken = strtok(extCopy, ";");
        while (extToken != nullptr)
        {
            safe_strcpy(filter, directory, std::size(filter));
            safe_strcat_path(filter, "*", std::size(filter));
            path_append_extension(filter, extToken, std::size(filter));

            auto scanner = Path::ScanDirectory(filter, false);
            while (scanner->Next())
            {
                LoadSaveListItem newListItem;
                newListItem.path = scanner->GetPath();
                newListItem.type = TYPE_FILE;
                newListItem.date_modified = platform_file_get_modified_time(newListItem.path.c_str());

                // Cache a human-readable version of the modified date.
                newListItem.date_formatted = Platform::FormatShortDate(newListItem.date_modified);
                newListItem.time_formatted = Platform::FormatTime(newListItem.date_modified);

                // Mark if file is the currently loaded game
                newListItem.loaded = newListItem.path.compare(gCurrentLoadedPath.c_str()) == 0;

                // Remove the extension (but only the first extension token)
                if (!showExtension)
                {
                    newListItem.name = Path::GetFileNameWithoutExtension(newListItem.path);
                }
                else
                {
                    newListItem.name = Path::GetFileName(newListItem.path);
                }

                _listItems.push_back(std::move(newListItem));
            }

            extToken = strtok(nullptr, ";");
            showExtension = true; // Show any extension after the first iteration
        }

        window_loadsave_sort_list();
    }

    w->Invalidate();
}

static void window_loadsave_invoke_callback(int32_t result, const utf8* path)
{
    if (_loadSaveCallback != nullptr)
    {
        _loadSaveCallback(result, path);
    }
}

static void save_path(utf8** config_str, const char* path)
{
    free(*config_str);
    *config_str = path_get_directory(path);
    config_save_default();
}

static bool is_valid_path(const char* path)
{
    char filename[MAX_PATH];
    safe_strcpy(filename, path_get_filename(path), sizeof(filename));

    // HACK This is needed because tracks get passed through with td?
    //      I am sure this will change eventually to use the new FileScanner
    //      which handles multiple patterns
    path_remove_extension(filename);

    return filename_valid_characters(filename);
}

static void window_loadsave_select(rct_window* w, const char* path)
{
    if (!is_valid_path(path))
    {
        context_show_error(STR_ERROR_INVALID_CHARACTERS, STR_NONE, {});
        return;
    }

    char pathBuffer[MAX_PATH];
    safe_strcpy(pathBuffer, path, sizeof(pathBuffer));

    switch (_type & 0x0F)
    {
        case (LOADSAVETYPE_LOAD | LOADSAVETYPE_GAME):
            save_path(&gConfigGeneral.last_save_game_directory, pathBuffer);
            window_loadsave_invoke_callback(MODAL_RESULT_OK, pathBuffer);
            window_close_by_class(WC_LOADSAVE);
            gfx_invalidate_screen();
            break;

        case (LOADSAVETYPE_SAVE | LOADSAVETYPE_GAME):
            save_path(&gConfigGeneral.last_save_game_directory, pathBuffer);
            if (scenario_save(pathBuffer, gConfigGeneral.save_plugin_data ? 1 : 0))
            {
                gScenarioSavePath = pathBuffer;
                gCurrentLoadedPath = pathBuffer;
                gFirstTimeSaving = false;

                window_close_by_class(WC_LOADSAVE);
                gfx_invalidate_screen();

                window_loadsave_invoke_callback(MODAL_RESULT_OK, pathBuffer);
            }
            else
            {
                context_show_error(STR_SAVE_GAME, STR_GAME_SAVE_FAILED, {});
                window_loadsave_invoke_callback(MODAL_RESULT_FAIL, pathBuffer);
            }
            break;

        case (LOADSAVETYPE_LOAD | LOADSAVETYPE_LANDSCAPE):
            save_path(&gConfigGeneral.last_save_landscape_directory, pathBuffer);
            if (Editor::LoadLandscape(pathBuffer))
            {
                gCurrentLoadedPath = pathBuffer;
                gfx_invalidate_screen();
                window_loadsave_invoke_callback(MODAL_RESULT_OK, pathBuffer);
            }
            else
            {
                // Not the best message...
                context_show_error(STR_LOAD_LANDSCAPE, STR_FAILED_TO_LOAD_FILE_CONTAINS_INVALID_DATA, {});
                window_loadsave_invoke_callback(MODAL_RESULT_FAIL, pathBuffer);
            }
            break;

        case (LOADSAVETYPE_SAVE | LOADSAVETYPE_LANDSCAPE):
            save_path(&gConfigGeneral.last_save_landscape_directory, pathBuffer);
            safe_strcpy(gScenarioFileName, pathBuffer, sizeof(gScenarioFileName));
            if (scenario_save(pathBuffer, gConfigGeneral.save_plugin_data ? 3 : 2))
            {
                gCurrentLoadedPath = pathBuffer;
                window_close_by_class(WC_LOADSAVE);
                gfx_invalidate_screen();
                window_loadsave_invoke_callback(MODAL_RESULT_OK, pathBuffer);
            }
            else
            {
                context_show_error(STR_SAVE_LANDSCAPE, STR_LANDSCAPE_SAVE_FAILED, {});
                window_loadsave_invoke_callback(MODAL_RESULT_FAIL, pathBuffer);
            }
            break;

        case (LOADSAVETYPE_SAVE | LOADSAVETYPE_SCENARIO):
        {
            save_path(&gConfigGeneral.last_save_scenario_directory, pathBuffer);
            int32_t parkFlagsBackup = gParkFlags;
            gParkFlags &= ~PARK_FLAGS_SPRITES_INITIALISED;
            gEditorStep = EditorStep::Invalid;
            safe_strcpy(gScenarioFileName, pathBuffer, sizeof(gScenarioFileName));
            int32_t success = scenario_save(pathBuffer, gConfigGeneral.save_plugin_data ? 3 : 2);
            gParkFlags = parkFlagsBackup;

            if (success)
            {
                window_close_by_class(WC_LOADSAVE);
                window_loadsave_invoke_callback(MODAL_RESULT_OK, pathBuffer);
                title_load();
            }
            else
            {
                context_show_error(STR_FILE_DIALOG_TITLE_SAVE_SCENARIO, STR_SCENARIO_SAVE_FAILED, {});
                gEditorStep = EditorStep::ObjectiveSelection;
                window_loadsave_invoke_callback(MODAL_RESULT_FAIL, pathBuffer);
            }
            break;
        }

        case (LOADSAVETYPE_LOAD | LOADSAVETYPE_TRACK):
        {
            save_path(&gConfigGeneral.last_save_track_directory, pathBuffer);
            auto intent = Intent(WC_INSTALL_TRACK);
            intent.putExtra(INTENT_EXTRA_PATH, std::string{ pathBuffer });
            context_open_intent(&intent);
            window_close_by_class(WC_LOADSAVE);
            window_loadsave_invoke_callback(MODAL_RESULT_OK, pathBuffer);
            break;
        }

        case (LOADSAVETYPE_SAVE | LOADSAVETYPE_TRACK):
        {
            save_path(&gConfigGeneral.last_save_track_directory, pathBuffer);

            path_set_extension(pathBuffer, "td6", sizeof(pathBuffer));

            T6Exporter t6Export{ _trackDesign };

            auto success = t6Export.SaveTrack(pathBuffer);

            if (success)
            {
                window_close_by_class(WC_LOADSAVE);
                window_ride_measurements_design_cancel();
                window_loadsave_invoke_callback(MODAL_RESULT_OK, path);
            }
            else
            {
                context_show_error(STR_FILE_DIALOG_TITLE_SAVE_TRACK, STR_TRACK_SAVE_FAILED, {});
                window_loadsave_invoke_callback(MODAL_RESULT_FAIL, path);
            }
            break;
        }

        case (LOADSAVETYPE_LOAD | LOADSAVETYPE_HEIGHTMAP):
            window_close_by_class(WC_LOADSAVE);
            window_loadsave_invoke_callback(MODAL_RESULT_OK, pathBuffer);
            break;
    }
}

#pragma region Overwrite prompt

constexpr int32_t OVERWRITE_WW = 200;
constexpr int32_t OVERWRITE_WH = 100;

enum
{
    WIDX_OVERWRITE_BACKGROUND,
    WIDX_OVERWRITE_TITLE,
    WIDX_OVERWRITE_CLOSE,
    WIDX_OVERWRITE_OVERWRITE,
    WIDX_OVERWRITE_CANCEL
};

static rct_widget window_overwrite_prompt_widgets[] = {
    WINDOW_SHIM_WHITE(STR_FILEBROWSER_OVERWRITE_TITLE, OVERWRITE_WW, OVERWRITE_WH),
    { WindowWidgetType::Button, 0, 10, 94, OVERWRITE_WH - 20, OVERWRITE_WH - 9, STR_FILEBROWSER_OVERWRITE_TITLE, STR_NONE },
    { WindowWidgetType::Button, 0, OVERWRITE_WW - 95, OVERWRITE_WW - 11, OVERWRITE_WH - 20, OVERWRITE_WH - 9,
      STR_SAVE_PROMPT_CANCEL, STR_NONE },
    WIDGETS_END,
};

static void window_overwrite_prompt_mouseup(rct_window* w, rct_widgetindex widgetIndex);
static void window_overwrite_prompt_paint(rct_window* w, rct_drawpixelinfo* dpi);

static rct_window_event_list window_overwrite_prompt_events([](auto& events) {
    events.mouse_up = &window_overwrite_prompt_mouseup;
    events.paint = &window_overwrite_prompt_paint;
});

static char _window_overwrite_prompt_name[256];
static char _window_overwrite_prompt_path[MAX_PATH];

static rct_window* window_overwrite_prompt_open(const char* name, const char* path)
{
    rct_window* w;

    window_close_by_class(WC_LOADSAVE_OVERWRITE_PROMPT);

    w = WindowCreateCentred(
        OVERWRITE_WW, OVERWRITE_WH, &window_overwrite_prompt_events, WC_LOADSAVE_OVERWRITE_PROMPT, WF_STICK_TO_FRONT);
    w->widgets = window_overwrite_prompt_widgets;
    w->enabled_widgets = (1ULL << WIDX_CLOSE) | (1ULL << WIDX_OVERWRITE_CANCEL) | (1ULL << WIDX_OVERWRITE_OVERWRITE);

    WindowInitScrollWidgets(w);

    w->flags |= WF_TRANSPARENT;
    w->colours[0] = TRANSLUCENT(COLOUR_BORDEAUX_RED);

    safe_strcpy(_window_overwrite_prompt_name, name, sizeof(_window_overwrite_prompt_name));
    safe_strcpy(_window_overwrite_prompt_path, path, sizeof(_window_overwrite_prompt_path));

    return w;
}

static void window_overwrite_prompt_mouseup(rct_window* w, rct_widgetindex widgetIndex)
{
    rct_window* loadsaveWindow;

    switch (widgetIndex)
    {
        case WIDX_OVERWRITE_OVERWRITE:
            loadsaveWindow = window_find_by_class(WC_LOADSAVE);
            if (loadsaveWindow != nullptr)
                window_loadsave_select(loadsaveWindow, _window_overwrite_prompt_path);
            // As the window_loadsave_select function can change the order of the
            // windows we can't use window_close(w).
            window_close_by_class(WC_LOADSAVE_OVERWRITE_PROMPT);
            break;

        case WIDX_OVERWRITE_CANCEL:
        case WIDX_OVERWRITE_CLOSE:
            window_close(w);
            break;
    }
}

static void window_overwrite_prompt_paint(rct_window* w, rct_drawpixelinfo* dpi)
{
    WindowDrawWidgets(w, dpi);

    auto ft = Formatter();
    ft.Add<rct_string_id>(STR_STRING);
    ft.Add<char*>(_window_overwrite_prompt_name);

    ScreenCoordsXY stringCoords(w->windowPos.x + w->width / 2, w->windowPos.y + (w->height / 2) - 3);
    DrawTextWrapped(dpi, stringCoords, w->width - 4, STR_FILEBROWSER_OVERWRITE_PROMPT, ft, { TextAlignment::CENTRE });
}

#pragma endregion
