#include "core_headers.h"

MRCFile::MRCFile(std::string filename, bool overwrite)
{
	OpenFile(filename, overwrite);

}

MRCFile::~MRCFile()
{
	CloseFile();
}

void MRCFile::CloseFile()
{
	if (rewrite_header_on_close == true) WriteHeader();
	if (my_file.is_open()) my_file.close();
}

void MRCFile::OpenFile(std::string filename, bool overwrite)
{
	MyDebugAssertFalse(my_file.is_open(), "File Already Open!");

	bool file_already_exists;

	// if overwrite is specified, then we delete the file nomatter what..
	// if it isn't, then we need to know if the file already exists..

	if (overwrite == true) file_already_exists = false;
	else file_already_exists = DoesFileExist(filename);


	// Now open it, truncating to 0 if it doesn't already exist, or we specified overwrite

	if (file_already_exists == true)
	{
		my_file.open(filename.c_str(), std::ios::in | std::ios::out | std::ios::binary);

		if (my_file.is_open() == false)
		{
			printf("Opening of file %s failed!! - Exiting..\n\n", filename.c_str());
			abort();
		}

		// read the header

		if (file_already_exists == true) my_header.ReadHeader(&my_file);

	}
	else
	{
		my_file.open(filename.c_str(), std::ios::in | std::ios::out | std::ios::trunc | std::ios::binary);

		if (my_file.is_open() == false)
		{
			printf("Opening of file %s failed!! - Exiting..\n\n", filename.c_str());
			abort();
		}

		// Blank the header, it'll have to be written with the correct values later..

		my_header.BlankHeader();
	}

	rewrite_header_on_close = false;
}

void MRCFile::ReadSlicesFromDisk(int start_slice, int end_slice, float *output_array)
{
	MyDebugAssertTrue(my_file.is_open(), "File not open!");
	MyDebugAssertTrue(start_slice <= ReturnNumberOfSlices(), "Start slice number larger than total slices!");
	MyDebugAssertTrue(end_slice <= ReturnNumberOfSlices(), "end slice number larger than total slices!");
	MyDebugAssertTrue(start_slice <= end_slice, "Start slice larger than end slice!");

	// calculate and seek to the start byte..

	long records_to_read = my_header.nx[0] * my_header.ny[0] * ((end_slice - start_slice) + 1);
	long bytes_per_slice = my_header.nx[0] * my_header.ny[0] * my_header.bytes_per_pixel;
	long image_offset = (start_slice - 1) * bytes_per_slice;

	my_file.seekg(1024 + image_offset + my_header.symmetry_data_bytes[0]);

	// we need a temp array for non float formats..

	switch ( my_header.mode[0] )
	{
		case 0:
		{
			char *temp_char_array = new char [records_to_read];
			my_file.read(temp_char_array, records_to_read);

			for (long counter = 0; counter < records_to_read; counter++)
			{
				output_array[counter] = float(temp_char_array[counter]);
			}

			delete [] temp_char_array;
		}
		break;

		case 1:
		{
			short *temp_short_array = new short [records_to_read];
			my_file.read((char*)temp_short_array, records_to_read * 2);

			for (long counter = 0; counter < records_to_read; counter++)
			{
				output_array[counter] = float(temp_short_array[counter]);
			}

			delete [] temp_short_array;
		}
		break;

		case 2:
			my_file.read((char*)output_array, records_to_read * 4);
		break;

		default:
		{
			printf("Error: Complex MRC files not currently supported!!\n");
			abort();
		}
		break;
	}
}

void MRCFile::WriteSlicesToDisk(int start_slice, int end_slice, float *input_array)
{
	MyDebugAssertTrue(my_file.is_open(), "File not open!");
	MyDebugAssertTrue(start_slice <= end_slice, "Start slice larger than end slice!");
	MyDebugAssertTrue(start_slice <= ReturnNumberOfSlices(), "Start slice number larger than total slices!");
	MyDebugAssertTrue(end_slice <= ReturnNumberOfSlices(), "end slice number larger than total slices!");


	// calculate and seek to the start byte..

	long records_to_read = my_header.nx[0] * my_header.ny[0] * ((end_slice - start_slice) + 1);
	long bytes_per_slice = my_header.nx[0] * my_header.ny[0] * my_header.bytes_per_pixel;
	long image_offset = (start_slice - 1) * bytes_per_slice;

	my_file.seekg(1024 + image_offset + my_header.symmetry_data_bytes[0]);

	// we need a temp array for non float formats..

	switch ( my_header.mode[0] )
	{
		case 0:
		{
			char *temp_char_array = new char [records_to_read];

			for (long counter = 0; counter < records_to_read; counter++)
			{
				temp_char_array[counter] = char(input_array[counter]);
			}

			my_file.write(temp_char_array, records_to_read);

			delete [] temp_char_array;
		}
		break;

		case 1:
		{
			short *temp_short_array = new short [records_to_read];

			for (long counter = 0; counter < records_to_read; counter++)
			{
				temp_short_array[counter] = short(input_array[counter]);
			}

			my_file.write((char*)temp_short_array, records_to_read * 2);

			delete [] temp_short_array;
		}
		break;

		case 2:
			my_file.write((char*)input_array, records_to_read * 4);
		break;

		default:
		{
			printf("Error: Complex MRC files not currently supported!!\n");
			abort();
		}
		break;
	}
}
