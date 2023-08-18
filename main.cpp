#include "app.hpp"

int main(int argc, char** argv) {
    WaylandApp app;
    if (!app.connect()) {
        return 1;
    }

    bool redraw_needed = true;

    while(true) {
        if (redraw_needed) {
            auto buffer = app.allocate_buffer(WaylandApp::DEFAULT_WINDOW_WIDTH, WaylandApp::DEFAULT_WINDOW_HEIGHT);
            app.present_buffer(buffer);
            redraw_needed = false;
        }
        app.handle_events();
    }

    app.disconnect();
    return 0;
}
