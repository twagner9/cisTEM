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

#include "../../../include/libtorch/libtorch_push_macros.h"
#undef N_ // FOR VM -- REMOVE
#include <torch/nn/functional/conv.h>
#include <torch/script.h>
#include <torch/torch.h>
#include "../../../include/libtorch/libtorch_pop_macros.h"

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

// void BlushRefinement::DoInteractiveUserInput( ) {
//     UserInput* my_input = new UserInput("Blush_Refinement", 1.0);

//     std::string input_volume_filename = my_input->GetFilenameFromUser("Input mrc filename", "The volume that will be denoised via blush refinement", "input.mrc", true);
//     std::string output_mrc_filename   = my_input->GetStringFromUser("Output mrc filename", "The denoised volume post blush refinement", "output.mrc");
//     bool        use_dbg               = my_input->GetYesNoFromUser("Use debug print statements?", "For development purposes and tracing program execution,", "No");
//     bool        apply_automask        = my_input->GetYesNoFromUser("Apply automask?", "Determines whether to apply an automask to the volume", "No");
//     bool        use_radial_mask       = my_input->GetYesNoFromUser("Apply radial mask before inference?", "Sets whether the volume tensor will be masked with a radial mask before passing blocks to the model for inference", "Yes");

//     bool  provide_mask_radius = false;
//     float particle_diameter = 0, desired_mask_radius_in_angstroms = 0;
//     if ( use_radial_mask ) {
//         provide_mask_radius = my_input->GetYesNoFromUser("Provide mask radius?", "If specifying a particular mask radius, enter yes; else, mask radius will be determined from the particle diameter", "No");
//         if ( ! provide_mask_radius ) {
//             particle_diameter = my_input->GetIntFromUser("Particle diameter", "Diameter of the particle in ", "180", 0, 2000);
//         }
//         else {
//             desired_mask_radius_in_angstroms = my_input->GetFloatFromUser("Mask radius (angstroms)", "The user desired mask radius since it's not being derived from particle diameter", "20.0", 0.0f);
//         }
//     }

//     int max_threads = 1;

// #ifdef _OPENMP
//     max_threads = my_input->GetIntFromUser("Enter desired number of threads", "Number of threads for parallelizing localized standard deviation calculation", "1", 1, 256);
// #endif

//     delete my_input;

//     my_current_job.Reset(5);
//     my_current_job.ManualSetArguments("ttbbbbffi", input_volume_filename.c_str( ),
//                                       output_mrc_filename.c_str( ),
//                                       use_dbg,
//                                       apply_automask,
//                                       use_radial_mask,
//                                       provide_mask_radius,
//                                       particle_diameter,
//                                       desired_mask_radius_in_angstroms,
//                                       max_threads);
// }

// NOTE: all libtorch functionality sorts by and assumes z, y, x rather than x, y, z
// bool BlushRefinement::DoCalculation( ) {

//     // NOTE: this is a no_grad guard, so that the Blush model does not track gradients in a computation graph for the forward pass.
//     // If it were to track gradients, it would consume a lot of memory (sometimes greater than 32 GB).
//     // These computation graphs are used for backpropagation during neural network training, which is not needed in this case as this
//     // is using the model in inference mode (i.e., for making predictions rather than training), along with loaded weights.
//     torch::NoGradGuard no_grad;

//     std::string input_volume_filename{my_current_job.arguments[0].ReturnStringArgument( )};
//     std::string output_mrc_filename{my_current_job.arguments[1].ReturnStringArgument( )};
//     const bool  use_dbg{my_current_job.arguments[2].ReturnBoolArgument( )};
//     const bool  apply_automask{my_current_job.arguments[3].ReturnBoolArgument( )};
//     const bool  use_radial_mask{my_current_job.arguments[4].ReturnBoolArgument( )};
//     const bool  provide_mask_radius{my_current_job.arguments[5].ReturnBoolArgument( )};
//     const float particle_diameter{my_current_job.arguments[6].ReturnFloatArgument( )};
//     float       desired_mask_radius_in_angstroms{my_current_job.arguments[7].ReturnFloatArgument( )};
//     const int   max_threads{my_current_job.arguments[8].ReturnIntegerArgument( )};

//     constexpr float model_voxel_size{1.5f}; // The expected voxel/pixel size of inputs to the blush model
//     constexpr float mask_edge_in_angstr{10.0f}; // Edge width of the mask in Angstroms; this is the same as the Python model uses
//     constexpr int   model_block_size{64};
//     constexpr int   strides{20};
//     constexpr int   in_channels{2};
//     constexpr int   batch_size{1};

