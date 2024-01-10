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

void align_scale_subtract(MRCFile* particle_mrc, MRCFile* classification_mrc, cisTEMParameters* ctf_params, wxArrayLong* class_members, int class_average_index, long number_of_images, long number_of_members, int max_threads);

IMPLEMENT_APP(MembraneSubtraction)

// Get all user inputs necessary for running the program
void MembraneSubtraction::DoInteractiveUserInput( ) {
    UserInput*  my_input                    = new UserInput("Subtraction of 2D Class Averages", 1.00);
    std::string database_filename           = my_input->GetFilenameFromUser("Database filename that contains relevant classification", "cisTEM .db file containing the class average that should be subtracted", "input_database.db", true);
    int         classification_id           = my_input->GetIntFromUser("Input the classification ID that contains the class average for subtraction", "Class ID of class average being used for subtraction", "1");
    int         class_average_index         = my_input->GetIntFromUser("Enter the position of the class average in the classification mrc that is needed for subtraction", "The index of the class average within its classification", "1");
    std::string particle_stack_filename     = my_input->GetFilenameFromUser("Input particle stack", "The filename for the relevant .mrc file", "input.mrc", true);
    std::string classification_mrc_filename = my_input->GetFilenameFromUser("Input the relevant classification containing the class average for subtraction", "Classifications have multiple 2D class averages within them; provide the file of the relevant classification.", "class_averages.mrc", true);

    int max_threads;

/* This #ifdef is called a precompiler directive, and is sought by the compiler before compiling actually begins.
 * OpenMP is a library that cisTEM uses for parallelizing code (i.e., using multiple threads), and
 * we can define it when we call the configure script in the build directory with:
 * ./configure --enable-openmp
*/
#ifdef _OPENMP
    max_threads = my_input->GetIntFromUser("Max number of threads", "The maximum number of threads to be used during calculations", "1");
#else
    // This else is our default -- if openmp is not active, we are just going to use one thread
    max_threads = 1;
// This ends the directive, so #ifdef and #else (and other precompiler directives) are no longer sought by the compiler
#endif

    delete my_input;

    my_current_job.Reset(6);
    my_current_job.ManualSetArguments("tiitti", database_filename.c_str( ),
                                      classification_id,
                                      class_average_index,
                                      particle_stack_filename.c_str( ),
                                      classification_mrc_filename.c_str( ),
                                      max_threads);
}

