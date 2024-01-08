#include "../../core/core_headers.h"

class MembraneSubtraction : public MyApp {
  public:
    void DoInteractiveUserInput( );
    bool DoCalculation( );

  private:
};

/*Struct: very similar to C++ class, only they are not true objects, just a collection
  of values; basically loosely linking a bunch of variables under a unifying name.
  We will need this for applying the ctf_parameters of each image to generate
  the 2D reference image, I think.
*/
typedef struct ctf_parameters {
    float acceleration_voltage; // keV
    float spherical_aberration; // mm
    float amplitude_contrast;
    float psi;
    float x_shift;
    float y_shift;
    float defocus_1; // A
    float defocus_2; // A
    float astigmatism_angle; // degrees
    float lowest_frequency_for_fitting; // 1/A
    float highest_frequency_for_fitting; // 1/A
    float astigmatism_tolerance; // A
    float pixel_size; // A
    float additional_phase_shift; // rad
} ctf_parameters;

/* Forward declarations of functions not defined yet; .h files elsewhere in cisTEM will usually contain these,
 * but this follows the current format found in other subdirectories within the cisTEM/src/programs directory.
 * These are also static -- meaning they are not part of/members of a class, but are
 * able to be defined without <class_name>::<function_name>, instead being
 * declared just as <return_type> <function_name>.
 * This also means they can be called without being attached to an object
*/
void apply_ctf( ); // Probably unneeded
void read_images_and_ctf(Image* class_average, MRCFile* all_particles_mrc, Image* class_members_stack, ctf_parameters* class_average_ctf_params, cisTEMParameters classification_parameters, NumericTextFile* class_average_members, long number_of_images, int selected_class_average, int number_of_threads, bool use_memory);
void rotate_shift_scale_subtract(MRCFile* particle_mrc, MRCFile* classification_mrc, cisTEMParameters ctf_params, wxArrayLong* class_members, int class_average_index, long number_of_images, long number_of_members);

IMPLEMENT_APP(MembraneSubtraction)

// Get all user inputs necessary for running the program
void MembraneSubtraction::DoInteractiveUserInput( ) {
    UserInput*  my_input                    = new UserInput("Subtraction of 2D Class Averages", 1.00);
    std::string database_filename           = my_input->GetFilenameFromUser("Database filename that contains relevant classification", "cisTEM .db file containing the class average that should be subtracted", "input_database.db", true);
    int         classification_id           = my_input->GetIntFromUser("Input the classification ID that contains the class average for subtraction", "Class ID of class average being used for subtraction", "1");
    int         class_average_index         = my_input->GetIntFromUser("Enter the position of the class average in the classification mrc that is needed for subtraction", "The index of the class average within its classification", "1");
    std::string particle_stack_filename     = my_input->GetFilenameFromUser("Input particle stack", "The filename for the relevant .mrc file", "input.mrc", true);
    std::string input_star_file             = my_input->GetFilenameFromUser("Input parameters file", "Enter the name of the input parameters file associated with the refinement package containing the extracted particle members of the class average", "input_star_parameters.star", true);
    std::string classification_mrc_filename = my_input->GetFilenameFromUser("Input the relevant classification containing the class average for subtraction", "Classifications have multiple 2D class averages within them; provide the file of the relevant classification.", "class_averages.mrc", true);

    /* This is an additional option that lets user's decide to put the images directly into memory or not; generally, you want to be able to do this, as it will
     * reduce the program's runtime, but if you have a very large stack but limited memory, it's not always feasible. So by adding this option,
     * we make it possible to run the program locally, without a computing cluster
    */
    bool use_memory = my_input->GetYesNoFromUser("Allocate images to memory", "Determines whether full stack will be placed in memory", "NO");

    // Like when we call make, we can actually use mutliple threads to perform calculations to reduce the runtime of the program -- and we'll have the user specify
    // how many threads they want to use (with the limit determined by their CPU)
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

    my_current_job.Reset(8);
    my_current_job.ManualSetArguments("tiitttbi", database_filename.c_str( ),
                                      classification_id,
                                      class_average_index,
                                      particle_stack_filename.c_str( ),
                                      input_star_file.c_str( ),
                                      classification_mrc_filename.c_str( ),
                                      use_memory,
                                      max_threads);
}