//     // NOTE: Using multiple threads causes differences in the outputs of tensor operations
//     torch::set_num_threads(max_threads);

//     // NOTE: These are parameters that are directly relevant to the processing in the model
//     // model_voxel_size and model_block_size are values that are predetermined in model parameters; the latter 3
//     // could technically be user specified.
//     std::string exe_path = wxStandardPaths::Get( ).GetExecutablePath( ).ToStdString( );

//     // remove the binary name from the path
//     size_t last_slash_idx = exe_path.rfind('/');
//     if ( std::string::npos != last_slash_idx ) {
//         exe_path = exe_path.substr(0, last_slash_idx + 1);
//     }
//     std::string model_filename = exe_path + "blush_weights.dat";

//     BlushModel model(2, 2);
//     try {
//         model.load_weights(model_filename);
//     } catch ( std::exception& e ) {
//         wxPrintf("%s\n", e.what( ));
//     }

//     // Set to evaluation mode:
//     model.eval( );

//     MRCFile     input_mrc(input_volume_filename);
//     const float vol_original_pixel_size = input_mrc.ReturnPixelSize( );
//     const int   original_box_size       = input_mrc.ReturnXSize( );

//     if ( use_dbg )
//         wxPrintf("Input volume pixel size: %f\n", vol_original_pixel_size);

//     Image input_volume;
//     input_volume.ReadSlices(&input_mrc, 1, input_mrc.ReturnNumberOfSlices( ));

//     // This was added as a way to testdifferences in blush output when an automask is or is not applied to the input volume
//     //	 FIXME: this check doesn't quite work; the desired_mask_radius_in_angstroms is not always filled in with a nonzero value
//     if ( apply_automask ) {
//         // TODO: fill in same logic that autorefine uses for applying an automask
//         Image tmp;
//         if ( ! (desired_mask_radius_in_angstroms >= 1.0f) ) {
//             desired_mask_radius_in_angstroms = 52.0; // 52 is the default value passed by AutoRefine3DPanel
//         }
//         float original_average_value = input_volume.ReturnAverageOfRealValues(desired_mask_radius_in_angstroms / vol_original_pixel_size, true);
//         tmp.CopyFrom(&input_volume);
//         wxPrintf("vol_original_pixel_size check: %f; desired_mask_radius_in_angstroms check: %f\n", vol_original_pixel_size, desired_mask_radius_in_angstroms);
//         tmp.ConvertToAutoMask(vol_original_pixel_size, desired_mask_radius_in_angstroms, 7.0f, 0.1, true);
//         for ( long addr = 0; addr < input_volume.real_memory_allocated; addr++ ) {
//             if ( tmp.real_values[addr] == 0.0f )
//                 input_volume.real_values[addr] = original_average_value;
//         }

//         tmp.SetMinimumValue(original_average_value);
//         input_volume.CosineMask(desired_mask_radius_in_angstroms / vol_original_pixel_size, 1.0, false, true, 0.0);
//         // DEBUG:
//         input_volume.QuickAndDirtyWriteSlices("test_automasking.mrc", 1, input_volume.logical_z_dimension, true, vol_original_pixel_size);
//     }

//     float actual_sf;
//     int   new_box_size;
//     bool  must_resample;
//     {
//         float           wanted_sf = vol_original_pixel_size / model_voxel_size;
//         constexpr float tolerance = 1e-2f;
//         new_box_size              = static_cast<int>(std::floor(input_volume.logical_x_dimension * wanted_sf + 0.5f));

//         if ( new_box_size % 2 != 0 ) {
//             new_box_size++;
//         }

//         actual_sf     = static_cast<float>(new_box_size) / static_cast<float>(input_volume.logical_x_dimension);
//         must_resample = (std::abs(actual_sf - 1.0f) > tolerance);

//         if ( must_resample ) {
//             input_volume.ForwardFFT( );
//             input_volume.Resize(new_box_size, new_box_size, new_box_size);
//             input_volume.BackwardFFT( );
//         }

//         if ( use_dbg ) {
//             input_volume.QuickAndDirtyWriteSlices("cpp_check_resample.mrc", 1, input_volume.logical_z_dimension, true);
//             wxPrintf("Wrote out resampled volume.\n");
//         }
//     }

