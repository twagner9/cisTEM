#include "../../core/core_headers.h"
#define TMP_DEBUG

//TODO: DO need to write out the star file; will need this for importing back into cisTEM and for reading
// I suppose this raises the question of whether it's better to keep the .star writing and member extraction
// separate from the main subtraction program
class MembraneSubtraction : public MyApp {
  public:
    void DoInteractiveUserInput( );
    bool DoCalculation( );

  private:
};

// NOTE: made to ensure proper association of the class_average_index and its members
typedef struct ClassAveragesAndMembers {
    int         class_average_index;
    wxArrayLong class_members;
} ClassAveragesAndMembers;

void align_scale_subtract(MRCFile& particle_mrc, MRCFile& classification_mrc, cisTEMParameters& ctf_params, ClassAveragesAndMembers* list_of_averages_and_members, const int number_of_averages, long number_of_images);

IMPLEMENT_APP(MembraneSubtraction)

// Get all user inputs necessary for running the program
void MembraneSubtraction::DoInteractiveUserInput( ) {
    UserInput*  my_input                       = new UserInput("Subtraction of 2D Class Averages", 1.00);
    std::string database_filename              = my_input->GetFilenameFromUser("Database filename that contains relevant classification", "cisTEM .db file containing the class average that should be subtracted", "input_database.db", true);
    int         classification_id              = my_input->GetIntFromUser("Input the classification ID that contains the class average for subtraction", "Class ID of class average being used for subtraction", "1");
    std::string class_average_indices_filename = my_input->GetFilenameFromUser("Enter the filename containing all of the class averages to subtracted", "File should contain integer values (single line each) that represent the 2D class average index", "class_average_indices.txt", true);
    std::string particle_stack_filename        = my_input->GetFilenameFromUser("Input particle stack", "The filename for the relevant .mrc file", "input.mrc", true);
    std::string classification_mrc_filename    = my_input->GetFilenameFromUser("Input the relevant classification containing the class average for subtraction", "Classifications have multiple 2D class averages within them; provide the file of the relevant classification.", "class_averages.mrc", true);

    int max_threads;
    /*
#ifdef _OPENMP
    max_threads = my_input->GetIntFromUser("Max number of threads", "The maximum number of threads to be used during calculations", "1");
#else
    // This else is our default -- if openmp is not active, we are just going to use one thread
    max_threads = 1;
// This ends the directive, so #ifdef and #else (and other precompiler directives) are no longer sought by the compiler
#endif
*/
    delete my_input;

    my_current_job.Reset(6);
    my_current_job.ManualSetArguments("tittti", database_filename.c_str( ),
                                      classification_id,
                                      class_average_indices_filename.c_str( ),
                                      particle_stack_filename.c_str( ),
                                      classification_mrc_filename.c_str( ),
                                      max_threads);
}

