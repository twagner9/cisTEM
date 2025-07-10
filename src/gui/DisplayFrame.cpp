#include "../core/gui_core_headers.h"

DisplayFrame::DisplayFrame(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style)
    : DisplayFrameParent(NULL, wxID_ANY, title, pos, size, style) {

    is_fullscreen = false;
    remember_path = wxGetCwd( );

    cisTEMDisplayPanel->EnableCanChangeFile( );
    cisTEMDisplayPanel->EnableCanCloseTabs( );
    cisTEMDisplayPanel->EnableCanMoveTabs( );
    cisTEMDisplayPanel->EnableCanFFT( );
    cisTEMDisplayPanel->Initialise( );

    // Set this bool to true so that DisplayPanel knows that this frame's panel is from the cisTEM_display program
    this->cisTEMDisplayPanel->is_from_display_program = true;

    int screen_x_size = wxSystemSettings::GetMetric(wxSYS_SCREEN_X);
    int screen_y_size = wxSystemSettings::GetMetric(wxSYS_SCREEN_Y);
    int x_offset;
    int y_offset;

    if ( screen_x_size > 1920 && screen_y_size > 1080 ) {
        x_offset = (screen_x_size - 1920) / 2;
        y_offset = (screen_y_size - 1080) / 2;

        if ( x_offset < 0 )
            x_offset = 0;
        if ( y_offset < 0 )
            y_offset = 0;

        SetSize(x_offset, y_offset, 1920, 1080);
    }
    else {
        Maximize(true);
    }

    Bind(wxEVT_CHAR_HOOK, &DisplayFrame::OnCharHook, this);
}

DisplayFrame::~DisplayFrame( ) {
}

void DisplayFrame::OnCharHook(wxKeyEvent& event) {
    if ( event.GetKeyCode( ) == WXK_F11 ) {
        if ( is_fullscreen == true ) {
            ShowFullScreen(false);
            is_fullscreen = false;
        }
        else {
            ShowFullScreen(true);
            is_fullscreen = true;
        }
    }
    event.Skip( );
}

void DisplayFrame::OnFileOpenClick(wxCommandEvent& event) {
    cisTEMDisplayPanel->OnOpen(event);
}

void DisplayFrame::OnCloseTabClick(wxCommandEvent& event) {
    if ( cisTEMDisplayPanel->ReturnCurrentPanel( ) != NULL ) {
        cisTEMDisplayPanel->my_notebook->DeletePage(cisTEMDisplayPanel->my_notebook->GetSelection( ));
    }
    if ( cisTEMDisplayPanel->my_notebook->GetSelection( ) == wxNOT_FOUND ) {
        DisableAllToolbarButtons( );
    }
}

void DisplayFrame::OnExitClick(wxCommandEvent& event) {
    this->Destroy( );
}

void DisplayFrame::OnLocationNumberClick(wxCommandEvent& event) {
    if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->show_label )
        cisTEMDisplayPanel->ReturnCurrentPanel( )->show_label = false;
    else
        cisTEMDisplayPanel->ReturnCurrentPanel( )->show_label = true;

    cisTEMDisplayPanel->ReturnCurrentPanel( )->ReDrawPanel( );
}

void DisplayFrame::OnImageSelectionModeClick(wxCommandEvent& event) {
    // if we are already in selections mode, we don't want to do anything, so
    // make a check.
    bool switch_mode = true;
    // first check if there are any selections
    // if there are, then perform the check below; othrewise, just update the GUI
    if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->number_of_coords > 0 ) {
        if ( IssueSelectionModeChangeWarning( ) )
            cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->Clear( );
        else
            switch_mode = false;
    }

    if ( switch_mode ) {
        cisTEMDisplayPanel->ReturnCurrentPanel( )->image_picking_mode_enabled    = true;
        cisTEMDisplayPanel->ReturnCurrentPanel( )->coords_picking_mode_enabled   = false;
        cisTEMDisplayPanel->ReturnCurrentPanel( )->filament_picking_mode_enabled = false;
        SelectInvertSelection->Enable(true);

        ClearTextFileFromPanel( );
        Refresh( );
        Update( );
    }
}