//     // TODO: REMOVE THIS, THIS IS FOR TESTING EXTERNAL PROGRAM TO CHECK FOR LIBRARY CONFLICTS
//     if ( use_dbg ) {
//         wxPrintf("Writing out the resampled volume to a text file\n");
//         write_real_values_to_text_file("resampled_data.txt", new_box_size, true, true, nullptr, input_volume.real_values);
//         wxPrintf("Finished writing out resampled_data.txt");
//     }

//     // if ( std::abs(vol_original_pixel_size - model_voxel_size) > 1e-2 )
//     //     must_resample = true;

//     // if ( must_resample ) {
//     //     actual_sf = vol_original_pixel_size / model_voxel_size;
//     //     new_box_size = myroundint(input_volume.logical_x_dimension * actual_sf);
//     //     if ( new_box_size % 2 != 0 ) {
//     //         new_box_size++;
//     //         actual_sf = float(new_box_size) / input_volume.logical_x_dimension;
//     //     }
//     // }

//     if ( use_dbg )
//         wxPrintf("new_box_size == %i\n", new_box_size);

//     wxDateTime overall_start = wxDateTime::Now( );
//     wxDateTime overall_finish;
//     wxDateTime local_std_dev_start;
//     wxDateTime local_std_dev_finish;
//     wxDateTime blush_start;
//     wxDateTime blush_finish;

//     wxPrintf("Reading input and loading volume and input mask...\n");

//     wxPrintf("actual_sf == %f\n", actual_sf);
//     // if ( must_resample ) {
//     //     input_volume.ForwardFFT( );
//     //     input_volume.Resize(new_box_size, new_box_size, new_box_size);
//     //     input_volume.BackwardFFT( );
//     // }

//     const int  box_size   = input_volume.logical_x_dimension;
//     const long num_pixels = std::pow(box_size, 3);

//     // TODO: it's technically possible to have a volume that is smaller than the model block size, so we should
//     // handle that case; for now, we assume that the input volume is larger than the model block size
//     torch::Tensor                 blocks;
//     std::vector<std::vector<int>> coords;
//     torch::Tensor                 volume_tensor;
//     torch::Tensor                 local_std_dev;
//     torch::Tensor                 weights;
//     torch::Tensor                 infer_grid;
//     torch::Tensor                 count_grid;
//     torch::Tensor                 mask_tensor;

//     try {
//         blocks        = torch::zeros({batch_size, model_block_size, model_block_size, model_block_size});
//         coords        = std::vector(batch_size, std::vector<int>(3, 0));
//         volume_tensor = torch::zeros({box_size, box_size, box_size}, torch::kFloat32);
//         local_std_dev = torch::zeros({box_size, box_size, box_size}); // locally derived standard deviation -- passed to model
//     } catch ( std::exception& e ) {
//         wxPrintf("Blush error - Error setting up tensors: %s\n", e.what( ));
//         SendErrorAndCrash(wxString::Format("Blush error - Error setting up tensors: %s\n", e.what( )));
//     }

//     input_volume.RemoveFFTWPadding( );
//     volume_tensor = torch::from_blob(input_volume.real_values, {box_size, box_size, box_size}, torch::kFloat32).clone( ).contiguous( );
//     volume_tensor = volume_tensor.permute({2, 1, 0}).contiguous( );

//     // Perform comparison of read in values in C++ and Python
//     if ( use_dbg ) {
//         write_real_values_to_text_file("cpp_tensorized_real_values.txt", new_box_size, false, true, &volume_tensor);
//         compare_text_file_lines("cpp_tensorized_real_values.txt", "py_tensorized_real_values.txt", "comp_tensorized_real_values.txt");
//     }

//     if ( torch::any(torch::isnan(volume_tensor)).item<bool>( ) ) {
//         wxPrintf("This tensor contains NaN values.\n");
//     }
//     if ( torch::any(torch::isinf(volume_tensor)).item<bool>( ) ) {
//         wxPrintf("Tensor contains inf values.\n");
//     }

//     // Get standard dev, normalize

//     wxPrintf("Generating post-processing grids.\n");

//     weights    = BlushHelpers::make_weight_box(model_block_size, 10);
//     infer_grid = torch::zeros({new_box_size, new_box_size, new_box_size}, torch::kFloat32);
//     count_grid = torch::zeros({new_box_size, new_box_size, new_box_size}, torch::kFloat32);

