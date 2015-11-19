/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2006 Robert Osfield
 *
 * This library is open source and may be redistributed and/or modified under
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * OpenSceneGraph Public License for more details.
*/

#ifndef FREETYPE_FONT
#define FREETYPE_FONT 1

#include <osgText/Font>

#include <ft2build.h>
#include FT_FREETYPE_H

class FreeTypeFont : public osgText::Font::FontImplementation
{
// declare the interface to a font.
public:

    FreeTypeFont(const std::string& filename, FT_Face face, unsigned int flags);
    FreeTypeFont(FT_Byte* buffer, FT_Face face, unsigned int flags);

    virtual ~FreeTypeFont();

    virtual std::string getFileName() const { return _filename; }

    virtual bool supportsMultipleFontResolutions() const { return true; }

    virtual osgText::Glyph* getGlyph(const osgText::FontResolution& fontRes,unsigned int charcode);

    virtual osgText::Glyph3D* getGlyph3D(unsigned int charcode);

    virtual osg::Vec2 getKerning(unsigned int leftcharcode,unsigned int rightcharcode, osgText::KerningType _kerningType);

    virtual bool hasVertical() const;

    virtual bool getVerticalSize(float & ascender, float & descender) const;

    float getCoordScale() const;

protected:

    void init();

    void setFontResolution(const osgText::FontResolution& fontSize);

    osgText::FontResolution _currentRes;

    long ft_round( long x ) { return (( x + 32 ) & -64); }
    long ft_floor( long x ) { return (x & -64); }
    long ft_ceiling( long x ){ return (( x + 63 ) & -64); }

    std::string             _filename;
    FT_Byte*                _buffer;
    FT_Face                 _face;
    unsigned int            _flags;
};

#endif
