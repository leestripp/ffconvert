
#include "MainWindow.h"
#include "ffconvert.h"
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/label.h>
#include <gtkmm/stringobject.h>
#include <iostream>
#include <filesystem>
#include <set>
#include <vector>
#include <stdexcept>
#include <thread>

namespace fs = std::filesystem;

MainWindow::MainWindow()
: m_mainPaned(Gtk::Orientation::HORIZONTAL),
  m_leftBox(Gtk::Orientation::VERTICAL, 6),
  m_leftButtonsBox(Gtk::Orientation::HORIZONTAL, 6),
  m_addFolderButton("Add Folder..."),
  m_clearListButton("Clear List"),
  m_rightBox(Gtk::Orientation::VERTICAL, 12),
  m_exportSettingsLabel("Export Settings"),
  m_exportLocationLabel("Export Location"),
  m_exportLocationBox(Gtk::Orientation::HORIZONTAL, 6),
  m_browseButton("Browse..."),
  m_progressBar(),
  m_exportButton("Export"),
  m_scaleLabel("Scale"),
  m_scaleCheckButton("Enable Scale"),
  m_scaleBox(Gtk::Orientation::HORIZONTAL, 6),
  m_widthLabel("Width:"),
  m_heightLabel("Height:")
{
    set_title("FFmpeg Video Converter");
    set_default_size(800, 600);
    set_child(m_mainPaned);

    // --- Left Pane ---
    m_leftBox.set_margin(12);
    m_mainPaned.set_start_child(m_leftBox);
    
    // Make buttons expand to fill space
    m_addFolderButton.set_hexpand(true);
    m_clearListButton.set_hexpand(true);
    m_leftButtonsBox.append(m_addFolderButton);
    m_leftButtonsBox.append(m_clearListButton);
    m_leftBox.append(m_leftButtonsBox);

    m_fileListScrolledWindow.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    m_fileListScrolledWindow.set_child(m_fileListBox);
    m_fileListScrolledWindow.set_expand();
    m_leftBox.append(m_fileListScrolledWindow);

    // --- Right Pane ---
    m_rightBox.set_margin(12);
    m_mainPaned.set_end_child(m_rightBox);
    m_mainPaned.set_position(350);

    m_exportSettingsLabel.set_halign(Gtk::Align::START);
    m_rightBox.append(m_exportSettingsLabel);

    std::vector<Glib::ustring> formats = {"H.264 (MP4)", "H.265 (MP4)", "Avid DNxHR (MOV)"};
    m_formatModel = Gtk::StringList::create(formats);
    m_formatDropDown.set_model(m_formatModel);
    m_formatDropDown.set_selected(0);
    m_rightBox.append(m_formatDropDown);

    // Scale options
    m_scaleLabel.set_halign(Gtk::Align::START);
    m_scaleLabel.set_margin_top(12);
    m_rightBox.append(m_scaleLabel);

    m_scaleCheckButton.set_margin_top(6);
    m_scaleCheckButton.set_active(false);
    m_rightBox.append(m_scaleCheckButton);

    // Width and height inputs (initially disabled)
    m_widthLabel.set_label("Width:");
    m_widthLabel.set_margin_start(12);
    m_widthEntry.set_placeholder_text("e.g. 1920");
    m_widthEntry.set_max_width_chars(8);
    m_widthEntry.set_sensitive(false);
    m_heightLabel.set_label("Height:");
    m_heightLabel.set_margin_start(12);
    m_heightEntry.set_placeholder_text("e.g. 1080");
    m_heightEntry.set_max_width_chars(8);
    m_heightEntry.set_sensitive(false);
    m_scaleBox.append(m_widthLabel);
    m_scaleBox.append(m_widthEntry);
    m_scaleBox.append(m_heightLabel);
    m_scaleBox.append(m_heightEntry);
    m_rightBox.append(m_scaleBox);

    m_exportLocationLabel.set_halign(Gtk::Align::START);
    m_exportLocationLabel.set_margin_top(12);
    m_rightBox.append(m_exportLocationLabel);
    m_exportLocationEntry.set_placeholder_text("Choose an output folder...");
    m_exportLocationEntry.set_editable(false);
    // The entry should expand horizontally to use available space.
    m_exportLocationEntry.set_hexpand(true);
    m_exportLocationBox.append(m_exportLocationEntry);
    m_exportLocationBox.append(m_browseButton);
    m_rightBox.append(m_exportLocationBox);

    // Add a spacer to push the progress bar and export button to the bottom
    auto& spacer = *Gtk::make_managed<Gtk::Box>();
    spacer.set_vexpand();
    m_rightBox.append(spacer);

    // Setup the progress bar
    m_progressBar.set_margin_bottom(6);
    m_progressBar.set_show_text(true);
    m_progressBar.set_text("Waiting to export...");
    // The progress bar is now always visible
    m_rightBox.append(m_progressBar);

    m_exportButton.add_css_class("suggested-action");
    m_exportButton.set_sensitive(false);
    m_rightBox.append(m_exportButton);

    // --- Signal Connections ---
    m_addFolderButton.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_add_folder_button_clicked));
    m_clearListButton.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_clear_list_button_clicked));
    m_browseButton.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_browse_button_clicked));
    m_exportButton.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_export_button_clicked));
    m_scaleCheckButton.signal_toggled().connect(sigc::mem_fun(*this, &MainWindow::on_scale_checkbox_toggled));
    
    m_dispatcher.connect(sigc::mem_fun(*this, &MainWindow::on_export_finished));
    m_progress_dispatcher.connect(sigc::mem_fun(*this, &MainWindow::on_progress_updated));
}

