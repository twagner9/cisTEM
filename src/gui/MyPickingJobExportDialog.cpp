#include "../core/gui_core_headers.h"
#include "MyPickingJobExportDialog.h"

extern MyMainFrame*                  main_frame;
extern MyImageAssetPanel*            image_asset_panel;
extern MyParticlePositionAssetPanel* particle_position_asset_panel;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

MyPickingJobExportDialog::MyPickingJobExportDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, wxT("Export Particle Positions"),
               wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE) {

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    main_sizer->Add(new wxStaticText(this, wxID_ANY, wxT("Select export source:")),
                    0, wxALL, 10);

    JobComboBox = new wxComboBox(this, wxID_ANY, wxEmptyString,
                                 wxDefaultPosition, wxSize(360, -1),
                                 0, NULL, wxCB_READONLY);
    main_sizer->Add(JobComboBox, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

    ParticleCountText = new wxStaticText(this, wxID_ANY, wxEmptyString);
    main_sizer->Add(ParticleCountText, 0, wxLEFT | wxBOTTOM, 10);

    main_sizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 5);

    wxBoxSizer* button_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton*   cancel_btn   = new wxButton(this, wxID_ANY, wxT("Cancel"));
    wxButton*   export_btn   = new wxButton(this, wxID_ANY, wxT("Export..."));
    button_sizer->Add(cancel_btn, 0, wxALL, 5);
    button_sizer->Add(export_btn, 0, wxALL, 5);
    main_sizer->Add(button_sizer, 0, wxALIGN_RIGHT | wxALL, 5);

    SetSizer(main_sizer);
    Fit( );
    Centre( );

    FillComboBox( );

    JobComboBox->Bind(wxEVT_COMBOBOX, &MyPickingJobExportDialog::OnJobSelectionChange, this);
    export_btn->Bind(wxEVT_BUTTON, &MyPickingJobExportDialog::OnExportButtonClick, this);
    cancel_btn->Bind(wxEVT_BUTTON, &MyPickingJobExportDialog::OnCancelButtonClick, this);
}

// ---------------------------------------------------------------------------
// Populate combo box
// ---------------------------------------------------------------------------

