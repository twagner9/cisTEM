#include "../../core/core_headers.h"

class TempApp : public MyApp {
  public:
    bool DoCalculation( );
    void DoInteractiveUserInput( );

  private:
};

IMPLEMENT_APP(TempApp)

void TempApp::DoInteractiveUserInput( ) {
    UserInput*  my_input             = new UserInput("Class Based Parameter Selection", 1.0);
    std::string database_filename    = my_input->GetFilenameFromUser("Enter the database filename", "The database filename is found in the cisTEM project directory, and ends with a .db extension", "input_database.db", true);
    int         class_id             = my_input->GetIntFromUser("Enter the desired classification ID", "The classification ID representing the desired class", "0", 1);
    int         wanted_class_average = my_input->GetIntFromUser("Enter the desired class average ID", "The specific class average result", "0", 0);

    my_current_job.Reset(3);
    my_current_job.ManualSetArguments("tii", database_filename.c_str( ),
                                      class_id,
                                      wanted_class_average);
};

bool TempApp::DoCalculation( ) {
    wxFileName database_filename       = wxFileName(my_current_job.arguments[0].ReturnStringArgument( ));
    long       class_id                = my_current_job.arguments[1].ReturnIntegerArgument( );
    int        wanted_class_average_id = my_current_job.arguments[2].ReturnIntegerArgument( );

    // Open db and prepare the refinement packages
    Database selected_db = Database( );
    selected_db.Open(database_filename);

    // This is the positions in the particle stack of all the class average members
    wxArrayLong class_members = selected_db.Return2DClassMembers(class_id, wanted_class_average_id);

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
    needed_class->WritecisTEMStarFile("extracted_subtraction_class", &needed_package, false);

    /*MRCFile extracted_particle_members(needed_package.);
    selected_db.Close(false);
    std::ofstream my_file;
    std::string   text_filename = "extracted_particles_" + std::to_string(class_id) + "_" + std::to_string(wanted_class_average_id) + ".txt";
    my_file.open(text_filename);

    // Write out the positions to a text file
    for ( int particle_counter = 0; particle_counter < class_members.GetCount( ); particle_counter++ )
        my_file << class_members[particle_counter] << std::endl;

    my_file.close( ); */
    return true;
    /*
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
    needed_class->WritecisTEMStarFile("extracted_class", &needed_package, false);

    MRCFile extracted_particle_members(needed_package.);
    */
}