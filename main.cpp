#include "app.hpp"

int main(int argc, char** argv) {
    WaylandApp app;
    if (!app.is_good()) {
        return 1;
    }

    app.enter_event_loop();

    return 0;
}
