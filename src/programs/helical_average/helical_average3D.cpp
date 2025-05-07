#include "../../core/core_headers.h"
#include <memory>

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
    float       pixel_size                = my_input->GetFloatFromUser("Pixel size (A)", "", "1.00", 0.1);
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
    bool        use_multiple_threads;
    int         num_threads;

    if ( ! refine_helical_parameters ) {
        // Turn size? Ask in angstroms, ask for pixel size, convert for user?
        axial_translation = my_input->GetFloatFromUser("Distance between consecutive monomers in z direction (A)", "Specified in pixels", "5.0", 0.1);
        azimuth_rotation  = my_input->GetFloatFromUser("Rotation between consecutive monomers in degrees", "Specified in degrees", "5.0", 0.1);
    }
    else {
        log_fname              = my_input->GetFilenameFromUser("log filename for storing parameters", "", "log.txt", false);
        axial_translation_min  = my_input->GetFloatFromUser("Min value in search of distance between consecutive monomers in z direction (in A)", "Specified in pixels", "0");
        axial_translation_max  = my_input->GetFloatFromUser("Max value in search of distance between consecutive monomers in z direction (in A)", "Specified in pixels", "0", axial_translation_min);
        axial_translation_step = my_input->GetFloatFromUser("Increment in search of distance between consecutive monomers in z direction (in A)", "Specified in pixels", "2");
        azimuth_rotation_min   = my_input->GetFloatFromUser("Min value in search of rotation between consecutive monomers in degrees", "Specified in degrees", "0", 0.0);
        azimuth_rotation_max   = my_input->GetFloatFromUser("Max value in search of rotation between consecutive monomers in degrees", "Specified in degrees", "360", azimuth_rotation_min, 360.0);
        azimuth_rotation_step  = my_input->GetFloatFromUser("Step size of search of rotation between consecutive monomers in degrees", "Specified in degrees", "5", 0.01, 360);
        use_multiple_threads   = my_input->GetYesNoFromUser("Use multiple threads?", "Use multiple threads to speed up the calculation", "NO");
        if ( use_multiple_threads ) {
            num_threads = my_input->GetIntFromUser("Number of threads to use", "Number of threads to use", "1", 1);
        }
        else {
            num_threads = 1;
        }
    }

    delete my_input;

    if ( ! refine_helical_parameters ) {
        my_current_job.Reset(11);
        my_current_job.ManualSetArguments("ttbfffiibff", in_fname.c_str( ),
                                          out_fname.c_str( ),
                                          reverse_handedness,
                                          pixel_size,
                                          inner_radius,
                                          outer_radius,
                                          start_z,
                                          end_z,
                                          refine_helical_parameters,
                                          axial_translation,
                                          azimuth_rotation);
    }
    else {
        my_current_job.Reset(17);
        my_current_job.ManualSetArguments("ttbfffiibtffffffi", in_fname.c_str( ),
                                          out_fname.c_str( ),
                                          reverse_handedness,
                                          pixel_size,
                                          inner_radius,
                                          outer_radius,
                                          start_z,
                                          end_z,
                                          refine_helical_parameters,
                                          log_fname.c_str( ),
                                          axial_translation_min,
                                          axial_translation_max,
                                          axial_translation_step,
                                          azimuth_rotation_min,
                                          azimuth_rotation_max,
                                          azimuth_rotation_step,
                                          num_threads);
    }
}