void MyPickingJobExportDialog::FillComboBox( ) {
    entries_.clear( );
    long total = particle_position_asset_panel->ReturnNumberOfAssets( );

    // --- Section 1: job-based entries ---

    // "All Jobs"
    entries_.push_back({EntryType::AllJobs, 0, wxT("particles_all")});
    JobComboBox->Append(wxString::Format(wxT("All Jobs (%ld particles)"), total));

    // Collect unique pick_job_ids in order of first appearance
    std::vector<int> seen_jobs;
    for ( long i = 0; i < total; i++ ) {
        int  jid   = particle_position_asset_panel->ReturnAssetPointer(i)->pick_job_id;
        bool found = false;
        for ( int v : seen_jobs )
            if ( v == jid ) {
                found = true;
                break;
            }
        if ( ! found )
            seen_jobs.push_back(jid);
    }

    for ( int jid : seen_jobs ) {
        long count = CountForEntry({(jid == -1 ? EntryType::ManualPicks : EntryType::Job), jid, wxEmptyString});
        if ( jid == -1 ) {
            entries_.push_back({EntryType::ManualPicks, -1, wxT("particles_manual")});
            JobComboBox->Append(wxString::Format(wxT("Manually picked (%ld particles)"), count));
        }
        else {
            entries_.push_back({EntryType::Job, jid,
                                wxString::Format(wxT("particles_job_%d"), jid)});
            JobComboBox->Append(wxString::Format(wxT("Job #%d (%ld particles)"), jid, count));
        }
    }

    // --- Section 2: user groups (groups 1+) ---

    int num_groups = particle_position_asset_panel->ReturnNumberOfGroups( );
    if ( num_groups > 1 ) {
        // Dotted separator — stored as Separator entry, not selectable
        entries_.push_back({EntryType::Separator, 0, wxEmptyString});
        JobComboBox->Append(wxT("  -  -  -  -  -  -  -  -  -  -  -  -  -"));

        for ( int g = 1; g < num_groups; g++ ) {
            wxString gname = particle_position_asset_panel->ReturnGroupName(g);
            long     count = particle_position_asset_panel->ReturnGroupSize(g);
            entries_.push_back({EntryType::Group, g,
                                wxT("particles_") + SanitizeFilename(gname)});
            JobComboBox->Append(wxString::Format(wxT("%s (%ld particles)"), gname, count));
        }
    }

    JobComboBox->SetSelection(0);
    last_valid_selection_ = 0;
    ParticleCountText->SetLabel(wxString::Format(wxT("%ld particles will be exported"), total));
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

long MyPickingJobExportDialog::CountForEntry(const ComboEntry& e) const {
    long total = particle_position_asset_panel->ReturnNumberOfAssets( );
    if ( e.type == EntryType::AllJobs )
        return total;
    if ( e.type == EntryType::Group )
        return particle_position_asset_panel->ReturnGroupSize(e.value);
    // Job or ManualPicks: count by pick_job_id
    long count = 0;
    for ( long i = 0; i < total; i++ )
        if ( particle_position_asset_panel->ReturnAssetPointer(i)->pick_job_id == e.value )
            count++;
    return count;
}

wxString MyPickingJobExportDialog::SanitizeFilename(const wxString& name) const {
    wxString result = name;
    result.Replace(wxT(" "), wxT("_"));
    result.Replace(wxT("/"), wxT("_"));
    result.Replace(wxT("\\"), wxT("_"));
    return result;
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void MyPickingJobExportDialog::OnJobSelectionChange(wxCommandEvent& event) {
    int sel = JobComboBox->GetSelection( );
    if ( sel == wxNOT_FOUND )
        return;

    // Separator: snap back to last valid selection, do nothing
    if ( entries_[sel].type == EntryType::Separator ) {
        JobComboBox->SetSelection(last_valid_selection_);
        return;
    }

    last_valid_selection_ = sel;
    long count            = CountForEntry(entries_[sel]);
    ParticleCountText->SetLabel(wxString::Format(wxT("%ld particles will be exported"), count));
    Fit( );
}

void MyPickingJobExportDialog::OnExportButtonClick(wxCommandEvent& event) {
    int sel = JobComboBox->GetSelection( );
    if ( sel == wxNOT_FOUND )
        return;
    const ComboEntry& entry = entries_[sel];
    if ( entry.type == EntryType::Separator )
        return;

    wxFileDialog save_dialog(this, wxT("Export particle positions"),
                             wxEmptyString, entry.default_filename + wxT(".txt"),
                             wxT("Text files (*.txt)|*.txt|All files (*.*)|*.*"),
                             wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if ( save_dialog.ShowModal( ) == wxID_CANCEL )
        return;

    wxFile output_file;
    if ( ! output_file.Open(save_dialog.GetPath( ), wxFile::write) ) {
        wxMessageBox(wxT("Could not open file for writing."), wxT("Export Error"),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    //output_file.Write(wxT("# Image# X Y\n"));

    if ( entry.type == EntryType::Group ) {
        // Export members of a user-created group
        int  g     = entry.value;
        long gsize = particle_position_asset_panel->ReturnGroupSize(g);
        for ( long m = 0; m < gsize; m++ ) {
            long                   asset_idx = particle_position_asset_panel->ReturnGroupMember(g, m);
            ParticlePositionAsset* asset     = particle_position_asset_panel->ReturnAssetPointer(asset_idx);
            int                    image_pos = image_asset_panel->ReturnArrayPositionFromAssetID(asset->parent_id);
            if ( image_pos < 0 )
                continue;
            ImageAsset* img   = image_asset_panel->ReturnAssetPointer(image_pos);
            float       x_pix = float(asset->x_position) / img->pixel_size + 1.0f;
            float       y_pix = float(img->y_size) - float(asset->y_position) / img->pixel_size + 1.0f;
            output_file.Write(wxString::Format(wxT("%d %.2f %.2f\n"), asset->parent_id, x_pix, y_pix));
        }
    }
    else {
        // Export by pick_job_id (AllJobs, Job, or ManualPicks)
        long total = particle_position_asset_panel->ReturnNumberOfAssets( );
        for ( long i = 0; i < total; i++ ) {
            ParticlePositionAsset* asset = particle_position_asset_panel->ReturnAssetPointer(i);
            if ( entry.type != EntryType::AllJobs && asset->pick_job_id != entry.value )
                continue;
            int image_pos = image_asset_panel->ReturnArrayPositionFromAssetID(asset->parent_id);
            if ( image_pos < 0 )
                continue;
            ImageAsset* img   = image_asset_panel->ReturnAssetPointer(image_pos);
            float       x_pix = float(asset->x_position) / img->pixel_size + 1.0f;
            float       y_pix = float(img->y_size) - float(asset->y_position) / img->pixel_size + 1.0f;
            output_file.Write(wxString::Format(wxT("%d %.2f %.2f\n"), asset->parent_id, x_pix, y_pix));
        }
    }

    output_file.Close( );
    Close( );
}

void MyPickingJobExportDialog::OnCancelButtonClick(wxCommandEvent& event) {
    Close( );
}
