// LibTorch includes MUST come first!!
#include <torch/nn/functional/conv.h>
#include <torch/script.h>
// #include <torch/torch.h>

#include "blush_model.cpp" // includes torch/torch.h
#include "../../core/core_headers.h"

class
        BlushRefinement : public MyApp {

  public:
    bool DoCalculation( );
    void DoInteractiveUserInput( );

  private:
};

IMPLEMENT_APP(BlushRefinement)

// Assumes cubic
// Image      generate_radial_mask(int box_size, float radius, int voxel_size); // TODO: maybe implement
at::Tensor get_local_std_dev(torch::Tensor grid, int size = 10, const bool use_dbg = false);

void BlushRefinement::DoInteractiveUserInput( ) {
    UserInput* my_input = new UserInput("Blush_Refinement", 1.0);

    std::string input_mrc_filename  = my_input->GetFilenameFromUser("Enter input mrc volume name", "The volume that will be denoised via blush refinement", "input.mrc", true);
    std::string output_mrc_filename = my_input->GetStringFromUser("Enter desired name of output mrc volume", "The denoised volume post blush refinement", "output.mrc");
    std::string model_path          = my_input->GetFilenameFromUser("Enter path to converted blush model", "The TorchScript converted form of the blush model", "blush_model.pt", true);
    bool        use_dbg             = my_input->GetYesNoFromUser("Use debug print statements?", "For development purposes and tracing program execution,", "No");
    // int         particle_diameter   = my_input->GetIntFromUser("Enter particle diameter", "Diameter of the particle in ", "180", 0, 2000);

    delete my_input;

    my_current_job.Reset(4);
    my_current_job.ManualSetArguments("tttb", input_mrc_filename.c_str( ),
                                      output_mrc_filename.c_str( ),
                                      model_path.c_str( ),
                                      use_dbg);
    //   particle_diameter);
}