void DisplayFrame::OnCoordsSelectionModeClick(wxCommandEvent& event) {
    bool switch_mode = true;
    if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->number_of_selections > 0 || cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->number_of_coords > 0 ) {
        if ( IssueSelectionModeChangeWarning( ) ) {
            if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->image_picking_mode_enabled )
                cisTEMDisplayPanel->ReturnCurrentPanel( )->ClearSelection(false);
            else
                cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->Clear( );
        }
        else {
            switch_mode = false;
        }
    }

    if ( switch_mode ) {
        cisTEMDisplayPanel->ReturnCurrentPanel( )->coords_picking_mode_enabled        = true;
        cisTEMDisplayPanel->ReturnCurrentPanel( )->image_picking_mode_enabled         = false;
        cisTEMDisplayPanel->ReturnCurrentPanel( )->filament_picking_mode_enabled      = false;
        cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->last_selected_image = -1;
        SelectInvertSelection->Enable(false);

        ClearTextFileFromPanel( );
        Refresh( );
        Update( );
    }
}

void DisplayFrame::OnFilamentSelectionModeClick(wxCommandEvent& event) {
    bool switch_mode = true;
    if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->number_of_selections > 0 || cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->number_of_coords > 0 ) {
        if ( IssueSelectionModeChangeWarning( ) ) {
            if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->image_picking_mode_enabled )
                cisTEMDisplayPanel->ReturnCurrentPanel( )->ClearSelection(false);
            else
                cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->Clear( );
        }
        else {
            switch_mode = false;
        }
    }

    if ( switch_mode ) {
        cisTEMDisplayPanel->ReturnCurrentPanel( )->filament_picking_mode_enabled      = true;
        cisTEMDisplayPanel->ReturnCurrentPanel( )->show_selection_distances           = true;
        cisTEMDisplayPanel->ReturnCurrentPanel( )->coords_picking_mode_enabled        = false;
        cisTEMDisplayPanel->ReturnCurrentPanel( )->image_picking_mode_enabled         = false;
        cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->last_selected_image = -1;
        SelectInvertSelection->Enable(false);

        ClearTextFileFromPanel( );
        Refresh( );
        Update( );
    }
}