//     wxPrintf("Completed generation of post-processing grids.\n");

//     try {
//         torch::Tensor tmp_volume_tensor = volume_tensor.clone( ).contiguous( );
//         wxPrintf("Calculating localized standard deviation...\n\n\n");
//         local_std_dev_start               = wxDateTime::Now( );
//         torch::Tensor tmp_local_std_dev   = BlushHelpers::get_local_std_dev(tmp_volume_tensor.unsqueeze(0), 10).squeeze(0).contiguous( ).clone( );
//         torch::Tensor local_std_dev_mean  = tmp_local_std_dev.mean( );
//         torch::Tensor volume_mean         = volume_tensor.mean( );
//         torch::Tensor real_values_std_dev = volume_tensor.std( );

//         local_std_dev = tmp_local_std_dev / local_std_dev_mean;
//         volume_tensor = (volume_tensor - volume_mean) / (real_values_std_dev + 1e-8);
//     } catch ( std::exception& e ) {
//         wxPrintf("Blush error - Error getting localized standard deviation and normalizing the volume tensor: %s\n", e.what( ));
//         local_std_dev_finish           = wxDateTime::Now( );
//         wxTimeSpan duration_of_std_dev = local_std_dev_finish.Subtract(local_std_dev_start);
//         wxPrintf("Duration of standard deviation calculation:\t\t%s\n", duration_of_std_dev.Format( ));
//         return false;
//     }

//     // Perform comparison of localized std dev between C++ and Python
//     if ( use_dbg ) {
//         write_real_values_to_text_file("cpp_local_std_dev.txt", new_box_size, false, true, &local_std_dev);
//         compare_text_file_lines("cpp_local_std_dev.txt", "py_local_std_dev.txt", "comp_local_std_dev.txt");
//     }

//     local_std_dev_finish           = wxDateTime::Now( );
//     wxTimeSpan duration_of_std_dev = local_std_dev_finish.Subtract(local_std_dev_start);
//     wxPrintf("Duration of calculating localized standard deviation:        %s\n\n", duration_of_std_dev.Format( ));

//     if ( use_radial_mask ) {
//         wxPrintf("Applying mask...\n");
//         // TEST: Create mask the same way Python does
//         int   mask_edge_width = static_cast<int>(20.0f / model_voxel_size);
//         float radius;
//         if ( ! provide_mask_radius ) {
//             float radius = particle_diameter * model_voxel_size + mask_edge_width / 2;
//             radius       = std::min(radius, (input_volume.logical_x_dimension - mask_edge_width) / 2.0f + 1.0f);
//         }
//         else {
//             radius = desired_mask_radius_in_angstroms;
//         }
//         wxPrintf("logical_x_dimension=%i, radius= %f, mask_edge_width = %i\n", input_volume.logical_x_dimension, radius, mask_edge_width);
//         mask_tensor = BlushHelpers::generate_radial_mask(input_volume.logical_x_dimension, radius, mask_edge_width);

//         // Perform comparison of generated masks between C++ and Python
//         if ( use_dbg ) {
//             write_real_values_to_text_file("cpp_mask.txt", new_box_size, false, true, &mask_tensor);
//             compare_text_file_lines("cpp_mask.txt", "py_mask.txt", "comp_mask.txt");
//         }

//         // DEBUG: conditional check
//         if ( use_radial_mask ) {
//             volume_tensor *= mask_tensor;
//             local_std_dev *= mask_tensor;
//         }
//     }

//     // DEBUG:
//     // Test if the volume has the weird boxiness
//     {
//         Image tmp;
//         tmp.CopyFrom(&input_volume);
//     }
//     // END DEBUG
//     volume_tensor = volume_tensor.unsqueeze(0);
//     local_std_dev = local_std_dev.unsqueeze(0);

//     try {
//         wxPrintf("Running blush model...\n");
//         blush_start = wxDateTime::Now( );

//         BlockIterator it({new_box_size, new_box_size, new_box_size}, model_block_size, strides);
//         int           bi = 0;

//         //NOTE: omp parallel is not allowed with range-based for loops; for this to work, the loop would
//         // have to be manually unrolled
//         for ( auto it_coords : it ) {
//             torch::Tensor volume_block  = torch::zeros({1, model_block_size, model_block_size, model_block_size});
//             torch::Tensor std_dev_block = torch::zeros({1, model_block_size, model_block_size, model_block_size});

