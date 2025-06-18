// LibTorch includes MUST come first!!
// LibTorch v2.5.0+cpu used to run this code.
#include <torch/nn/functional/conv.h>
#include <torch/script.h>
// #include <torch/torch.h>
#include <chrono>

#include "../../core/core_headers.h"
#include <numeric>

#include "blush_model.h" // includes torch/torch.h
#include "blush_helpers.h"
#include "block_iterator.h"

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

void BlushRefinement::DoInteractiveUserInput( ) {
    UserInput* my_input = new UserInput("Blush_Refinement", 1.0);

    std::string input_volume_filename = my_input->GetFilenameFromUser("Enter input mrc volume name", "The volume that will be denoised via blush refinement", "input.mrc", true);
    std::string output_mrc_filename   = my_input->GetStringFromUser("Enter desired name of output mrc volume", "The denoised volume post blush refinement", "output.mrc");
    bool        use_dbg               = my_input->GetYesNoFromUser("Use debug print statements?", "For development purposes and tracing program execution,", "No");
    int         particle_diameter     = my_input->GetIntFromUser("Enter particle diameter", "Diameter of the particle in ", "180", 0, 2000);
    int         max_threads           = 1;

#ifdef _OPENMP
    max_threads = my_input->GetIntFromUser("Enter desired number of threads", "Number of threads for parallelizing localized standard deviation calculation", "1", 1, 256);
#endif

    delete my_input;

    my_current_job.Reset(5);
    my_current_job.ManualSetArguments("ttbii", input_volume_filename.c_str( ),
                                      output_mrc_filename.c_str( ),
                                      use_dbg,
                                      particle_diameter,
                                      max_threads);
}

