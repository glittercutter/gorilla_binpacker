/*
Copyright (c) 2014 Sebastien Raymond <github.com/glittercutter>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#include "binpack2d.hpp"
#include "gorilla_binpacker.hpp"

#include <FreeImage.h>

#include <fstream>
#include <iostream>
#include <string>
#include <deque>
#include <stdexcept>
#include <vector>
#include <stdio.h>
#include <string.h>


unsigned g_num_of_bin = 1;
unsigned g_min_bin_dimension = 128;


int loadImages(const std::deque<std::string>& inputFilenames, BinPack2D::ContentAccumulator<MyContent>& inputContent)
{
    // Load files
    for (std::deque<std::string>::const_iterator it = inputFilenames.begin(); it != inputFilenames.end(); it++)
    {
        MyContent mycontent(*it);
        inputContent += BinPack2D::Content<MyContent>(
            mycontent, BinPack2D::Coord(), 
            BinPack2D::Size(mycontent.getWidth(), mycontent.getHeight()), false);
    }

    // Create whitepixel
    MyContent mycontent(g_whitepixel_name);
    inputContent += BinPack2D::Content<MyContent>(
        mycontent, BinPack2D::Coord(), 
        BinPack2D::Size(mycontent.getWidth(), mycontent.getHeight()), false);

    // Sort the input content by size... usually packs better.
    inputContent.Sort();
}


void appendGorillaWhitePixel(std::ofstream& file, BinPack2D::ContentAccumulator<MyContent>& outputContent)
{
    for (binpack2d_iterator itor = outputContent.Get().begin(); itor != outputContent.Get().end(); itor++)
    {
        const BinPack2D::Content<MyContent> &content = *itor;

        // retreive your data.
        const MyContent& myContent = content.content;

        if (myContent.getName() == g_whitepixel_name)
        {
            file << "whitepixel "; 
            file << content.coord.x + content.size.w/2 << " "; 
            file << content.coord.y + content.size.h/2 << '\n'; 
            return;
        }
    }

    throw std::runtime_error("Where is the whitepixel?");
}


void appendGorillaFonts(std::ofstream& file, BinPack2D::ContentAccumulator<MyContent>& outputContent)
{
    for (binpack2d_iterator itor = outputContent.Get().begin(); itor != outputContent.Get().end(); itor++)
    {
        BinPack2D::Content<MyContent> &content = *itor;

        // retreive your data.
        MyContent& myContent = content.content;

        if (myContent.isFont()) myContent.appendGorilla(file, content);
    }
}


void appendGorillaSprites(std::ofstream& file, BinPack2D::ContentAccumulator<MyContent>& outputContent)
{
    for (binpack2d_iterator itor = outputContent.Get().begin(); itor != outputContent.Get().end(); itor++)
    {
        BinPack2D::Content<MyContent> &content = *itor;

        // retreive your data.
        MyContent& myContent = content.content;

        if (!myContent.isFont() && myContent.getName() != g_whitepixel_name) myContent.appendGorilla(file, content);
    }
}


int packImages(const BinPack2D::ContentAccumulator<MyContent>& inputContent, const std::string& outputFilename, unsigned width, unsigned height)
{
    // Create some bins!
    BinPack2D::CanvasArray<MyContent> canvasArray = 
        BinPack2D::UniformCanvasArrayBuilder<MyContent>(width, height, g_num_of_bin).Build();

    // A place to store content that didnt fit into the canvas array.
    BinPack2D::ContentAccumulator<MyContent> remainder;

    // try to pack content into the bins.
    bool success = canvasArray.Place(inputContent, remainder);

    // A place to store packed content.
    BinPack2D::ContentAccumulator<MyContent> outputContent;

    // Read all placed content.
    canvasArray.CollectContent(outputContent);

    // Parse output.
    printf("\nResult for a bin of size %dx%d.\n", width, height);
    printf("  PLACED: %d/%d\n", outputContent.Get().size(), inputContent.Get().size());
    for (binpack2d_iterator itor = outputContent.Get().begin(); itor != outputContent.Get().end(); itor++)
    {
        const BinPack2D::Content<MyContent> &content = *itor;

        // retreive your data.
        const MyContent& myContent = content.content;

        printf("    %s of size %3dx%3d at position %3d,%3d,%2d rotated=%s\n",
        myContent.getName().c_str(), 
        content.size.w, 
        content.size.h, 
        content.coord.x, 
        content.coord.y, 
        content.coord.z, 
        (content.rotated ? "yes":" no"));
    }

    printf("  NOT PLACED: %d/%d\n", remainder.Get().size(), inputContent.Get().size());
    for(binpack2d_iterator itor = remainder.Get().begin(); itor != remainder.Get().end(); itor++)
    {
        const BinPack2D::Content<MyContent> &content = *itor;

        const MyContent &myContent = content.content;

        printf("    %s of size %3dx%3d\n",
        myContent.getName().c_str(), 
        content.size.w, 
        content.size.h);
    }

    if (!remainder.Get().empty()) return 1;

    // Create image
    FIBITMAP* outputBitmap = FreeImage_Allocate(width, height, 32);
    if (!outputBitmap) throw std::runtime_error("Error creating output image");

    // Fill background
    BYTE transparent_byte = 0x00;
    FreeImage_SetTransparencyTable(outputBitmap, &transparent_byte, 1);

    // Pack output image with data from our bin
    for (binpack2d_iterator itor = outputContent.Get().begin(); itor != outputContent.Get().end(); itor++)
    {
        const BinPack2D::Content<MyContent> &content = *itor;

        // retreive your data.
        const MyContent& myContent = content.content;

        if (!FreeImage_Paste(outputBitmap, const_cast<FIBITMAP*>(myContent.getBitmap()), 
                             content.coord.x, content.coord.y,
                             256)) throw std::runtime_error("Error pasting to output image");
    }

    // Save image to file
    FREE_IMAGE_FORMAT fmt = FreeImage_GetFIFFromFilename(outputFilename.c_str());
    if (fmt == FIF_UNKNOWN) throw std::runtime_error("Unknow output file format");
    FreeImage_Save(fmt, outputBitmap, outputFilename.c_str(), 0);

    // Create the gorilla file
    std::string filename = stripExtension(outputFilename)+".gorilla"; // Swap file extension
    std::ofstream file(filename.c_str());

    // Append header (file/whitepixel)
    file << "[Texture]" << std::endl;
    file << "file " << outputFilename << std::endl;
    appendGorillaWhitePixel(file, outputContent);
    file << std::endl;
    
    // Append fonts
    appendGorillaFonts(file, outputContent);
    file << std::endl;

    // Append sprites
    file << "[Sprites]" << std::endl;
    appendGorillaSprites(file, outputContent);
    
    return 0;
}


int main(int argc, char** argv)
{
    std::deque<std::string> inputFilenames;
    std::string outputFilename;

    // Parse arguments
    {
        for (size_t i = 1; i < argc; i++)
        {
            if (!strcmp(argv[i], "-o") && ++i < argc) outputFilename = std::string(argv[i]);
            // TODO image size...
            //else if (!strcmp(argv[i], "-port") && ++i < argc) mSettings.server_port = std::stoi(std::string(argv[i]));
            else
            {
                inputFilenames.push_back(std::string(argv[i]));
            }
        }

        if (inputFilenames.empty() || outputFilename.empty())
        {
            std::cout<<"Usage: [ -o output filename ] [ input filenames ... ]";
            return 1;
        }
    }

    FreeImage_Initialise();

    BinPack2D::ContentAccumulator<MyContent> inputContent;
    loadImages(inputFilenames, inputContent);
    printf("\n");

    if (!inputContent.Get().empty())
    {
        unsigned curr_size = g_min_bin_dimension;
        unsigned next_size = curr_size * 2;

        while (true)
        {
            // Try all size combinations
            if (!packImages(inputContent, outputFilename, curr_size, curr_size)) break;
            if (!packImages(inputContent, outputFilename, next_size, curr_size)) break;
            if (!packImages(inputContent, outputFilename, curr_size, next_size)) break;

            curr_size = next_size;
            next_size *= 2;
        }
    }

    FreeImage_DeInitialise();

    return 0;
}