bool BlushRefinement::DoCalculation( ) {
    std::string input_mrc_filename{my_current_job.arguments[0].ReturnStringArgument( )};
    std::string output_mrc_filename{my_current_job.arguments[1].ReturnStringArgument( )};
    std::string model_filename{my_current_job.arguments[2].ReturnStringArgument( )};
    const bool  use_dbg{my_current_job.arguments[3].ReturnBoolArgument( )};
    // int         particle_diameter{my_current_job.arguments[2].ReturnIntegerArgument( )};

    // Load the model
    // Image                           input_mask; // May not need
    // TODO: replace with newly implemented C++ version of the model
    // torch::jit::script::Module      model;
    BlushModel                      model;
    MRCFile                         input_mrc(input_mrc_filename);
    Image                           input_volume;
    at::Tensor                      real_values_tensor;
    at::Tensor                      in_std; // locally derived standard deviation -- passed to model
    const int                       batch_size       = 2;
    const int                       in_channels      = 2;
    const int                       block_size       = 128;
    std::string                     weights_filename = "blush_v1.0.ckpt";
    std::vector<torch::jit::IValue> inputs; // For passing tensors to model

    input_volume.ReadSlices(&input_mrc, 1, input_mrc.ReturnNumberOfSlices( ));
    if ( use_dbg )
        wxPrintf("Volume read in successfully\n");
    const int     box_size   = input_volume.logical_x_dimension;
    const int64_t num_pixels = std::pow(box_size, 3);
    at::Tensor    blocks;
    {
        std::vector<int64_t> tmp_blocks_vector = {batch_size, in_channels, block_size, block_size, block_size};
        blocks                                 = torch::zeros(tmp_blocks_vector, torch::TensorOptions( ).dtype(torch::kFloat32));
    }

    if ( use_dbg ) {
        wxPrintf("blocks shape: [%ld, %ld, %ld, %ld, %ld]\n\n",
                 blocks.size(0),
                 blocks.size(1),
                 blocks.size(2),
                 blocks.size(3),
                 blocks.size(4));
    }

    try {
        // FIXME: this works for now for testing purposes; but, this would need to be included alongside the project whenever blush was needed
        // model = torch::jit::load("/workspaces/cisTEM/src/programs/blush_refinement/blush_model.pt");
        // model = torch::jit::load("/home/tim/VS_Projects/cisTEM/src/programs/blush_refinement/blush_model.pt");
        wxPrintf("Loading model...\n");

        // TODO: According to sample libtorch code this should work, but doesn't; determine why

        // For loading PyTorch model weights
        // std::unordered_map<std::string, torch::Tensor> state_dict;
        // torch::load(state_dict, model_filename);
        model.load_weights(model_filename);
    } catch ( std::exception& e ) {
        wxPrintf("Failed to load blush model: %s\n", e.what( ));
    }

    if ( use_dbg )
        wxPrintf("Blush model successfully loaded\n");
    // const float pixel_size{input_mrc.ReturnPixelSize( )};

    // torch::Tensor model_voxel_size_tensor{torch::tensor({pixel_size})};
    // torch::Tensor box_size_tensor{torch::tensor({box_size})};

    // input_volume.Normalize( );
    // FIXME: this may not work as an input; the model expects a 3D array, not a 1D array containing all elements of the 3D
    real_values_tensor = torch::from_blob(input_volume.real_values, {num_pixels}, torch::kFloat32);
    real_values_tensor = real_values_tensor.view({box_size, box_size, box_size}).contiguous( ); // Shape this as a 3D volume instead of linearized 1D
    if ( use_dbg )
        wxPrintf("Loaded real_values into tensor; shape is: %ld, %ld, %ld\n", real_values_tensor.size(0), real_values_tensor.size(1), real_values_tensor.size(2));
    // Get standard dev, normalize
    {
        try {
            // NOTE: clone used for creating a copy of the original tensor
            at::Tensor tmp_real_values_tensor = real_values_tensor.clone( );
            wxPrintf("Calculating localized standard deviation...\n");
            at::Tensor tmp_in_std = get_local_std_dev(tmp_real_values_tensor, 10, use_dbg);

            if ( use_dbg )
                wxPrintf("tmp_in_std shape: %ld, %ld, %ld, %ld\n", tmp_in_std.size(0), tmp_in_std.size(1), tmp_in_std.size(2), tmp_in_std.size(3));

            in_std = tmp_in_std / torch::mean(tmp_in_std); // Necessary if already normalized?
            wxPrintf("Standard deviation computed.\n");
            at::Tensor mean    = torch::mean(real_values_tensor);
            at::Tensor std_dev = torch::std(real_values_tensor);
            // This is just setting zero-mean (normalizing) -- may be able to just use already normalized real_values array in the future
            real_values_tensor = (tmp_real_values_tensor - mean) / (std_dev + 1e-8); // Get to do this because libtorch C++ backend automatically applies tensor ops to whole tensor
        } catch ( std::exception& e ) {
            wxPrintf("Error when getting standard deviation and normalizing the real_values tensor: %s\n", e.what( ));
            return false;
        }
    } // end getting std_dev, normalizing

    if ( use_dbg )
        wxPrintf("real_values tensor acquired\n");

    // Handling the complex_values array is a little more complicated; have to create a tensor with real part, then imaginary part, then stack them
    // at::Tensor complex_tensor;

    // This complex thing will not be needed
    // Get complex tensor
    // {
    //     std::vector<float> real_part;
    //     std::vector<float> imag_part;
    //     // DEBUG:
    //     // wxPrintf("max size of float vector: %zu\n", imag_part.max_size( ));

    //     std::complex<float> cur_val;
    //     input_volume.ForwardFFT( );

    //     // NOTE: any time access of complex_values is needed, limit by real_memory_allocated / 2 for looping over all the pixels
    //     for ( int i = 0; i < input_volume.real_memory_allocated / 2; i++ ) {
    //         cur_val = input_volume.complex_values[i];
    //         real_part.push_back(real(cur_val));
    //         imag_part.push_back(imag(cur_val));
    //         // DEBUG:
    //         // wxPrintf("i == %i\n", i);
    //     }
    //     input_volume.BackwardFFT( );
    //     // Stack real and imaginary parts to make complex tensor
    //     at::Tensor real_tensor = torch::tensor(real_part);
    //     at::Tensor imag_tensor = torch::tensor(imag_part);
    //     complex_tensor         = torch::stack({real_tensor, imag_tensor}, 0);
    // } // End tensorising complex_values

    // DEBUG:
    // wxPrintf("Succesfully converted volume data to tensors\n");

    // torch::jit:IValue is type-erased
    // NOTE: inputs must only receive tensors; tensors can be scalars, vectors, or matrices
    // Raison mentioned that the model should only really need the real_values; so let's do the necessary setup steps and then see what happens
    // TODO: necessary steps are from apply_model; also, note that TWO inputs are required; I think either something is missing from Raison's
    // explanation or I've fucked up somehow
    try {
        real_values_tensor = real_values_tensor.view({1, box_size, box_size, box_size});
    } catch ( std::exception& e ) {
        wxPrintf("Error line 143 calling view on real_values_tensor: %s\n", e.what( ));
    }
    // FIXME: instead of using this test_tensor, actually just get the normalized stddev and pass it alongside the volume
    try {
        in_std = in_std.view({1, box_size, box_size, box_size});
    } catch ( std::exception& e ) {
        wxPrintf("Error line 149 calling view on in_std: %s\n", e.what( ));
    }
    if ( use_dbg ) {
        wxPrintf("\nreal_values_tensor shape: [%ld, %ld, %ld, %ld]\n\n",
                 real_values_tensor.size(0),
                 real_values_tensor.size(1),
                 real_values_tensor.size(2),
                 real_values_tensor.size(3));
        wxPrintf("in_std shape = [%ld, %ld, %ld, %ld]\n\n",
                 in_std.size(0),
                 in_std.size(1),
                 in_std.size(2),
                 in_std.size(3));
    }
    // For debugging/knowledge check purposes: index of linearized form of 3D volume: z * box_size^2 + y * box_size + x
    // This should get the final pixel value of the first slice
    if ( use_dbg ) {
        wxPrintf("real_values_tensor[0][191][191][191]: %f\n", real_values_tensor[0][191][191][191].item<float>( )); // [191][191]
        wxPrintf("original real_values at corresponding 1D position: %f\n", input_volume.real_values[7077887]); // 36863
        wxPrintf("Current padding jump value: %i\n", input_volume.padding_jump_value);
    }
    // inputs.push_back(real_values_tensor);
    // inputs.push_back(in_std);
    // inputs.push_back(complex_tensor);
    // inputs.push_back(model_voxel_size_tensor);
    // inputs.push_back(box_size_tensor);

    if ( use_dbg )
        wxPrintf("Successfully passed all inputs\n");
    // FIXME: model crashes when trying to run with input
    // Curious of how the tensors will be returned, given C++ doesn't support
    // multiple returns -- recommended way to handle this normally is with a tuple, so hopefully it will be something of that sort
    std::tuple<at::Tensor, at::Tensor> output;
    // NOTE: using this try-catch syntax is vitally important for understanding what's going on
    // with the TorchScript side of the code; without it, it will throw an exception that is uncaught and gives no info.

    // TODO: implement batch processing in the same way as apply_model in util.py
    try {
        wxPrintf("Running blush model...\n");
        output = model.forward(real_values_tensor, in_std);
    } catch ( std::exception& e ) {
        wxPrintf("Couldn't run model; exception occurred: %s\n", e.what( ));
    }

    if ( use_dbg )
        wxPrintf("Successfully ran model.forward with inputs\n");

    // input_volume.ForwardFFT( );
    // input_volume.Normalize( );

    return true;
}