// This function is main -- does actual execution
bool MembraneSubtraction::DoCalculation( ) {
    // Transfer inputs
    wxFileName  database_filename            = wxFileName(my_current_job.arguments[0].ReturnStringArgument( ));
    const int   classification_id            = my_current_job.arguments[1].ReturnIntegerArgument( );
    const int   class_average_index          = my_current_job.arguments[2].ReturnIntegerArgument( );
    std::string particle_stack_filename      = my_current_job.arguments[3].ReturnStringArgument( );
    std::string classifications_mrc_filename = my_current_job.arguments[4].ReturnStringArgument( );
    int         number_of_threads            = my_current_job.arguments[5].ReturnIntegerArgument( );

    wxDateTime overall_start = wxDateTime::Now( );
    wxDateTime overall_finish;

    // Open db and prepare the refinement packages
    Database selected_db = Database( );
    selected_db.Open(database_filename);

    // This is the positions in the particle stack of all the class average members
    wxArrayLong class_members     = selected_db.Return2DClassMembers(classification_id, class_average_index);
    long        number_of_members = class_members.GetCount( );

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

    // Write member positions to a text file so they can be found within the stack when reviewing subtraction
    std::ofstream my_file;
    std::string   text_filename = "extracted_particles_" + std::to_string(classification_id) + "_" + std::to_string(class_average_index) + ".txt";
    my_file.open(text_filename);
    for ( int particle_counter = 0; particle_counter < class_members.GetCount( ); particle_counter++ )
        my_file << class_members[particle_counter] << std::endl;

    my_file.close( );

#ifdef TMP_DEBUG
    wxPrintf("\nRead in class member array; number of members = %li\n", number_of_members);
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

    align_scale_subtract(&particle_stack_mrc, &classification_mrc, &input_star_parameters, &class_members, class_average_index, number_of_images, number_of_members, number_of_threads);

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
 * @param class_average_index Index of the class average in the classification that will be subtracted
 * @param number_of_images Number of images in the full particle stack
 * @param number_of_members Number of images in the class_average
 * @param max_threads Maximum number of threads available to OpenMP for calculations
 */
void align_scale_subtract(MRCFile* particle_mrc, MRCFile* classification_mrc, cisTEMParameters* ctf_params, wxArrayLong* class_members, const int class_average_index, long number_of_images, long number_of_members, int max_threads) {
    Image current_image;
    Image class_average_image;
    CTF   current_ctf;
    long  member_counter = 0;
    long  image_counter;
    long  pixel_counter;
    long  current_member;

    // For CTF
    const float microscope_voltage   = ctf_params->ReturnMicroscopekV(1);
    const float spherical_abberation = ctf_params->ReturnMicroscopeCs(1);
    const float pixel_size           = ctf_params->ReturnPixelSize(1);
    float       psi;
    float       x_shift;
    float       y_shift;
    float       defocus_1;
    float       defocus_2;
    float       astigmatism_angle;
    float       amplitude_contrast;
    float       additional_phase_shift;

    // For scaling
    double sum_of_pixelwise_product;
    double sum_of_squares;
    double scale_factor;

#pragma omp parallel num_threads(max_threads) default(none) shared(class_average_index, class_members, microscope_voltage, spherical_abberation, pixel_size, number_of_images, ctf_params, classification_mrc, particle_mrc, member_counter, number_of_members) private(current_image, class_average_image, current_ctf, image_counter, current_member, pixel_counter, psi, x_shift, y_shift, defocus_1, defocus_2, astigmatism_angle, amplitude_contrast, additional_phase_shift, sum_of_pixelwise_product, sum_of_squares, scale_factor)
    { // begin omp
#pragma omp for ordered
        for ( image_counter = 0; image_counter < number_of_images; image_counter++ ) {
            current_image.ReadSlice(particle_mrc, image_counter + 1);

#pragma omp ordered
            if ( member_counter < number_of_members && class_members->Item(member_counter) - 1 == image_counter ) {
                class_average_image.ReadSlice(classification_mrc, class_average_index);
                current_member = class_members->Item(member_counter);
#pragma omp critical
                ++member_counter; // increment so that we advance to the next memeber of the class for subtraction when doing if comparison
#ifdef TMP_DEBUG
                wxPrintf("\nmember_counter = %li", member_counter);
#endif

                // Read in CTF
                psi                    = ctf_params->ReturnPsi(current_member);
                x_shift                = ctf_params->ReturnXShift(current_member);
                y_shift                = ctf_params->ReturnYShift(current_member);
                defocus_1              = ctf_params->ReturnDefocus1(current_member);
                defocus_2              = ctf_params->ReturnDefocus2(current_member);
                astigmatism_angle      = ctf_params->ReturnDefocusAngle(current_member);
                amplitude_contrast     = ctf_params->ReturnAmplitudeContrast(current_member);
                additional_phase_shift = ctf_params->ReturnPhaseShift(current_member);

                //Apply to class average
                current_ctf.Init(microscope_voltage, spherical_abberation, amplitude_contrast, defocus_1, defocus_2, astigmatism_angle, pixel_size, additional_phase_shift);
                class_average_image.ForwardFFT( );
                class_average_image.ZeroCentralPixel( );
                class_average_image.ApplyCTF(current_ctf);
                class_average_image.BackwardFFT( );

                // Align
                class_average_image.Rotate2DInPlace(psi);
                class_average_image.PhaseShift(x_shift, y_shift);

                // Scale: A * B / A * A
                // A is class average, B is current image
                for ( pixel_counter = 0; pixel_counter < class_average_image.number_of_real_space_pixels; pixel_counter++ ) {
                    sum_of_pixelwise_product += class_average_image.real_values[pixel_counter] * current_image.real_values[pixel_counter];
                    sum_of_squares += class_average_image.real_values[pixel_counter] * class_average_image.real_values[pixel_counter];
                }
                scale_factor = sum_of_pixelwise_product / sum_of_squares;

                class_average_image.MultiplyByConstant(scale_factor);
                current_image.SubtractImage(&class_average_image);

                // Reset the class average so CTF isn't applied still during the next round as this would apply 2 CTFs at once
                class_average_image.ReadSlice(classification_mrc, class_average_index);
            }
#pragma omp ordered
            current_image.QuickAndDirtyWriteSlice("subtracted_stack.mrc", image_counter + 1);
        }
    } // end omp
}