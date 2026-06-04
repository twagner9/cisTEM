// LibTorch includes MUST come first!!
// LibTorch v2.5.0+cpu used to run this code.

#include "../../core/core_headers.h"
#include "blush_helpers.h"
#include <numeric>
#include <chrono>

// There are type conflicts with the LibTorch libraries
// within defines.h; they must be undefined to be able
// to compile successfully with LibTorch.
// #include "../../../include/libtorch/cistem_torch_helper.h"

using namespace torch::indexing;

class
        BlushRefinement : public MyApp {

  public:
    bool DoCalculation( );
    void DoInteractiveUserInput( );

  private:
};

IMPLEMENT_APP(BlushRefinement)

// Useful debugging functions -- these were heavily utilized in going between the Python version of blush and
// the C++ version as a way to compare results of tensor operations and model outputs
void write_real_values_to_text_file(const std::string& ofname, const int& box_size, const bool& is_single_dim, bool overwrite = true, torch::Tensor* tensor_data = nullptr, float* array_data = nullptr);
void compare_text_file_lines(std::string fname1, std::string fname2, std::string ofname, const bool& print_vals = true);

/**
 * @brief For testing the BlushHelpers::ApplyBlush function in the standalone CLI program
 * 
 */
void BlushRefinement::DoInteractiveUserInput( ) {

    UserInput* my_input = new UserInput("Blush_Refinement", 1.0);

    std::string input_volume_filename            = my_input->GetFilenameFromUser("Input mrc filename", "The volume that will be denoised via blush refinement", "input.mrc", true);
    std::string output_mrc_filename              = my_input->GetStringFromUser("Output mrc filename", "The denoised volume post blush refinement", "output.mrc");
    float       desired_mask_radius_in_angstroms = my_input->GetFloatFromUser("Mask radius (angstroms)", "The user desired mask radius since it's not being derived from particle diameter", "20.0", 0.0f);
    float       pixel_size                       = my_input->GetFloatFromUser("Pixel size", "", "1.5", 0.5);
    int         batch_size                       = my_input->GetIntFromUser("Batch size", "Number of 64x64x64 blocks per forward pass to blush model", "1", 1, 10);

    int max_threads = 1;

#ifdef _OPENMP
    max_threads = my_input->GetIntFromUser("Enter desired number of threads", "Number of threads for parallelizing localized standard deviation calculation", "1", 1, 256);
#endif

    delete my_input;

    my_current_job.Reset(5);
    my_current_job.ManualSetArguments("ttffi", input_volume_filename.c_str( ),
                                      output_mrc_filename.c_str( ),
                                      desired_mask_radius_in_angstroms,
                                      pixel_size,
                                      max_threads);
}

bool BlushRefinement::DoCalculation( ) {
    // Now, instead of trying to have this separate standalone program, just reuse the function created
    // in blush_helpers.cpp after reading in some variables
    std::string ifname{my_current_job.arguments[0].ReturnStringArgument( )};
    std::string ofname{my_current_job.arguments[1].ReturnStringArgument( )};
    float       mask_radius{my_current_job.arguments[2].ReturnFloatArgument( )};
    float       pixel_size{my_current_job.arguments[3].ReturnFloatArgument( )};
    int         batch_size{my_current_job.arguments[4].ReturnIntegerArgument( )};
    int         max_threads{my_current_job.arguments[5].ReturnIntegerArgument( )};

    // Read in the MRC
    MRCFile   input_file(ifname);
    Image     input_volume;
    const int box_size = input_file.ReturnXSize( );
    input_volume.ReadSlices(&input_file, 1, box_size);

    // Determine total number of iterations:
    constexpr int block_size{64};
    constexpr int stride_size{20};
    const int     total_blush_iterations = pow(((input_volume.logical_x_dimension - block_size) / stride_size) + 1, 3);

    // Maybe use the default loading bar that is used in cisTEM and update it via the lambda
    ProgressBar* progress_bar = new ProgressBar(total_blush_iterations);
    wxPrintf("\n\nBlushing %s...\n\n", ifname);
    auto stop_flag = std::make_shared<std::atomic<bool>>(false);
    auto startTime = std::chrono::high_resolution_clock::now( );
    //double totalForwardTime =
    BlushHelpers::ApplyBlush(input_volume, pixel_size, mask_radius, total_blush_iterations, batch_size, max_threads, stop_flag, [progress_bar](int percent, long seconds_remaining) {
        progress_bar->Update(percent + 1);
        return true;
    });
    auto   endTime  = std::chrono::high_resolution_clock::now( );
    double duration = std::chrono::duration<double>(endTime - startTime).count( );
    // wxPrintf("\n\ntotalForwardTime == %g min\naverage forward time == %g min\n", totalForwardTime / 60, (totalForwardTime / 60) / total_blush_iterations);
    wxPrintf("Total run time: %g\n\n", duration);
    delete progress_bar;
    MRCFile ofile(ofname, true);
    input_volume.WriteSlices(&ofile, 1, box_size);
    ofile.SetPixelSizeAndWriteHeader(pixel_size);

    return true;
}

