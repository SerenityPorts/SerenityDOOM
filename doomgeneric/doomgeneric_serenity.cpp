#include "doomkeys.h"
#include "doomgeneric.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>

#include <LibGfx/Bitmap.h>
#include <LibGUI/Window.h>
#include <LibGUI/Widget.h>
#include <LibGUI/Event.h>
#include <LibGUI/Painter.h>
#include <LibCore/EventLoop.h>
#include <LibCore/Timer.h>
#include <LibGUI/Menu.h>
#include <LibGUI/MenuBar.h>

static RefPtr<GUI::Window> g_window;
static RefPtr<Gfx::Bitmap> g_bitmap;

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

extern "C" void DG_SetFullscreen(bool fullscreen);

static unsigned char convertToDoomKey(const GUI::KeyEvent& event)
{
    unsigned char key = 0;
    switch (event.key()) {
    case Key_Return:
        key = KEY_ENTER;
        break;
    case Key_Escape:
        key = KEY_ESCAPE;
        break;
    case Key_Left:
        key = KEY_LEFTARROW;
        break;
    case Key_Right:
        key = KEY_RIGHTARROW;
        break;
    case Key_Up:
        key = KEY_UPARROW;
        break;
    case Key_Down:
        key = KEY_DOWNARROW;
        break;
    case Key_Control:
        key = KEY_FIRE;
        break;
    case Key_Space:
        key = KEY_USE;
        break;
    case Key_LeftShift:
    case Key_RightShift:
        key = KEY_RSHIFT;
        break;
    case Key_Alt:
        key = KEY_RALT;
        break;
    default:
        if (!event.text().is_empty())
            key = tolower(event.text()[0]);
        break;
    }

    return key;
}

static void addKeyToQueue(const GUI::KeyEvent& event)
{
    bool pressed = event.type() == GUI::Event::KeyDown;
    unsigned char key = convertToDoomKey(event);

    unsigned short keyData = (pressed << 8) | key;

    s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
    s_KeyQueueWriteIndex++;
    s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

class DoomWidget final : public GUI::Widget {
    C_OBJECT(DoomWidget)
public:
    DoomWidget() {}

    virtual void keydown_event(GUI::KeyEvent&) override;
    virtual void keyup_event(GUI::KeyEvent&) override;
    virtual void paint_event(GUI::PaintEvent&) override;
};

void DoomWidget::keydown_event(GUI::KeyEvent& event)
{
    addKeyToQueue(event);
    GUI::Widget::keydown_event(event);
}

void DoomWidget::keyup_event(GUI::KeyEvent& event)
{
    addKeyToQueue(event);
    GUI::Widget::keyup_event(event);
}

void DoomWidget::paint_event(GUI::PaintEvent& event)
{
    GUI::Painter painter(*this);
    painter.add_clip_rect(event.rect());

    painter.draw_scaled_bitmap(rect(), *g_bitmap, g_bitmap->rect());
}

static RefPtr<DoomWidget> g_doom_widget;

extern "C" void DG_Init()
{
    memset(s_KeyQueue, 0, KEYQUEUE_SIZE * sizeof(unsigned short));

    // window creation

    g_bitmap = Gfx::Bitmap::create_wrapper(Gfx::BitmapFormat::Indexed8, Gfx::IntSize(DOOMGENERIC_RESX, DOOMGENERIC_RESY), 1, DOOMGENERIC_RESX, DG_ScreenBuffer);

    g_window = GUI::Window::construct();
    g_window->set_double_buffering_enabled(false);
    g_window->set_rect(100, 100, DOOMGENERIC_RESX * 2, DOOMGENERIC_RESY * 2);
    g_window->set_icon(Gfx::Bitmap::load_from_file("/res/icons/16x16/doom.png"));

    auto menubar = GUI::MenuBar::construct();

    auto& doom_menu = menubar->add_menu("DOOM");
    doom_menu.add_action(GUI::CommonActions::make_quit_action([](auto&) {
        exit(0);
    }));

    auto& view_menu = menubar->add_menu("View");
    auto fullscreen_action = GUI::CommonActions::make_fullscreen_action([&](auto& action) {
        action.set_checked(!action.is_checked());
        DG_SetFullscreen(action.is_checked());
    });
    fullscreen_action->set_checkable(true);
    view_menu.add_action(fullscreen_action);

    g_window->set_menubar(move(menubar));

    g_doom_widget = DoomWidget::construct();
    g_window->set_main_widget(g_doom_widget);

    g_window->show();
}

extern "C" void DG_DrawFrame()
{
    g_doom_widget->update();
}

extern "C" void DG_PumpEventLoop()
{
    Core::EventLoop::current().pump(Core::EventLoop::WaitMode::PollForEvents);
}

extern "C" void DG_SleepMs(uint32_t ms)
{
    usleep (ms * 1000);
}

extern "C" uint32_t DG_GetTicksMs()
{
    struct timeval  tp;
    struct timezone tzp;

    gettimeofday(&tp, &tzp);

    return (tp.tv_sec * 1000) + (tp.tv_usec / 1000); /* return milliseconds */
}

extern "C" int DG_GetKey(int* pressed, unsigned char* doomKey)
{
    if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex)
    {
        //key queue is empty

        return 0;
    }
    else
    {
        unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
        s_KeyQueueReadIndex++;
        s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

        *pressed = keyData >> 8;
        *doomKey = keyData & 0xFF;

        return 1;
    }
}

extern "C" void DG_SetWindowTitle(const char * title)
{
    if (g_window)
        g_window->set_title(title);
}

extern "C" void DG_SetPalette(const struct color* colors)
{
    for (int i = 0; i < 256; ++i) {
        auto& c = colors[i];
        g_bitmap->set_palette_color(i, Color(c.r, c.g, c.b));
    }
}

extern "C" void DG_SetFullscreen(bool fullscreen)
{
    g_window->set_fullscreen(fullscreen);
}