void DisplayFrame::OnOpenTxtClick(wxCommandEvent& event) {
    bool     valid_file = true;
    wxString name_of_file;
    wxString caption;
    wxString wildcard;
    wxString default_dir;
    wxString default_filename;
    wxString path;

    // We want to open image selections if we're in IMAGES_PICK mode
    if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->image_picking_mode_enabled ) {
        if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->number_of_selections > 0 ) {
            wxMessageDialog open_without_saving_selections_dialog(this, "To open image selections, all current selections must be cleared. Do you want to continue without saving?", "Proceed without Saving Current Selections?", wxYES_NO | wxNO_DEFAULT | wxICON_EXCLAMATION);
            if ( open_without_saving_selections_dialog.ShowModal( ) == wxID_YES ) {
                cisTEMDisplayPanel->ClearSelection(true);
                ClearTextFileFromPanel( );
            }
            // User does not want to switch; do nothing
            else
                return;
        }

        caption                   = wxT("Open selections from text file");
        wildcard                  = wxT("TXT files (*.txt)|*.txt");
        default_dir               = remember_path;
        default_filename          = wxEmptyString;
        wxFileDialog* open_dialog = new wxFileDialog(this, caption, default_dir, default_filename, wildcard, wxFD_OPEN);
        if ( open_dialog->ShowModal( ) == wxID_OK ) {
            //Start with setting up the file info
            path          = open_dialog->GetPath( );
            remember_path = open_dialog->GetDirectory( );
            name_of_file  = open_dialog->GetFilename( );
            std::ifstream image_selections_file(path.ToStdString( ));
            // wxTextFile*   file_to_open = new wxTextFile(path);

            // Start reading from the file
            // file_to_open->Open( );
            // wxString current_line;
            std::string current_line;
            while ( std::getline(image_selections_file, current_line) && valid_file ) {
                std::istringstream iss(current_line);
                valid_file = LoadImageSelections(iss);
            }

            // // Continue reading until through the file
            // size_t line_counter = 0;
            // while ( valid_file && line_counter < file_to_open->GetLineCount( ) ) {
            //     current_line = file_to_open->GetLine(line_counter);
            //     valid_file   = LoadImageSelections(current_line);
            //     line_counter++;
            // }
        }
    }
    // Otherwise, we're in coords mode
    else {
        if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->number_of_coords > 0 ) {
            wxMessageDialog question_dialog(this, "Opening a text file with coordinates selected will clear any currently selected coordinates, so if they are needed, it is recommended to save them first. Do you want to continue without saving?", "Clear Coordinates?", wxYES_NO | wxNO_DEFAULT | wxICON_EXCLAMATION);
            if ( question_dialog.ShowModal( ) == wxID_YES ) {
                cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->Clear( ); // Clear out any selections before adding new ones.
                ClearTextFileFromPanel( );
            }
            // User wants to save first, so do nothing
            else
                return;
        }

        // Now we're definitely in coords mode with no coords selected; let user select file
        caption                   = wxT("Open coordinates from text file");
        wildcard                  = wxT("TXT files (*.txt)|*.txt");
        default_dir               = remember_path;
        default_filename          = wxEmptyString;
        wxFileDialog* open_dialog = new wxFileDialog(this, caption, default_dir, default_filename, wildcard, wxFD_OPEN);
        if ( open_dialog->ShowModal( ) == wxID_OK ) {
            //Start with setting up the file info
            path          = open_dialog->GetPath( );
            remember_path = open_dialog->GetDirectory( );
            name_of_file  = open_dialog->GetFilename( );
            std::ifstream coords_txt_file(path.ToStdString( ), std::ios::app);
            // wxTextFile*   file_to_open = new wxTextFile(path);

            // Start reading from the file
            // file_to_open->Open( );
            // wxString current_line;
            std::string current_line;
            long        x, y, image_number;

            while ( std::getline(coords_txt_file, current_line) && valid_file ) {
                std::istringstream iss(current_line);
                valid_file = LoadCoords(iss, x, y, image_number);
            }

            // Continue reading until through the file
            // size_t line_counter = 0;
            // while ( valid_file && line_counter < file_to_open->GetLineCount( ) ) {
            // current_line = file_to_open->GetLine(line_counter);
            // valid_file   = LoadCoords(current_line, x, y, image_number);
            // line_counter++;
            // }
        }
    }
    if ( valid_file ) {
        cisTEMDisplayPanel->ReturnCurrentPanel( )->short_txt_filename = name_of_file;
        cisTEMDisplayPanel->ReturnCurrentPanel( )->current_file_path  = path;
        cisTEMDisplayPanel->ReturnCurrentPanel( )->have_txt_filename  = true;
        cisTEMDisplayPanel->SetTabNameSaved( );
    }

    Refresh( );
    Update( );
}

