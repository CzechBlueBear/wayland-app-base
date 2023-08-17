#include "app.hpp"

const int WINDOW_WIDTH = 1920;
const int WINDOW_HEIGHT = 1280;

int main(int argc, char** argv) {
    WaylandApp app;
    if (!app.connect()) {
        return 1;
    }
    if (!app.create_buffer_pool(WINDOW_WIDTH, WINDOW_HEIGHT)) {
        return 1;
    }
    app.attach_and_redraw_buffer(0);
    app.handle_events();
    app.disconnect();
    return 0;
}
