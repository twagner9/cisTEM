#include "../core/gui_core_headers.h"
#include <wx/gauge.h>

extern MyRefinementPackageAssetPanel* refinement_package_asset_panel;
extern MyVolumeAssetPanel*            volume_asset_panel;

// On construction, we want to load in all of the classes, and default initialize the selected refinement to the first in the currently selected class (both will just be the first)
BinningDialog::BinningDialog(wxWindow* parent) : BinningDialogParent(parent) {
    actual_pixel_size             = 0.0f;
    previously_entered_pixel_size = 1.0f;
    actual_box_size               = 0;
    DesiredPixelSizeTextCtrl->SetPrecision(4);
    DesiredPixelSizeTextCtrl->SetMinMaxValue(0, 100);
    // Bind this programmer specified event to the NumericTextCtrl
    DesiredPixelSizeTextCtrl->Bind(wxEVT_TEXT, &BinningDialog::OnDesiredPixelSizeChanged, this);
    Layout( );
    Fit( );
}

void BinningDialog::OnDesiredPixelSizeChanged(wxCommandEvent& event) {
    // Perhaps I can check the NumericTextCtrl specifically, and then
    // DesiredPixelSizeTextCtrl->CheckValues( ); // Checks that values fall within min and max; saves the need for the subsequent check?
    // FIXME: May be the ReturnValue function that's causing the problem; it's not used anywhere else in the code
    float desired_pixel_size = DesiredPixelSizeTextCtrl->ReturnValue( );
    if ( desired_pixel_size <= 0.0f || desired_pixel_size > 100.0f )
        OKButton->Enable(false);
    else
        OKButton->Enable(true);
    // Do some algebra to get the closest pixel size given the current box size (must guarantee even box size number)
    // Only update if input changed
    if ( desired_pixel_size > 0 ) {
        if ( desired_pixel_size != previously_entered_pixel_size ) {
            float start_pixel_size     = refinement_package_asset_panel->all_refinement_packages[refinement_package_asset_panel->selected_refinement_package].output_pixel_size;
            int   start_stack_box_size = refinement_package_asset_panel->all_refinement_packages[refinement_package_asset_panel->selected_refinement_package].stack_box_size;

            float initial_conversion_ratio = desired_pixel_size / start_pixel_size;
            // TODO: modify these names/this methodology to be a bit nicer
            actual_box_size = float(start_stack_box_size) / initial_conversion_ratio;

            // Can't have odd box size; modify it and base the ratio off of this instead,
            // and inform the user what they'll actually get
            if ( actual_box_size % 2 != 0 )
                actual_box_size++;

            // Have to recalculate the new conversion ratio based on the box size; this is the limiting factor
            // as it needs to be an even int
            // Conversion ratio = new_pixel_size / start_pixel_size == old_box_size / new_box_size
            float actual_conversion_ratio = float(start_stack_box_size) / float(actual_box_size);
            actual_pixel_size             = actual_conversion_ratio * start_pixel_size;

            previously_entered_pixel_size = desired_pixel_size;
            ActualPixelSizeText->SetLabel(wxString::Format("Actual Pixel Size: %.4f", actual_pixel_size));
            ActualBoxSizeText->SetLabel(wxString::Format("Actual Box Size: %i", actual_box_size));
            Layout( );
            Fit( );
        }
    }
    else {
        ActualPixelSizeText->SetLabel(wxString::Format("Actual Pixel Size: %.4f", 0.0f));
        ActualBoxSizeText->SetLabel(wxString::Format("Actual Box Size: %i", 0));
        Layout( );
        Fit( );
    }
}

// TODO: Load in the class and refinement selection panels, then fill the comboxes properly
// RefinementPackage current_package = refinement_package_asset_panel->all_refinement_packages[refinement_package_asset_panel->selected_refinement_package];

// Refinement selection
// refinement_selection_panel = new CombinedPackageRefinementSelectPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
// refinement_selection_panel->RefinementText->SetLabel("Refinement from " + current_package.name + ": ");
// refinement_selection_panel->RefinementText->Wrap(300);
// refinement_selection_panel->RefinementComboBox->FillComboBox(refinement_package_asset_panel->selected_refinement_package, false);
// BinningRefinementSelectionSizer->Add(refinement_selection_panel, 1, wxALL | wxALIGN_CENTER_HORIZONTAL, 5);