bool HelicalAverage3D::DoCalculation( ) {
    // Always the same params
    std::string in_fname                  = my_current_job.arguments[0].ReturnStringArgument( );
    std::string out_fname                 = my_current_job.arguments[1].ReturnStringArgument( );
    bool        reverse_handedness        = my_current_job.arguments[2].ReturnBoolArgument( );
    float       pixel_size                = my_current_job.arguments[3].ReturnFloatArgument( );
    float       inner_radius              = my_current_job.arguments[4].ReturnFloatArgument( );
    float       outer_radius              = my_current_job.arguments[5].ReturnFloatArgument( );
    int         start_z                   = my_current_job.arguments[6].ReturnIntegerArgument( );
    int         end_z                     = my_current_job.arguments[7].ReturnIntegerArgument( );
    bool        refine_helical_parameters = my_current_job.arguments[8].ReturnBoolArgument( );

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
    int      num_threads;

    if ( ! refine_helical_parameters ) {
        axial_translation = my_current_job.arguments[9].ReturnFloatArgument( );
        axial_translation /= pixel_size;
        axial_translation = std::floor(axial_translation * 100) / 100.0f; // Do we want to floor to hundredth's place?
        azimuth_rotation  = my_current_job.arguments[10].ReturnFloatArgument( );
        azimuth_rotation *= -1;
    }
    else {
        log_fname              = wxString(my_current_job.arguments[9].ReturnStringArgument( ));
        axial_translation_min  = my_current_job.arguments[10].ReturnFloatArgument( );
        axial_translation_max  = my_current_job.arguments[11].ReturnFloatArgument( );
        axial_translation_step = my_current_job.arguments[12].ReturnFloatArgument( );

        // Convert translation stuff to pixels
        axial_translation_min /= pixel_size;
        axial_translation_max /= pixel_size;
        axial_translation_step /= pixel_size;

        azimuth_rotation_min  = my_current_job.arguments[13].ReturnFloatArgument( );
        azimuth_rotation_max  = my_current_job.arguments[14].ReturnFloatArgument( );
        azimuth_rotation_step = my_current_job.arguments[15].ReturnFloatArgument( );
        num_threads           = my_current_job.arguments[16].ReturnIntegerArgument( );
    }

    // Now begin implementation of the program
    MRCFile                input_volume_mrc(in_fname);
    std::unique_ptr<Image> helix_volume = std::make_unique<Image>( );
    Image                  helical_average;
    helix_volume->ReadSlices(&input_volume_mrc, 1, input_volume_mrc.ReturnZSize( ));
    wxDateTime averaging_start = wxDateTime::Now( );
    wxDateTime averaging_end;
    wxTimeSpan averaging_duration;

    if ( ! refine_helical_parameters ) {
        wxPrintf("\nPerforming helical averaging...\n");

        azimuth_rotation *= d2r;
        wxPrintf("Check parameters: inner_radius = %f; outer_radius = %f; start_z = %i; end_z = %i; axial_translation = %f; azimuth_rotation = %f\n", inner_radius, outer_radius, start_z, end_z, axial_translation, azimuth_rotation);
        helix_volume->HelicalAverage(reverse_handedness, start_z, end_z, inner_radius, outer_radius, axial_translation, azimuth_rotation, cross_corr);
        helix_volume->QuickAndDirtyWriteSlices(out_fname, 1, helix_volume->logical_z_dimension, true, pixel_size);

        wxPrintf("Cross correlation score: %f\n", cross_corr);
        averaging_end = wxDateTime::Now( );
    }
    else {
        wxPrintf("\nPerforming helical averaging with refinement...\n");

        float best_cross_corr;
        float best_axial_translation;
        float best_azimuth_rotation;

        std::ofstream log_file(log_fname.ToStdString( ), std::ofstream::trunc | std::ofstream::out);
        log_file.close( );
        log_file.open(log_fname.ToStdString( ), std::ios::app);
        if ( ! log_file.is_open( ) ) {
            wxPrintf("Error opening log file %s. Aborting.\n", log_fname);
            return false;
        }

        log_file << "helicalaverage3D\n\n";
        log_file << "Basic Parameters:\n";
        log_file << wxString::Format("Input file:   %s\n", in_fname);
        log_file << wxString::Format("Output file:  %s\n", out_fname);
        log_file << wxString::Format("Reverse hand: %s\n", (reverse_handedness) ? "true" : "false");
        log_file << wxString::Format("Max radius:   %f\n", outer_radius);
        log_file << wxString::Format("Start z:      %i\n", start_z);
        log_file << wxString::Format("End z:        %i\n", end_z);
        log_file << wxString::Format("Axial translation min:  %f\n", axial_translation_min * pixel_size);
        log_file << wxString::Format("Axial translation max:  %f\n", axial_translation_max * pixel_size);
        log_file << wxString::Format("Axial translation step: %f\n", axial_translation_step * pixel_size);
        log_file << wxString::Format("Azimuth rotation min:   %f\n", azimuth_rotation_min);
        log_file << wxString::Format("Azimuth rotation max:   %f\n", azimuth_rotation_max);
        log_file << wxString::Format("Azimuth rotation step:  %f\n", azimuth_rotation_step);
        log_file << wxString::Format("\nSearch for best fit parameters:\n");
        log_file << wxString::Format("Axial Translation \tAzimuth Rotation \tcross_corr\n");

        // Set up progress bar increments before converting
        const int num_translation_steps = std::floor(1 + (axial_translation_max - axial_translation_min) / axial_translation_step);
        const int num_rotation_steps    = std::floor(1 + (azimuth_rotation_max - azimuth_rotation_min) / azimuth_rotation_step);
        const int num_iterations        = num_translation_steps * num_rotation_steps;

        azimuth_rotation_min *= d2r;
        azimuth_rotation_max *= d2r;
        azimuth_rotation_step *= d2r;

        long allocation_size = (ceill((axial_translation_max - axial_translation_min) / axial_translation_step) + 1) * (ceill((azimuth_rotation_max - azimuth_rotation_min) / azimuth_rotation_step) + 1);

        // std::vector<float> cross_corr_vec(allocation_size);
        // std::vector<float> axial_translation_vec(allocation_size);
        // std::vector<float> azimuth_rotation_vec(allocation_size);

        best_axial_translation = axial_translation_min;
        best_azimuth_rotation  = azimuth_rotation_min;
        best_cross_corr        = -2.0;
        long index             = 0;
        int  write_counter     = 0;

        std::unique_ptr<ProgressBar> progress_bar      = std::make_unique<ProgressBar>(num_iterations);
        int                          current_num_steps = 0;

        // Loop over all translations and rotations; must use this format to parallelize, as OMP does not accept
        // floating point values for iteration.
#pragma omp parallel for ordered num_threads(num_threads) private(azimuth_rotation, axial_translation, helical_average, cross_corr, index) shared(helix_volume, best_cross_corr, best_axial_translation, best_azimuth_rotation, log_file)
        for ( int translation_counter = 0; translation_counter < num_translation_steps; translation_counter++ ) {
            axial_translation = axial_translation_min + translation_counter * axial_translation_step;
            for ( int rotation_counter = 0; rotation_counter < num_rotation_steps; rotation_counter++ ) {
                azimuth_rotation = azimuth_rotation_min + rotation_counter * azimuth_rotation_step;

                helical_average.CopyFrom(helix_volume.get( ));
                helical_average.HelicalAverage(reverse_handedness, start_z, end_z, inner_radius, outer_radius, axial_translation, -1 * azimuth_rotation, cross_corr);

                if ( cross_corr > best_cross_corr ) {
                    best_cross_corr        = cross_corr;
                    best_axial_translation = axial_translation;
                    best_azimuth_rotation  = azimuth_rotation;
                }

                // Add measurements to vectors
                // cross_corr_vec[index]        = cross_corr;
                // axial_translation_vec[index] = axial_translation;
                // azimuth_rotation_vec[index]  = azimuth_rotation;
                index++;

// wxPrintf("Axial translation=%f, Azimuth rotation=%f, cross_corr=%f\n", axial_translation, r2d * azimuth_rotation, cross_corr);
#pragma omp critical
                log_file << wxString::Format("%f\t\t\t%f\t\t\t%f", axial_translation * pixel_size, r2d * azimuth_rotation, cross_corr).ToStdString( ) << std::endl;
                // helical_average.QuickAndDirtyWriteSlices(cur_filename, 1, helical_average.logical_z_dimension, true);
                write_counter++;
                progress_bar->Update(++current_num_steps);
            }
        }
        wxPrintf("\nBest fit from brute force search:\n");
        wxPrintf("Axial translation=%f, Azimuth rotation=%f, cross corr=%f\n", best_axial_translation * pixel_size, r2d * best_azimuth_rotation, best_cross_corr);
        log_file << "\nBest fit from brute force search:" << std::endl;
        log_file << wxString::Format("Axial translation=%f, Azimuth rotation=%f, cross corr=%f", best_axial_translation * pixel_size, r2d * best_azimuth_rotation, best_cross_corr).ToStdString( ) << std::endl;
        log_file.close( );
    }

    return true;
}