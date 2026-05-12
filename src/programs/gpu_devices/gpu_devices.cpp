#include "../../core/core_headers.h"
#include "../../gpu/DeviceManager.h"

class
        GpuDevices : public MyApp {

  public:
    bool                                     DoCalculation( );
    void                                     DoInteractiveUserInput( );
    std::vector<MyApp::InteractiveParameter> GetInteractiveParameters( ) const override;

  private:
};

IMPLEMENT_APP(GpuDevices)

// override the DoInteractiveUserInput

void GpuDevices::DoInteractiveUserInput( ) {
}

// override the do calculation method which will be what is actually run..

bool GpuDevices::DoCalculation( ) {
    DeviceManager gpuDev;

    wxPrintf("\nGpuDevices is running...\n\n");

    gpuDev.ListDevices( );
    return true;
}

// Auto-added by scripts/add_interactive_parameters.py
std::vector<MyApp::InteractiveParameter> GpuDevices::GetInteractiveParameters( ) const {
    std::vector<MyApp::InteractiveParameter> params;
    (void)params; // no parameters detected

    return params;
}