void DisplayFrame::OnSaveTxtClick(wxCommandEvent& event) {
    if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->have_txt_filename && cisTEMDisplayPanel->ReturnCurrentPanel( )->txt_is_saved ) {
        cisTEMDisplayPanel->SetTabNameSaved( ); // Just make sure it's saved and tab name is up to date
        return;
    }
    // Have unsaved file; update the file with current selections
    else if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->have_txt_filename && ! cisTEMDisplayPanel->ReturnCurrentPanel( )->txt_is_saved ) {
        // FIXME: wxTextFile kinda sucks compared with C++ STL; switch to ofstream
        // Wipe out the original contents of the text file since coords being saved are in memory.
        std::ofstream file_to_update(cisTEMDisplayPanel->ReturnCurrentPanel( )->current_file_path, std::ofstream::trunc | std::ofstream::out);
        file_to_update.close( );

        // Reopen in append mode
        file_to_update.open(cisTEMDisplayPanel->ReturnCurrentPanel( )->current_file_path, std::ios::app);
        // wxTextFile file_to_update(cisTEMDisplayPanel->ReturnCurrentPanel( )->current_file_path); // Get a wxTextFile from extant file
        if ( ! file_to_update.is_open( ) ) {
            wxMessageDialog nonexistent_dialog(this, "The text file you're attempting to overwrite does not exist.", "Error: File to save does not exist.", wxOK | wxOK_DEFAULT | wxICON_EXCLAMATION);
            return;
        }
        else {
            // // Just open, clear, and re-fill
            // file_to_update.Open( );
            // file_to_update.Clear( );

            if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->image_picking_mode_enabled ) {
                for ( long i = 0; i <= cisTEMDisplayPanel->ReturnCurrentPanel( )->ReturnNumberofImages( ); i++ ) {
                    if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->image_is_selected[i] )
                        // file_to_update.AddLine(wxString::Format("%li", i));
                        file_to_update << std::to_string(i) << std::endl;
                }
            }

            // coords mode
            else {
                for ( int i = 0; i < cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->number_of_coords; i++ ) {
                    file_to_update << wxString::Format("%li %li %li", cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->coords[i].x_pos, cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->coords[i].y_pos, cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->coords[i].image_number).ToStdString( ) << std::endl;
                    // file_to_update.AddLine(wxString::Format("%li %li %li", cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->coords[i].x_pos, cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->coords[i].y_pos, cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->coords[i].image_number));
                }
            }
            file_to_update.close( );
            // file_to_update.Close( );
        }
        cisTEMDisplayPanel->SetTabNameSaved( );
    }
}

