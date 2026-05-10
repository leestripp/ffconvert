#pragma once

#include <gtkmm/applicationwindow.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/paned.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/stringlist.h>

#include <glibmm/dispatcher.h>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

class MainWindow : public Gtk::ApplicationWindow
{
public:
    MainWindow();
    virtual ~MainWindow();

protected:
    // Signal handlers:
    void on_add_folder_button_clicked();
    void on_clear_list_button_clicked();
    void on_browse_button_clicked();
    void on_export_button_clicked();
    void on_export_finished(); // Called by the dispatcher on the main thread
    void on_progress_updated(); // Called by the progress dispatcher
    void on_scale_checkbox_toggled();

    // Helper methods:
    void add_videos_from_folder(const std::string& folder_path);
    void update_export_button_sensitivity();
    void set_ui_interactive(bool interactive);

    // The actual conversion work, run in a separate thread
    void export_thread_worker();

    // Main layout widget
    Gtk::Paned m_mainPaned;

    // Left pane widgets (File List)
    Gtk::Box m_leftBox;
    Gtk::Box m_leftButtonsBox;
    Gtk::Button m_addFolderButton;
    Gtk::Button m_clearListButton;
    Gtk::ScrolledWindow m_fileListScrolledWindow;
    Gtk::ListBox m_fileListBox;

    // Right pane widgets (Export Options)
    Gtk::Box m_rightBox;
    Gtk::Label m_exportSettingsLabel;
    Gtk::DropDown m_formatDropDown;
    Gtk::Label m_exportLocationLabel;
    Gtk::Box m_exportLocationBox;
    Gtk::Entry m_exportLocationEntry;
    Gtk::Button m_browseButton;
    Gtk::ProgressBar m_progressBar;
    Gtk::Button m_exportButton;

    // Scale options
    Gtk::Label m_scaleLabel;
    Gtk::CheckButton m_scaleCheckButton;
    Gtk::Box m_scaleBox;
    Gtk::Label m_widthLabel;
    Gtk::Entry m_widthEntry;
    Gtk::Label m_heightLabel;
    Gtk::Entry m_heightEntry;

    // Model for the DropDown
    Glib::RefPtr<Gtk::StringList> m_formatModel;

    // --- Threading and Data ---
    // Store file paths and export location from UI interaction
    std::vector<std::string> m_video_files;
    std::string m_export_path;

    // Data for the current export job, copied to be thread-safe
    std::vector<std::string> m_files_to_export;
    std::string m_export_format;
    std::string m_export_destination;
    int m_export_scale_width = 0;
    int m_export_scale_height = 0;
    std::atomic<int> m_files_processed_count{0};

    // Thread management
    std::thread m_export_thread;
    Glib::Dispatcher m_dispatcher;
    Glib::Dispatcher m_progress_dispatcher;
};
