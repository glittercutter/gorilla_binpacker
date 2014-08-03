#pragma once

#include "binpack2d.hpp"

#include <FreeImage.h>

#include <fstream>
#include <iostream>
#include <string>
#include <stdexcept>


std::string g_whitepixel_name = "__whitepixel__";
unsigned g_whitepixel_size = 3;


class GlyphData
{
public:
    int x() { return values[0]; }
    int y() { return values[1]; }
    int w() { return values[2]; }
    int h() { return values[3]; }

    void extractFromLine(const std::string& line)
    {
        std::size_t startPos = 0;
        std::size_t endPos = 0;

        for (unsigned i = 0; i < 4; i++)
        {
            startPos = line.find(" ", endPos);
            endPos = line.find(" ", startPos+1);
            if (startPos == std::string::npos) throw std::runtime_error("Error extracting glyph data");
            std::string num = line.substr(startPos, endPos);
            values[i] = atoi(num.c_str());
        }
    }

protected:
    int values[4];
};


std::string stripPath(const std::string& filename)
{
    std::size_t startPos = 0;

    // Nix directory separator
    std::size_t symbolPos = filename.find_last_of('/');
    if (symbolPos != std::string::npos) startPos = symbolPos + 1;

    // Windows directory separator
    symbolPos = filename.find_last_of('\\');
    if (symbolPos != std::string::npos && symbolPos > startPos) startPos = symbolPos + 1;

    return filename.substr(startPos, std::string::npos);
}


std::string stripExtension(const std::string& filename)
{
    return filename.substr(0, filename.find_last_of('.'));
}


class GorillaFontParser
{
public:
    GorillaFontParser(const std::string& imageFilename)
    {
        std::string fn = stripExtension(imageFilename);
        fn += ".gorilla";
        mFile.open(fn.c_str());
        if (!isLoaded()) return;

        parseDimension();
    }

    bool isLoaded() const { return mFile.is_open(); }
    unsigned getWidth() const { return mWidth; }
    unsigned getHeight() const { return mHeight; }

    void appendGorilla(std::ofstream& outFile, unsigned xOffset, unsigned yOffset)
    {
        resetSeek();

        appendInfo(outFile, "[Font.");
        appendInfo(outFile, "lineheight ");
        appendInfo(outFile, "spacelength ");
        appendInfo(outFile, "baseline ");
        appendInfo(outFile, "kerning ");
        appendInfo(outFile, "letterspacing ");
        appendInfo(outFile, "monowidth ");
        appendInfo(outFile, "range ");
        outFile << "offset " << xOffset << " " << yOffset << std::endl;

        resetSeek();

        std::string line;
        GlyphData glyphs;

        while (!mFile.eof())
        {
            std::getline(mFile, line);
            if (line.find("glyph_") != std::string::npos)
            {
                outFile << line.substr(0, line.find(" ")) << " ";
                glyphs.extractFromLine(line);
                outFile << glyphs.x() << " ";
                outFile << glyphs.y() << " ";
                outFile << glyphs.w() << " ";
                outFile << glyphs.h() << " ";
                outFile << std::endl;
            }
            else if (line.find("verticaloffset_") != std::string::npos)
            {
                outFile << line << std::endl;
            }
        }
    }

protected:
    void parseDimension()
    {
        resetSeek();

        mWidth = mHeight = 0;
        std::string line;

        GlyphData glyphs;

        while (!mFile.eof())
        {
            std::getline(mFile, line);
            if (line.find("glyph_") != std::string::npos)
            {
                glyphs.extractFromLine(line);

                unsigned w = glyphs.x() + glyphs.w();
                if (w > mWidth) mWidth = w;

                unsigned h = glyphs.y() + glyphs.h();
                if (h > mHeight) mHeight = h;
            }
        }
    }

    void appendInfo(std::ofstream& outFile, const std::string& info)
    {
        resetSeek();

        std::string line;
        while (!mFile.eof())
        {
            std::getline(mFile, line);
            if (line.find(info) != std::string::npos) 
            {
                outFile << line << std::endl;
                break;
            }
        }
    }

    void resetSeek()
    {
        mFile.clear();
        mFile.seekg(0, std::ios::beg);
    }

    unsigned mWidth;
    unsigned mHeight;
    std::ifstream mFile;
};


// Your data - whatever you want to associate with 'rectangle'
class MyContent
{
public:
    MyContent(const std::string& name) : mName(name), mBitmap(NULL), mFontParser(NULL)
    {
        if (name == g_whitepixel_name)
        {
            mBitmap = FreeImage_Allocate(g_whitepixel_size, g_whitepixel_size, 32);
            if (!mBitmap) throw std::runtime_error("Error creating white pixel");
            BYTE white[] = {0xff, 0xff, 0xff, 0xff};
            FreeImage_FillBackground(mBitmap, white, 0);
            mWidth = mHeight = g_whitepixel_size;
        }
        else
        {
            FREE_IMAGE_FORMAT fmt = FreeImage_GetFileType(mName.c_str(), 0);
            if (fmt == FIF_UNKNOWN) throw std::runtime_error("Error loading input image:"+mName);

            mBitmap = FreeImage_Load(fmt, mName.c_str(), 0);

            mWidth = FreeImage_GetWidth(mBitmap);
            mHeight = FreeImage_GetHeight(mBitmap);

            initFontParser();
        }

        std::cout<<"New image loaded: "<<getName()<<" - width:"<<getWidth()<<" - height:"<<getHeight()<<std::endl;
    }

    void appendGorilla(std::ofstream& file, const BinPack2D::Content<MyContent>& content)
    {
        if (isFont())
        {
            mFontParser->appendGorilla(file, content.coord.x, content.coord.y);
        }
        else
        {
            file << stripExtension(stripPath(mName)) << " ";
            file << content.coord.x << " ";
            file << content.coord.y << " ";
            file << content.size.w << " ";
            file << content.size.h << " ";
            file << std::endl;
        }
    }

    bool isFont() const { return mFontParser; }
    const std::string& getName() const { return mName; }
    unsigned getWidth() const { return mWidth; }
    unsigned getHeight() const { return mHeight; }
    const FIBITMAP* getBitmap() const { return mBitmap; }

protected:
    void initFontParser()
    {
        mFontParser = new GorillaFontParser(mName);
        if (!mFontParser->isLoaded())
        {
            // Assume this file is not a font
            delete mFontParser;
            mFontParser = NULL;
            return;
        }

        // Update size using font data
        mWidth = mFontParser->getWidth();
        mHeight = mFontParser->getHeight();

        // Crop using new dimensions
        FIBITMAP* croppedBitmap = FreeImage_Copy(mBitmap, 0,0,mWidth,mHeight);
        FreeImage_Unload(mBitmap);
        mBitmap = croppedBitmap;
    }

    std::string mName;
    FIBITMAP* mBitmap;
    GorillaFontParser* mFontParser;
    unsigned mWidth;
    unsigned mHeight;
};


typedef BinPack2D::Content<MyContent>::Vector::iterator binpack2d_iterator;