void DisplayFrame::OnSaveTxtAsClick(wxCommandEvent& event) {
    wxString   caption;
    wxString   wildcard;
    wxString   default_dir;
    wxString   default_filename;
    wxString   path;
    wxFileName mrc_name;
    wxFileName temp_filename;
    int        temp_int;

    if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->image_picking_mode_enabled ) {
        caption          = wxT("Save image selections as text file");
        wildcard         = wxT("TXT files (*.txt)|*.txt");
        default_dir      = remember_path;
        mrc_name         = cisTEMDisplayPanel->ReturnCurrentPanel( )->filename;
        default_filename = "selections_" + mrc_name.GetName( ) + ".txt";
        temp_filename    = default_filename;
        temp_int         = 1;

        // If the default filename already exists, apppend an integer to default name
        if ( temp_filename.Exists( ) ) {
            while ( temp_filename.Exists( ) ) {
                temp_filename = default_filename;
                temp_filename = wxString::Format("%i_" + default_filename, temp_int);
                temp_int++;
            }
        }
        default_filename = temp_filename.GetFullName( );

        wxFileDialog* save_dialog = new wxFileDialog(this, caption, default_dir, default_filename, wildcard, wxFD_SAVE);
        if ( save_dialog->ShowModal( ) == wxID_OK ) {
            default_filename = save_dialog->GetFilename( );
            path             = save_dialog->GetPath( );
            remember_path    = save_dialog->GetDirectory( );
            std::ofstream new_selections_file(path.ToStdString( ), std::ios::app);
            // wxTextFile*   new_selections_file = new wxTextFile(path);
            for ( long i = 0; i <= cisTEMDisplayPanel->ReturnCurrentPanel( )->ReturnNumberofImages( ); i++ ) {
                if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->image_is_selected[i] )
                    new_selections_file << std::to_string(i);
                // new_selections_file->AddLine(wxString::Format("%li", i));
            }
            new_selections_file.close( );
            // new_selections_file->Write( );
            // new_selections_file->Close( );
        }
    }

    // coords mode
    else {
        caption          = wxT("Save coordinates as text file");
        wildcard         = wxT("TXT files (*.txt)|*.txt");
        default_dir      = remember_path;
        mrc_name         = cisTEMDisplayPanel->ReturnCurrentPanel( )->filename;
        default_filename = "coords_" + mrc_name.GetName( ) + ".txt";
        temp_filename    = default_filename;
        int temp_int     = 1;

        // If the filename already exists, apppend an integer to default name
        if ( temp_filename.Exists( ) ) {
            while ( temp_filename.Exists( ) ) {
                temp_filename = default_filename;
                temp_filename = wxString::Format("%i_" + default_filename, temp_int);
                temp_int++;
            }
        }
        default_filename = temp_filename.GetFullName( );

        // Now set up the file with the new name and then open the dialog for saving
        wxFileDialog* save_dialog = new wxFileDialog(NULL, caption, default_dir, default_filename, wildcard, wxFD_SAVE);
        if ( save_dialog->ShowModal( ) == wxID_OK ) {
            default_filename = save_dialog->GetFilename( );
            path             = save_dialog->GetPath( );
            remember_path    = save_dialog->GetDirectory( );
            std::ofstream new_coords_file(path.ToStdString( ), std::ios::app);
            // wxTextFile*   new_coords_file = new wxTextFile(path);
            for ( int i = 0; i < cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->number_of_coords; i++ ) {
                new_coords_file << wxString::Format("%li %li %li", cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->coords[i].x_pos, cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->coords[i].y_pos, cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->coords[i].image_number).ToStdString( ) << std::endl;
                // new_coords_file->AddLine(wxString::Format("%li %li %li", cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->coords[i].x_pos, cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->coords[i].y_pos, cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->coords[i].image_number));
            }
            // new_coords_file->Write( );
            // new_coords_file->Close( );
            new_coords_file.close( );
        }
    }
    // Track the currently opened file for saving in case user makes further selections
    cisTEMDisplayPanel->ReturnCurrentPanel( )->short_txt_filename = default_filename;
    cisTEMDisplayPanel->ReturnCurrentPanel( )->current_file_path  = path;
    cisTEMDisplayPanel->ReturnCurrentPanel( )->have_txt_filename  = true;
    cisTEMDisplayPanel->SetTabNameSaved( );
}

void DisplayFrame::OnInvertSelectionClick(wxCommandEvent& event) {
    for ( long image_counter = 1; image_counter <= cisTEMDisplayPanel->ReturnCurrentPanel( )->ReturnNumberofImages( ); image_counter++ ) {
        cisTEMDisplayPanel->ToggleImageSelected(image_counter, false);
    }
    cisTEMDisplayPanel->RefreshCurrentPanel( );
    cisTEMDisplayPanel->SetTabNameUnsaved( );
}

void DisplayFrame::OnClearSelectionClick(wxCommandEvent& event) {
    if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->image_picking_mode_enabled ) {
        for ( int image_counter = 0; image_counter < cisTEMDisplayPanel->ReturnCurrentPanel( )->ReturnNumberofImages( ); image_counter++ ) {
            cisTEMDisplayPanel->ClearSelection(false);
        }
    }
    else if ( ! cisTEMDisplayPanel->ReturnCurrentPanel( )->image_picking_mode_enabled ) {
        cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->Clear( );
    }
    ClearTextFileFromPanel( );
    cisTEMDisplayPanel->RefreshCurrentPanel( );
}

void DisplayFrame::OnSize3(wxCommandEvent& event) {
    cisTEMDisplayPanel->ReturnCurrentPanel( )->selected_point_size = 3;
    Refresh( );
    Update( );
}

void DisplayFrame::OnSize5(wxCommandEvent& event) {
    cisTEMDisplayPanel->ReturnCurrentPanel( )->selected_point_size = 5;
    Refresh( );
    Update( );
}

void DisplayFrame::OnSize7(wxCommandEvent& event) {
    cisTEMDisplayPanel->ReturnCurrentPanel( )->selected_point_size = 7;
    Refresh( );
    Update( );
}