//             int x = std::get<0>(it_coords);
//             int y = std::get<1>(it_coords);
//             int z = std::get<2>(it_coords);

//             // -1 is the stop value returned by the BlockIterator when all blocks are done; could probably be improved
//             // for clairty in the future.
//             if ( x > -1 ) {
//                 float         cur_array_sum = 0.0f, mask_mean;
//                 torch::Tensor current_slice;
//                 if ( use_radial_mask ) {
//                     auto current_slice = mask_tensor.slice(0, z, z + model_block_size, 1).slice(1, y, y + model_block_size, 1).slice(2, x, x + model_block_size, 1);
//                     mask_mean          = current_slice.mean( ).item<float>( );
//                 }
//                 else {
//                     // TEST: checking if automasked blush output is comparable/better than the radial mask by basing blocks passed only on the
//                     torch::Tensor current_slice = volume_tensor.slice(0, z, z + model_block_size, 1).slice(1, y, y + model_block_size, 1).slice(2, x, x + model_block_size, 1);
//                     mask_mean                   = current_slice.mean( ).item<float>( );
//                 }

//                 constexpr float mean_threshold = 0.3f;
//                 if ( mask_mean < mean_threshold )
//                     continue;

//                 volume_block  = volume_tensor.slice(1, z, z + model_block_size, 1).slice(2, y, y + model_block_size, 1).slice(3, x, x + model_block_size, 1).clone( );
//                 std_dev_block = local_std_dev.slice(1, z, z + model_block_size, 1).slice(2, y, y + model_block_size, 1).slice(3, x, x + model_block_size, 1).clone( );
//                 bi++;
//             }

//             // NOTE: this will always be done right after the initial loading, so we don't need to worry about bi really
//             if ( bi == batch_size ) {
//                 auto          init_output = model.forward(volume_block, std_dev_block);
//                 torch::Tensor output      = std::get<0>(init_output);

//                 // Write out the output tensor to MRC for comparing with Python
//                 // if ( use_dbg && z == 120 && y == 180 && x == 120 ) {
//                 //     torch::Tensor output_for_mrc = output.clone( );
//                 //     // Write dimensions of output tensor
//                 //     wxPrintf("Output tensor shape: [%ld, %ld, %ld, %ld]\n", output_for_mrc.size(0), output_for_mrc.size(1), output_for_mrc.size(2), output_for_mrc.size(3));
//                 //     output_for_mrc = output_for_mrc.squeeze(0);
//                 //     output_for_mrc = output_for_mrc.permute({2, 1, 0}).contiguous( ); // switch to x-fastest
//                 //     Image output_sample;
//                 //     output_sample.Allocate(model_block_size, model_block_size, model_block_size);
//                 //     output_sample.RemoveFFTWPadding( );
//                 //     std::memcpy(output_sample.real_values, output_for_mrc.data_ptr<float>( ), std::pow(model_block_size, 3) * sizeof(float));
//                 //     output_sample.AddFFTWPadding( );
//                 //     output_sample.QuickAndDirtyWriteSlices("z120_y180_x120_cpp.mrc", 1, model_block_size, true);
//                 // }

//                 for ( int i = 0; i < batch_size; i++ ) {
//                     // Update inference grid
//                     // NOTE: slicing this way makes a "view" of the tensor, which means that the original tensor is modified as well;
//                     // thus the final addition to the infer_grid and count_grid tensors will be reflected in the original tensors
//                     torch::Tensor infer_grid_slice = infer_grid.slice(0, z, z + model_block_size, 1).slice(1, y, y + model_block_size, 1).slice(2, x, x + model_block_size, 1);
//                     auto          update           = output[i] * weights;
//                     infer_grid_slice += update;

//                     // Update count grid
//                     torch::Tensor count_grid_slice = count_grid.slice(0, z, z + model_block_size, 1).slice(1, y, y + model_block_size, 1).slice(2, x, x + model_block_size, 1);
//                     count_grid_slice += weights;
//                 }
//                 bi = 0;
//             }
//         }

//         // First, compare the fully loaded inference grid to see if there are differences
//         // This will be faster than trying to write and compare each individual iteration.
//         // if ( use_dbg ) {
//         //     write_real_values_to_text_file("cpp_infer_grid_result.txt", new_box_size, false, true, &infer_grid);
//         //     compare_text_file_lines("cpp_infer_grid_result.txt", "py_infer_grid_result.txt", "comp_infer_grid_result.txt");
//         // }

