#include "../../core/core_headers.h"

class HelicalAverage3D : public MyApp {
  public:
    bool DoCalculation( );
    void DoInteractiveUserInput( );
};

IMPLEMENT_APP(HelicalAverage3D)

void HelicalAverage3D::DoInteractiveUserInput( ) {
    UserInput* my_input = new UserInput("Helical Average 3D", 1.0);

    std::string in_fname                  = my_input->GetFilenameFromUser("Input MRC file", "The volume to which helical averaging should be applied", "input.mrc", true);
    std::string out_fname                 = my_input->GetFilenameFromUser("Output MRC filename", "The volume to which helical averaging should be applied", "output.mrc", false);
    bool        reverse_handedness        = my_input->GetYesNoFromUser("Reverse handedness", "Determine whether averaging should reverse helix handedness", "NO");
    float       inner_radius              = my_input->GetFloatFromUser("Inner mask radius (in pixels)", "", "25.0", 0.0);
    float       outer_radius              = my_input->GetFloatFromUser("Outer mask radius (in pixels)", "", wxString::Format("%.1f", inner_radius + 1).c_str( ), inner_radius + 1);
    int         start_z                   = my_input->GetIntFromUser("Starting coordinate of z range helical averaging is to be performed", "", "0");
    int         end_z                     = my_input->GetIntFromUser("End coordinate of z range helical averaging is to be performed", "", "200", start_z + 1);
    bool        refine_helical_parameters = my_input->GetYesNoFromUser("Try a range of helical parameters?", "", "NO"); // Always index 7

    float       axial_translation;
    float       azimuth_rotation;
    std::string log_fname;
    float       axial_translation_min;
    float       axial_translation_max;
    float       axial_translation_step;
    float       azimuth_rotation_min;
    float       azimuth_rotation_max;
    float       azimuth_rotation_step;

    if ( ! refine_helical_parameters ) {
        // Turn size? Ask in angstroms, ask for pixel size, convert for user?
        axial_translation = my_input->GetFloatFromUser("Distance between consecutive monomers in z direction", "Specified in pixels", "5.0", 0.1);
        azimuth_rotation  = my_input->GetFloatFromUser("Rotation between consecutive monomers in degrees", "Specified in degrees", "5.0", 1.0);
    }
    else {
        log_fname              = my_input->GetFilenameFromUser("log filename for storing parameters", "", "log.txt", false);
        axial_translation_min  = my_input->GetFloatFromUser("Min value in search of distance between consecutive monomers in z direction", "Specified in pixels", "0");
        axial_translation_max  = my_input->GetFloatFromUser("Max value in search of distance between consecutive monomers in z direction", "Specified in pixels", "0", axial_translation_min);
        axial_translation_step = my_input->GetFloatFromUser("Increment in search of distance between consecutive monomers in z direction", "Specified in pixels", "2");
        azimuth_rotation_min   = my_input->GetFloatFromUser("Min value in search of rotation between consecutive monomers in degrees", "Specified in degrees", "0", 0.0);
        azimuth_rotation_max   = my_input->GetFloatFromUser("Max value in search of rotation between consecutive monomers in degrees", "Specified in degrees", "360", azimuth_rotation_min, 360.0);
        azimuth_rotation_step  = my_input->GetFloatFromUser("Step size of search of rotation between consecutive monomers in degrees", "Specified in degrees", "5", 0.01, 360);
    }

    delete my_input;

    if ( ! refine_helical_parameters ) {
        my_current_job.Reset(10);
        my_current_job.ManualSetArguments("ttbffiibff", in_fname.c_str( ),
                                          out_fname.c_str( ),
                                          reverse_handedness,
                                          inner_radius,
                                          outer_radius,
                                          start_z,
                                          end_z,
                                          refine_helical_parameters,
                                          axial_translation,
                                          azimuth_rotation);
    }
    else {
        my_current_job.Reset(15);
        my_current_job.ManualSetArguments("ttbffiibtffffff", in_fname.c_str( ),
                                          out_fname.c_str( ),
                                          reverse_handedness,
                                          inner_radius,
                                          outer_radius,
                                          start_z,
                                          end_z,
                                          refine_helical_parameters,
                                          log_fname,
                                          axial_translation_min,
                                          axial_translation_max,
                                          axial_translation_step,
                                          azimuth_rotation_min,
                                          azimuth_rotation_max,
                                          azimuth_rotation_step);
    }
}