void DisplayFrame::OnSize10(wxCommandEvent& event) {
    cisTEMDisplayPanel->ReturnCurrentPanel( )->selected_point_size = 10;
    Refresh( );
    Update( );
}

void DisplayFrame::OnSingleImageModeClick(wxCommandEvent& event) {
    if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->single_image ) {
        cisTEMDisplayPanel->ReturnCurrentPanel( )->single_image = false;
    }
    else {
        cisTEMDisplayPanel->ReturnCurrentPanel( )->single_image               = true;
        cisTEMDisplayPanel->ReturnCurrentPanel( )->image_picking_mode_enabled = false;
    }

    cisTEMDisplayPanel->ReturnCurrentPanel( )->panel_image_has_correct_greys = false;
    cisTEMDisplayPanel->ReturnCurrentPanel( )->ReDrawPanel( );
    Refresh( );
    Update( );
}

void DisplayFrame::OnShowSelectionDistancesClick(wxCommandEvent& event) {
    if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->show_selection_distances )
        cisTEMDisplayPanel->ReturnCurrentPanel( )->show_selection_distances = false;
    else
        cisTEMDisplayPanel->ReturnCurrentPanel( )->show_selection_distances = true;
    Refresh( );
    Update( );
}

void DisplayFrame::OnShowResolution(wxCommandEvent& event) {
    double wanted_pixel_size;

    if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->resolution_instead_of_radius )
        cisTEMDisplayPanel->ReturnCurrentPanel( )->resolution_instead_of_radius = false;
    else {
        wxTextEntryDialog text_dialog(this, wxT("Pixel Size (Angstroms)"), wxT("Select Pixel Size"), wxString::Format(wxT("%.2f"), cisTEMDisplayPanel->ReturnCurrentPanel( )->pixel_size), wxOK | wxCANCEL | wxCENTRE, wxDefaultPosition);
        text_dialog.ShowModal( );

        wxString current_value = text_dialog.GetValue( );
        text_dialog.Destroy( );
        if ( current_value.ToDouble(&wanted_pixel_size) == true ) {
            cisTEMDisplayPanel->ReturnCurrentPanel( )->pixel_size                   = wanted_pixel_size;
            cisTEMDisplayPanel->ReturnCurrentPanel( )->resolution_instead_of_radius = true;
        }
    }
}

void DisplayFrame::OnDocumentationClick(wxCommandEvent& event) {
    wxLaunchDefaultBrowser("http://www.cistem.org/documentation");
}

// This prevents using buttons when an image or stack is not open to act on
void DisplayFrame::DisableAllToolbarButtons( ) {

    // Open menu only needs close tab disabled
    DisplayCloseTab->Enable(false);

    // Label menu
    LabelLocationNumber->Enable(false);

    // Select menu
    SelectImageSelectionMode->Enable(false);
    SelectCoordsSelectionMode->Enable(false);
    SelectFilamentSelectionMode->Enable(false);
    SelectOpenTxt->Enable(false);
    SelectSaveTxt->Enable(false);
    SelectSaveTxtAs->Enable(false);
    SelectInvertSelection->Enable(false);
    SelectClearSelection->Enable(false);

    // Options menu
    OptionsSingleImageMode->Enable(false);
    OptionsShowSelectionDistances->Enable(false);
    OptionsShowResolution->Enable(false);
}

// Call when an image is opened to activate all toolbar buttons
void DisplayFrame::EnableAllToolbarButtons( ) {
    // Open menu only needs close tab disabled
    DisplayCloseTab->Enable( );

    // Label menu
    LabelLocationNumber->Enable( );

    // Select menu
    SelectImageSelectionMode->Enable(true);
    SelectCoordsSelectionMode->Enable(true);
    SelectFilamentSelectionMode->Enable(true);
    SelectOpenTxt->Enable(true);
    SelectSaveTxt->Enable(true);
    SelectSaveTxtAs->Enable(true);
    SelectInvertSelection->Enable(true);
    SelectClearSelection->Enable(true);

    // Options menu
    OptionsSingleImageMode->Enable(true);
    OptionsShowSelectionDistances->Enable(true);
    OptionsShowResolution->Enable(true);
}

