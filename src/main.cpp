#include <iostream>
#include <string>

#include "dds_loader.h"

void Help() 
{
    
}

int main(int argc, char* argv[])
{

    // 1. Command line parsing 
    if (argc < 2) {
        Help();
        return 1;
    }

    std::string input_file = argv[1];

    // 2. Read DDS file 
    DdsLoader loader;
    if (!loader.Load(input_file))
    {
        std::cerr << "Failed to load DDS file: " << input_file << std::endl;
        return 1;
    }

    // 4. Checking Size of Resoluation 
    std::cout << "Original Size : " << loader.width() << "x" << loader.height() << std::endl;
    std::cout << "MipMap Count  : " << loader.mip_map_count() << std::endl;
    std::cout << "BC1 Block Cnt : " << loader.bc1_blocks().size() << std::endl;

    // 5. Allocate memory of texture mipmap accroding to mipmap size 

    // 6. Prepare mutil-threading or GPU backend 

    // 7. Generate texture mipmap 

    // 8. Store generated mipmap data

    // 9. exit 

    return 0;
}