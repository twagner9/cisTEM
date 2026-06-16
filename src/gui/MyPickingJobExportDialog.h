#ifndef _SRC_GUI_MYPICKINGJOBEPORTDIALOG_H_
#define _SRC_GUI_MYPICKINGJOBEPORTDIALOG_H_

#include <vector>
#include <wx/dialog.h>
#include <wx/combobox.h>
#include <wx/stattext.h>

class MyPickingJobExportDialog : public wxDialog {
  public:
    MyPickingJobExportDialog(wxWindow* parent);

    ~MyPickingJobExportDialog( ) {}

  private:
    enum class EntryType { AllJobs,
                           Job,
                           ManualPicks,
                           Separator,
                           Group };

    struct ComboEntry {
        EntryType type;
        int       value; // pick_job_id for Job, group index for Group
        wxString  default_filename; // pre-filled in the save dialog
    };

    wxComboBox*             JobComboBox;
    wxStaticText*           ParticleCountText;
    std::vector<ComboEntry> entries_;
    int                     last_valid_selection_ = 0;

    void     FillComboBox( );
    long     CountForEntry(const ComboEntry& e) const;
    wxString SanitizeFilename(const wxString& name) const;

    void OnJobSelectionChange(wxCommandEvent& event);
    void OnExportButtonClick(wxCommandEvent& event);
    void OnCancelButtonClick(wxCommandEvent& event);
};

#endif