// Image generate_radial_mask(int box_size, float radius, int voxel_size) {

//     Image mask(box_size / 2, box_size / 2, box_size / 2);

//     return mask;
// }

// FIXME: something went wrong: the dimensions of std is wrong somehow
at::Tensor get_local_std_dev(torch::Tensor grid, int size, const bool use_dbg) {
    // Unsqueeze and clone the grid tensor
    if ( use_dbg )
        wxPrintf("Dimensions of grid: %ld, %ld, %ld\n", grid.size(0), grid.size(1), grid.size(2));
    auto new_grid = grid.clone( ).unsqueeze(0).unsqueeze(0);
    auto grid2    = new_grid.square( );

    if ( use_dbg )
        wxPrintf("Dimensions of new_grid (after unsqueezing grid): %ld, %ld, %ld, %ld, %ld\n", new_grid.size(0), new_grid.size(1), new_grid.size(2), new_grid.size(3), new_grid.size(4));

    // Create the kernel
    auto ls     = torch::linspace(-1.5, 1.5, 2 * size + 1); // Create tensor of size 2 * size + 1 with evenly spaced values between -1.5 and 1.5
    auto kernel = torch::exp(-ls.square( )).to(new_grid.device( )); // Make normal dist (gets rid of negatives (square), applies exponential function(exp))
    kernel /= kernel.sum( ); // Divide by sum...why?

    kernel = kernel.unsqueeze(0).unsqueeze(0).unsqueeze(2).unsqueeze(3); // Shape: (1, 1, kernel_size, 1, 1)
    kernel = kernel.expand({1, 1, 2 * size + 1, 2 * size + 1, 2 * size + 1});

    if ( use_dbg )
        wxPrintf("kernel shape: %ld, %ld, %ld, %ld, %ld\n", kernel.size(0), kernel.size(1), kernel.size(2), kernel.size(3), kernel.size(4));

    // Perform 3D convolution
    for ( int i = 0; i < 3; ++i ) {
        new_grid = new_grid.permute({0, 1, 2, 3, 4}); // Change shape to (N, C, D, H, W)
        torch::nn::functional::Conv3dFuncOptions options;
        // options.stride({1, 1, 1});
        options.padding({size, size, size});
        new_grid = torch::nn::functional::conv3d(new_grid, /*weight=*/kernel, options);

        grid2 = grid2.permute({0, 1, 2, 3, 4}); // Same permutation
        grid2 = torch::nn::functional::conv3d(grid2, /*weight=*/kernel, options);
    }

    auto std = torch::sqrt(torch::clamp(grid2 - new_grid.square( ), 0));
    if ( use_dbg )
        wxPrintf("\nAfter get_local_std_dev (pre squeeze): %ld, %ld, %ld, %ld, %ld\n", std.size(0), std.size(1), std.size(2), std.size(3), std.size(4));
    std = std.squeeze(1); // Remove the second dimension
    if ( use_dbg )
        wxPrintf("After std after squeeze: %ld, %ld, %ld, %ld\n\n", std.size(0), std.size(1), std.size(2), std.size(3));

    return std;
}