bool HelicalAverage3D::DoCalculation( ) {
    // Always the same params
    std::string in_fname                  = my_current_job.arguments[0].ReturnStringArgument( );
    std::string out_fname                 = my_current_job.arguments[1].ReturnStringArgument( );
    bool        reverse_handedness        = my_current_job.arguments[2].ReturnBoolArgument( );
    float       inner_radius              = my_current_job.arguments[3].ReturnFloatArgument( );
    float       outer_radius              = my_current_job.arguments[4].ReturnFloatArgument( );
    int         start_z                   = my_current_job.arguments[5].ReturnIntegerArgument( );
    int         end_z                     = my_current_job.arguments[6].ReturnIntegerArgument( );
    bool        refine_helical_parameters = my_current_job.arguments[7].ReturnBoolArgument( );

    float cross_corr = 0.0;
    float d2r        = acos(-1.0) / 180.0;
    float r2d        = 1.0 / d2r;

    // No helical parameter refinement
    float axial_translation;
    float azimuth_rotation;

    // Only if doing helical parameter refinement
    wxString log_fname;
    float    axial_translation_min;
    float    axial_translation_max;
    float    axial_translation_step;
    float    azimuth_rotation_min;
    float    azimuth_rotation_max;
    float    azimuth_rotation_step;

    if ( ! refine_helical_parameters ) {
        axial_translation = my_current_job.arguments[8].ReturnFloatArgument( );
        azimuth_rotation  = my_current_job.arguments[9].ReturnFloatArgument( );
    }
    else {
        log_fname              = wxString(my_current_job.arguments[8].ReturnStringArgument( ));
        axial_translation_min  = my_current_job.arguments[9].ReturnFloatArgument( );
        axial_translation_max  = my_current_job.arguments[10].ReturnFloatArgument( );
        axial_translation_step = my_current_job.arguments[11].ReturnFloatArgument( );
        azimuth_rotation_min   = my_current_job.arguments[12].ReturnFloatArgument( );
        azimuth_rotation_max   = my_current_job.arguments[13].ReturnFloatArgument( );
        azimuth_rotation_step  = my_current_job.arguments[14].ReturnFloatArgument( );
    }

    // Now begin implementation of the program
    MRCFile input_volume_mrc(in_fname);
    Image   helix_volume;
    Image   helical_average;
    helix_volume.ReadSlices(&input_volume_mrc, 1, input_volume_mrc.ReturnZSize( ));

    wxDateTime averaging_start = wxDateTime::Now( );
    wxDateTime averaging_end;
    wxTimeSpan averaging_duration;

    if ( ! refine_helical_parameters ) {
        wxPrintf("\nPerforming helical averaging...\n");
        wxPrintf("Check parameters: inner_radius = %f; outer_radius = %f; start_z = %i; end_z = %i; axial_translation = %f; azimuth_rotation = %f\n", inner_radius, outer_radius, start_z, end_z, axial_translation, azimuth_rotation);

        azimuth_rotation *= d2r;
        // Returns cross_corr containing the parabola peak fit of the helical average cross correlation
        helix_volume.HelicalAverage(reverse_handedness, start_z, end_z, inner_radius, outer_radius, axial_translation, azimuth_rotation, cross_corr);
        helix_volume.QuickAndDirtyWriteSlices(out_fname, 1, helix_volume.logical_z_dimension, true);
        // Cross corr only works in 2D right now
        // helical_average.CalculateCrossCorrelationImageWith(&helix_volume);
        // Peak cross_corr_score = helical_average.FindPeakWithParabolaFit(start_z, end_z);

        wxPrintf("Cross correlation score: %f\n", cross_corr);
        averaging_end = wxDateTime::Now( );
    }
    else {
        wxPrintf("\nPerforming helical averaging with refinement...\n");

        float best_cross_corr;
        float best_axial_translation;
        float best_azimuth_rotation;

        wxTextFile log_file(log_fname);
        log_file.Create( );
        log_file.Open( );
        if ( ! log_file.IsOpened( ) ) {
            wxPrintf("Error opening log file %s. Aborting.\n", log_fname);
            return false;
        }
        else {
            log_file.Clear( );
        }

        log_file.AddLine(wxString::Format("helicalaverage3D\n\n"));
        log_file.AddLine(wxString::Format("Basic Parameters:\n"));
        log_file.AddLine(wxString::Format("Input file:   %s\n", log_fname));
        log_file.AddLine(wxString::Format("Output file:  %s\n", out_fname));
        log_file.AddLine(wxString::Format("Reverse hand: %s\n", (reverse_handedness) ? "true" : "false"));
        log_file.AddLine(wxString::Format("Max radius:   %f\n", outer_radius));
        log_file.AddLine(wxString::Format("Start z:      %i\n", start_z));
        log_file.AddLine(wxString::Format("End z:        %i\n", end_z));
        log_file.AddLine(wxString::Format("Axial translation min:  %f\n", axial_translation_min));
        log_file.AddLine(wxString::Format("Axial translation max:  %f\n", axial_translation_max));
        log_file.AddLine(wxString::Format("Axial translation step: %f\n", axial_translation_step));
        log_file.AddLine(wxString::Format("Azimuth rotation min:   %f\n", azimuth_rotation_min));
        log_file.AddLine(wxString::Format("Azimuth rotation max:   %f\n", azimuth_rotation_max));
        log_file.AddLine(wxString::Format("Azimuth rotation step:  %f\n", azimuth_rotation_step));
        log_file.AddLine(wxString::Format("\nSearch for best fit parameters:\n"));
        log_file.AddLine(wxString::Format("Axial Translation \tAzimuth Rotation \tcross_corr\n"));

        azimuth_rotation_min *= d2r;
        azimuth_rotation_max *= d2r;
        azimuth_rotation_step *= d2r;

        long allocation_size = (ceill((axial_translation_max - axial_translation_min) / axial_translation_step) + 1) * (ceill((azimuth_rotation_max - azimuth_rotation_min) / azimuth_rotation_step) + 1);

        std::vector<float> cross_corr_vec(allocation_size);
        std::vector<float> axial_translation_vec(allocation_size);
        std::vector<float> azimuth_rotation_vec(allocation_size);

        best_axial_translation = axial_translation_min;
        best_azimuth_rotation  = azimuth_rotation_min;
        best_cross_corr        = -2.0;
        long index             = 0;
        int write_counter = 0;

        for ( axial_translation = axial_translation_min; axial_translation <= axial_translation_max; axial_translation += axial_translation_step ) {
            for ( azimuth_rotation = azimuth_rotation_min; azimuth_rotation <= azimuth_rotation_max; azimuth_rotation += azimuth_rotation_step ) {
                helical_average.CopyFrom(&helix_volume);
                helical_average.HelicalAverage(reverse_handedness, start_z, end_z, inner_radius, outer_radius, axial_translation, azimuth_rotation, cross_corr);

                if ( cross_corr > best_cross_corr ) {
                    best_cross_corr        = cross_corr;
                    best_axial_translation = axial_translation;
                    best_azimuth_rotation  = azimuth_rotation;
                }

                // Add measurements to vectors
                cross_corr_vec[index]        = cross_corr;
                axial_translation_vec[index] = axial_translation;
                azimuth_rotation_vec[index]  = azimuth_rotation;
                index++;

                wxPrintf("Axial translation=%f, Azimuth rotation=%f, cross_corr=%f\n", axial_translation, r2d * azimuth_rotation, cross_corr);
                log_file.AddLine(wxString::Format("%f\t%f\t%f\n", axial_translation, r2d * azimuth_rotation, cross_corr));
                helical_average.QuickAndDirtyWriteSlices(wxString::Format("%s_%i.mrc",out_fname.substr(0, out_fname.size() - 4), write_counter).ToStdString(), 1, helical_average.logical_z_dimension, true);
                write_counter++;
            }
            log_file.Write( );
        }
        wxPrintf("\nBest fit from brute force search:\n");
        wxPrintf("Axial translation=%f, Azimuth rotation=%f, cross corr=%f\n", best_axial_translation, best_azimuth_rotation, best_cross_corr);
        log_file.AddLine("\nBest fit from brute force search:\n");
        log_file.AddLine(wxString::Format("Axial translation=%f, Azimuth rotation=%f, cross corr=%f\n", best_axial_translation, best_azimuth_rotation, best_cross_corr));
        log_file.Close( );

        averaging_end = wxDateTime::Now( );
    }

    averaging_duration = averaging_end.Subtract(averaging_start);
    wxPrintf("Helical Average duration:\t\t%s\n", averaging_duration.Format( ));

    // Now I have to write the volume out...

    return true;
}