MainWindow::~MainWindow()
{
    if (m_export_thread.joinable())
    {
        m_export_thread.join();
    }
}

void MainWindow::on_clear_list_button_clicked()
{
    while (auto row = m_fileListBox.get_row_at_index(0)) {
        m_fileListBox.remove(*row);
    }
    m_video_files.clear();
    update_export_button_sensitivity();
}

void MainWindow::on_add_folder_button_clicked()
{
    auto dialog = new Gtk::FileChooserDialog("Please choose a folder", Gtk::FileChooser::Action::SELECT_FOLDER);
    dialog->set_transient_for(*this);
    dialog->set_modal(true);
    dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
    dialog->add_button("_Open", Gtk::ResponseType::OK);
	
    dialog->signal_response().connect(
        [this, dialog](int response_id) {
            if (response_id == Gtk::ResponseType::OK) {
                auto folder = dialog->get_file();
                if (folder) add_videos_from_folder(folder->get_path());
            }
            delete dialog;
        });
    dialog->show();
}

void MainWindow::on_browse_button_clicked()
{
    auto dialog = new Gtk::FileChooserDialog("Choose export location", Gtk::FileChooser::Action::SELECT_FOLDER);
    dialog->set_transient_for(*this);
    dialog->set_modal(true);
    dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
    dialog->add_button("_Choose", Gtk::ResponseType::OK);

    dialog->signal_response().connect(
        [this, dialog](int response_id) {
            if (response_id == Gtk::ResponseType::OK) {
                auto folder = dialog->get_file();
                if (folder) {
                    m_export_path = folder->get_path();
                    m_exportLocationEntry.set_text(m_export_path);
                    update_export_button_sensitivity();
                }
            }
            delete dialog;
        });
    dialog->show();
}