void DisplayFrame::OnUpdateUI(wxUpdateUIEvent& event) {
    // First, do we have an image open?
    if ( cisTEMDisplayPanel->my_notebook->GetSelection( ) != wxNOT_FOUND ) {
        EnableAllToolbarButtons( );

        // Check that there are coords selected
        if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->number_of_coords > 0 || cisTEMDisplayPanel->ReturnCurrentPanel( )->number_of_selections > 0 ) {
            SelectSaveTxtAs->Enable(true);
            if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->have_txt_filename )
                SelectSaveTxt->Enable(true);
            else
                SelectSaveTxt->Enable(false);
        }
        else {
            SelectSaveTxtAs->Enable(false);
            SelectSaveTxt->Enable(false);
        }

        // NOTE: previously used radio check to visually represent selection mode;
        // However, upon adding the third option the radio button would fail to update properly
        if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->image_picking_mode_enabled ) {
            // SelectImageSelectionMode->Check(true);
            SelectInvertSelection->Enable(true);
        }
        else if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->coords_picking_mode_enabled ) {
            // SelectCoordsSelectionMode->Check(true);
            SelectInvertSelection->Enable(false);
        }
        else if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->filament_picking_mode_enabled ) {
            // SelectFilamentSelectionMode->Check(true);
            OptionsShowSelectionDistances->Enable(false);
            SelectInvertSelection->Enable(false);
        }

        // Make sure correct radio is checked for point size selection submenu
        if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->selected_point_size == 3 )
            CoordSize3->Check(true);
        else if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->selected_point_size == 5 )
            CoordSize5->Check(true);
        else if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->selected_point_size == 7 )
            CoordSize7->Check(true);
        else if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->selected_point_size == 10 )
            CoordSize10->Check(true);

        // Make sure single image mode is checked/unchecked based on current panel
        if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->single_image ) {
            if ( ! OptionsSingleImageMode->IsChecked( ) )
                OptionsSingleImageMode->Check(true);
            SelectImageSelectionMode->Enable(false);
        }
        else {
            if ( OptionsSingleImageMode->IsChecked( ) )
                OptionsSingleImageMode->Check(false);
            SelectImageSelectionMode->Enable(true);
        }

        // Repeat above for res instead of radius
        if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->resolution_instead_of_radius ) {
            if ( ! OptionsShowResolution->IsChecked( ) ) {
                OptionsShowResolution->Check(true);
            }
        }
        else {
            if ( OptionsShowResolution->IsChecked( ) ) {
                OptionsShowResolution->Check(false);
            }
        }

        // Repeat again for selection distance option
        if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->show_selection_distances ) {
            if ( ! OptionsShowSelectionDistances->IsChecked( ) ) {
                OptionsShowSelectionDistances->Check(true);
            }
        }
        else {
            if ( OptionsShowSelectionDistances->IsChecked( ) ) {
                OptionsShowSelectionDistances->Check(false);
            }
        }
    }
    // No image -- don't want buttons active
    else
        DisableAllToolbarButtons( );
}