// Class selection
// class_selection_panel = new CombinedPackageClassSelectionPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
// class_selection_panel->ClassText->SetLabel("Class from " + current_package.name + ": ");
// class_selection_panel->ClassText->Wrap(300);
// class_selection_panel->FillComboBox(refinement_package_asset_panel->selected_refinement_package);
// BinningClassSelectionSizer->Add(class_selection_panel, 1, wxALL | wxALIGN_CENTER_HORIZONTAL, 5);

// // Initial reference selection (ClassVolumeSelectPanel)
// initial_reference_panel = new ClassVolumeSelectPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
// initial_reference_panel->ClassText->SetLabel("Initial reference: ");
// initial_reference_panel->ClassText->Wrap(300);
// initial_reference_panel->VolumeComboBox->FillComboBox(true, true);
// InitialReferenceSelectionSizer->Add(initial_reference_panel, 1, wxALL | wxALIGN_CENTER_HORIZONTAL, 5);

// Magic that refreshes the dialog to force it to look nice

BinningDialog::~BinningDialog( ) {
}

// void BinningDialog::OnUpdateUI(wxUpdateUIEvent& event) {
//     // Perhaps I can check the NumericTextCtrl specifically, and then
//     // DesiredPixelSizeTextCtrl->CheckValues( ); // Checks that values fall within min and max; saves the need for the subsequent check?
//     // FIXME: May be the ReturnValue function that's causing the problem; it's not used anywhere else in the code
//     float desired_pixel_size = DesiredPixelSizeTextCtrl->ReturnValue( );
//     // TODO: Check if this is needed or if handled by NumericTextCtrl class
//     if ( desired_pixel_size <= 0.0f )
//         OKButton->Enable(false);
//     // Do some algebra to get the closest pixel size given the current box size (must guarantee even box size number)
//     else {
//         OKButton->Enable(true);
//         // Only update if input changed
//         if ( desired_pixel_size != previously_entered_pixel_size ) {
//             float start_pixel_size   = refinement_package_asset_panel->all_refinement_packages[refinement_package_asset_panel->selected_refinement_package].output_pixel_size;
//             float conversion_ratio = desired_pixel_size / start_pixel_size;

//             int start_stack_box_size = refinement_package_asset_panel->all_refinement_packages[refinement_package_asset_panel->selected_refinement_package].stack_box_size;
//             actual_box_size        = myroundint(float(start_stack_box_size) / conversion_ratio);

//             // Ratio for proper conversion:
//             // new_pixel_size / start_pixel_size == old_box_size / new_box_size
//             // Can't have odd box size; modify it and base the ratio off of this instead, and inform the user what they'll actually get
//             // FIXME: perhaps make adjustments to this to get the same pixel size with a given box size
//             if ( actual_box_size % 2 != 0 ) {
//                 actual_box_size++;
//                 conversion_ratio  = float(start_stack_box_size) / float(actual_box_size);
//                 actual_pixel_size = conversion_ratio * start_pixel_size;
//             }
//             else {
//                 actual_pixel_size = desired_pixel_size;
//             }

//             previously_entered_pixel_size = desired_pixel_size;
//             ActualPixelSizeText->SetLabel(wxString::Format("Actual Pixel Size: %.3f", actual_pixel_size));
//             ActualBoxSizeText->SetLabel(wxString::Format("Actual Box Size: %i", actual_box_size));
//             Layout( );
//             Fit( );
//         }
//     }
// }

