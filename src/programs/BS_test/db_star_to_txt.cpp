#include "../../core/core_headers.h"

class TempApp : public MyApp {
  public:
    bool DoCalculation( );
    void DoInteractiveUserInput( );

  private:
};

IMPLEMENT_APP(TempApp)

void TempApp::DoInteractiveUserInput( ) {
    UserInput*  my_input          = new UserInput("Class Based Parameter Selection", 1.0);
    std::string database_filename = my_input->GetFilenameFromUser("Enter the database filename", "The database filename is found in the cisTEM project directory, and ends with a .db extension", "input_database.db", true);
    std::string output_filename   = my_input->GetStringFromUser("Enter the output filename (no extension)", "The name of the text file after the operation is complete", "output"); // This is not needed; only db name and class_id
    int         class_id          = my_input->GetIntFromUser("Enter the desired classification ID", "The classification ID representing the desired class", "0", 1);

    my_current_job.Reset(3);
    my_current_job.ManualSetArguments("tti", database_filename.c_str( ),
                                      output_filename.c_str( ),
                                      class_id);
};

bool TempApp::DoCalculation( ) {
    wxFileName database_filename = wxFileName(my_current_job.arguments[0].ReturnStringArgument( ));
    wxString   output_filename   = wxString(my_current_job.arguments[1].ReturnStringArgument( ));
    long       class_id          = my_current_job.arguments[2].ReturnIntegerArgument( );

    // Open db and prepare the refinement packages
    Database selected_db = Database( );
    selected_db.Open(database_filename);
    selected_db.BeginAllRefinementPackagesSelect( );
    ArrayOfRefinementPackages refinement_package_list;
    RefinementPackage*        temp_package;

    // Load all the refinement packages into an array
    while ( selected_db.last_return_code == SQLITE_ROW ) {
        temp_package = selected_db.GetNextRefinementPackage( );
        refinement_package_list.Add(temp_package);
    }
    selected_db.EndBatchSelect( );
    // setup finished; now do the actual work of writing out the star file containing the params
    Classification* needed_class = selected_db.GetClassificationByID(class_id);
    selected_db.Close(false);
    RefinementPackage needed_package = refinement_package_list[needed_class->refinement_package_asset_id - 1]; // Select refinement package using classification we made
    needed_class->WritecisTEMStarFile(output_filename, temp_package, false);
    return true;
}