/**
 * @brief Debugging functioun: write out the real values of an image to a text file.
 * 
 * @param fname Desired filename.
 * @param box_size Cubic box size of the volume.
 * @param is_single_dim Specifies whether processing will be done in 3 dimensions or 1 dimension.
 * @param overwrite Specifies whether it's preferred to clear previous contents from file before writing.
 * @param tensor_data When data is contained in Tensor; nullptr if not using a tensor.
 * @param array_data When data is in 1D linearized form; nullptr if not using float array.
 */
void write_real_values_to_text_file(const std::string& ofname, const int& box_size, const bool& is_single_dim, bool overwrite, torch::Tensor* tensor_data, float* array_data) {
    // 1. Create output filestream
    std::ofstream ofs;

    // 2. Optionally (mostly for debugging/testing) clear the text file when opening
    if ( overwrite ) {
        ofs.open(ofname, std::ofstream::trunc | std::ofstream::out);
        ofs.close( );
    }

    // 3. Open the text file for appending.
    ofs.open(ofname, std::ios::app);
    if ( ! ofs.is_open( ) ) {
        wxPrintf("Failed to open %s.\n", ofname);
    }

    // REMOVE SINGLE DIM LOGIC -- using linearized arrays means just looping directly over the array, don't need to account for single dimensionality
    if ( is_single_dim ) {
        // Only relevant for volumes; if this was a single 2D image, you'd probably get the dimensions from the MRC -- needed especially if not a square
        const int num_pixels = std::pow(box_size, 3);

        if ( array_data ) {
            for ( int i = 0; i < num_pixels; i++ ) {
                ofs << array_data[i] << std::endl;
            }
        }
        else if ( tensor_data ) {
            for ( int i = 0; i < num_pixels; i++ ) {
                ofs << tensor_data->index({i}).item<float>( ) << std::endl;
            }
        }
        else {
            MyDebugPrintWithDetails("Error: array_data and tensor_data are null!\n");
        }
    }
    // NOTE: for simplicity's sake, I'm just assuming array_data is always 1D, and only tensor_data can be 3D
    else {
        if ( tensor_data ) {
            torch::Tensor tensor_copy = tensor_data->clone( );
            auto          s           = tensor_data->sizes( );
            for ( int i = 0; i < s.size( ); i++ ) {
                wxPrintf("%li ", s[i]);
            }
            wxPrintf("\n");
            while ( tensor_copy.dim( ) > 3 ) {
                tensor_copy = tensor_copy.squeeze(0);
            }
            for ( int k = 0; k < box_size; k++ ) {
                for ( int j = 0; j < box_size; j++ ) {
                    for ( int i = 0; i < box_size; i++ ) {
                        ofs << tensor_copy.index({i, j, k}).item<float>( ) << std::endl;
                    }
                }
            }
        }
        else {
            MyDebugPrintWithDetails("Error: tensor data is null!\n");
        }
    }

    ofs.close( );
}

/**
 * @brief Debugging function: compare line by line the values contained in 2 text files.
 * 
 * @param fname1 Filename 1.
 * @param fname2 Filename 2.
 * @param ofname Output filename.
 * @param print_vals Whether to print the values alongside DIFFERENT or SAME
 */
void compare_text_file_lines(std::string fname1, std::string fname2, std::string ofname, const bool& print_vals) {
    std::ifstream f1, f2;
    f1.open(fname1);
    f2.open(fname2);

    if ( ! f1.is_open( ) ) {
        wxPrintf("ERROR: %s not open!\n", fname1);
    }
    if ( ! f2.is_open( ) ) {
        wxPrintf("ERROR: %s not open!\n", fname2);
    }
    std::ofstream comp_f(ofname, std::ofstream::trunc | std::ofstream::out);
    comp_f.close( );
    comp_f.open(ofname, std::ios::app);

    std::string s1, s2;
    while ( std::getline(f1, s1) && std::getline(f2, s2) ) {
        std::istringstream ss1(s1), ss2(s2);
        double             dev1, dev2;
        if ( ! (ss1 >> dev1) ) {
            wxPrintf("Error reading ss1 (istringstream) into dev1 (float).\n");
            break;
        }
        if ( ! (ss2 >> dev2) ) {
            wxPrintf("Error reading ss2 (istringstream) into dev2 (float).\n");
            break;
        }
        if ( ! print_vals )
            (std::abs(dev1 - dev2) > 1e-2) ? comp_f << "DIFFERENT" << std::endl : comp_f << "SAME" << std::endl;
        else
            (std::abs(dev1 - dev2) > 1e-2) ? comp_f << "DIFFERENT " << dev1 << " | " << dev2 << std::endl : comp_f << "SAME " << dev1 << " | " << dev2 << std::endl;
    }
    wxPrintf("Completed comparison of text files %s and %s.\n", fname1, fname2);
}