// This function is main -- does actual execution
bool MembraneSubtraction::DoCalculation( ) {
    // Transfer inputs
    wxFileName  database_filename              = wxFileName(my_current_job.arguments[0].ReturnStringArgument( ));
    const int   classification_id              = my_current_job.arguments[1].ReturnIntegerArgument( );
    std::string class_average_indices_filename = my_current_job.arguments[2].ReturnStringArgument( );
    std::string particle_stack_filename        = my_current_job.arguments[3].ReturnStringArgument( );
    std::string classifications_mrc_filename   = my_current_job.arguments[4].ReturnStringArgument( );
    int         number_of_threads              = my_current_job.arguments[5].ReturnIntegerArgument( );

    wxDateTime overall_start = wxDateTime::Now( );
    wxDateTime overall_finish;

    // Start with getting the txt file ready
    NumericTextFile*         class_average_indices_list   = new NumericTextFile(class_average_indices_filename, OPEN_TO_READ);
    const int                number_of_averages           = class_average_indices_list->number_of_lines;
    ClassAveragesAndMembers* list_of_averages_and_members = new ClassAveragesAndMembers[number_of_averages];

    if ( class_average_indices_list->records_per_line != 1 ) {
        SendError("Error: Number of records per line in list of class averages should be 1!");
        DEBUG_ABORT;
    }

    // Open db and prepare the refinement packages
    Database selected_db = Database( );
    selected_db.Open(database_filename);

    float tmp_float[number_of_averages + 1]; // Stores the indices of the class averages
    // Get the members of each average, keeping them properly grouped

    for ( int average_counter = 0; average_counter < number_of_averages; average_counter++ ) {
        class_average_indices_list->ReadLine(tmp_float);
        list_of_averages_and_members[average_counter].class_average_index = long(tmp_float[0]);
        list_of_averages_and_members[average_counter].class_members       = selected_db.Return2DClassMembers(classification_id, tmp_float[0]);
    }

#ifdef TMP_DEBUG
    wxPrintf("\nNumber of averages = %i", number_of_averages);
    for ( int i = 0; i < number_of_averages; i++ ) {
        wxPrintf("\nNumber of members in average %i: %li", list_of_averages_and_members[i].class_average_index, list_of_averages_and_members[i].class_members.GetCount( ));
    }
#endif

    // Load refinement packages for writing .star
    ArrayOfRefinementPackages refinement_package_list;
    RefinementPackage*        temp_package;

    selected_db.BeginAllRefinementPackagesSelect( );
    while ( selected_db.last_return_code == SQLITE_ROW ) {
        temp_package = selected_db.GetNextRefinementPackage( );
        refinement_package_list.Add(temp_package);
    }
    selected_db.EndBatchSelect( );

    // Write .star file, save name for reading as cisTEMParameters
    Classification* needed_class = selected_db.GetClassificationByID(classification_id);
    selected_db.Close(false);
    RefinementPackage needed_package = refinement_package_list[needed_class->refinement_package_asset_id - 1]; // Select refinement package using classification we made
    needed_class->WritecisTEMStarFile("extracted_subtraction_class", &needed_package, false);
    std::string star_filename = "extracted_subtraction_class_" + std::to_string(classification_id) + ".star"; // so-so method if .star naming convention ever changes

#ifdef TMP_DEBUG
    wxPrintf("\nRead in all class averages and their members.\n");
    wxPrintf("About to read in particle stack and classification MRCs\n");
#endif
    // Read in necessary .mrc(s) files
    MRCFile particle_stack_mrc(particle_stack_filename, false);
    MRCFile classification_mrc(classifications_mrc_filename, false);
    long    number_of_images = particle_stack_mrc.ReturnNumberOfSlices( );

#ifdef TMP_DEBUG
    wxPrintf("MRCs read in; number of images = %li; now read in .STAR params\n", number_of_images);
#endif

    // Read in CTF params
    cisTEMParameters input_star_parameters;
    input_star_parameters.ReadFromcisTEMStarFile(star_filename, true);

    // Make sure .star lines match up with number of particles in particle stack
    if ( number_of_images != input_star_parameters.ReturnNumberofLines( ) ) {
        SendError("Error: number of images in stack does not match number of lines in .star file.");
        DEBUG_ABORT;
    }

#ifdef TMP_DEBUG
    wxPrintf("STAR file read; now about to enter rotate_shift_scale_subtract.\n");
#endif

    align_scale_subtract(particle_stack_mrc, classification_mrc, input_star_parameters, list_of_averages_and_members, number_of_averages, number_of_images);

    overall_finish            = wxDateTime::Now( );
    wxTimeSpan time_to_finish = overall_finish.Subtract(overall_start);
    wxPrintf("\nTotal runtime: %s\n", time_to_finish.Format( ));

    return true;
}

/**
 * @brief Aligns the class average, scales it, and then subtracts it from the full stack.
 * 
 * @param particle_mrc Particle stack associated with the refinement package that contains the needed classification
 * @param classification_mrc Classification mrc that contains all class averages from the selected classification
 * @param ctf_params CTF parameters read in from .star file
 * @param class_members Array containing the list of all the members
 * @param list_of_averages_and_members Vector containing the grouped class averages and their included members
 * @param number_of_images Number of images in the full particle stack
 */