void MainWindow::add_videos_from_folder(const std::string& folder_path)
{
    const std::set<std::string> video_extensions = {
        ".mp4", ".mkv", ".webm", ".flv", ".vob", ".ogv", ".ogg", ".drc",
        ".gifv", ".mng", ".avi", ".mov", ".qt", ".wmv", ".yuv", ".rm",
        ".rmvb", ".asf", ".amv", ".m4p", ".m4v", ".mpg", ".mp2", ".mpeg",
        ".mpe", ".mpv", ".svi", ".3gp", ".3g2", ".mxf", ".roq", ".nsv"
    };

    try {
        for (const auto& entry : fs::directory_iterator(folder_path)) {
            if (entry.is_regular_file()) {
                fs::path file_path = entry.path();
                std::string extension = file_path.extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c){ return std::tolower(c); });

                if (video_extensions.count(extension)) {
                    // Prevent adding duplicate files to the list
                    bool found = false;
                    for(const auto& existing_file : m_video_files) {
                        if (existing_file == file_path.string()) {
                            found = true;
                            break;
                        }
                    }

                    if (!found) {
                        m_video_files.push_back(file_path.string());
                        auto row = Gtk::make_managed<Gtk::Label>(file_path.filename().string());
                        row->set_halign(Gtk::Align::START);
                        row->set_margin(6);
                        m_fileListBox.append(*row);
                    }
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }
    update_export_button_sensitivity();
}

void MainWindow::update_export_button_sensitivity()
{
    const bool ready_to_export = !m_video_files.empty() && !m_export_path.empty();
    m_exportButton.set_sensitive(ready_to_export);
}

void MainWindow::set_ui_interactive(bool interactive)
{
    m_addFolderButton.set_sensitive(interactive);
    m_clearListButton.set_sensitive(interactive);
    m_fileListBox.set_sensitive(interactive);
    m_formatDropDown.set_sensitive(interactive);
    m_browseButton.set_sensitive(interactive);
    m_scaleCheckButton.set_sensitive(interactive);
    // Width/Height entries depend on both interactive AND checkbox state
    if (interactive && m_scaleCheckButton.get_active()) {
        m_widthEntry.set_sensitive(true);
        m_heightEntry.set_sensitive(true);
    } else {
        m_widthEntry.set_sensitive(false);
        m_heightEntry.set_sensitive(false);
    }
    
    // The export button's sensitivity depends on other factors when re-enabling
    if (interactive) {
        update_export_button_sensitivity();
    } else {
        m_exportButton.set_sensitive(false);
    }
}

void MainWindow::on_export_button_clicked()
{
    if (m_export_thread.joinable()) return;

    auto selected_item_obj = m_formatDropDown.get_selected_item();
    if(!selected_item_obj) return;
    auto string_item = std::dynamic_pointer_cast<Gtk::StringObject>(selected_item_obj);
    if (!string_item) return;

    // Get scale settings if enabled
    if (m_scaleCheckButton.get_active()) {
        try {
            m_export_scale_width = std::stoi(m_widthEntry.get_text());
            m_export_scale_height = std::stoi(m_heightEntry.get_text());
        } catch (const std::exception&) {
            // Invalid input, ignore scale
            m_export_scale_width = 0;
            m_export_scale_height = 0;
        }
    } else {
        m_export_scale_width = 0;
        m_export_scale_height = 0;
    }

    m_files_to_export = m_video_files;
    m_export_destination = m_export_path;
    m_export_format = string_item->get_string();
    m_files_processed_count = 0;

    set_ui_interactive(false);
    m_exportButton.set_label("Exporting...");

    m_progressBar.set_fraction(0.0);
    m_progressBar.set_text("Starting export...");

    m_export_thread = std::thread(&MainWindow::export_thread_worker, this);
}

void MainWindow::export_thread_worker()
{
    std::cout << "--- Starting Export Thread ---" << std::endl;
    std::cout << "Format: " << m_export_format << std::endl;
    std::cout << "Export Location: " << m_export_destination << std::endl;
    
    for (const auto& input_path_str : m_files_to_export) {
        fs::path input_path(input_path_str);
        fs::path output_path = fs::path(m_export_destination) / input_path.filename();

        if (m_export_format.find("(MP4)") != std::string::npos) {
            output_path.replace_extension(".mp4");
        } else if (m_export_format.find("(MOV)") != std::string::npos) {
            output_path.replace_extension(".mov");
        }

        std::cout << "Converting " << input_path.filename().string() << "..." << std::endl;
        if (FFConvert::convert_video_file(input_path.string(), output_path.string(), m_export_format, m_export_scale_width, m_export_scale_height)) {
            std::cout << "  Success." << std::endl;
        } else {
            std::cout << "  FAILED." << std::endl;
        }

        // Increment counter and notify the main thread to update the UI
        m_files_processed_count++;
        m_progress_dispatcher.emit();
    }

    std::cout << "--- Export Thread Finished ---" << std::endl;

    m_dispatcher.emit();
}

void MainWindow::on_export_finished()
{
    set_ui_interactive(true);
    m_exportButton.set_label("Export");

    m_progressBar.set_fraction(1.0);
    m_progressBar.set_text("Export complete!");

    if (m_export_thread.joinable()) {
        m_export_thread.join();
    }
}

void MainWindow::on_progress_updated()
{
    if (m_files_to_export.empty()) return;

    const int processed = m_files_processed_count.load();
    const int total = m_files_to_export.size();
    const double fraction = static_cast<double>(processed) / total;

    m_progressBar.set_fraction(fraction);
    
    Glib::ustring text = Glib::ustring::compose("Exporting %1 of %2...", processed, total);
    m_progressBar.set_text(text);
}

void MainWindow::on_scale_checkbox_toggled()
{
    const bool enabled = m_scaleCheckButton.get_active();
    m_widthEntry.set_sensitive(enabled);
    m_heightEntry.set_sensitive(enabled);
}