void BinningDialog::OnOK(wxCommandEvent& event) {
    // This will involve determining new dimensions based on the current x and y dims
    // May want to only allow integer values; alternatively, simply round to the even number
    // Get the refinements and classes accessible via array
    EndModal(0);
    // long class_id               = class_selection_panel->ClassComboBox->GetSelection( ); // TEST: Does this get the ID itself or the index of the selection?
    // long selected_refinement_id = refinement_package_asset_panel->all_refinement_packages[refinement_package_asset_panel->selected_refinement_package].refinement_ids[refinement_selection_panel->RefinementComboBox->GetSelection( )];
    // FIXME: this method for pulling the reference index needs to be tested because of generate from params...
    // int      initial_reference_index = initial_reference_panel->VolumeComboBox->GetSelection( ) - 1;

    // This has been replaced with private member actual_pixel_size
    // wxString binning_val_text        = BinningFactorTextCtrl->GetValue( );

    // double binning_val;
    // if ( ! binning_val_text.ToDouble(&binning_val) ) {
    //     wxMessageDialog error_msg(this, "Invalid binning factor!", "Invalid Binning Factor", wxOK | wxOK_DEFAULT | wxICON_EXCLAMATION);
    //     error_msg.ShowModal( );
    //     Destroy( );
    // }

    RefinementPackage package_to_bin = refinement_package_asset_panel->all_refinement_packages.Item(refinement_package_asset_panel->selected_refinement_package);

    // Pixel size gets multipled by the binning factor to adjust size
    // float   new_pixel_size = package_to_bin.output_pixel_size * binning_val;
    MRCFile original_particle_stack(package_to_bin.stack_filename.ToStdString( ), false);

    long total_progress_increments = package_to_bin.contained_particles.GetCount( ) * 2;

    // DEBUG:
    wxPrintf("total increments == %li\n", total_progress_increments);

    OneSecondProgressDialog* my_dialog = new OneSecondProgressDialog("Refinement Package", "Resampling original particle stack...", package_to_bin.contained_particles.GetCount( ) * 2, this, wxPD_REMAINING_TIME | wxPD_AUTO_HIDE | wxPD_APP_MODAL);
    // int                      new_box_size = myroundint(original_particle_stack.ReturnXSize( ) / binning_val);

    // if ( new_box_size % 2 != 0 )
    //     new_box_size--;

    // TODO: Is there a need to adjust the coords? Keep in mind
    // In any case, loop over the stack
    Image                 current_image;
    EmpiricalDistribution stack_distribution;

    std::string combined_stack_filename = wxString::Format(main_frame->current_project.particle_stack_directory.GetFullPath( ) + "/particle_stack_%li.mrc", refinement_package_asset_panel->current_asset_number).ToStdString( );
    MRCFile     binned_stack_file(combined_stack_filename);

    // Now handle the actual resample
    long overall_progress = 0;
    for ( long img_counter = 0; img_counter < original_particle_stack.ReturnNumberOfSlices( ); img_counter++ ) {
        current_image.ReadSlice(&original_particle_stack, img_counter + 1);

        current_image.ForwardFFT( );
        current_image.Resize(actual_box_size, actual_box_size, 1, 0.0f);
        current_image.BackwardFFT( );

        current_image.WriteSlice(&binned_stack_file, img_counter + 1);
        current_image.UpdateDistributionOfRealValues(&stack_distribution);
        overall_progress++;
        my_dialog->Update(overall_progress);
        wxYield( );
    }
    float std = stack_distribution.GetSampleVariance( );
    if ( std > 0.0 ) {
        std = sqrt(std);
    }

    // This should populate the new mrc file
    binned_stack_file.SetDensityStatistics(stack_distribution.GetMinimum( ), stack_distribution.GetMaximum( ), stack_distribution.GetSampleMean( ), std);
    binned_stack_file.SetPixelSize(actual_pixel_size);
    binned_stack_file.WriteHeader( );
    binned_stack_file.CloseFile( );
    original_particle_stack.CloseFile( );
    // DEBUG:
    wxPrintf("After stack resample, overall_progress == %li\n", overall_progress);

    // Now, create a new RefinementPackage asset that will be added to the list of refinement packages
    // Also, update the refinement parameters where necessary
    RefinementPackage* binned_pkg = new RefinementPackage( );
    // Gets the first refinement containing the randomized angles
    Refinement* old_refinement    = main_frame->current_project.database.GetRefinementByID(package_to_bin.refinement_ids[0]);
    Refinement* binned_refinement = new Refinement;
    // This value is the basis for all subsequent refinement_ids -- will have to use a loop to go over all refinements, and add counter + 1 to this to get it to work correctly
    binned_pkg->asset_id                             = refinement_package_asset_panel->current_asset_number + 1;
    binned_pkg->name                                 = wxString::Format("Refinement Package #%li", refinement_package_asset_panel->current_asset_number);
    binned_pkg->estimated_particle_size_in_angstroms = package_to_bin.estimated_particle_size_in_angstroms;
    binned_pkg->estimated_particle_weight_in_kda     = package_to_bin.estimated_particle_weight_in_kda;
    // FIXME: determine whether output pixel size should be changed
    binned_pkg->output_pixel_size       = actual_pixel_size;
    binned_pkg->stack_box_size          = actual_box_size;
    binned_pkg->stack_filename          = combined_stack_filename;
    binned_pkg->stack_has_white_protein = package_to_bin.stack_has_white_protein;
    binned_pkg->symmetry                = package_to_bin.symmetry;

    // We need this for getting all the refinements and classes resampled as well
    binned_pkg->number_of_run_refinments = package_to_bin.number_of_run_refinments;
    binned_pkg->number_of_classes        = 1;
    binned_pkg->references_for_next_refinement.Add(-1);

    // Now the refinement
    binned_refinement->name                             = "Generate from params.";
    binned_refinement->refinement_id                    = main_frame->current_project.database.ReturnHighestRefinementID( ) + 1;
    binned_refinement->resolution_statistics_box_size   = actual_box_size;
    binned_refinement->resolution_statistics_pixel_size = actual_pixel_size;
    // See if this needs to be specified or if it just defaults
    // binned_refinement->starting_refinement_id           = binned_refinement->refinement_id;
    binned_refinement->number_of_classes           = 1;
    binned_refinement->percent_used                = 100.0f;
    binned_refinement->datetime_of_run             = wxDateTime::Now( );
    binned_refinement->refinement_package_asset_id = binned_pkg->asset_id;
    binned_refinement->number_of_particles         = old_refinement->number_of_particles;

    binned_pkg->refinement_ids.Add(binned_refinement->refinement_id);

    // FIXME: this needs to use a seperate selector for the reference
    // The refinement parameters and the reference itself are different, and you
    // are able to select a set of parameters that can be applied to the reference.
    // So, let's see if there's a reference selector that already exists in assets or my_controls

    // Fill in particle info
    RefinementPackageParticleInfo current_particle_info;
    // Only using a single refinement, so only one class is being included; and we need to fill up the class_refinement_results with blank particles that will be filled during the loop
    binned_refinement->SizeAndFillWithEmpty(old_refinement->number_of_particles, 1);
    for ( int particle_counter = 0; particle_counter < package_to_bin.contained_particles.GetCount( ); particle_counter++ ) {
        current_particle_info                   = package_to_bin.contained_particles[particle_counter];
        current_particle_info.position_in_stack = particle_counter + 1;
        current_particle_info.pixel_size        = actual_pixel_size;
        binned_pkg->contained_particles.Add(current_particle_info);

        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].position_in_stack                  = particle_counter + 1;
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].defocus1                           = old_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].defocus1;
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].defocus2                           = old_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].defocus2;
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].defocus_angle                      = old_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].defocus_angle;
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].phase_shift                        = old_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].phase_shift;
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].logp                               = old_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].logp;
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].pixel_size                         = actual_pixel_size;
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].microscope_voltage_kv              = old_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].microscope_voltage_kv;
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].microscope_spherical_aberration_mm = old_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].microscope_spherical_aberration_mm;
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].amplitude_contrast                 = old_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].amplitude_contrast;
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].beam_tilt_x                        = old_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].beam_tilt_x;
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].beam_tilt_y                        = old_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].beam_tilt_y;
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].image_shift_x                      = old_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].image_shift_x;
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].image_shift_y                      = old_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].image_shift_y;
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].occupancy                          = 100.0f;

        // TODO: Check w/ Tim; does this pull same initial randomized angles?
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].phi             = old_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].phi;
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].theta           = old_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].theta;
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].psi             = old_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].psi;
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].score           = old_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].score;
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].image_is_active = old_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].image_is_active;
        binned_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].sigma           = old_refinement->class_refinement_results[0].particle_refinement_results[particle_counter].sigma;
        overall_progress++;
        my_dialog->Update(overall_progress, "Filling Refinement Package with particles...");
        wxYield( );
    }

    // DEBUG:
    wxPrintf("After particle_filling, overall_progress == %li\n", overall_progress);

    // TODO: check if this yield even works
    wxYield( );

    // VolumeAsset* tmp_asset = new VolumeAsset( );
    // // TODO: potentially change name this to include box size for easier identification
    // wxString resampled_3d_filename = main_frame->current_project.volume_asset_directory.GetFullPath( ) + wxString::Format("/resampled_%i_volume_%li_%i.mrc", new_box_size, binned_refinement->refinement_id, 1);

    // if ( initial_reference_index > 0 ) {
    //     my_dialog->Update(overall_progress, "Resampling 3D reference stack...");
    //     VolumeAsset* original_volume_asset = volume_asset_panel->all_assets_list->ReturnVolumeAssetPointer(initial_reference_index);
    //     tmp_asset->CopyFrom(volume_asset_panel->all_assets_list->ReturnVolumeAssetPointer(initial_reference_index));
    //     // Now that we have the particular asset, we'll change the parameters that need changing
    //     tmp_asset->asset_id            = volume_asset_panel->current_asset_number;
    //     tmp_asset->pixel_size          = binned_refinement->resolution_statistics_pixel_size;
    //     tmp_asset->x_size              = binned_refinement->resolution_statistics_box_size;
    //     tmp_asset->y_size              = binned_refinement->resolution_statistics_box_size;
    //     tmp_asset->z_size              = binned_refinement->resolution_statistics_box_size;
    //     tmp_asset->asset_name          = wxString::Format("Resampled %s", tmp_asset->asset_name);
    //     tmp_asset->half_map_1_filename = "";
    //     tmp_asset->half_map_2_filename = "";
    //     tmp_asset->Update(resampled_3d_filename);
    //     tmp_asset->reconstruction_job_id = -1; // FIXME: may not work; thinking it should asset creation wasn't actually a job, just a copy
    //     binned_pkg->references_for_next_refinement.Add(tmp_asset->asset_id);

    //     MRCFile               original_volume_file(original_volume_asset->filename.GetFullPath( ).ToStdString( ));
    //     MRCFile               binned_volume_file(resampled_3d_filename.ToStdString( ));
    //     Image                 current_slice;
    //     EmpiricalDistribution volume_distribution;
    //     const long            number_of_slices = original_volume_file.ReturnNumberOfSlices( );
    //     for ( int slice_counter = 0; slice_counter < number_of_slices; slice_counter++ ) {
    //         current_slice.ReadSlices(&original_volume_file, 1, number_of_slices);

    //         current_slice.ForwardFFT( );
    //         current_slice.Resize(tmp_asset->x_size, tmp_asset->y_size, tmp_asset->z_size, 0.f);
    //         current_slice.BackwardFFT( );

    //         current_slice.WriteSlices(&binned_volume_file, 1, tmp_asset->z_size);
    //         current_slice.UpdateDistributionOfRealValues(&volume_distribution);

    //         ++overall_progress;
    //         my_dialog->Update(overall_progress);
    //     }
    //     float std = volume_distribution.GetSampleVariance( );
    //     if ( std > 0.0 ) {
    //         std = sqrt(std);
    //     }

    //     // This should populate the new mrc file
    //     binned_volume_file.SetDensityStatistics(volume_distribution.GetMinimum( ), volume_distribution.GetMaximum( ), volume_distribution.GetSampleMean( ), std);
    //     binned_volume_file.SetPixelSize(new_pixel_size);
    //     binned_volume_file.WriteHeader( );
    //     binned_volume_file.CloseFile( );
    //     original_volume_file.CloseFile( );

    //     // DEBUG:
    //     wxPrintf("After volume resample, overall_progress == %li\n", overall_progress);
    // }
    // else {
    //     binned_pkg->references_for_next_refinement.Add(-1);
    // }

    // Now that the package and its associated particles are established, put it into the database
    main_frame->current_project.database.Begin( );
    refinement_package_asset_panel->AddAsset(binned_pkg);
    main_frame->current_project.database.AddRefinementPackageAsset(binned_pkg);
    // volume_asset_panel->AddAsset(tmp_asset);

    binned_refinement->class_refinement_results[0].class_resolution_statistics.Init(binned_pkg->output_pixel_size, binned_refinement->resolution_statistics_box_size);
    binned_refinement->class_refinement_results[0].class_resolution_statistics.GenerateDefaultStatistics(binned_pkg->estimated_particle_weight_in_kda);
    main_frame->current_project.database.AddRefinement(binned_refinement);
    // main_frame->current_project.database.BeginVolumeAssetInsert( );
    // main_frame->current_project.database.AddNextVolumeAsset(tmp_asset->asset_id, tmp_asset->asset_name, tmp_asset->filename.GetFullPath( ), tmp_asset->reconstruction_job_id, tmp_asset->pixel_size, tmp_asset->x_size, tmp_asset->y_size, tmp_asset->z_size, tmp_asset->half_map_1_filename.GetFullPath( ), tmp_asset->half_map_2_filename.GetFullPath( ));
    // main_frame->current_project.database.EndVolumeAssetInsert( );
    // delete tmp_asset;

    ArrayofAngularDistributionHistograms all_histograms = binned_refinement->ReturnAngularDistributions(binned_pkg->symmetry);
    main_frame->current_project.database.AddRefinementAngularDistribution(all_histograms[0], binned_refinement->refinement_id, 1);

    ShortRefinementInfo binned_info;
    binned_info = binned_refinement;
    refinement_package_asset_panel->all_refinement_short_infos.Add(binned_info);

    main_frame->current_project.database.Commit( );
    my_dialog->Destroy( );
    Destroy( );
}

void BinningDialog::OnCancel(wxCommandEvent& event) {
    EndModal(0);
    Destroy( );
}