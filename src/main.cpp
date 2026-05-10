#include "MainWindow.h"
#include <gtkmm/application.h>

int main(int argc, char* argv[])
{
    // The application ID should be in reverse-DNS format
    auto app = Gtk::Application::create("com.example.ffconvert");

    // This will create and show a new MainWindow and then run the main loop.
	return app->make_window_and_run<MainWindow>(argc, argv);
}

