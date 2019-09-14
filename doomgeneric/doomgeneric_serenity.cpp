#include "doomkeys.h"
#include "doomgeneric.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>

#include <LibDraw/GraphicsBitmap.h>
#include <LibGUI/GWindow.h>
#include <LibGUI/GWidget.h>
#include <LibGUI/GEvent.h>
#include <LibGUI/GPainter.h>
#include <LibCore/CEventLoop.h>
#include <LibCore/CTimer.h>

static GWindow* g_window;
static RefPtr<GraphicsBitmap> g_bitmap;

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static unsigned char convertToDoomKey(const GKeyEvent& event)
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
	default:
        if (!event.text().is_empty())
		    key = tolower(event.text()[0]);
		break;
	}

	return key;
}

static void addKeyToQueue(const GKeyEvent& event)
{
    bool pressed = event.type() == GEvent::KeyDown;
	unsigned char key = convertToDoomKey(event);

	unsigned short keyData = (pressed << 8) | key;

	s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
	s_KeyQueueWriteIndex++;
	s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

class DoomWidget final : public GWidget {
    C_OBJECT(DoomWidget)
public:
    DoomWidget(GWidget* parent = nullptr)
        : GWidget(parent)
    {
    }

    virtual void keydown_event(GKeyEvent&) override;
    virtual void keyup_event(GKeyEvent&) override;
    virtual void paint_event(GPaintEvent&) override;
};

void DoomWidget::keydown_event(GKeyEvent& event)
{
    addKeyToQueue(event);
    GWidget::keydown_event(event);
}

void DoomWidget::keyup_event(GKeyEvent& event)
{
    addKeyToQueue(event);
    GWidget::keyup_event(event);
}

void DoomWidget::paint_event(GPaintEvent& event)
{
    GPainter painter(*this);
    painter.add_clip_rect(event.rect());

    painter.draw_scaled_bitmap(rect(), *g_bitmap, g_bitmap->rect());
}

static DoomWidget* g_doom_widget;

extern "C" void DG_Init()
{
	memset(s_KeyQueue, 0, KEYQUEUE_SIZE * sizeof(unsigned short));

    // window creation

    g_bitmap = GraphicsBitmap::create_wrapper(GraphicsBitmap::Format::RGB32, Size(DOOMGENERIC_RESX, DOOMGENERIC_RESY), DOOMGENERIC_RESX * 4, DG_ScreenBuffer);

    g_window = new GWindow;
    g_window->set_double_buffering_enabled(false);
    g_window->set_rect(100, 100, DOOMGENERIC_RESX * 2, DOOMGENERIC_RESY * 2);

    g_doom_widget = new DoomWidget;
    g_window->set_main_widget(g_doom_widget);

    new CTimer(33, [] {
        g_doom_widget->update();
    });

    g_window->show();
}


extern "C" void DG_DrawFrame()
{
    CEventLoop::current().pump(CEventLoop::WaitMode::PollForEvents);
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
