#include "app.hpp"

int main(int argc, char** argv) {
    WaylandApp app;

    if (!app.connect()) {
        return 1;
    }

    app.enter_event_loop();

    app.disconnect();
    return 0;
}
