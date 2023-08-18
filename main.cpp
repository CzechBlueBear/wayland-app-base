#include "app.hpp"

int main(int argc, char** argv) {
    WaylandApp app;

    if (!app.connect()) {
        return 1;
    }

    while(true) {
        app.handle_events();
    }

    app.disconnect();
    return 0;
}