//         infer_grid = torch::where(count_grid > 0, infer_grid / count_grid, infer_grid);
//         infer_grid = torch::where(count_grid < 1e-1f, 0, infer_grid); // Set values where count_grid is less than 0.1 to 0
//         if ( use_radial_mask )
//             infer_grid *= mask_tensor;

//         infer_grid = infer_grid * (volume_tensor.std( ) + 1e-8) + volume_tensor.mean( ); // Normalize the inference grid

//         // Also compare the inference grid post normalization -- these values need only be very close
//         if ( use_dbg ) {
//             write_real_values_to_text_file("cpp_norm_infer_grid.txt", new_box_size, false, true, &infer_grid);
//             compare_text_file_lines("cpp_norm_infer_grid.txt", "py_norm_infer_grid.txt", "comp_norm_infer_grid.txt");
//         }

//         blush_finish              = wxDateTime::Now( );
//         wxTimeSpan blush_duration = blush_finish.Subtract(blush_start);

//         wxPrintf("Finished running blush model. Total duration:        %s\n", blush_duration.Format( ));
//     } catch ( std::exception& e ) {
//         wxPrintf("Couldn't run model; exception occurred: %s\n", e.what( ));
//         return false;
//     }

//     wxPrintf("Finishing post-processing...\n");

//     // Apply same post-inference logic:
//     // 1. Mask the output with the same mask used prior to inference.
//     if ( use_radial_mask )
//         infer_grid *= mask_tensor;

//     // 2. Resample the image
//     Image output_volume;
//     output_volume.Allocate(new_box_size, new_box_size, new_box_size);
//     infer_grid = infer_grid.permute({2, 1, 0}).contiguous( ); // switch back to x-fastest
//     std::memcpy(output_volume.real_values, infer_grid.data_ptr<float>( ), sizeof(float) * std::pow(new_box_size, 3));
//     output_volume.AddFFTWPadding( );

//     if ( must_resample ) {
//         // Image tmp = output_volume;
//         // tmp.ChangePixelSize(&output_volume, 1.0f / actual_sf, 1e-3f);
//         output_volume.ForwardFFT( );
//         output_volume.Resize(original_box_size, original_box_size, original_box_size);
//         output_volume.BackwardFFT( );
//     }

//     // 3. Normalize by multiplying pixels by (original size / resampled size)^3
//     // This should mirror: denoised_df *= (denoised_df.shape[0] / denoised_df_nv.shape[0])**3
//     // output_volume.ForwardFFT( );
//     // for ( int z = 0; z < output_volume.logical_z_dimension; z++ ) {
//     //     for ( int y = 0; y < output_volume.logical_y_dimension; y++ ) {
//     //         for ( int x = 0; x < output_volume.logical_x_dimension; x++ ) {
//     //             int index = output_volume.ReturnFourier1DAddressFromLogicalCoord(x, y, z);
//     //             output_volume.complex_values[index] *= std::pow(output_volume.logical_z_dimension * new_box_size, 3);
//     //         }
//     //     }
//     // }
//     // output_volume.NormalizeFT( );
//     // for ( int p = 0; p < output_volume.ReturnComplexPixelFromLogicalCoord( ); p++ ) {
//     //     output_volume.real_values[p] *= std::pow(output_volume.logical_x_dimension * new_box_size, 3);
//     // }

//     // 4. Calculate the crossover grid (the new and hard part)
//     // const float filter_edge_width                      = 3.0f;
//     // output_volume.std::complex<float>[] crossover_grid = get_crossover_grid(output_volume.physical_upper_bound_complex_x - 2, output_volume.logical_x_dimension, filter_edge_width);

//     output_volume.QuickAndDirtyWriteSlices(output_mrc_filename, 1, output_volume.logical_x_dimension, true);
//     wxPrintf("Blush complete.\n");
//     overall_finish      = wxDateTime::Now( );
//     wxTimeSpan duration = overall_finish.Subtract(overall_start);
//     wxPrintf("Total blush runtime:         %s\n", duration.Format( ));

//     return true;
// }

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
    int         max_threads{my_current_job.arguments[4].ReturnIntegerArgument( )};

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
    BlushHelpers::ApplyBlush(input_volume, pixel_size, mask_radius, total_blush_iterations, max_threads, stop_flag, [progress_bar](int percent, long seconds_remaining) {
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
    ofile.SetPixelSizeAndWriteHeader(1.5);

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