#include "../../core/core_headers.h"

class MembraneSubtraction : MyApp {
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
void generate_reference_image(Image* input_stack, Image* input_stack_times_ctf, MRCFile* input_file, ctf_parameters* ctf_params, int number_of_images, int); // Like azimuthal average, only using the 2D class .star
void apply_ctf( );
void apply_shifts_and_rotations( );
void scale_and_subtract( );

// Get all user inputs necessary for running the program
void MembraneSubtraction::DoInteractiveUserInput( ) {
    UserInput*  my_input                            = new UserInput( );
    std::string extracted_particle_members_filename = my_input->GetFilenameFromUser("Input text file containing particle positions", "The name of the text file containing all members of the class average", "extracted_particles.txt", true);
    std::string input_star_file                     = my_input->GetFilenameFromUser("Input parameters file", "Enter the name of the input parameters file associated with the refinement package containing the extracted particle members of the class average", "input_parameters.star", true);

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

    my_current_job.Reset(4);
    my_current_job.ManualSetArguments("ttbi", extracted_particle_members_filename.c_str( ),
                                      star_file.c_str( ),
                                      use_memory,
                                      max_threads);
}

// The main function that gets executed when the program is run
// This will also contain calls to the forward declared functions above
bool DoCalculation( ) {
    float acceleration_voltage;
    float spherical_aberration;
    float amplitude_contrast;
    float pixel_size;
    float defocus_1              = 0.0;
    float defocus_2              = 0.0;
    float astigmatism_angle      = 0.0;
    float additional_phase_shift = 0.0;

    std::string extracted_particle_members_filename = my_current_job.arguments[0].ReturnStringArgument( );
    wxString    star_filename                       = wxString(my_current_job.arguments[1].ReturnStringArgument( ));
    bool        use_memory                          = my_current_job.arguments[2].ReturnBoolArgument( );
    int         number_of_threads                   = my_current_job.arguments[3].ReturnIntegerArgument( );

    // Get the stack into usable form
    MRCFile input_stack(input_filename, false);
    long    number_of_images = input_stack.ReturnNumberOfSlices( );

    // Get the star params into usable form
    cisTEMParameters input_parameters;
    input_parameters.ReadFromcisTEMStarFile(star_filename, true);

    // Check to see if the nubmer of images in the stack matches
    // the number of lines in the star file
    if ( number_of_images != input_parameters.ReturnNumberofLines( ) ) {
        SendError("Error: number of images in stack does not match number of lines in .star file.");
        DEBUG_ABORT;
    }

    // Now, access the star file and set parameter values in our program
    acceleration_voltage = input_parameters.ReturnMicroscopeKv( );
    pixel_size           = input_parameters.ReturnPixelSize( );
    spherical_aberration = input_parameters.ReturnMicroscopeCs( );
    amplitude_contrast   = input_parameters.ReturnAmplitudeContrast( );
    // TODO: What else from params does not change that we can assign a value right here?

    // Initialize CTF parameters to an array equal in size to the number of images we have
    ctf_parameters* ctf_params = new ctf_parameters(number_of_images);
    CTF             current_ctf;
    // Begs question of whether we even need the reciprocal space parameters?
    current_ctf.Init(acceleration_voltage, spherical_aberration, amplitude_contrast, defocus_1, defocus_2, astigmatism_angle, 0.0f, 0.5f, 0.0f, pixel_size, additional_phase_shift);

    // These will allow us to track the amount of time the program takes to run, as well as individual steps;
    // especially useful for comparing between memory-allocation and no mem allocation runtimes
    // Also serves to make sure the user can tell visually that the program is running properly
    wxDateTime overall_start = wxDateTime::Now( );
    wxDateTime overall_finish;
    wxDateTime read_frames_start;
    wxDateTime read_frames_finish;
    wxDateTime alignment_start;
    wxDateTime alignment_finish;
    wxDateTime subtract_start;
    wxDateTime subtract_finish;

    wxPrintf("\nReading Images...\n");
    read_frames_start = wxDateTime::Now( );

    // Now we deal with the user's choice to use memory or not
    Image* image_stack;
    Image* input_stack_times_ctf;
    if ( use_memory ) {
        image_stack           = new Image[number_of_images];
        input_stack_times_ctf = new Image[number_of_images];
    }
    else {
        image_stack           = nullptr;
        input_stack_times_ctf = nullptr;
    }

    /* TODO: Continue with setup (getting arrays storing x shifts, y shifts, psi angles, defocus values...or just pull from the params as needed?)
     * Becomes a question of how often we will need to use each of the parameters/what the overhead -- i.e., time/processing cost -- would be for having such function calls
     * versus just allocating to memory
     * To be honest, I think we only need to access these 2 or 3 times maximally; once for setting up the CTF, once for setting up the 2D...and something else I can't think of atm
     * I'm not positive on this however.
    */
    // For post read:
    read_frames_finish      = wxDateTime::Now( );
    wxTimeSpan time_to_read = read_frames_finish.Subtract(read_frames_start);
    wxPrintf("Read frames runtime: %s\n", time_to_read.Format( ));

    // TODO: Add function calls for performing rotations and shifts
}
