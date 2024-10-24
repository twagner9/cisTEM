#include <torch/torch.h>
#include <torch/serialize.h>
#include <vector>
#include <iostream>

namespace F = torch::nn::functional;
// WIP: rebuild the PyTorch architecture in C++ for more customization and potentially efficiency with multi-threading

// Constants
const int  BLOCK_SIZE = 64;
const int  DEPTH      = 5;
const int  WIDTH      = 16;
const bool TRILINEAR  = true;
const bool MASK_INFER = true;

// Normalization module
using NORM_MODULE = torch::nn::InstanceNorm3d;
using ACTIVATION  = torch::nn::SiLU;

// Weight box creation
torch::Tensor make_weight_box(int size, int margin = 4) {
    margin = margin > 0 ? margin : 1;
    int s  = size - margin * 2;

    torch::Tensor z = torch::linspace(-s / 2, s / 2, s).view({-1, 1, 1, 1});
    torch::Tensor y = torch::linspace(-s / 2, s / 2, s).view({1, -1, 1, 1});
    torch::Tensor x = torch::linspace(-s / 2, s / 2, s).view({1, 1, -1, 1});

    auto r = torch::maximum(torch::abs(x), torch::abs(y));
    r      = torch::maximum(torch::abs(z), r);
    r      = torch::cos(r / r.max( ) * (M_PI / 2)) * 0.6 + 0.4;

    auto w                                                                                            = torch::zeros({size, size, size});
    w.slice(0, margin, size - margin).slice(1, margin, size - margin).slice(2, margin, size - margin) = r;

    return w;
}

// Double Conv blockout
struct DoubleConv : torch::nn::Module {
    DoubleConv(int in_channels, int out_channels, int mid_channels = -1) : conv(register_module("conv", torch::nn::Sequential(
                                                                                                                torch::nn::Conv3d(torch::nn::Conv3dOptions(in_channels, (mid_channels < 0) ? out_channels : mid_channels, /*kernel_size=*/3).padding(1)),
                                                                                                                torch::nn::InstanceNorm3d(torch::nn::InstanceNorm3dOptions((mid_channels < 0) ? out_channels : mid_channels).affine(true)), // Normalization
                                                                                                                torch::nn::SiLU( ), // Activation
                                                                                                                torch::nn::Conv3d(torch::nn::Conv3dOptions((mid_channels < 0) ? out_channels : mid_channels, out_channels, /*kernel_size=*/3).padding(1)),
                                                                                                                torch::nn::InstanceNorm3d(torch::nn::InstanceNorm3dOptions(out_channels).affine(true)), // Normalization
                                                                                                                torch::nn::SiLU( )))) {
        // if ( mid_channels == -1 ) {
        //     mid_channels = out_channels;
        // }
        // conv = register_module("conv", torch::nn::Sequential(
        //                                        torch::nn::Conv3d(torch::nn::Conv3dOptions(in_channels, mid_channels, /*kernel_size=*/3).padding(1)),
        //                                        torch::nn::InstanceNorm3d(torch::nn::InstanceNorm3dOptions(mid_channels).affine(true)), // Normalization
        //                                        torch::nn::SiLU( ), // Activation
        //                                        torch::nn::Conv3d(torch::nn::Conv3dOptions(mid_channels, out_channels, /*kernel_size=*/3).padding(1)),
        //                                        torch::nn::InstanceNorm3d(torch::nn::InstanceNorm3dOptions(out_channels).affine(true)), // Normalization
        //                                        torch::nn::SiLU( ))); // Activation
    }

    torch::Tensor forward(torch::Tensor x) {
        return conv->forward(x);
    }

    torch::Tensor& conv1_weight( ) { return conv[0]->weight; }

    torch::Tensor& conv1_bias( ) { return conv[0]->bias; }

    torch::Tensor& conv2_weight( ) { return conv[3]->weight; }

    torch::Tensor& conv2_bias( ) { return conv[3]->bias; }

    torch::nn::Sequential conv{nullptr};
};

// Upsample and Conv block
struct Up : torch::nn::Module {
    Up(int in_channels, int out_channels, bool trilinear = true, bool pad = false) {
        this->pad = pad;
        if ( trilinear ) {
            up   = register_module("up", torch::nn::Upsample(torch::nn::UpsampleOptions( ).scale_factor(std::vector<double>({2})).mode(torch::kTrilinear).align_corners(true)));
            conv = register_module("conv", std::make_shared<DoubleConv>(in_channels, out_channels, in_channels / 2));
        }

        else {
            up_ct = register_module("up_ct", torch::nn::ConvTranspose3d(torch::nn::ConvTranspose3dOptions(in_channels, in_channels / 2, 2)));
            conv  = register_module("conv", std::make_shared<DoubleConv>(in_channels, out_channels));
        }
    }

    torch::Tensor forward(torch::Tensor x1, torch::Tensor x2) {
        x1 = up->forward(x1);

        // Gets difference between dimensions for padding
        if ( pad ) {
            int64_t diffZ = x2.size(2) - x1.size(2);
            int64_t diffY = x2.size(3) - x1.size(3);
            int64_t diffX = x2.size(4) - x1.size(4);
            x1            = F::pad(x1, F::PadFuncOptions({diffX / 2, diffX - diffX / 2, diffY / 2, diffY - diffY / 2, diffZ / 2, diffZ - diffZ / 2}));
        }
        auto x = torch::cat({x2, x1}, /*dim=*/1);
        return conv->forward(x);
    }