// The main function that gets executed when the program is run
// This will also contain calls to the forward declared functions above
bool MembraneSubtraction::DoCalculation( ) {
    wxFileName  database_filename            = wxFileName(my_current_job.arguments[0].ReturnStringArgument( ));
    const int   classification_id            = my_current_job.arguments[1].ReturnIntegerArgument( );
    const int   class_average_index          = my_current_job.arguments[2].ReturnIntegerArgument( );
    std::string particle_stack_filename      = my_current_job.arguments[3].ReturnStringArgument( );
    std::string input_star_filename          = my_current_job.arguments[4].ReturnStringArgument( );
    std::string classifications_mrc_filename = my_current_job.arguments[5].ReturnStringArgument( );
    bool        use_memory                   = my_current_job.arguments[6].ReturnBoolArgument( );
    int         number_of_threads            = my_current_job.arguments[7].ReturnIntegerArgument( );

    // Get the stack into usable form

    // TODO: add checks for list consitency with member count (the .mrc of class members length, I believe)

    // Check to see if the nubmer of images in the stack matches
    // the number of lines in the star file
    // NOTE: this check does not work; the .star will probably have all members of the original stack, but we're
    // only subtracting from the members of the class, so .star SHOULD have more lines
    /*if ( number_of_images != input_star_parameters.ReturnNumberofLines( ) ) {
        SendError("Error: number of images in stack does not match number of lines in .star file.");
        DEBUG_ABORT;
    }*/

    // These will allow us to track the amount of time the program takes to run, as well as individual steps;
    // especially useful for comparing between memory-allocation and no mem allocation runtimes.
    // Also serves to make sure the user can tell visually that the program is running properly
    wxDateTime overall_start = wxDateTime::Now( );
    wxDateTime overall_finish;
    /*wxDateTime read_frames_start;
    wxDateTime read_frames_finish;
    wxDateTime alignment_start;
    wxDateTime alignment_finish;
    wxDateTime subtract_start;
    wxDateTime subtract_finish;

    wxPrintf("\nReading Images...\n");
    read_frames_start = wxDateTime::Now( );
*/

    // Open db and prepare the refinement packages
    Database selected_db = Database( );
    selected_db.Open(database_filename);

    // This is the positions in the particle stack of all the class average members
    wxArrayLong class_members     = selected_db.Return2DClassMembers(classification_id, class_average_index);
    long        number_of_members = class_members.GetCount( );
    selected_db.Close(false);

    // Read in necessary .mrc(s) files
    MRCFile particle_stack_mrc(particle_stack_filename, false);
    MRCFile classification_mrc(classifications_mrc_filename, false);
    long    number_of_images = particle_stack_mrc.ReturnNumberOfSlices( );

    // Read in CTF params
    cisTEMParameters input_star_parameters;
    input_star_parameters.ReadFromcisTEMStarFile(input_star_filename, true);

    // Initialize CTF parameters to an array equal in size to the number of images we actually need
    /*ctf_parameters* ctf_params = new ctf_parameters[number_of_images];

    float temp_float[3]; // Just temporarily stores each line from our .txt of extracted members
    long  current_line;

    // Loop over .star for reading in the rest
    for ( int particle_counter = 0; particle_counter < number_of_images; particle_counter++ ) {
        ctf_params[particle_counter].acceleration_voltage = acceleration_voltage;
        ctf_params[particle_counter].spherical_aberration = spherical_aberration;

        ctf_params[particle_counter].pixel_size = pixel_size;

        particle_positions_list->ReadLine(temp_float);
        current_line = long(temp_float[0]);

        ctf_params[particle_counter].psi                = input_star_parameters.ReturnPsi(current_line);
        ctf_params[particle_counter].x_shift            = input_star_parameters.ReturnXShift(current_line);
        ctf_params[particle_counter].y_shift            = input_star_parameters.ReturnYShift(current_line);
        ctf_params[particle_counter].defocus_1          = input_star_parameters.ReturnDefocus1(current_line);
        ctf_params[particle_counter].defocus_2          = input_star_parameters.ReturnDefocus2(current_line);
        ctf_params[particle_counter].astigmatism_angle  = input_star_parameters.ReturnDefocusAngle(current_line);
        ctf_params[particle_counter].amplitude_contrast = input_star_parameters.ReturnAmplitudeContrast(current_line);

        // Not needed?
        ctf_params[particle_counter].lowest_frequency_for_fitting  = 0.0f;
        ctf_params[particle_counter].highest_frequency_for_fitting = 0.5f;
        ctf_params[particle_counter].astigmatism_tolerance         = 0.0f;
        ctf_params[particle_counter].additional_phase_shift        = 0.0f;
    }
*/
    // For post read:
    //read_frames_finish      = wxDateTime::Now( );
    //wxTimeSpan time_to_read = read_frames_finish.Subtract(read_frames_start);
    //wxPrintf("Read frames runtime: %s\n", time_to_read.Format( ));

    // TODO: Add function call for performing rotations and shifts
    return true;
}