void align_scale_subtract(MRCFile& particle_mrc, MRCFile& classification_mrc, cisTEMParameters& ctf_params, ClassAveragesAndMembers* list_of_averages_and_members, const int number_of_averages, long number_of_images) {
    Image current_image;
    Image class_average_image;
    CTF   current_ctf;
    long  image_counter;
    long  pixel_counter;
    long  current_member;
    int   current_class_average_index;
    bool  image_is_already_written = false;

    // For CTF
    const float microscope_voltage     = ctf_params.ReturnMicroscopekV(0);
    const float spherical_abberation   = ctf_params.ReturnMicroscopeCs(0);
    const float pixel_size             = ctf_params.ReturnPixelSize(0);
    const float additional_phase_shift = ctf_params.ReturnPhaseShift(0); // Only non-const if phase plate is used?
    float       psi;
    float       x_shift;
    float       y_shift;
    float       defocus_1;
    float       defocus_2;
    float       astigmatism_angle;
    float       amplitude_contrast;

    // For scaling
    double sum_of_pixelwise_product;
    double sum_of_squares;
    double scale_factor;

    for ( image_counter = 0; image_counter < number_of_images; image_counter++ ) {
        current_image.ReadSlice(&particle_mrc, image_counter + 1);
        image_is_already_written = false;

        // Get the average and its member ready to subtract
        for ( int class_average_counter = 0; class_average_counter < number_of_averages; class_average_counter++ ) {
            if ( list_of_averages_and_members[class_average_counter].member_counter < list_of_averages_and_members[class_average_counter].number_of_members ) {
                if ( list_of_averages_and_members[class_average_counter].class_members.Item(list_of_averages_and_members[class_average_counter].member_counter) - 1 == image_counter ) {
                    class_average_image.ReadSlice(&classification_mrc, list_of_averages_and_members[class_average_counter].class_average_index);

                    // -1 because the cisTEMParameters class contains an array of all params -- this means 0 indexing
                    current_member = list_of_averages_and_members[class_average_counter].class_members.Item(list_of_averages_and_members[class_average_counter].member_counter) - 1;

                    // Read in CTF params
                    psi                = ctf_params.ReturnPsi(current_member);
                    x_shift            = ctf_params.ReturnXShift(current_member);
                    y_shift            = ctf_params.ReturnYShift(current_member);
                    defocus_1          = ctf_params.ReturnDefocus1(current_member);
                    defocus_2          = ctf_params.ReturnDefocus2(current_member);
                    astigmatism_angle  = ctf_params.ReturnDefocusAngle(current_member);
                    amplitude_contrast = ctf_params.ReturnAmplitudeContrast(current_member);

                    //Apply to class average
                    current_ctf.Init(microscope_voltage, spherical_abberation, amplitude_contrast, defocus_1, defocus_2, astigmatism_angle, pixel_size, additional_phase_shift);
                    class_average_image.ForwardFFT( );
                    class_average_image.ZeroCentralPixel( );
                    class_average_image.ApplyCTF(current_ctf);
                    class_average_image.BackwardFFT( );

                    // Align
                    class_average_image.Rotate2DInPlace(-psi);
                    class_average_image.PhaseShift(-x_shift, -y_shift);
                    class_average_image.QuickAndDirtyWriteSlice("aligned_ctf_average.mrc", image_counter + 1);

                    // Scale: A * B / A * A
                    // A is class average, B is current image
                    for ( pixel_counter = 0; pixel_counter < class_average_image.number_of_real_space_pixels; pixel_counter++ ) {
                        sum_of_pixelwise_product += class_average_image.real_values[pixel_counter] * current_image.real_values[pixel_counter];
                        sum_of_squares += class_average_image.real_values[pixel_counter] * class_average_image.real_values[pixel_counter];
                    }
                    scale_factor = sum_of_pixelwise_product / sum_of_squares;

                    class_average_image.MultiplyByConstant(scale_factor);
                    current_image.SubtractImage(&class_average_image);
                    current_image.QuickAndDirtyWriteSlice("subtracted_stack.mrc", image_counter + 1);
                    image_is_already_written = true;
                    list_of_averages_and_members[class_average_counter].member_counter++;
                    break; // Did what we needed; now done with class averages and should move to next image before looping them again.
                }
            }
        }
        if ( ! image_is_already_written ) {
            current_image.QuickAndDirtyWriteSlice("subtracted_stack.mrc", image_counter + 1);
            current_image.QuickAndDirtyWriteSlice("aligned_ctf_average.mrc", image_counter + 1); // Filler for sanity check -- ask Tim how to improve this
        }
    }
}