    // These shared pointers are used for user-defined objects because this is the type expected by register_module; they will be used throughout the source file
    torch::nn::Upsample        up{nullptr};
    torch::nn::ConvTranspose3d up_ct{nullptr};

    std::shared_ptr<DoubleConv> conv;
    bool                        pad;
};

// Main Model
struct BlushModel : torch::nn::Module {
    BlushModel(int in_channels = 2, int out_channels = 2) {
        // Initial convolution
        inc = register_module("inc", std::make_shared<DoubleConv>(in_channels, WIDTH));

        // Downsampling layers
        for ( int i = 0; i < DEPTH - 1; ++i ) {
            int n = 1 << i; // 2^i
            down.push_back(register_module("down" + std::to_string(i), std::make_shared<DoubleConv>(WIDTH * n, WIDTH * n * 2)));
        }
        down.push_back(register_module("down" + std::to_string(DEPTH - 1), std::make_shared<DoubleConv>(WIDTH * (1 << (DEPTH - 1)), WIDTH * (1 << DEPTH) / (TRILINEAR ? 2 : 1))));

        // Up sampling layers
        for ( int i = 0; i < DEPTH - 1; ++i ) {
            int n = 1 << (DEPTH - 1 - i); // 2^(DEPTH-1-i)
            up.push_back(register_module("up" + std::to_string(i), std::make_shared<Up>(WIDTH * n * 2, WIDTH * n / (TRILINEAR ? 2 : 1), TRILINEAR)));
        }
        up.push_back(register_module("up" + std::to_string(DEPTH - 1), std::make_shared<Up>(WIDTH * 2, WIDTH, TRILINEAR)));

        outc = register_module("outc", torch::nn::Conv3d(torch::nn::Conv3dOptions(WIDTH, out_channels, 1)));
    }

    std::tuple<torch::Tensor, torch::Tensor> forward(torch::Tensor grid, torch::Tensor local_std) {
        auto std           = torch::std(grid, {-1, -2, -3}, /*keepdim=*/true);
        auto mean          = torch::mean(grid, {-1, -2, -3}, /*keepdim=*/true);
        auto grid_standard = (grid - mean) / (std + 1e-12);
        auto input         = torch::cat({grid_standard.unsqueeze(1), local_std.unsqueeze(1)}, /*dim=*/1);

        auto                       nn = inc->forward(input);
        std::vector<torch::Tensor> skip;

        // Downward path
        for ( int i = 0; i < DEPTH; ++i ) {
            skip.push_back(nn);
            nn = F::max_pool3d(nn, torch::nn::MaxPool3dOptions(2));
            nn = down[i]->forward(nn);
        }

        // Reverse skip connections
        std::reverse(skip.begin( ), skip.end( ));

        // Upward path
        for ( int i = 0; i < DEPTH; ++i ) {
            nn = up[i]->forward(nn, skip[i]);
        }

        nn = outc->forward(nn);

        auto output = grid_standard - nn.slice(1, 0, 1);
        output      = output * (std + 1e-12) + mean;

        torch::Tensor mask_logit = MASK_INFER ? nn.slice(1, 1, 2) : torch::Tensor( );
        return {output, mask_logit};
    }

    void load_weights(const std::string& filename) {
        torch::serialize::InputArchive archive;
        archive.load_from(filename);

        // Load weights and biases for the input layer
        archive >> inc->conv[0]->weight;
        archive >> inc->conv[0]->bias;

        // Load weights for downsampling layers
        for ( size_t i = 0; i < down.size( ); ++i ) {
            auto& layer = down[i];
            archive >> layer->conv1_weight( );
            archive >> layer->conv1_bias( );
            archive >> layer->conv2_weight( );
            archive >> layer->conv2_bias( );
        }

        // Load weights for upsampling layers
        for ( size_t i = 0; i < up.size( ); ++i ) {
            auto& layer = up[i];
            archive >> layer->conv1_weight( );
            archive >> layer->conv1_bias( );
            archive >> layer->conv2_weight( );
            archive >> layer->conv2_bias( );
        }

        // Load weights for the output layer
        archive >> outc->weight;
        archive >> outc->bias;
    }

    std::shared_ptr<DoubleConv>              inc;
    std::vector<std::shared_ptr<DoubleConv>> down;
    std::vector<std::shared_ptr<Up>>         up;
    torch::nn::Conv3d                        outc{nullptr};
};

// Main function (for testing purposes)
// int main( ) {
// torch::manual_seed(123);

// Example usage of BlushModel
// BlushModel    model;
// torch::Tensor grid      = torch::randn({1, 2, BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE});
// torch::Tensor local_std = torch::randn({1, 1, BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE});

// auto [output, mask_logit] = model.forward(grid, local_std);

// std::cout << "Output size: " << output.sizes( ) << std::endl;
// if ( MASK_INFER ) {
// std::cout << "Mask logit size: " << mask_logit.sizes( ) << std::endl;
// }

// return 0;
// }