bool DisplayFrame::LoadCoords(std::istringstream& current_line, long& x, long& y, long& image_number) {
    // Extract values, checking that the text file is formatted correctly
    if ( ! (current_line >> x >> y >> image_number) ) {
        wxMessageDialog wrong_file_format(this, "Cannot open Image Selection text file in Coordinate Selection mode.", "Incorrect File Format", wxOK | wxOK_DEFAULT | wxICON_EXCLAMATION);
        wrong_file_format.ShowModal( );
        return false;
    }

    // Check that the x and y coordinates are box size, and num images within range.
    if ( x < cisTEMDisplayPanel->ReturnCurrentPanel( )->ReturnImageXSize( ) && y < cisTEMDisplayPanel->ReturnCurrentPanel( )->ReturnImageYSize( ) && image_number <= cisTEMDisplayPanel->ReturnCurrentPanel( )->ReturnNumberofImages( ) ) {
        cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->ToggleCoord(image_number, x, y, cisTEMDisplayPanel->ReturnCurrentPanel( )->filament_picking_mode_enabled);
        return true;
    }
    else {
        wxMessageDialog invalid_file_dialog(this, wxString::Format("The selected coordinates exceed the dimensions of the currently opened *.mrc file. Cannot open selected coordinates.\nSelected x: %li, selected y: %li, image num: %li for image(s) with dimensions x: %i, y: %i, num images: %i).\nTry checking the selection mode and/or the text file contents.", x, y, image_number, cisTEMDisplayPanel->ReturnCurrentPanel( )->ReturnImageXSize( ), cisTEMDisplayPanel->ReturnCurrentPanel( )->ReturnImageYSize( ), cisTEMDisplayPanel->ReturnCurrentPanel( )->ReturnNumberofImages( )), "Invalid Coordinates for Current Image(s)", wxOK | wxOK_DEFAULT | wxICON_EXCLAMATION);
        if ( invalid_file_dialog.ShowModal( ) == wxID_OK )
            cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->Clear( );
        return false;
    }
}

bool DisplayFrame::LoadImageSelections(std::istringstream& current_line) {
    // Quick check of file format
    long image_number = 0;
    if ( ! current_line >> image_number ) {
        wxMessageDialog wrong_file_format(this, "Cannot open Coordinate Selection text file in Image Selection mode.", "Incorrect File Format", wxOK | wxOK_DEFAULT | wxICON_EXCLAMATION);
        wrong_file_format.ShowModal( );
        return false;
    }

    if ( image_number <= cisTEMDisplayPanel->ReturnCurrentPanel( )->ReturnNumberofImages( ) ) {
        cisTEMDisplayPanel->ReturnCurrentPanel( )->SetImageSelected(image_number, false);
        return true;
    }
    // If the value exceeds the possible dimensions don't try to access the index for setting selected
    else {
        wxMessageDialog invalid_file_dialog(this, wxString::Format("The file being opened contains selected images that exceed the number of images in the current file. Cannot open the selections.(Images in open file: %i. Image index sought: %li)", cisTEMDisplayPanel->ReturnCurrentPanel( )->ReturnNumberofImages( ), image_number), "Invalid Selection(s) for Current Image(s)", wxOK | wxOK_DEFAULT | wxICON_EXCLAMATION);
        cisTEMDisplayPanel->ClearSelection(false);
        return false;
    }
}

void DisplayFrame::ClearTextFileFromPanel( ) {
    cisTEMDisplayPanel->ReturnCurrentPanel( )->have_txt_filename  = false;
    cisTEMDisplayPanel->ReturnCurrentPanel( )->short_txt_filename = wxEmptyString;
    cisTEMDisplayPanel->ReturnCurrentPanel( )->current_file_path  = wxEmptyString;
    cisTEMDisplayPanel->SetTabNameSaved( );
}

/**
 * @brief Helper for prompting user if they wish to save selections before
 * swithcing selection modes.
 * 
 * @return true If user wants to wipe currently held selections.
 * @return false If user wants to save selections.
 */
bool DisplayFrame::IssueSelectionModeChangeWarning( ) {
    if ( cisTEMDisplayPanel->ReturnCurrentPanel( )->coord_tracker->number_of_coords > 0 || cisTEMDisplayPanel->ReturnCurrentPanel( )->number_of_selections > 0 ) {
        wxMessageDialog question_dialog(this, "By switching the selection mode, you will lose your current selections if they are unsaved. You can save by clicking File and selecting a save option.\nDo you want to continue?", "Swtich Selection Modes?", wxYES_NO | wxNO_DEFAULT | wxICON_EXCLAMATION);
        if ( question_dialog.ShowModal( ) == wxID_YES )
            return true;
    }
    return false;
}