// NOTE: all libtorch functionality sorts by and assumes z, y, x rather than x, y, z
bool BlushRefinement::DoCalculation( ) {

    // NOTE: this is a no_grad guard, so that the Blush model does not track gradients in a computation graph for the forward pass.
    // If it were to track gradients, it would consume a lot of memory (sometimes greater than 32 GB).
    // These computation graphs are used for backpropagation during neural network training, which is not needed in this case as this
    // is using the model in inference mode (i.e., for making predictions rather than training), along with loaded weights.
    torch::NoGradGuard no_grad;

    std::string input_volume_filename{my_current_job.arguments[0].ReturnStringArgument( )};
    std::string output_mrc_filename{my_current_job.arguments[1].ReturnStringArgument( )};
    const bool  use_dbg{my_current_job.arguments[2].ReturnBoolArgument( )};
    const int   particle_diameter{my_current_job.arguments[3].ReturnIntegerArgument( )};
    const int   max_threads{my_current_job.arguments[4].ReturnIntegerArgument( )};

    std::string     model_filename;
    constexpr float model_voxel_size{1.5f}; // The expected voxel/pixel size of inputs to the blush model
    constexpr int   model_block_size{64};
    constexpr int   num_subset_pixels{model_block_size * model_block_size * model_block_size};
    constexpr int   strides{20};
    constexpr int   in_channels{2};
    constexpr int   batch_size{1};
    constexpr float mask_edge_in_angstr{10.0f}; // Edge width of the mask in Angstroms; this is the same as the Python model uses
    bool            must_resample = false;

    // NOTE: Using multiple threads causes differences in the outputs of tensor operations
    torch::set_num_threads(1);

    // NOTE: These are parameters that are directly relevant to the processing in the model
    // model_voxel_size and model_block_size are values that are predetermined in model parameters; the latter 3
    // could technically be user specified.

    BlushModel model(2, 2);
    try {
        model.load_weights("test_weights_saving_function.dat");
    } catch ( std::exception& e ) {
        wxPrintf("%s\n", e.what( ));
    }

    // Set to evaluation mode:
    model.eval( );

    MRCFile input_mrc(input_volume_filename);
    float   vol_original_pixel_size = input_mrc.ReturnPixelSize( );
    if ( use_dbg )
        wxPrintf("Input volume pixel size: %f\n", vol_original_pixel_size);
    Image input_volume;

    float scale_factor = vol_original_pixel_size / model_voxel_size;
    int   new_box_size = input_mrc.ReturnXSize( ) * scale_factor;
    if ( new_box_size % 2 != 0 ) {
        new_box_size++;
        scale_factor = new_box_size / static_cast<float>(input_mrc.ReturnXSize( ));
    }

    if ( std::abs(scale_factor - 1.0f) > 1e-5f )
        must_resample = true;

    if ( use_dbg )
        wxPrintf("new_box_size == %i\n", new_box_size);

    wxDateTime overall_start = wxDateTime::Now( );
    wxDateTime overall_finish;
    wxDateTime local_std_dev_start;
    wxDateTime local_std_dev_finish;
    wxDateTime blush_start;
    wxDateTime blush_finish;

    wxPrintf("Reading input and loading volume and input mask...\n");
    input_volume.ReadSlices(&input_mrc, 1, input_mrc.ReturnNumberOfSlices( ));

    wxPrintf("scale_factor == %f\n", scale_factor);
    if ( must_resample ) {
        input_volume.ForwardFFT( );
        input_volume.Resize(new_box_size, new_box_size, new_box_size);
        input_volume.BackwardFFT( );
    }

    const int  box_size   = input_volume.logical_x_dimension;
    const long num_pixels = std::pow(box_size, 3);

    // TODO: it's technically possible to have a volume that is smaller than the model block size, so we should
    // handle that case; for now, we assume that the input volume is larger than the model block size
    torch::Tensor                 blocks(torch::zeros({batch_size, model_block_size, model_block_size, model_block_size}));
    std::vector<std::vector<int>> coords(batch_size, std::vector<int>(3, 0));
    torch::Tensor                 real_values_tensor = torch::zeros({box_size, box_size, box_size}, torch::kFloat32);

    torch::Tensor in_std;
    in_std = torch::zeros({box_size, box_size, box_size}); // locally derived standard deviation -- passed to model
    input_volume.RemoveFFTWPadding( );

    real_values_tensor = torch::zeros({box_size, box_size, box_size}, torch::kFloat32);
    real_values_tensor = torch::from_blob(input_volume.real_values, {box_size, box_size, box_size}, torch::kFloat32).clone( ).contiguous( );
    real_values_tensor = real_values_tensor.permute({2, 1, 0}).contiguous( );

    if ( torch::any(torch::isnan(real_values_tensor)).item<bool>( ) ) {
        wxPrintf("This tensor contains NaN values.\n");
    }
    if ( torch::any(torch::isinf(real_values_tensor)).item<bool>( ) ) {
        wxPrintf("Tensor contains inf values.\n");
    }

    // Get standard dev, normalize

    wxPrintf("Generating post-processing grids.\n");

    torch::Tensor      weights_tensor = make_weight_box(model_block_size, 10);
    std::vector<float> weights_grid(std::pow(model_block_size, 3));
    std::memcpy(weights_grid.data( ), weights_tensor.data_ptr<float>( ), std::pow(model_block_size, 3) * sizeof(float));

    torch::Tensor infer_grid = torch::zeros({box_size, box_size, box_size}, torch::kFloat32);
    torch::Tensor count_grid = torch::zeros({box_size, box_size, box_size}, torch::kFloat32);

    wxPrintf("Completed generation of post-processing grids.\n");

    try {
        torch::Tensor tmp_real_values_tensor = real_values_tensor.clone( ).contiguous( );
        wxPrintf("Calculating localized standard deviation...\n\n\n");
        local_std_dev_start               = wxDateTime::Now( );
        torch::Tensor tmp_in_std          = get_local_std_dev(tmp_real_values_tensor.unsqueeze(0), 10).squeeze(0).contiguous( ).clone( );
        torch::Tensor local_std_dev_mean  = tmp_in_std.mean( );
        torch::Tensor real_values_mean    = real_values_tensor.mean( );
        torch::Tensor real_values_std_dev = real_values_tensor.std( );

        in_std             = tmp_in_std / local_std_dev_mean;
        real_values_tensor = (real_values_tensor - real_values_mean) / (real_values_std_dev + 1e-8);
    } catch ( std::exception& e ) {
        wxPrintf("Error when getting standard deviation and normalizing the real_values tensor: %s\n", e.what( ));
        local_std_dev_finish           = wxDateTime::Now( );
        wxTimeSpan duration_of_std_dev = local_std_dev_finish.Subtract(local_std_dev_start);
        wxPrintf("Duration of standard deviation calculation:\t\t%s\n", duration_of_std_dev.Format( ));
        return false;
    }

    local_std_dev_finish           = wxDateTime::Now( );
    wxTimeSpan duration_of_std_dev = local_std_dev_finish.Subtract(local_std_dev_start);
    wxPrintf("Duration of calculating localized standard deviation:        %s\n\n", duration_of_std_dev.Format( ));

    wxPrintf("Applying mask...\n");
    // TEST: Create mask the same way Python does
    int   mask_edge_width = static_cast<int>(mask_edge_in_angstr / model_voxel_size);
    float radius          = particle_diameter / (2 * model_voxel_size) + mask_edge_width / 2;
    radius                = std::min(radius, (input_volume.logical_x_dimension - mask_edge_width) / 2.0f + 1.0f);
    wxPrintf("logical_x_dimension=%i, radius= %f, mask_edge_width = %i\n", input_volume.logical_x_dimension, radius, mask_edge_width);
    torch::Tensor mask_tensor = generate_radial_mask(input_volume.logical_x_dimension, radius, mask_edge_width);

    real_values_tensor *= mask_tensor;
    in_std *= mask_tensor;
    real_values_tensor = real_values_tensor.unsqueeze(0);
    in_std             = in_std.unsqueeze(0);

    try {
        wxPrintf("Running blush model...\n");
        blush_start = wxDateTime::Now( );

        BlockIterator it({new_box_size, new_box_size, new_box_size}, model_block_size, strides);
        int           bi = 0;

        //NOTE: omp parallel is not allowed with range-based for loops; for this to work, the loop would
        // have to be manually unrolled
        for ( auto it_coords : it ) {
            torch::Tensor real_val_block = torch::zeros({1, model_block_size, model_block_size, model_block_size});
            torch::Tensor std_dev_block  = torch::zeros({1, model_block_size, model_block_size, model_block_size});

            int x = std::get<0>(it_coords);
            int y = std::get<1>(it_coords);
            int z = std::get<2>(it_coords);

            // -1 is the stop value returned by the BlockIterator when all blocks are done; could probably be improved
            // for clairty in the future.
            if ( x > -1 ) {
                float cur_array_sum = 0.0f;
                auto  current_slice = mask_tensor.slice(0, z, z + model_block_size, 1).slice(1, y, y + model_block_size, 1).slice(2, x, x + model_block_size, 1);
                float mask_mean     = current_slice.mean( ).item<float>( );

                if ( mask_mean < 0.3 )
                    continue;

                real_val_block = real_values_tensor.slice(1, z, z + model_block_size, 1).slice(2, y, y + model_block_size, 1).slice(3, x, x + model_block_size, 1).clone( );
                std_dev_block  = in_std.slice(1, z, z + model_block_size, 1).slice(2, y, y + model_block_size, 1).slice(3, x, x + model_block_size, 1).clone( );
                bi++;
            }

            // NOTE: this will always be done right after the initial loading, so we don't need to worry about bi really
            if ( bi == batch_size ) {
                auto          init_output = model.forward(real_val_block, std_dev_block);
                torch::Tensor output      = std::get<0>(init_output);

                // Write out the output tensor to MRC for comparing with Python
                if ( use_dbg && z == 120 && y == 180 && x == 120 ) {
                    torch::Tensor output_for_mrc = output.clone( );
                    // Write dimensions of output tensor
                    wxPrintf("Output tensor shape: [%ld, %ld, %ld, %ld]\n", output_for_mrc.size(0), output_for_mrc.size(1), output_for_mrc.size(2), output_for_mrc.size(3));
                    output_for_mrc = output_for_mrc.squeeze(0);
                    output_for_mrc = output_for_mrc.permute({2, 1, 0}).contiguous( ); // switch to x-fastest
                    Image output_sample;
                    output_sample.Allocate(model_block_size, model_block_size, model_block_size);
                    output_sample.RemoveFFTWPadding( );
                    std::memcpy(output_sample.real_values, output_for_mrc.data_ptr<float>( ), std::pow(model_block_size, 3) * sizeof(float));
                    output_sample.AddFFTWPadding( );
                    output_sample.QuickAndDirtyWriteSlices("z120_y180_x120_cpp.mrc", 1, model_block_size, true);
                }

                for ( int i = 0; i < batch_size; i++ ) {
                    // Update inference grid
                    // NOTE: slicing this way makes a "view" of the tensor, which means that the original tensor is modified as well;
                    // thus the final addition to the infer_grid and count_grid tensors will be reflected in the original tensors
                    torch::Tensor infer_grid_slice = infer_grid.slice(0, z, z + model_block_size, 1).slice(1, y, y + model_block_size, 1).slice(2, x, x + model_block_size, 1);
                    auto          update           = output[i] * weights_tensor;
                    infer_grid_slice += update;

                    // Update count grid
                    torch::Tensor count_grid_slice = count_grid.slice(0, z, z + model_block_size, 1).slice(1, y, y + model_block_size, 1).slice(2, x, x + model_block_size, 1);
                    count_grid_slice += weights_tensor;
                }
                bi = 0;
            }
        }

        infer_grid = torch::where(count_grid > 0, infer_grid / count_grid, infer_grid);
        infer_grid = torch::where(count_grid < 1e-1f, 0, infer_grid); // Set values where count_grid is less than 0.1 to 0
        infer_grid *= mask_tensor;

        infer_grid = infer_grid * (real_values_tensor.std( ) + 1e-8) + real_values_tensor.mean( ); // Normalize the inference grid

        blush_finish              = wxDateTime::Now( );
        wxTimeSpan blush_duration = blush_finish.Subtract(blush_start);

        wxPrintf("Finished running blush model. Total duration:        %s\n", blush_duration.Format( ));
    } catch ( std::exception& e ) {
        wxPrintf("Couldn't run model; exception occurred: %s\n", e.what( ));
        return false;
    }

    wxPrintf("Finishing post-processing...\n");

    Image output_volume;
    output_volume.Allocate(new_box_size, new_box_size, new_box_size);
    infer_grid = infer_grid.permute({2, 1, 0}).contiguous( ); // switch back to x-fastest
    std::memcpy(output_volume.real_values, infer_grid.data_ptr<float>( ), num_pixels * sizeof(float));
    output_volume.AddFFTWPadding( );

    // Check if the scale factor is 1.0; if so, we can skip the resampling if not needed
    if ( must_resample ) {
        output_volume.ForwardFFT( );
        output_volume.Resize(input_volume.logical_x_dimension / scale_factor, input_volume.logical_x_dimension / scale_factor, input_volume.logical_x_dimension / scale_factor);
        output_volume.BackwardFFT( );
    }

    output_volume.QuickAndDirtyWriteSlices(output_mrc_filename, 1, output_volume.logical_x_dimension, true);

    wxPrintf("Comparing means from Python and C++...\n");
    wxPrintf("Blush complete.\n");
    overall_finish      = wxDateTime::Now( );
    wxTimeSpan duration = overall_finish.Subtract(overall_start);
    wxPrintf("Total blush runtime:         %s\n", duration.Format( ));

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