/**
 * @brief The function that applies CTF parameters to the class average to subtract the average from the
 * full stack.
 * 
 * @param particle_mrc Particle stack associated with the refinement package that containst the needed classification
 * @param classification_mrc Classification mrc that has all the class averages from the selected classification
 * @param ctf_params CTF parameters read in from the user-passed star file
 * @param class_members Array containing the list of all the members
 */
void rotate_shift_scale_subtract(MRCFile* particle_mrc, MRCFile* classification_mrc, cisTEMParameters ctf_params, wxArrayLong* class_members, int class_average_index, long number_of_images, long number_of_members) {
    Image current_image;
    Image class_average_image;
    CTF   current_ctf;
    long  member_counter = 0;
    long  current_member;

    // For CTF
    const float microscope_voltage     = ctf_params.ReturnMicroscopekV(1);
    const float spherical_abberation   = ctf_params.ReturnMicroscopeCs(1);
    const float pixel_size             = ctf_params.ReturnPixelSize(1);
    const float additional_phase_shift = 0.0f;
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

    class_average_image.ReadSlice(classification_mrc, class_average_index);

    // FIXME?: Better to just re-read the plain image, or to remove the most recently used CTF?
    // Unsure of which is more efficient, though removal seems to be more likely to be better
    for ( int image_counter = 0; image_counter < number_of_images; image_counter ) {
        current_image.ReadSlice(particle_mrc, image_counter + 1);
        if ( class_members->Item(member_counter) - 1 == image_counter ) {
            current_member = class_members->Item(member_counter);
            // Apply CTF
            psi                = ctf_params.ReturnPsi(current_member);
            x_shift            = ctf_params.ReturnXShift(current_member);
            y_shift            = ctf_params.ReturnYShift(current_member);
            defocus_1          = ctf_params.ReturnDefocus1(current_member);
            defocus_2          = ctf_params.ReturnDefocus2(current_member);
            astigmatism_angle  = ctf_params.ReturnDefocusAngle(current_member);
            amplitude_contrast = ctf_params.ReturnAmplitudeContrast(current_member);

            current_ctf.Init(microscope_voltage, spherical_abberation, amplitude_contrast, defocus_1, defocus_2, astigmatism_angle, pixel_size, additional_phase_shift);
            class_average_image.ForwardFFT( );
            class_average_image.ApplyCTF(current_ctf);
            class_average_image.BackwardFFT( );

            // All shifts and rotations applied, now scale: A * B / A * A
            // A is class average, B is current image
            for ( long pixel_counter = 0; pixel_counter < class_average_image.number_of_real_space_pixels; pixel_counter++ ) {
                sum_of_pixelwise_product += class_average_image.real_values[pixel_counter] * current_image.real_values[pixel_counter];
                sum_of_squares += class_average_image.real_values[pixel_counter] * class_average_image.real_values[pixel_counter];
            }
            scale_factor = sum_of_pixelwise_product / sum_of_squares;

            class_average_image.MultiplyByConstant(scale_factor);
            current_image.SubtractImage(&class_average_image);
        }
        current_image.QuickAndDirtyWriteSlice("subtracted_stack.mrc", image_counter + 1);
    }
}