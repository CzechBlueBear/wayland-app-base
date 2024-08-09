#include "app.hpp"

int main(int argc, char** argv) {
    WaylandApp app;
    app.enter_event_loop();
    return 0;
}
